/* SPDX-License-Identifier: MIT */
/* The kernel half of the NativeAdreno stack on the host: the real GPU context bring-up (software SMMU register peer), VM, ring,
 * submit object and user-client core over the authored memory doubles, with a software "CP" that retires every submission at
 * once. It runs no shader and touches no hardware; it exists so the Turnip darwin backend can be driven through a real
 * Vulkan instance before the userspace side ever runs on the tablet. Narrow C interface for the IOKit shim translation unit. */
#include "XnuMemoryFixture.h"
#include "NativeAdrenoGpuContext.h"
#include "NativeAdrenoUserClient.h"
#include "NativeAdrenoTimer.h"
#include <chrono>
#include <cstdio>
#include <thread>

IOReturn NativeAdrenoQueryCpu4K(NativeAdrenoCpuState *state) {*state={};return kIOReturnSuccess;}
IOReturn NativeAdrenoMaintainOwnedPages(void *,size_t,NativeAdrenoCacheOperation) {return kIOReturnSuccess;}

namespace {
constexpr size_t kSmmuBytes=0x40000;
alignas(8) uint32_t mmio[kSmmuBytes/4];
int sread32(void *,size_t at,uint32_t *v) {*v=mmio[at/4];return 0;}
int sread64(void *,size_t at,uint64_t *v) {std::memcpy(v,mmio+at/4,8);return 0;}
int swrite32(void *,size_t at,uint32_t v) {
    if((at&0xfff)==0x58 && at>=0x20000) {mmio[at/4]&=~v;return 0;}   /* context FSR is write-one-to-clear */
    mmio[at/4]=v;return 0;
}
int swrite64(void *,size_t at,uint64_t v) {std::memcpy(mmio+at/4,&v,8);return 0;}
int sbarrier(void *) {return 0;}

/* Software CP: a write of the ring write pointer retires everything up to it, and the fence the ring reads is the number of
 * submissions so far (one write pointer update per submission). */
std::vector<uint32_t> regs(0x10000);
volatile uint32_t *memptrs=nullptr;
unsigned submissions=0;
int cwrite(void *,uint32_t reg,uint32_t v) {
    regs[reg]=v;
    if(reg==0x0807) {regs[0x0806]=v;++submissions;if(memptrs)memptrs[1]=submissions;}
    return 0;
}
int cread(void *,uint32_t reg,uint32_t *v) {*v=regs[reg];return 0;}
int cbarrier(void *) {return 0;}
int cpublish(void *,const volatile uint32_t *,size_t) {return 0;}
int cacquire(void *,volatile uint32_t *,size_t) {return 0;}
uint64_t cnow(void *) {
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
int cidle(void *) {std::this_thread::sleep_for(std::chrono::microseconds(50));return 0;}
int timeread(void *,uint32_t address,uint32_t *value) {
    static uint64_t ticks=1000;
    if(address==0x0980)ticks+=1000;
    *value=address==0x0980?uint32_t(ticks):(address==0x0981?uint32_t(ticks>>32):0);return 0;
}
const NativeAdrenoAbiDeviceInfo info={0x43050a01u,3u<<20,0x1000000ull,0x1fffffffff000ull,0x20000000ull,0x60000000ull,
    16,6,1,4,NativeAdrenoDeviceClientIova,0};
const NativeAdrenoRingIo ringio{nullptr,cwrite,cread,cbarrier};
const NativeAdrenoRingCache ringcache{nullptr,cpublish,cacquire};
const NativeAdrenoRingClock clock_{nullptr,cnow,cidle};
const NativeAdrenoTimeIo timeio{nullptr,timeread};

NativeAdrenoGpuContext *context;
NativeAdrenoRing ring;
NativeAdrenoSubmit execbuf;
NativeAdrenoUserClient client;
NativeAdrenoTimer timer;
bool built=false,opened=false;

bool build() {
    if(built)return true;
    std::memset(mmio,0,sizeof(mmio));
    mmio[0]=0x002f0406;mmio[0x20/4]=0x4c017e18;mmio[0x24/4]=0x40000016;mmio[0x28/4]=0x1433;
    for(unsigned i=0;i<24;++i)mmio[(0xc00+4*i)/4]=0x20000;
    static const ArmSmmu4KIo io{nullptr,kSmmuBytes,sread32,sread64,swrite32,swrite64,sbarrier};
    context=new NativeAdrenoGpuContext;
    NativeAdrenoGmuContextReport report;
    const unsigned slots[2]={2,3};
    const uint16_t sids[2]={0,1};
    const IOReturn up=context->bringUp(&io,1,2,slots,sids,nullptr,&report);
    if(up!=kIOReturnSuccess) {std::fprintf(stderr,"host gpu: context bring-up failed %d at step %u\n",up,report.step);return false;}
    const NativeAdrenoMemory &ringmem=context->buffer(1),&memory=context->buffer(2);
    memptrs=static_cast<volatile uint32_t *>(memory.mapping());
    if(ring.attach(&ringio,&ringcache,ringmem.mapping(),ringmem.bytes(),GpuRingIova,memory.mapping(),memory.bytes(),
        GpuMemptrsIova)!=kIOReturnSuccess) {std::fprintf(stderr,"host gpu: ring attach failed\n");return false;}
    if(execbuf.attach(context->vm(),&ring)!=kIOReturnSuccess) {std::fprintf(stderr,"host gpu: submit attach failed\n");return false;}
    if(timer.attach(&timeio,NativeAdrenoTimeCp)!=kIOReturnSuccess)return false;
    /* Ring attach already wrote the write pointer once (the ring's own start-up packets); only submissions after this count as fences. */
    submissions=0;memptrs[1]=0;
    built=true;
    return true;
}
IOReturn translate(IOReturn status) {return status;}
}

extern "C" {
/* Return codes are the fixture's IOReturn values; the shim translates them. */
int host_gpu_open() {
    if(!build() || opened)return kIOReturnBusy;
    const IOReturn st=client.open(NativeAdrenoAbiVersion,context->vm(),&execbuf,&ring,31,40);
    if(st) {std::fprintf(stderr,"host gpu: client open failed %d\n",st);return kIOReturnNotReady;}
    client.attachTimer(&timer);
    client.attachDeviceInfo(&info);
    opened=true;
    return kIOReturnSuccess;
}
int host_gpu_close() {
    if(!opened)return kIOReturnNotReady;
    const IOReturn status=client.releaseAll(&clock_,2000000);
    if(!status)opened=false;
    return translate(status);
}
int host_gpu_call(uint32_t method,const void *in,size_t in_size,void *out,size_t out_size) {
    if(!opened)return kIOReturnNotReady;
    /* As the IOKit wrapper does (NativeAdrenoPublish.cpp): an import is the calling task's pages, here a stand-in task. */
    if(method==NativeAdrenoMethodBoImport) {
        if(!in || in_size!=sizeof(NativeAdrenoAbiBoImport) || !out || out_size!=sizeof(NativeAdrenoAbiBoAllocReply))return kIOReturnBadArgument;
        NativeAdrenoAbiBoImport request{};std::memcpy(&request,in,sizeof(request));
        NativeAdrenoAbiBoAllocReply reply{};
        const IOReturn status=client.importBo(&request,reinterpret_cast<void *>(0x1000),&reply);
        if(!status)std::memcpy(out,&reply,sizeof(reply));
        return translate(status);
    }
    return translate(client.dispatch(method,in,in_size,out,out_size,&clock_));
}
int host_gpu_map(uint32_t token,void **address,uint64_t *bytes) {
    NativeAdrenoMemory *memory=opened?client.memoryForToken(token):nullptr;
    if(!memory)return kIOReturnBadArgument;
    *address=memory->mapping();*bytes=memory->bytes();return kIOReturnSuccess;
}
unsigned host_gpu_submissions() {return submissions;}
unsigned host_gpu_live_objects() {return unsigned(client.liveObjects());}
}
