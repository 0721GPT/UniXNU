/* SPDX-License-Identifier: MIT */
/* Real VM/memory/SMMU code over a software SMMUv2 register peer.
 * Proves ownership/lifetime/refusal logic only; no hardware is touched. */
#include "XnuMemoryFixture.h"
#include "NativeAdrenoVm.h"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#define BYTES 0x40000
#define CB 0x22000
IOReturn NativeAdrenoQueryCpu4K(NativeAdrenoCpuState *out)
{CHECK(out);*out={};return kIOReturnSuccess;}
IOReturn NativeAdrenoMaintainOwnedPages(void *mapping,size_t bytes,NativeAdrenoCacheOperation op)
{
    CHECK(mapping && bytes && !(((uintptr_t)mapping|bytes)&4095));
    CHECK(op==NativeAdrenoClean || op==NativeAdrenoCleanInvalidate || op==NativeAdrenoInvalidate);
    ++fixture.cache_calls;
    return fixture.cache_calls==fixture.cache_fail_call?kIOReturnIOError:kIOReturnSuccess;
}
static uint32_t memory[BYTES/4];
static struct {unsigned calls,fail_call,after;} f;
static ArmSmmu4KIo io;
static ArmSmmu4KStreamState streams;
static ArmSmmu4KStream routes[2]={{2,0},{4,1}};
static ArmSmmu4KStreamResult route_result;
static ArmSmmu4KContextConfig context;
static ArmSmmu4KContextImage image;
static int event(void){return ++f.calls==f.fail_call;}
static int read32(void *cookie,size_t at,uint32_t *v)
{CHECK(cookie==&f && at%4==0 && at<=BYTES-4);if(event())return 1;*v=memory[at/4];return 0;}
static int read64(void *cookie,size_t at,uint64_t *v)
{CHECK(cookie==&f && at%8==0 && at<=BYTES-8);if(event())return 1;std::memcpy(v,memory+at/4,8);return 0;}
static int write32(void *cookie,size_t at,uint32_t v)
{
    CHECK(cookie==&f && at%4==0 && at<=BYTES-4);
    int fail=event();if(fail && !f.after)return 1;
    memory[at/4]=v;return fail;
}
static int write64(void *cookie,size_t at,uint64_t v){(void)at;(void)v;(void)cookie;CHECK(0);return 1;}
static int barrier(void *cookie){CHECK(cookie==&f);return event();}
struct Fixture {
    NativeAdrenoMemory tables,object;
    ArmSmmu4KLiveArena arena;
    ArmSmmu4KLiveState state{};
    ArmSmmu4KLiveVisibility visibility;
    NativeAdrenoVm vm;
};
static void mmioReset(void)
{
    std::memset(memory,0,sizeof(memory));f={};streams=(ArmSmmu4KStreamState){};
    io=(ArmSmmu4KIo){&f,BYTES,read32,read64,write32,write64,barrier};
    memory[0x20/4]=(1u<<30)|(1u<<27)|(4u<<9)|8;
    memory[0x24/4]=0x40000010;memory[0x28/4]=0x1433;memory[0]=0x436;
    for(unsigned i=0;i<8;++i)memory[(0xc00+4*i)/4]=0x20000;
    memory[0x800/4]=1u<<31|9;memory[0xc00/4]=3;
}
/* Real prepared arena, enabled actual context image and exact stream routes. */
static IOReturn exposed(Fixture &s)
{
    mmioReset();
    IOReturn status=s.tables.allocate(32*4096,40,true,32*4096);if(status)return status;
    IOReturn smmu=NativeAdrenoPrepareSmmuMemory(&s.tables,32,40,0x800000,65536,0,16,
        &s.arena,&s.state,&s.visibility);
    if(smmu)return smmu;
    context=(ArmSmmu4KContextConfig){.lower=&s.arena.config,.lower_tables=&s.state.tables,
        .id0=memory[0x20/4],.id2=memory[0x28/4],.asid=1,.coherent_walk=1,
        .stall_on_fault=0,.continue_after_fault=1};
    ArmSmmu4KContextConfig actual=context;
    CHECK(ArmSmmu4KEncodeContext(&actual,&image)==ArmSmmu4KSuccess);
    memory[0x1008/4]=image.cbar;memory[0x1808/4]=image.cba2r;
    memory[CB/4]=image.sctlr;memory[(CB+0x10)/4]=image.tcr2;memory[(CB+0x30)/4]=image.tcr;
    std::memcpy(memory+(CB+0x20)/4,&image.ttbr0,8);
    std::memcpy(memory+(CB+0x28)/4,&image.ttbr1,8);
    memory[(CB+0x38)/4]=image.mair0;
    if(ArmSmmu4KAttachStreams(&io,&streams,routes,2,2,&context,&route_result)!=ArmSmmu4KSuccess)
        return kIOReturnBadArgument;
    NativeAdrenoVmRegion lower={&s.tables,&s.arena,&s.state};
    return s.vm.attach(&io,&context,&streams,&lower,nullptr,32,1000);
}
static IOReturn object(Fixture &s,size_t pages)
{return s.object.allocate(pages*4096,40,false,pages*4096);}
int main()
{
    /* A real map/hold/unmap transaction across the actual code paths. */
    {
        Fixture s;
        CHECK(exposed(s)==kIOReturnSuccess);
        CHECK(object(s,2)==kIOReturnSuccess);
        uint64_t address=0,handle=0;
        CHECK(s.vm.findAddress(0,8192,8192,&address)==kIOReturnSuccess);
        CHECK(address==0x800000);
        CHECK(s.vm.objectCount()==0 && s.vm.spanCount()==0);
        CHECK(s.vm.map(&s.object,0,address,3,0,2,&handle)==kIOReturnSuccess);
        CHECK(handle && s.vm.objectCount()==1 && s.vm.spanCount()==s.object.spanCount());
        CHECK(s.object.deviceReferences()==1); /* one mapping reference */
        CHECK(s.vm.findAddress(0,8192,8192,&address)==kIOReturnSuccess && address==0x802000);
        CHECK(s.vm.holdJob(handle)==kIOReturnSuccess && s.object.deviceReferences()==2);
        CHECK(s.vm.unmap(handle)==kIOReturnBusy);
        CHECK(s.vm.retireJobAfterCompletion(handle)==kIOReturnSuccess);
        CHECK(s.vm.unmap(handle)==kIOReturnSuccess);
        CHECK(s.vm.objectCount()==0 && s.vm.spanCount()==0);
        CHECK(s.object.deviceReferences()==0 && s.object.release()==kIOReturnSuccess);
        CHECK(s.tables.release()==kIOReturnBusy);
        CHECK(s.vm.detach()==kIOReturnSuccess);
        CHECK(s.tables.retireAfterQuiescence()==kIOReturnSuccess);
        CHECK(s.tables.release()==kIOReturnSuccess);
        CHECK(fixture.live.empty() && fixture.metadata.empty());
    }
    /* Refusals precede any ownership or table mutation. */
    {
        Fixture s;
        CHECK(exposed(s)==kIOReturnSuccess);
        CHECK(object(s,2)==kIOReturnSuccess);
        unsigned refs=s.object.deviceReferences(),spans=s.vm.spanCount(),reads=f.calls;
        uint64_t handle=0;
        CHECK(s.vm.map(&s.object,0,0x800001,3,0,2,&handle)==kIOReturnBadArgument);
        CHECK(s.vm.map(&s.object,1,0x800000,3,0,2,&handle)==kIOReturnBadArgument);
        CHECK(s.vm.map(&s.object,0,0xec0000,3,0,2,&handle)==kIOReturnBadArgument);
        CHECK(s.vm.map(&s.object,0,0x800000,2,0,2,&handle)==kIOReturnBadArgument);
        CHECK(s.vm.map(&s.object,0,0x800000,3,2,2,&handle)==kIOReturnBadArgument);
        CHECK(s.vm.map(&s.object,0,0x800000,3,0,1,&handle)==kIOReturnBadArgument);
        CHECK(s.vm.map(nullptr,0,0x800000,3,0,2,&handle)==kIOReturnBadArgument);
        CHECK(!handle && refs==s.object.deviceReferences() && spans==s.vm.spanCount() && reads==f.calls);
        CHECK(s.vm.map(&s.object,0,0x800000,3,0,2,&handle)==kIOReturnSuccess);
        uint64_t mapped=handle;
        /* A refused map clears only its own out-parameter. */
        CHECK(s.vm.map(&s.object,0,0x800000,3,0,2,&handle)==kIOReturnBusy && !handle);
        CHECK(s.vm.unmap(mapped+100)==kIOReturnBadArgument);
        CHECK(s.vm.holdJob(mapped+100)==kIOReturnBadArgument);
        CHECK(s.vm.retireJobAfterCompletion(mapped)==kIOReturnBadArgument);
        CHECK(s.vm.unmap(mapped)==kIOReturnSuccess);
        CHECK(s.object.release()==kIOReturnSuccess);
        CHECK(s.vm.detach()==kIOReturnSuccess);
        CHECK(s.tables.retireAfterQuiescence()==kIOReturnSuccess && s.tables.release()==kIOReturnSuccess);
        CHECK(fixture.live.empty() && fixture.metadata.empty());
    }
    /* Post-hold CPU publication failure: poison, retain everything, refuse. */
    {
        Fixture s;
        CHECK(exposed(s)==kIOReturnSuccess);
        CHECK(object(s,2)==kIOReturnSuccess);
        fixture.cache_fail_call=fixture.cache_calls+1; /* next publication fails */
        uint64_t handle=0;
        CHECK(s.vm.map(&s.object,0,0x800000,3,0,2,&handle)==kIOReturnIOError);
        CHECK(!handle && s.vm.poisoned() && s.vm.spanCount()==0); /* new BO reference is retained */
        CHECK(s.object.quarantined() && s.object.deviceReferences()>=1 && s.tables.quarantined());
        CHECK(s.state.prepared && s.state.poisoned);
        CHECK(s.vm.map(&s.object,0,0x800000,3,0,2,&handle)==kIOReturnNotReady);
        CHECK(s.vm.unmap(0)==kIOReturnNotReady && s.vm.detach()==kIOReturnBusy);
        CHECK(s.vm.attach(&io,&context,&streams,nullptr,nullptr,32,1000)==kIOReturnBusy);
        CHECK(s.object.release()==kIOReturnBusy && s.tables.release()==kIOReturnBusy);
        CHECK(!fixture.live.empty());
    }
    std::printf("PASS: 1 native map/hold/unmap transaction, 13 pre-ownership refusals and 1 post-hold poison/retention case over a software SMMU register peer\n");
}
