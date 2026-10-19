/* SPDX-License-Identifier: MIT */
#include "NativeAdrenoVm.h"
extern "C" volatile uint32_t native_adreno_trace_stage;
extern "C" {int (*native_adreno_dram_read32)(uint64_t,uint32_t *);}
#include <string.h>
#include <IOKit/IOLib.h>
static bool overlap(uint64_t a,uint64_t ab,uint64_t b,uint64_t bb)
{return a<b+bb && b<a+ab;}
static bool validBinding(const NativeAdrenoVmRegion &r,unsigned upper)
{
    return r.tables && r.arena && r.state && !r.tables->quarantined() &&
        r.tables->deviceReferences() && r.tables->spanCount()==1 &&
        r.tables->spans()[0].bytes==r.tables->bytes() && !r.tables->spans()[0].offset &&
        r.arena->mapping==r.tables->mapping() && r.arena->upper_region==upper &&
        r.arena->config.table_physical==r.tables->spans()[0].physical &&
        r.arena->capacity_pages==r.tables->bytes()/4096 && r.state->prepared &&
        !r.state->poisoned && !r.state->tables.leaf_pages;
}
ArmSmmu4KLiveRegion NativeAdrenoVm::region(unsigned upper) const
{
    return {regions_[upper].arena,regions_[upper].state,
        ranges_+(upper?lower_count_:0),upper?upper_count_:lower_count_};
}
ArmSmmu4KStatus NativeAdrenoVm::observe() const
{
    ArmSmmu4KStreamResult routes;
    ArmSmmu4KStatus status=ArmSmmu4KObserveStreams(&io_,streams_,context_,&routes);if(status)return status;
    ArmSmmu4KLiveRegion lower=region(0),upper=region(1);ArmSmmu4KLiveResult audit;
    status=ArmSmmu4KLiveAuditContext(context_,lower.arena?&lower:nullptr,upper.arena?&upper:nullptr,&audit);
    if(status)return status;
    return ArmSmmu4KObserveStreams(&io_,streams_,context_,&routes);
}
IOReturn NativeAdrenoVm::fail(ArmSmmu4KStatus status)
{
    poisoned_=true;result_.operation=status;result_.complete=false;
    for(size_t i=0;mappings_ && i<NativeAdrenoVmMaxObjects;++i)if(mappings_[i].memory)mappings_[i].memory->quarantine();
    for(auto &r:regions_)if(r.arena) {r.tables->quarantine();r.state->poisoned=1;}
    ArmSmmu4KStreamResult routes;
    result_.quarantine=ArmSmmu4KFaultStreams(&io_,streams_,&routes);
    return kIOReturnIOError;
}
IOReturn NativeAdrenoVm::attach(const ArmSmmu4KIo *io,ArmSmmu4KContextConfig *context,
    ArmSmmu4KStreamState *streams,const NativeAdrenoVmRegion *lower,const NativeAdrenoVmRegion *upper,
    size_t capacity,unsigned budget)
{
    if(attached_ || ranges_ || poisoned_)return kIOReturnBusy;
    if(!io || !context || !streams || (!lower && !upper) || !capacity || capacity>65536 ||
        !budget || budget>1048576 || (lower && !validBinding(*lower,0)) || (upper && !validBinding(*upper,1)))
        return kIOReturnBadArgument;
    ranges_=IONewData(ArmSmmu4KRange,capacity);if(!ranges_)return kIOReturnNoMemory;
    mappings_=IONewData(Mapping,NativeAdrenoVmMaxObjects);
    if(!mappings_) {IODeleteData(ranges_,ArmSmmu4KRange,capacity);return kIOReturnNoMemory;}
    memset(mappings_,0,sizeof(Mapping)*NativeAdrenoVmMaxObjects);
    capacity_=capacity;io_=*io;context_=context;streams_=streams;budget_=budget;
    if(lower)regions_[0]=*lower;if(upper)regions_[1]=*upper;
    for(unsigned i=0;i<2;++i)if(regions_[i].arena) {
        if(NativeAdrenoBindSmmuVisibility(regions_[i].tables,&visibility_[i])!=kIOReturnSuccess) {
            IODeleteData(ranges_,ArmSmmu4KRange,capacity_);IODeleteData(mappings_,Mapping,NativeAdrenoVmMaxObjects);
            regions_[0]={};regions_[1]={};return kIOReturnBadArgument;
        }
    }
    attached_=true;
    ArmSmmu4KStatus status=observe();if(status)return fail(status);
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::detach()
{
    if(poisoned_ || objects_ || spanCount())return kIOReturnBusy;
    if(!attached_)return kIOReturnNotReady;
    IODeleteData(ranges_,ArmSmmu4KRange,capacity_);IODeleteData(mappings_,Mapping,NativeAdrenoVmMaxObjects);
    attached_=false;regions_[0]={};regions_[1]={};context_=nullptr;streams_=nullptr;capacity_=0;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::findAddress(unsigned upper,size_t bytes,uint64_t alignment,uint64_t *out) const
{
    if(!out)return kIOReturnBadArgument;*out=0;
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    if(upper>1 || !regions_[upper].arena || !bytes || (bytes&4095) || alignment<4096 ||
        (alignment&(alignment-1)))return kIOReturnBadArgument;
    const ArmSmmu4KLiveArena *a=regions_[upper].arena;
    uint64_t offset=0;
    for(size_t i=upper?lower_count_:0;;++i) {
        uint64_t rem=(a->iova+offset)&(alignment-1),padding=rem?alignment-rem:0;
        if(padding>a->bytes-offset)return kIOReturnNoResources;offset+=padding;
        if(bytes>a->bytes-offset)return kIOReturnNoResources;
        if(i==(upper?spanCount():lower_count_) ||
            (ranges_[i].iova-a->iova>=offset && bytes<=ranges_[i].iova-a->iova-offset))
            {*out=a->iova+offset;return kIOReturnSuccess;}
        uint64_t end=ranges_[i].iova-a->iova+ranges_[i].bytes;
        if(end>offset)offset=end;
    }
}
IOReturn NativeAdrenoVm::preflight(const NativeAdrenoMemory *memory,unsigned upper,uint64_t iova,
    unsigned permissions,unsigned writeback,unsigned shareability) const
{
    if(!memory || upper>1 || !regions_[upper].arena || !memory->backed() || memory->quarantined() ||
        !memory->bytes() || !memory->spanCount() || !(permissions&ArmSmmu4KRead) || (permissions&~15u) ||
        writeback>1 || (shareability!=2 && shareability!=3))return kIOReturnBadArgument;
    const ArmSmmu4KLiveArena *a=regions_[upper].arena;
    if((iova&4095) || iova<a->iova || iova-a->iova>=a->bytes || memory->bytes()>a->bytes-(iova-a->iova))
        return kIOReturnBadArgument;
    if(objects_>=NativeAdrenoVmMaxObjects || memory->spanCount()>capacity_-spanCount())return kIOReturnNoResources;
    for(size_t i=0;i<spanCount();++i) {
        /* Native addresses may end at 2^64; compare offsets in the region. */
        if((i<lower_count_)==!upper && iova<=ranges_[i].iova+(ranges_[i].bytes-1) &&
            ranges_[i].iova<=iova+(memory->bytes()-1))return kIOReturnBusy;
    }
    if(a->config.output_bits<32 || a->config.output_bits>48)return kIOReturnBadArgument;
    uint64_t offset=0,limit=UINT64_C(1)<<a->config.output_bits;
    for(size_t j=0;j<memory->spanCount();++j) {
        const auto &span=memory->spans()[j];
        if(span.offset!=offset || !span.physical || !span.bytes || ((span.physical|span.bytes)&4095) ||
            span.bytes>memory->bytes()-offset || span.physical>=limit || span.bytes>limit-span.physical)
            return kIOReturnBadArgument;
        for(const auto &r:regions_)if(r.arena && overlap(span.physical,span.bytes,
            r.arena->config.table_physical,r.arena->capacity_pages*UINT64_C(4096)))return kIOReturnBadArgument;
        for(size_t i=0;i<spanCount();++i)if(overlap(span.physical,span.bytes,ranges_[i].physical,ranges_[i].bytes) &&
            (writeback!=ranges_[i].writeback || shareability!=ranges_[i].shareability ||
                ((permissions^ranges_[i].permissions)&ArmSmmu4KPrivileged)))return kIOReturnBadArgument;
        offset+=span.bytes;
    }
    return offset==memory->bytes()?kIOReturnSuccess:kIOReturnBadArgument;
}
NativeAdrenoVm::Mapping *NativeAdrenoVm::find(uint64_t handle)
{if(handle && mappings_)for(size_t i=0;i<NativeAdrenoVmMaxObjects;++i)if(mappings_[i].handle==handle && mappings_[i].memory)return &mappings_[i];return nullptr;}
void NativeAdrenoVm::insert(const ArmSmmu4KRange &range,unsigned upper)
{
    size_t at=0,count=spanCount();while(at<count && ranges_[at].iova<range.iova)++at;
    for(size_t i=count;i>at;--i)ranges_[i]=ranges_[i-1];ranges_[at]=range;
    if(upper)++upper_count_;else ++lower_count_;
}
void NativeAdrenoVm::erase(size_t at,unsigned upper)
{
    size_t count=spanCount();for(size_t i=at+1;i<count;++i)ranges_[i-1]=ranges_[i];
    if(upper)--upper_count_;else --lower_count_;
}
IOReturn NativeAdrenoVm::map(NativeAdrenoMemory *memory,unsigned upper,uint64_t iova,
    unsigned permissions,unsigned writeback,unsigned shareability,uint64_t *handle)
{
    if(!handle)return kIOReturnBadArgument;*handle=0;result_={};
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    IOReturn checked=preflight(memory,upper,iova,permissions,writeback,shareability);if(checked)return checked;
    if(next_handle_==UINT64_MAX)return kIOReturnNoResources;
    ArmSmmu4KStatus status=observe();if(status)return fail(status);
    checked=memory->holdForDevice();if(checked)return checked;
    Mapping *record=nullptr;for(size_t i=0;i<NativeAdrenoVmMaxObjects;++i)if(!mappings_[i].memory){record=&mappings_[i];break;}
    if(!record) {memory->quarantine();return fail(ArmSmmu4KStateMismatch);}
    *record={memory,0,iova,memory->spanCount(),0,permissions,writeback,shareability,0};++objects_;
    checked=memory->publishForDevice(0,memory->bytes(),permissions&ArmSmmu4KWrite);
    if(checked)return fail(ArmSmmu4KIoError);
    for(size_t i=0;i<record->spans;++i) {
        const auto &span=memory->spans()[i];
        ArmSmmu4KRange range={iova+span.offset,span.physical,span.bytes,permissions,writeback,shareability};
        ArmSmmu4KLiveRegion lower=region(0),high=region(1);ArmSmmu4KLiveResult update;
        status=ArmSmmu4KLiveUpdateContext(&io_,streams_->bank,context_,lower.arena?&lower:nullptr,
            high.arena?&high:nullptr,upper,&visibility_[upper],&range,1,budget_,&update);
        if(status)return fail(status);
        insert(range,upper);++record->mapped_spans;++result_.completed_spans;
    }
    status=observe();if(status)return fail(status);
    record->handle=next_handle_++;*handle=record->handle;result_.complete=true;return kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::unmap(uint64_t handle)
{
    native_adreno_trace_stage=10;
    result_={};if(!attached_ || poisoned_)return kIOReturnNotReady;
    Mapping *record=find(handle);if(!record)return kIOReturnBadArgument;
    if(record->jobs)return kIOReturnBusy;
    ArmSmmu4KStatus status=observe();
    native_adreno_trace_stage=11;
    if(status)return fail(status);
    unsigned upper=regions_[1].arena && record->iova>=regions_[1].arena->iova;
    if(record->permissions&ArmSmmu4KWrite) {
        if(record->memory->acquireDeviceWrites(0,record->memory->bytes())!=kIOReturnSuccess)
            return fail(ArmSmmu4KIoError);
    }
    for(size_t i=0;i<record->spans;++i) {
        const auto &span=record->memory->spans()[i];
        ArmSmmu4KRange change={record->iova+span.offset,span.physical,span.bytes,record->permissions,record->writeback,record->shareability};
        size_t at=0;while(at<spanCount() && ranges_[at].iova!=change.iova)++at;
        if(at==spanCount())return fail(ArmSmmu4KStateMismatch);
        ArmSmmu4KLiveRegion lower=region(0),high=region(1);ArmSmmu4KLiveResult update;
        native_adreno_trace_stage=12;
        status=ArmSmmu4KLiveUpdateContext(&io_,streams_->bank,context_,lower.arena?&lower:nullptr,
            high.arena?&high:nullptr,upper,&visibility_[upper],&change,0,budget_,&update);
        native_adreno_trace_stage=13;
        if(status)return fail(status);
        erase(at,upper);--record->mapped_spans;++result_.completed_spans;
    }
    status=observe();
    native_adreno_trace_stage=14;
    if(status)return fail(status);
    if(record->memory->retireAfterQuiescence()!=kIOReturnSuccess)return fail(ArmSmmu4KStateMismatch);
    *record={};--objects_;result_.complete=true;return kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::describe(uint64_t handle,uint64_t *iova,size_t *bytes,
    unsigned *permissions) const
{
    if(!iova || !bytes || !permissions)return kIOReturnBadArgument;
    *iova=0;*bytes=0;*permissions=0;
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    for(size_t m=0;mappings_ && m<NativeAdrenoVmMaxObjects;++m)if(mappings_[m].handle==handle && mappings_[m].memory) {
        const Mapping &mapping=mappings_[m];
        *iova=mapping.iova;*bytes=mapping.memory->bytes();*permissions=mapping.permissions;
        return mapping.mapped_spans==mapping.spans?kIOReturnSuccess:kIOReturnNotReady;
    }
    return kIOReturnBadArgument;
}
IOReturn NativeAdrenoVm::invalidateTlb()
{
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    ArmSmmu4KProgramResult r;
    const ArmSmmu4KStatus status=ArmSmmu4KInvalidateAllOwnedContext(&io_,streams_->bank,context_,budget_,&r);
    return status?fail(status):kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::publishMapped()
{
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    for(size_t i=0;i<NativeAdrenoVmMaxObjects;++i) {
        const Mapping &m=mappings_[i];
        if(!m.memory || m.memory->imported())continue;
        if(m.memory->publishForDevice(0,m.memory->bytes(),true))return fail(ArmSmmu4KIoError);
    }
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::sampleWord(uint64_t iova,uint32_t *cached,uint32_t *dram) const
{
    if(!cached || !dram)return kIOReturnBadArgument;
    *cached=*dram=~0u;
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    if(iova&3)return kIOReturnBadArgument;
    for(size_t i=0;i<NativeAdrenoVmMaxObjects;++i) {
        const Mapping &m=mappings_[i];
        if(!m.memory || iova<m.iova || iova-m.iova>=m.memory->bytes())continue;
        const size_t offset=(size_t)(iova-m.iova);
        if(m.memory->mapping())*cached=static_cast<const volatile uint32_t *>(m.memory->mapping())[offset/4];
        for(size_t j=0;j<m.memory->spanCount();++j) {
            const auto &span=m.memory->spans()[j];
            if(offset<span.offset || offset-span.offset>=span.bytes)continue;
            uint32_t value=0;
            if(native_adreno_dram_read32 && !native_adreno_dram_read32(span.physical+(offset-span.offset),&value))*dram=value;
            break;
        }
        return kIOReturnSuccess;
    }
    return kIOReturnBadArgument;
}
IOReturn NativeAdrenoVm::holdJob(uint64_t handle)
{
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    Mapping *record=find(handle);if(!record)return kIOReturnBadArgument;
    if(record->jobs==UINT32_MAX)return kIOReturnNoResources;
    IOReturn status=record->memory->holdForDevice();if(status)return status;
    ++record->jobs;return kIOReturnSuccess;
}
IOReturn NativeAdrenoVm::retireJobAfterCompletion(uint64_t handle)
{
    if(!attached_ || poisoned_)return kIOReturnNotReady;
    Mapping *record=find(handle);if(!record || !record->jobs)return kIOReturnBadArgument;
    IOReturn status=record->memory->retireAfterQuiescence();if(status)return status;
    --record->jobs;return kIOReturnSuccess;
}
