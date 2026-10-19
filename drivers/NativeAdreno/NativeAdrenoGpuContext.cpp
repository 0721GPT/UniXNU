/* SPDX-License-Identifier: MIT */
#include "NativeAdrenoGpuContext.h"
#include "ArmSmmu4KMmio.h"
#include <IOKit/IOLib.h>

static const uint64_t kIovas[GpuBufferCount]={GpuSqeIova,GpuRingIova,GpuMemptrsIova,GpuPwrupIova,GpuScratchIova};
static const size_t kBytes[GpuBufferCount]={GpuSqeBytes,GpuRingBytes,GpuPageBytes,GpuPageBytes,GpuPageBytes};
IOReturn NativeAdrenoGpuContext::bringUp(const ArmSmmu4KIo *io,unsigned bank,unsigned asid,const unsigned slots[2],
    const uint16_t sids[2],void (*progress)(uint32_t),NativeAdrenoGmuContextReport *report)
{
    if(!report)return kIOReturnBadArgument;
    *report={};
    if(used_)return kIOReturnBusy;
    if(!io || !slots || !sids || bank>63 || !asid || asid>0xffff || slots[0]>127 || slots[1]>127 ||
        slots[0]==slots[1])return kIOReturnBadArgument;
    used_=true;
    IOReturn status=kIOReturnSuccess;
    auto step=[&](Step s) {report->step=s;if(progress)progress(s);};
    step(Allocate);
    static const uint8_t oas_bits[16]={32,36,40,42,44,48};
    if(io->read32(io->cookie,0x28,&report->id2))status=kIOReturnIOError;
    else {
        const unsigned code=(report->id2>>4)&15;
        report->output_bits=code<6?oas_bits[code]:0;
        if(report->output_bits>40)report->output_bits=40;
        if(!report->output_bits)status=kIOReturnUnsupported;
    }
    if(!status)status=tables_.allocate(TablePages*0x1000,report->output_bits,true,TablePages*0x1000);
    for(unsigned i=0;!status && i<GpuBufferCount;++i)
        status=buffers_[i].allocate(kBytes[i],report->output_bits,true,kBytes[i]);
    if(!status) {
        step(Prepare);
        status=smmu_.prepare(&tables_,0,0,GpuApertureBytes,NativeAdrenoSmmuContext::MaxLeaves,
            bank,asid,1000,report->output_bits);
    }
    if(!status) {
        step(Program);
        status=smmu_.program(io);
        const ArmSmmu4KProgramResult *p=smmu_.programResult();
        report->program_phase=p->phase;report->program_writes=p->writes;
        report->program_readback_valid=p->readback_valid;report->program_enabled=p->enabled;
        report->id1=p->id1;report->program_status=smmu_.programStatus();
        report->image=p->readback;
    }
    if(!status) {
        step(Attach);
        const ArmSmmu4KStream routes[2]={{slots[0],sids[0]},{slots[1],sids[1]}};
        status=smmu_.attach(routes,2);
        const ArmSmmu4KStreamResult *s=smmu_.streamResult();
        report->route_status=smmu_.attachStatus();
        report->route_phase=s->phase;report->route_writes=s->writes;report->routes_verified=s->streams_verified;
    }
    if(!status) {
        step(VmAttach);
        NativeAdrenoVmRegion lower={&tables_,smmu_.arenaHandle(),smmu_.stateHandle()};
        status=vm_.attach(io,smmu_.configHandle(),smmu_.streamsHandle(),&lower,nullptr,16384,262144);
    }
    if(!status) {
        step(Map);
        /* adreno_fw_create_bo maps the SQE MSM_BO_GPU_READONLY without NOEXEC; every other kernel BO read/write. */
        for(unsigned i=0;!status && i<GpuBufferCount;++i)
            status=vm_.map(&buffers_[i],0,kIovas[i],i?3u:5u,0,2,&handles_[i]);
        report->spans=uint32_t(vm_.spanCount());
    }
    if(!status) {
        step(Ready);
        ready_=true;
    }
    report->status=status;
    return status;
}
static NativeAdrenoGpuContext *board_gpu_context;
extern "C" IOReturn NativeAdrenoGpuContextBringup(volatile uint8_t *smmu,size_t bytes,unsigned bank,unsigned asid,
    unsigned slot0,unsigned sid0,unsigned slot1,unsigned sid1,void (*progress)(uint32_t),
    NativeAdrenoGmuContextReport *report)
{
    static ArmSmmu4KMmio mmio;
    static ArmSmmu4KIo io;
    if(!report || board_gpu_context || !smmu || !bytes || sid0>0xffff || sid1>0xffff)return kIOReturnBadArgument;
    mmio={reinterpret_cast<volatile unsigned char *>(smmu),bytes};
    if(ArmSmmu4KBindMmio(&mmio,&io)!=ArmSmmu4KSuccess)return kIOReturnBadArgument;
    board_gpu_context=new NativeAdrenoGpuContext;
    if(!board_gpu_context)return kIOReturnNoMemory;
    const unsigned slots[2]={slot0,slot1};
    const uint16_t sids[2]={uint16_t(sid0),uint16_t(sid1)};
    return board_gpu_context->bringUp(&io,bank,asid,slots,sids,progress,report);
}
extern "C" IOReturn NativeAdrenoGpuContextBuffers(NativeAdrenoGpuBuffer out[5])
{
    if(!out)return kIOReturnBadArgument;
    for(unsigned i=0;i<GpuBufferCount;++i)out[i]={};
    if(!board_gpu_context || !board_gpu_context->ready())return kIOReturnNotReady;
    for(unsigned i=0;i<GpuBufferCount;++i) {
        const NativeAdrenoMemory &m=board_gpu_context->buffer(i);
        if(m.spanCount()!=1 || m.spans()[0].bytes!=m.bytes())return kIOReturnUnsupported;
        out[i]={m.spans()[0].physical,m.bytes(),kIovas[i]};
    }
    return kIOReturnSuccess;
}
extern "C" NativeAdrenoVm *NativeAdrenoGpuContextVm()
{
    return board_gpu_context?board_gpu_context->vm():nullptr;
}
