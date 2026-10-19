/* SPDX-License-Identifier: MIT */
/* IOKit surface for an already-running GPU context. The board code (kits/sheng) brings the GPU up, then hands this file
 * the live VM, submit object, ring, clock and optional timer; one IOService is published and each IOServiceOpen creates a
 * connection that mounts the NativeAdrenoUserClient core. Only one connection exists at a time: the core, the VM and the
 * ring have one owner, and closing the connection waits for the CP, then unmaps and frees everything it created.
 * Not yet done: per-client isolation (all objects share one GPU address space), entitlement/privilege checks on open, and
 * a reset path. This is not the Metal or Turnip ABI; it carries NativeAdrenoABI.h unchanged.
 * BO CPU mappings are handed out as uncached physical-range mappings to match the GPU's non-cacheable PTEs.
 * The connection class is named AGXDeviceUserClient on purpose: the original sandbox profiles of the processes that use Metal
 * allow IOServiceOpen for that user-client class name, and nothing else in this system can create one (the Apple AGX kext is
 * never loaded without an Apple GPU), so the name only decides whether the Adreno Metal plugin may open its own driver. */
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOUserClient.h>
#include <string.h>
#include <kern/thread.h>
#include "NativeAdrenoUserClient.h"

extern "C" void proc_selfname(char *, int);

extern "C" {
/* What the processes that reached this driver did, for the boot diagnostic page. Plain counters and short strings. */
struct NativeAdrenoMetalDiag {
    uint32_t opens, busy, bad, closes, reports, property_reads, foreign_opens, foreign_type, name_reads, calls, last_selector, last_status, ext_entries, ext_selector, ext_status, ext_in, ext_out, tunnel_calls, probes, sel_bits[8], mb_maps, mb_calls, marks_total, marks[16], chk_macf, chk_res, chk_type, type_pass_lo, type_pass_hi, attach_status, inflight, inflight_since_calls, forced_unmaps;
    char opener[3][17];      /* process names of the latest connections, newest first */
    char reader[3][17];      /* process names that read a registry property of the accelerator, newest first */
    char name_reader[17];    /* latest process that read MetalPluginName specifically */
    char starter[3][17];     /* processes that sent plugin marker 1 (the plugin's initWithAcceleratorPort: started), newest first */
    char report[64];         /* the plugin's own result line (SetMetadata text), latest */
    char report2[64];        /* the plugin's Turnip-phase line (text starting "T " or "W "), latest */
} native_adreno_metal_diag;
}

/* The open type the Adreno plugin passes to IOServiceOpen ('AG'); it lets the driver tell the plugin from the original IOGPU library
 * (type 1 plus an API value) and from any other client of the accelerator. */
enum : UInt32 {NativeAdrenoPluginOpenType=0x4147,NativeAdrenoTunnelMagic=0x4e544e4c /* 'NTNL' */};

/* Mailbox: a last-resort command channel for a sandbox that admits no external-method selector at all. It is one shared page (memory
 * type NativeAdrenoMailboxType through IOConnectMapMemory64, which the selector filter does not cover) and a kernel poller that runs
 * whatever the live connection writes into it. Layout (little endian dwords): +0 magic, +4 request seq, +8 response seq, +12 method,
 * +16 status, +20 input bytes, +24 output bytes, input at +0x40 (max 0x700), output at +0x800 (max 0x700). */
enum : UInt32 {NativeAdrenoMailboxType=0xC0DE,NativeAdrenoMailboxMagic=0x4d41494c /* 'MAIL' */};
namespace {
IOBufferMemoryDescriptor *gMailbox=nullptr;
volatile uint32_t *gMailboxWords=nullptr;
volatile bool gPollerStarted=false;
}
class AGXDeviceUserClient;
static volatile bool gMailboxLive=false;   /* a connection is mounted; set and cleared by it, read by the poller, which never touches the object */
void nativeAdrenoMailboxPoller(void *,wait_result_t);
static IOReturn runCore(bool live,uint32_t method,const void *input,size_t input_size,void *output,size_t output_size);

/* The two sandbox gates IOServiceOpen applies after newUserClient (XNU IOUserClient.cpp is_io_service_open_extended): the MACF open check on
 * the client object and the sandbox's filter resolver. They are run here for the caller too, only to record which one refuses it. */
extern "C" int mac_iokit_check_open(void *cred,void *client,unsigned type);
extern "C" void *kauth_cred_get(void);
extern const struct io_filter_callbacks *gIOUCFilterCallbacks;

extern "C" volatile uint32_t native_adreno_trace_stage;
extern "C" {
/* One entry per IOServiceOpen of the accelerator (not markers): who, which type, what the two sandbox gates said. */
struct NativeAdrenoOpenRecord {char name[20]; uint32_t type, macf, res, attach, foreign;};
struct NativeAdrenoOpenRecord native_adreno_open_log[8];
volatile uint32_t native_adreno_open_log_count;
}

namespace {
void shiftName(char names[3][17]) {
    memcpy(names[2], names[1], 17);
    memcpy(names[1], names[0], 17);
    proc_selfname(names[0], 17);
}
struct Published {
    NativeAdrenoUserClient *core=nullptr;
    NativeAdrenoVm *vm=nullptr;
    NativeAdrenoSubmit *submit=nullptr;
    NativeAdrenoRing *ring=nullptr;
    NativeAdrenoRingClock clock{};
    const NativeAdrenoTimer *timer=nullptr;
    const NativeAdrenoAbiDeviceInfo *info=nullptr;
    IOLock *lock=nullptr;
    IOService *service=nullptr;
    bool connected=false;
    uint32_t input_bits=0,output_bits=0;
    uint32_t opened=0,closed=0;
} g;
}

class NativeAdrenoGpuPublished : public IOService {
    OSDeclareDefaultStructors(NativeAdrenoGpuPublished)
public:
    IOReturn newUserClient(task_t owningTask,void *securityID,UInt32 type,
        OSDictionary *properties,IOUserClient **handler) APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(NativeAdrenoGpuPublished,IOService)

class AGXDeviceUserClient : public IOUserClient {
    OSDeclareDefaultStructors(AGXDeviceUserClient)
    bool live_=false;
    /* One user-view descriptor per BO token, reused by every clientMemoryForType call. IOConnectUnmapMemory64 asks clientMemoryForType for
     * the descriptor again and unmaps only a mapping that belongs to THAT descriptor (kIOMapReference); a fresh descriptor per call made the
     * unmap a silent no-op, the user PTEs outlived BoCpuUnmap, and BoFree handed pages with a live user mapping to the VM:
     * pmap_assert_free panicked the freeing core and the panic screen parked it (the "BoFree hang", dead touch). */
    struct View {uint32_t token;IOMemoryDescriptor *descriptor;};
    enum {MaxViews=64};
    View views_[MaxViews]={};
    void dropViews(uint32_t token);
    static IOReturn action(OSObject *target,void *reference,IOExternalMethodArguments *arguments);
    IOReturn run(uint32_t method,const void *input,size_t input_size,void *output,size_t output_size);
public:
    bool mount();
    IOReturn clientClose() APPLE_KEXT_OVERRIDE;
    void free() APPLE_KEXT_OVERRIDE;
    IOReturn clientMemoryForType(UInt32 type,IOOptionBits *options,IOMemoryDescriptor **memory) APPLE_KEXT_OVERRIDE;
    IOReturn externalMethod(uint32_t selector,IOExternalMethodArguments *arguments,
        IOExternalMethodDispatch *dispatch,OSObject *target,void *reference) APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(AGXDeviceUserClient,IOUserClient)

void nativeAdrenoMailboxPoller(void *,wait_result_t)
{
    uint32_t last=0;
    for(;;) {
        IOSleep(1);
        if(!gMailboxLive || !gMailboxWords)continue;
        const uint32_t request=gMailboxWords[1];
        if(request==last || request==gMailboxWords[2])continue;
        last=request;
        const uint32_t method=gMailboxWords[3],in_size=gMailboxWords[5],out_size=gMailboxWords[6];
        IOReturn status=kIOReturnBadArgument;
        if(in_size<=0x700 && out_size<=0x700) {
            uint8_t input[0x700],output[0x700];
            memcpy(input,const_cast<uint32_t *>(gMailboxWords)+0x10,in_size);
            status=runCore(g.connected,method,input,in_size,output,out_size);
            if(!status)memcpy(const_cast<uint32_t *>(gMailboxWords)+0x200,output,out_size);
        }
        native_adreno_metal_diag.mb_calls=native_adreno_metal_diag.mb_calls+1;
        gMailboxWords[4]=uint32_t(status);
        __asm__ volatile("dmb sy" ::: "memory");
        gMailboxWords[2]=request;
        __asm__ volatile("dmb sy" ::: "memory");
    }
}

bool AGXDeviceUserClient::mount()
{
    IOLockLock(g.lock);
    bool ok=false;
    if(!g.connected && g.core->open(NativeAdrenoAbiVersion,g.vm,g.submit,g.ring,g.input_bits,g.output_bits)==kIOReturnSuccess) {
        if(g.timer)g.core->attachTimer(g.timer);
        if(g.info)g.core->attachDeviceInfo(g.info);
        g.connected=true;live_=true;++g.opened;ok=true;
        if(!gMailbox) {
            gMailbox=IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,kIODirectionInOut,4096,4096);
            if(gMailbox) {gMailboxWords=static_cast<volatile uint32_t *>(gMailbox->getBytesNoCopy());memset(const_cast<uint32_t *>(gMailboxWords),0,4096);}
        }
        if(gMailboxWords) {
            memset(const_cast<uint32_t *>(gMailboxWords),0,4096);
            gMailboxWords[0]=NativeAdrenoMailboxMagic;
            if(!gPollerStarted) {
                thread_t thread=nullptr;
                if(kernel_thread_start(nativeAdrenoMailboxPoller,nullptr,&thread)==KERN_SUCCESS) {thread_deallocate(thread);gPollerStarted=true;}
            }
        }
        gMailboxLive=true;
    }
    IOLockUnlock(g.lock);
    return ok;
}
/* Removes the cached view(s) of token (0 = all) and tears down every user mapping of them, so the BO's pages are never freed while a user
 * PTE still points at them, whether or not the client unmapped. */
void AGXDeviceUserClient::dropViews(uint32_t token)
{
    for(;;) {
        IOMemoryDescriptor *view=nullptr;
        IOLockLock(g.lock);
        for(View &v:views_)if(v.descriptor && (!token || v.token==token)) {view=v.descriptor;v.descriptor=nullptr;v.token=0;break;}
        IOLockUnlock(g.lock);
        if(!view)return;
        /* The returned reference is the last one outside the descriptor's own list: dropping it frees the map, which unmaps the user range. */
        while(removeMappingForDescriptor(view))native_adreno_metal_diag.forced_unmaps=native_adreno_metal_diag.forced_unmaps+1;
        view->release();
    }
}
IOReturn AGXDeviceUserClient::clientClose()
{
    dropViews(0);
    IOLockLock(g.lock);
    IOReturn status=kIOReturnSuccess;
    if(live_) {
        status=g.core->releaseAll(&g.clock,2000000);
        /* A failed teardown leaves the objects live and the slot taken: nothing is forced or reused. */
        if(!status) {gMailboxLive=false;live_=false;g.connected=false;++g.closed;native_adreno_metal_diag.closes=native_adreno_metal_diag.closes+1;}
    }
    IOLockUnlock(g.lock);
    if(!status)terminate();
    return status;
}
void AGXDeviceUserClient::free()
{
    dropViews(0);
    IOUserClient::free();
}
IOReturn AGXDeviceUserClient::clientMemoryForType(UInt32 type,IOOptionBits *options,IOMemoryDescriptor **memory)
{
    if(memory)*memory=nullptr;
    if(!memory || !options)return kIOReturnBadArgument;
    IOReturn status=kIOReturnNotFound;
    if(type==NativeAdrenoMailboxType) {
        if(live_ && gMailbox) {
            gMailbox->retain();
            *memory=gMailbox;
            *options=kIOMapAnywhere;   /* ordinary cached memory: both sides are CPUs, so it is coherent */
            native_adreno_metal_diag.mb_maps=native_adreno_metal_diag.mb_maps+1;
            return kIOReturnSuccess;
        }
        return kIOReturnNotReady;
    }
    IOLockLock(g.lock);
    for(View &v:views_)if(v.descriptor && v.token==type) {
        v.descriptor->retain();*memory=v.descriptor;*options=kIOMapAnywhere|kIOMapInhibitCache;
        IOLockUnlock(g.lock);
        return kIOReturnSuccess;
    }
    NativeAdrenoMemory *backing=live_?g.core->memoryForToken(type):nullptr;
    if(backing) {
        /* The BO's pages are cached in the kernel and the GPU's PTEs are non-cacheable, so the client needs an uncached
         * view. A mapping of the buffer descriptor with a different cache mode is not reliable, so describe the same raw
         * physical spans (the ones the SMMU maps) and ask for kIOMapInhibitCache, exactly how the board code maps the
         * ring and the GMU windows. The descriptor does not retain the pages: the client must unmap before BoCpuUnmap/BoFree. */
        enum {MaxSpans=64};
        IOAddressRange ranges[MaxSpans];
        const size_t count=backing->spanCount();
        if(!count || count>MaxSpans)status=kIOReturnNoResources;
        else {
            for(size_t i=0;i<count;++i)ranges[i]={backing->spans()[i].physical,backing->spans()[i].bytes};
            IOMemoryDescriptor *descriptor=IOMemoryDescriptor::withAddressRanges(ranges,(UInt32)count,
                static_cast<IOOptionBits>(kIODirectionInOut)|static_cast<IOOptionBits>(kIOMemoryTypePhysical64),nullptr);
            if(!descriptor)status=kIOReturnNoMemory;
            else {
                for(View &v:views_)if(!v.descriptor) {v.token=type;v.descriptor=descriptor;descriptor->retain();break;}
                *memory=descriptor;
                *options=kIOMapAnywhere|kIOMapInhibitCache;
                status=kIOReturnSuccess;
            }
        }
    }
    IOLockUnlock(g.lock);
    return status;
}
static IOReturn runCore(bool live,uint32_t method,const void *input,size_t input_size,void *output,size_t output_size)
{
    if(method==NativeAdrenoMethodSetMetadata && input && input_size==sizeof(NativeAdrenoAbiMetadata)) {
        const NativeAdrenoAbiMetadata *meta=static_cast<const NativeAdrenoAbiMetadata *>(input);
        /* The raw test's line and the Turnip phase's line ("T ..." result or "W ..." watchdog sample) are kept apart, so neither hides the other. */
        const bool turnip=meta->bytes>=2 && (meta->text[0]=='T' || meta->text[0]=='W') && meta->text[1]==' ';
        char *line=turnip?native_adreno_metal_diag.report2:native_adreno_metal_diag.report;
        uint32_t n=meta->bytes<sizeof(native_adreno_metal_diag.report)-1?meta->bytes:uint32_t(sizeof(native_adreno_metal_diag.report)-1);
        memset(line,0,sizeof(native_adreno_metal_diag.report));
        memcpy(line,meta->text,n);
        native_adreno_metal_diag.reports=native_adreno_metal_diag.reports+1;
    }
    /* A call that is still running when the boot page is photographed shows as inflight = its method number + 1 (0 = none), so a hang in the
     * kernel is attributable to one ABI method. */
    native_adreno_metal_diag.inflight=method+1;
    native_adreno_metal_diag.inflight_since_calls=native_adreno_metal_diag.calls;
    IOLockLock(g.lock);
    const IOReturn status=live?g.core->dispatch(method,input,input_size,output,output_size,&g.clock):kIOReturnNotReady;
    IOLockUnlock(g.lock);
    native_adreno_metal_diag.inflight=0;
    native_adreno_metal_diag.calls=native_adreno_metal_diag.calls+1;
    native_adreno_metal_diag.last_selector=method;
    native_adreno_metal_diag.last_status=uint32_t(status);
    return status;
}
IOReturn AGXDeviceUserClient::run(uint32_t method,const void *input,size_t input_size,void *output,size_t output_size)
{
    if(method==NativeAdrenoMethodBoCpuUnmap && input && input_size==sizeof(NativeAdrenoAbiBoCpuUnmap))
        dropViews(static_cast<const NativeAdrenoAbiBoCpuUnmap *>(input)->token);
    if(method==NativeAdrenoMethodBoImport) {
        /* The pages belong to the task making this call (an external method runs in the caller's context). */
        if(!input || input_size!=sizeof(NativeAdrenoAbiBoImport) || !output || output_size!=sizeof(NativeAdrenoAbiBoAllocReply))
            return kIOReturnBadArgument;
        NativeAdrenoAbiBoImport request{};
        memcpy(&request,input,sizeof(request));
        NativeAdrenoAbiBoAllocReply reply{};
        native_adreno_metal_diag.inflight=method+1;
        IOLockLock(g.lock);
        const IOReturn status=live_?g.core->importBo(&request,current_task(),&reply):kIOReturnNotReady;
        IOLockUnlock(g.lock);
        native_adreno_metal_diag.inflight=0;
        native_adreno_metal_diag.calls=native_adreno_metal_diag.calls+1;
        native_adreno_metal_diag.last_selector=method;
        native_adreno_metal_diag.last_status=uint32_t(status);
        if(!status)memcpy(output,&reply,sizeof(reply));
        return status;
    }
    return runCore(live_,method,input,input_size,output,output_size);
}
IOReturn AGXDeviceUserClient::action(OSObject *target,void *,IOExternalMethodArguments *arguments)
{
    AGXDeviceUserClient *connection=OSDynamicCast(AGXDeviceUserClient,target);
    if(!connection || !arguments)return kIOReturnBadArgument;
    return connection->run(arguments->selector,arguments->structureInput,arguments->structureInputSize,
        arguments->structureOutput,arguments->structureOutputSize);
}
IOReturn AGXDeviceUserClient::externalMethod(uint32_t selector,IOExternalMethodArguments *arguments,
    IOExternalMethodDispatch *,OSObject *target,void *reference)
{
    /* One entry per ABI method; sizes are the exact wire records. */
    static const IOExternalMethodDispatch table[]={
        {&action,0,0,0,sizeof(NativeAdrenoAbiParams)},
        {&action,0,sizeof(NativeAdrenoAbiBoAlloc),0,sizeof(NativeAdrenoAbiBoAllocReply)},
        {&action,0,sizeof(NativeAdrenoAbiBoMap),0,sizeof(NativeAdrenoAbiBoMapReply)},
        {&action,0,sizeof(NativeAdrenoAbiBoFree),0,0},
        {&action,0,sizeof(NativeAdrenoAbiSubmit),0,sizeof(NativeAdrenoAbiSubmitReply)},
        {&action,0,sizeof(NativeAdrenoAbiWaitFence),0,0},
        {&action,0,0,0,sizeof(NativeAdrenoAbiTimestamp)},
        {&action,0,0,0,sizeof(NativeAdrenoAbiStatus)},
        {&action,0,sizeof(NativeAdrenoAbiMetadata),0,0},
        {&action,0,sizeof(NativeAdrenoAbiMetadata),0,sizeof(NativeAdrenoAbiMetadata)},
        {&action,0,sizeof(NativeAdrenoAbiQueue),0,sizeof(NativeAdrenoAbiQueueReply)},
        {&action,0,sizeof(NativeAdrenoAbiQueue),0,0},
        {&action,0,sizeof(NativeAdrenoAbiBoCpuMap),0,sizeof(NativeAdrenoAbiBoCpuMapReply)},
        {&action,0,sizeof(NativeAdrenoAbiBoCpuUnmap),0,0},
        {&action,0,0,0,sizeof(NativeAdrenoAbiDeviceInfo)},
        {&action,0,sizeof(NativeAdrenoAbiBoImport),0,sizeof(NativeAdrenoAbiBoAllocReply)}};
    native_adreno_metal_diag.ext_entries=native_adreno_metal_diag.ext_entries+1;
    if(selector<256)native_adreno_metal_diag.sel_bits[selector>>5]|=1u<<(selector&31);
    /* Tunnel. The sandbox resolves a filter policy for this class at open time and then admits only the selectors its profile lists,
     * so which selector numbers reach this object is decided outside the driver. A tunnel call carries the real ABI method in its
     * first eight bytes and may use ANY admitted selector: {magic, method, payload...} in, the method's exact output out. Method
     * 0xffffffff is a probe that only proves the selector was admitted. */
    if(arguments && arguments->structureInput && arguments->structureInputSize>=8 &&
        static_cast<const uint32_t *>(arguments->structureInput)[0]==NativeAdrenoTunnelMagic) {
        const uint32_t *header=static_cast<const uint32_t *>(arguments->structureInput);
        if(header[1]==0xffffffffu) {
            native_adreno_metal_diag.probes=native_adreno_metal_diag.probes+1;
            return kIOReturnSuccess;
        }
        native_adreno_metal_diag.tunnel_calls=native_adreno_metal_diag.tunnel_calls+1;
        const IOReturn tunneled=run(header[1],reinterpret_cast<const uint8_t *>(arguments->structureInput)+8,
            arguments->structureInputSize-8,arguments->structureOutput,arguments->structureOutputSize);
        native_adreno_metal_diag.ext_status=uint32_t(tunneled);
        return tunneled;
    }
    native_adreno_metal_diag.ext_selector=selector;
    native_adreno_metal_diag.ext_in=arguments?arguments->structureInputSize:0xffff;
    native_adreno_metal_diag.ext_out=arguments?arguments->structureOutputSize:0xffff;
    if(selector<1 || selector>sizeof(table)/sizeof(table[0])) {
        native_adreno_metal_diag.ext_status=uint32_t(kIOReturnUnsupported);
        return kIOReturnUnsupported;
    }
    const IOReturn result=IOUserClient::externalMethod(selector,arguments,const_cast<IOExternalMethodDispatch *>(&table[selector-1]),
        target?target:this,reference);
    native_adreno_metal_diag.ext_status=uint32_t(result);
    return result;
}
/* limit > 0 caps the connections ever granted: a plugin that crashes its host process on every launch (the process respawns and
 * loads it again) must not be able to keep a system process in a crash loop, so after the cap further opens are refused. */
extern "C" IOReturn native_adreno_new_user_client(IOService *provider,task_t owningTask,void *securityID,UInt32 type,
    OSDictionary *properties,IOUserClient **handler,unsigned limit)
{
    if(handler)*handler=nullptr;
    if(!handler || !g.core)return kIOReturnNotReady;
    /* Progress markers: open types 0x4147xxxx (xxxx nonzero) are not connections. The plugin sends one before and after each step so
     * that a sandbox that admits neither selectors nor mapping, or a crash, still leaves a trace of how far it got. */
    if((type>>16)==NativeAdrenoPluginOpenType && (type&0xffff)) {
        const uint32_t value=type&0xffff,slot=native_adreno_metal_diag.marks_total&15;
        /* Text stream into report2, one character per marker: 0x7fff starts a line, 0x70cc appends character cc. It needs no connection,
         * so the plugin's Turnip watchdog can report while Turnip itself holds the single connection slot. Not counted as markers. */
        if(value>=0x7000 && value<0x8000) {
            /* 0x7ffe starts a line in report (GPU7) instead: the Metal bridge's result, after the raw test's line has been read. */
            static uint32_t length;
            static char *line=native_adreno_metal_diag.report2;
            static_assert(sizeof(native_adreno_metal_diag.report)==sizeof(native_adreno_metal_diag.report2),"stream lines");
            if(value==0x7fff || value==0x7ffe) {
                line=value==0x7fff?native_adreno_metal_diag.report2:native_adreno_metal_diag.report;
                memset(line,0,sizeof(native_adreno_metal_diag.report2));length=0;
            }
            else if(value<0x7100 && length<sizeof(native_adreno_metal_diag.report2)-1) {line[length++]=char(value&0xff);line[length]=0;}
            return kIOReturnUnsupported;
        }
        native_adreno_metal_diag.marks[slot]=value;
        native_adreno_metal_diag.marks_total=native_adreno_metal_diag.marks_total+1;
        if(value==1) {
            memcpy(native_adreno_metal_diag.starter[2],native_adreno_metal_diag.starter[1],17);
            memcpy(native_adreno_metal_diag.starter[1],native_adreno_metal_diag.starter[0],17);
            proc_selfname(native_adreno_metal_diag.starter[0],17);
        }
        return kIOReturnUnsupported;
    }
    shiftName(native_adreno_metal_diag.opener);
    AGXDeviceUserClient *connection=new AGXDeviceUserClient;
    if(!connection)return kIOReturnNoMemory;
    if(!connection->initWithTask(owningTask,securityID,type,properties)) {
        connection->release();
        native_adreno_metal_diag.bad=native_adreno_metal_diag.bad+1;
        return kIOReturnNoResources;
    }
    /* IOService::newUserClient attaches and starts what it creates (a client without a provider chain is also a reason for the sandbox to
     * refuse). Then the two sandbox gates IOServiceOpen would apply afterwards are run here for every opener and logged with the
     * process name and open type, so the boot page shows which processes the sandbox lets reach this class and with which type. */
    unsigned attach_state=0;
    if(provider) {
        const bool attached=connection->attach(provider);
        attach_state=attached?1:2;
        if(attached && !connection->start(provider)) {
            connection->detach(provider);
            attach_state=3;
        }
    }
    native_adreno_metal_diag.attach_status=attach_state;
    const uint32_t macf=uint32_t(mac_iokit_check_open(kauth_cred_get(),connection,type));
    uint32_t resolver=0xfeedfeed;
    if(gIOUCFilterCallbacks && gIOUCFilterCallbacks->io_filter_resolver) {
        io_filter_policy_t policy=0;
        const IOReturn resolved=gIOUCFilterCallbacks->io_filter_resolver(owningTask,connection,type,&policy);
        resolver=uint32_t(resolved);
        if(resolved==kIOReturnSuccess && policy && gIOUCFilterCallbacks->io_filter_release)gIOUCFilterCallbacks->io_filter_release(policy);
    }
    native_adreno_metal_diag.chk_type=type;
    native_adreno_metal_diag.chk_macf=macf;
    native_adreno_metal_diag.chk_res=resolver;
    {
        NativeAdrenoOpenRecord &record=native_adreno_open_log[native_adreno_open_log_count&7];
        proc_selfname(record.name,sizeof(record.name));
        record.type=type;record.macf=macf;record.res=resolver;record.attach=attach_state;record.foreign=type!=NativeAdrenoPluginOpenType;
        native_adreno_open_log_count=native_adreno_open_log_count+1;
    }
    /* Only the Adreno plugin's open (type NativeAdrenoPluginOpenType) is served. The original IOGPU library opens the accelerator with type 1
     * (and an API value in the upper bits) for an IOGPUDeviceUserClient, which this driver is not; refuse it cleanly. */
    if(type!=NativeAdrenoPluginOpenType) {
        connection->release();
        native_adreno_metal_diag.foreign_opens=native_adreno_metal_diag.foreign_opens+1;
        native_adreno_metal_diag.foreign_type=type;
        return kIOReturnUnsupported;
    }
    if(macf || (resolver!=uint32_t(kIOReturnSuccess) && resolver!=uint32_t(kIOReturnUnsupported))) {
        /* The sandbox refuses this process; IOServiceOpen would refuse it after us, so do not take the single connection slot. */
        connection->release();
        return macf?kIOReturnNotPermitted:IOReturn(resolver);
    }
    if(limit && native_adreno_metal_diag.opens>=limit) {
        connection->release();
        native_adreno_metal_diag.busy=native_adreno_metal_diag.busy+1;
        return kIOReturnBusy;
    }
    if(!connection->mount()) {
        connection->release();
        native_adreno_metal_diag.busy=native_adreno_metal_diag.busy+1;
        return kIOReturnBusy;
    }
    native_adreno_metal_diag.opens=native_adreno_metal_diag.opens+1;
    *handler=connection;
    return kIOReturnSuccess;
}
extern "C" void native_adreno_note_property_read(const char *key)
{
    if(key && !strcmp(key,"MetalPluginName")) {
        proc_selfname(native_adreno_metal_diag.name_reader,17);
        native_adreno_metal_diag.name_reads=native_adreno_metal_diag.name_reads+1;
    }
    shiftName(native_adreno_metal_diag.reader);
    native_adreno_metal_diag.property_reads=native_adreno_metal_diag.property_reads+1;
}
IOReturn NativeAdrenoGpuPublished::newUserClient(task_t owningTask,void *securityID,UInt32 type,
    OSDictionary *properties,IOUserClient **handler)
{
    return native_adreno_new_user_client(this,owningTask,securityID,type,properties,handler,0);
}

/* Publishes the service once. Everything passed in must outlive the system (the board probe keeps them in statics) and
 * the VM, submit object and ring must already be attached to the live GPU context. The core must not be open. */
extern "C" IOReturn native_adreno_publish(NativeAdrenoVm *vm,NativeAdrenoSubmit *submit,NativeAdrenoRing *ring,
    const NativeAdrenoRingClock *clock,const NativeAdrenoTimer *timer,const NativeAdrenoAbiDeviceInfo *info,
    uint32_t input_bits,uint32_t output_bits)
{
    if(g.core || !vm || !submit || !ring || !clock || !clock->now_us)return kIOReturnBadArgument;
    g.lock=IOLockAlloc();
    g.core=new NativeAdrenoUserClient;
    NativeAdrenoGpuPublished *service=new NativeAdrenoGpuPublished;
    if(!g.lock || !g.core || !service || !service->init() || !service->attach(IOService::getPlatform())) {
        if(service)service->release();
        return kIOReturnNoMemory;
    }
    g.vm=vm;g.submit=submit;g.ring=ring;g.clock=*clock;g.timer=timer;g.info=info;g.input_bits=input_bits;g.output_bits=output_bits;
    g.service=service;
    service->setName("NativeAdrenoGpu");
    service->registerService();
    return kIOReturnSuccess;
}
extern "C" uint32_t native_adreno_publish_stats(uint32_t *closed)
{
    if(closed)*closed=g.closed;
    return g.opened;
}
extern "C" IOService *native_adreno_published_service()
{
    return g.service;
}
