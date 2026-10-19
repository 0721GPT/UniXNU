/* SPDX-License-Identifier: MIT */
#include "NativeAdrenoUserClient.h"
extern "C" volatile uint32_t native_adreno_trace_stage;
#include <IOKit/IOLib.h>
#include <string.h>
NativeAdrenoUserClient::Entry *NativeAdrenoUserClient::find(uint64_t handle)
{
    if(handle)for(size_t i=0;i<entry_count_;++i)if(entries_[i].used && entries_[i].handle==handle)return &entries_[i];
    return nullptr;
}
void NativeAdrenoUserClient::clear(Entry &entry)
{
    entry.handle=0;entry.mapped=0;entry.iova=0;entry.used=false;
    entry.mapped_state=false;entry.unreleasable=false;entry.metadata_bytes=0;
    entry.token=0;entry.mapping_leases=0;
}
NativeAdrenoUserClient::Entry *NativeAdrenoUserClient::free()
{
    for(size_t i=0;i<entry_count_;++i)if(!entries_[i].used)return &entries_[i];
    return nullptr;
}
size_t NativeAdrenoUserClient::liveObjects() const
{
    size_t count=0;for(size_t i=0;i<entry_count_;++i)if(entries_[i].used)++count;return count;
}
bool NativeAdrenoUserClient::mapped(uint64_t handle)
{
    Entry *entry=find(handle);return entry && entry->mapped_state;
}
NativeAdrenoMemory *NativeAdrenoUserClient::memoryOf(uint64_t handle)
{
    Entry *entry=find(handle);return entry && !entry->unreleasable?&entry->memory:nullptr;
}
IOReturn NativeAdrenoUserClient::open(uint32_t abi_version,NativeAdrenoVm *vm,
    NativeAdrenoSubmit *submit,NativeAdrenoRing *ring,uint32_t input_bits,uint32_t output_bits)
{
    if(open_ || vm_ || submit_)return kIOReturnBusy;
    if(abi_version!=NativeAdrenoAbiVersion)return kIOReturnUnsupported;
    if(!vm || !submit || !ring || input_bits<31 || input_bits>48 ||
        output_bits<32 || output_bits>48 || vm->poisoned() || submit->poisoned() ||
        ring->poisoned())return kIOReturnBadArgument;
    entries_=IONewData(Entry,NativeAdrenoClientMaxObjects);
    if(!entries_)return kIOReturnNoMemory;
    memset(entries_,0,sizeof(Entry)*NativeAdrenoClientMaxObjects);
    entry_count_=NativeAdrenoClientMaxObjects;
    vm_=vm;submit_=submit;ring_=ring;input_bits_=input_bits;output_bits_=output_bits;
    next_handle_=1;open_=true;
    queues_[0]={0,true};
    /* Each client is a new GPU context for the CP: its first submission clears the BR/BV synchronization state. */
    ring_->requestContextReset();
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::close()
{
    if(!open_)return kIOReturnNotReady;
    if(liveObjects() || submit_->outstanding())return kIOReturnBusy;
    for(size_t i=1;i<NativeAdrenoAbiMaxQueues;++i)if(queues_[i].active)return kIOReturnBusy;
    IODeleteData(entries_,Entry,entry_count_);entry_count_=0;
    open_=false;for(auto &queue:queues_)queue=Queue{};vm_=nullptr;submit_=nullptr;ring_=nullptr;timer_=nullptr;status_=nullptr;info_=nullptr;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::releaseAll(const NativeAdrenoRingClock *clock,uint32_t timeout_us)
{
    if(!open_)return kIOReturnNotReady;
    if(!clock)return kIOReturnBadArgument;
    IOReturn status=submit_->waitAll(clock,timeout_us);
    if(status)return status;
    for(size_t index=0;index<entry_count_;++index) {
        Entry &entry=entries_[index];
        if(!entry.used)continue;
        entry.mapping_leases=0;
        NativeAdrenoAbiBoFree request{entry.handle};
        status=freeBo(&request);
        if(status)return status;
    }
    for(size_t i=1;i<NativeAdrenoAbiMaxQueues;++i)queues_[i]=Queue{};
    return close();
}
NativeAdrenoMemory *NativeAdrenoUserClient::memoryForToken(uint32_t token)
{
    if(!open_ || !token || token>entry_count_)return nullptr;
    Entry &entry=entries_[token-1];
    return entry.used && !entry.unreleasable && entry.token==token && entry.mapping_leases?&entry.memory:nullptr;
}
IOReturn NativeAdrenoUserClient::query(NativeAdrenoAbiParams *params)
{
    if(!params)return kIOReturnBadArgument;
    *params={};
    if(!open_)return kIOReturnNotReady;
    params->abi_version=NativeAdrenoAbiVersion;
    params->input_bits=input_bits_;params->output_bits=output_bits_;
    params->flags=NativeAdrenoParamsHostImport;params->max_bo_bytes=UINT64_C(1)<<NativeAdrenoAbiMaxBoBytesShift;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::allocBo(const NativeAdrenoAbiBoAlloc *request,
    NativeAdrenoAbiBoAllocReply *reply)
{
    if(!request || !reply)return kIOReturnBadArgument;reply->handle=0;
    if(!open_)return kIOReturnNotReady;
    const uint64_t limit=UINT64_C(1)<<NativeAdrenoAbiMaxBoBytesShift;
    if(!request->bytes || (request->bytes&4095) || request->bytes>limit ||
        request->align_log2<12 || request->align_log2>24 || request->contiguous>1)
        return kIOReturnBadArgument;
    Entry *entry=free();
    if(!entry)return kIOReturnNoResources;
    /* A scattered allocation costs one VM span per physical page (int-030: 16 MiB came back as 4096 single-page spans, the
     * whole span table), so larger objects try one contiguous run first and fall back to scattered pages unless the
     * caller demanded contiguity. */
    IOReturn status=kIOReturnUnsupported;
    const bool prefer_contiguous=request->contiguous==0 && request->bytes>=NativeAdrenoClientContiguousFirstBytes;
    if(prefer_contiguous) {
        status=entry->memory.allocate((size_t)request->bytes,output_bits_,true,(size_t)request->bytes);
        if(status && !entry->memory.quarantined() && entry->memory.release()==kIOReturnSuccess)status=kIOReturnUnsupported;
    }
    if(status)
        status=entry->memory.allocate((size_t)request->bytes,output_bits_,request->contiguous!=0,(size_t)request->bytes);
    if(status) {
        /* A failed allocation can leave quarantine behind; never reuse it. */
        clear(*entry);
        if(entry->memory.release()!=kIOReturnSuccess) {
            entry->used=true;entry->unreleasable=true;
        }
        return status==kIOReturnUnsupported?kIOReturnUnsupported:status;
    }
    entry->used=true;entry->handle=next_handle_++;entry->token=(uint32_t)(entry-entries_)+1;
    entry->mapped=0;entry->iova=0;entry->mapped_state=false;
    reply->handle=entry->handle;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::importBo(const NativeAdrenoAbiBoImport *request,void *task,
    NativeAdrenoAbiBoAllocReply *reply)
{
    if(!request || !reply)return kIOReturnBadArgument;reply->handle=0;
    if(!open_)return kIOReturnNotReady;
    const uint64_t limit=UINT64_C(1)<<NativeAdrenoAbiMaxBoBytesShift;
    if(!task || !request->address || ((request->address|request->bytes)&4095) || !request->bytes || request->bytes>limit)
        return kIOReturnBadArgument;
    Entry *entry=free();
    if(!entry)return kIOReturnNoResources;
    const IOReturn status=entry->memory.importTask(task,request->address,(size_t)request->bytes,output_bits_,(size_t)request->bytes);
    if(status) {
        clear(*entry);
        if(entry->memory.release()!=kIOReturnSuccess) {entry->used=true;entry->unreleasable=true;}
        return status;
    }
    entry->used=true;entry->handle=next_handle_++;entry->token=(uint32_t)(entry-entries_)+1;
    entry->mapped=0;entry->iova=0;entry->mapped_state=false;
    reply->handle=entry->handle;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::mapBo(const NativeAdrenoAbiBoMap *request,
    NativeAdrenoAbiBoMapReply *reply)
{
    if(!request || !reply)return kIOReturnBadArgument;reply->iova=0;
    if(!open_)return kIOReturnNotReady;
    if(!(request->permissions&1) || (request->permissions&~3u) || request->writeback>1 ||
        (request->shareability!=2 && request->shareability!=3) || request->reserved ||
        (request->iova&4095))return kIOReturnBadArgument;
    Entry *entry=find(request->handle);
    if(!entry || entry->unreleasable)return kIOReturnBadArgument;
    if(entry->mapped_state)return kIOReturnBusy;
    uint64_t handle=0;
    const IOReturn status=vm_->map(&entry->memory,0,request->iova,request->permissions,
        request->writeback,request->shareability,&handle);
    if(status)return status;
    entry->mapped=handle;entry->iova=request->iova;entry->mapped_state=true;
    reply->iova=request->iova;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::freeBo(const NativeAdrenoAbiBoFree *request)
{
    native_adreno_trace_stage=1;
    if(!request)return kIOReturnBadArgument;
    if(!open_)return kIOReturnNotReady;
    Entry *entry=find(request->handle);
    if(!entry)return kIOReturnBadArgument;
    if(entry->unreleasable)return kIOReturnBusy;
    if(entry->mapping_leases)return kIOReturnBusy;
    native_adreno_trace_stage=2;
    if(entry->mapped_state) {
        /* Completed jobs must not pin the buffer: retire what the CP has passed before the unmap decides. */
        submit_->retireCompleted(); /* a poisoned execbuf refuses and the unmap below stays refused too */
        native_adreno_trace_stage=3;
        const IOReturn unmap=vm_->unmap(entry->mapped);
        native_adreno_trace_stage=4;
        if(unmap)return unmap;
        entry->mapped_state=false;entry->mapped=0;entry->iova=0;
    }
    const IOReturn release=entry->memory.release();
    if(release)return release;
    clear(*entry);
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::submit(const NativeAdrenoAbiSubmit *request,
    NativeAdrenoAbiSubmitReply *reply,const NativeAdrenoRingClock *clock)
{
    if(!request || !reply)return kIOReturnBadArgument;reply->fence=0;
    if(!open_)return kIOReturnNotReady;
    if(!request->buffer_count || request->buffer_count>NativeAdrenoAbiMaxBuffers ||
        request->reserved)return kIOReturnBadArgument;
    if(request->queue>=NativeAdrenoAbiMaxQueues || !queues_[request->queue].active)
        return kIOReturnBadArgument;
    if(request->wait_fence) {
        if(!clock)return kIOReturnBadArgument;
        const uint32_t wait_us=request->wait_timeout_ns==0?0:
            (request->wait_timeout_ns>UINT64_C(0xffffffff)*1000u?0xffffffffu:
            (uint32_t)(request->wait_timeout_ns/1000));
        const IOReturn waited=submit_->observeFence((uint32_t)request->wait_fence,clock,wait_us);
        if(waited)return waited;
    }
    NativeAdrenoSubmitBuffer buffers[NativeAdrenoAbiMaxBuffers];
    for(uint32_t i=0;i<request->buffer_count;++i) {
        const NativeAdrenoAbiSubmitBuffer &wire=request->buffers[i];
        if(!wire.bytes || (wire.bytes&3))return kIOReturnBadArgument;
        Entry *entry=find(wire.handle);
        if(!entry || !entry->mapped_state)return kIOReturnBadArgument;
        buffers[i]={entry->mapped,wire.iova,(size_t)wire.bytes};
    }
    uint32_t fence=0;
    const IOReturn status=submit_->submit(buffers,request->buffer_count,
        static_cast<uint32_t>(request->queue),&fence);
    if(status)return status;
    reply->fence=fence;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::attachTimer(const NativeAdrenoTimer *timer)
{
    if(!open_)return kIOReturnNotReady;
    if(!timer || timer_ )return kIOReturnBadArgument;
    timer_=timer;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::timestamp(NativeAdrenoAbiTimestamp *reply)
{
    if(!reply)return kIOReturnBadArgument;*reply={};
    if(!open_)return kIOReturnNotReady;
    if(!timer_)return kIOReturnUnsupported;
    return timer_->read(&reply->ticks);
}
IOReturn NativeAdrenoUserClient::attachDeviceInfo(const NativeAdrenoAbiDeviceInfo *info)
{
    if(!open_)return kIOReturnNotReady;
    if(!info || info_ || info->reserved || !info->chip_id || !info->va_size)return kIOReturnBadArgument;
    info_=info;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::deviceInfo(NativeAdrenoAbiDeviceInfo *reply)
{
    if(!reply)return kIOReturnBadArgument;*reply={};
    if(!open_)return kIOReturnNotReady;
    if(!info_)return kIOReturnUnsupported;
    *reply=*info_;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::attachStatus(const NativeAdrenoStatus *status)
{
    if(!open_)return kIOReturnNotReady;
    if(!status || status_)return kIOReturnBadArgument;
    status_=status;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::status(NativeAdrenoAbiStatus *reply)
{
    if(!reply)return kIOReturnBadArgument;*reply={};
    if(!open_)return kIOReturnNotReady;
    if(!status_)return kIOReturnUnsupported;
    const NativeAdrenoStatusCounts counts=status_->counts();
    reply->faults=counts.faults;reply->suspends=counts.suspends;
    return kIOReturnSuccess;
}
size_t NativeAdrenoUserClient::inputSize(uint32_t method)
{
    switch(method) {
    case NativeAdrenoMethodQuery:
    case NativeAdrenoMethodGetTimestamp:
    case NativeAdrenoMethodGetStatus:
    case NativeAdrenoMethodGetDeviceInfo:return 0;
    case NativeAdrenoMethodBoAlloc:return sizeof(NativeAdrenoAbiBoAlloc);
    case NativeAdrenoMethodBoMap:return sizeof(NativeAdrenoAbiBoMap);
    case NativeAdrenoMethodBoImport:return sizeof(NativeAdrenoAbiBoImport);
    case NativeAdrenoMethodBoFree:return sizeof(NativeAdrenoAbiBoFree);
    case NativeAdrenoMethodSubmit:return sizeof(NativeAdrenoAbiSubmit);
    case NativeAdrenoMethodSetMetadata:
    case NativeAdrenoMethodGetMetadata:return sizeof(NativeAdrenoAbiMetadata);
    case NativeAdrenoMethodCreateQueue:
    case NativeAdrenoMethodCloseQueue:return sizeof(NativeAdrenoAbiQueue);
    case NativeAdrenoMethodBoCpuMap:return sizeof(NativeAdrenoAbiBoCpuMap);
    case NativeAdrenoMethodBoCpuUnmap:return sizeof(NativeAdrenoAbiBoCpuUnmap);
    case NativeAdrenoMethodWaitFence:return sizeof(NativeAdrenoAbiWaitFence);
    default:return SIZE_MAX;
    }
}
size_t NativeAdrenoUserClient::outputSize(uint32_t method)
{
    switch(method) {
    case NativeAdrenoMethodQuery:return sizeof(NativeAdrenoAbiParams);
    case NativeAdrenoMethodBoAlloc:return sizeof(NativeAdrenoAbiBoAllocReply);
    case NativeAdrenoMethodBoMap:return sizeof(NativeAdrenoAbiBoMapReply);
    case NativeAdrenoMethodBoFree:return 0;
    case NativeAdrenoMethodSubmit:return sizeof(NativeAdrenoAbiSubmitReply);
    case NativeAdrenoMethodWaitFence:return 0;
    case NativeAdrenoMethodGetTimestamp:return sizeof(NativeAdrenoAbiTimestamp);
    case NativeAdrenoMethodGetStatus:return sizeof(NativeAdrenoAbiStatus);
    case NativeAdrenoMethodGetDeviceInfo:return sizeof(NativeAdrenoAbiDeviceInfo);
    case NativeAdrenoMethodGetMetadata:return sizeof(NativeAdrenoAbiMetadata);
    case NativeAdrenoMethodSetMetadata:return 0;
    case NativeAdrenoMethodCreateQueue:return sizeof(NativeAdrenoAbiQueueReply);
    case NativeAdrenoMethodCloseQueue:return 0;
    case NativeAdrenoMethodBoCpuMap:return sizeof(NativeAdrenoAbiBoCpuMapReply);
    case NativeAdrenoMethodBoCpuUnmap:return 0;
    case NativeAdrenoMethodBoImport:return sizeof(NativeAdrenoAbiBoAllocReply);
    default:return SIZE_MAX;
    }
}
IOReturn NativeAdrenoUserClient::dispatch(uint32_t method,const void *input,
    size_t input_size,void *output,size_t output_size,const NativeAdrenoRingClock *clock)
{
    const size_t expected_in=inputSize(method),expected_out=outputSize(method);
    if(expected_in==SIZE_MAX || expected_out==SIZE_MAX)return kIOReturnUnsupported;
    if(input_size!=expected_in || output_size!=expected_out)return kIOReturnBadArgument;
    if((expected_in && !input) || (expected_out && !output))return kIOReturnBadArgument;
    IOReturn status=kIOReturnSuccess;
    switch(method) {
    case NativeAdrenoMethodQuery: {
        NativeAdrenoAbiParams reply{};
        status=query(&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodBoAlloc: {
        NativeAdrenoAbiBoAlloc request{};
        memcpy(&request,input,sizeof(request));
        NativeAdrenoAbiBoAllocReply reply{};
        status=allocBo(&request,&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodBoImport:
        status=kIOReturnUnsupported;
        break;
    case NativeAdrenoMethodBoMap: {
        NativeAdrenoAbiBoMap request{};
        memcpy(&request,input,sizeof(request));
        NativeAdrenoAbiBoMapReply reply{};
        status=mapBo(&request,&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodBoFree: {
        NativeAdrenoAbiBoFree request{};
        memcpy(&request,input,sizeof(request));
        status=freeBo(&request);
        break;
    }
    case NativeAdrenoMethodSubmit: {
        NativeAdrenoAbiSubmit request{};
        memcpy(&request,input,sizeof(request));
        NativeAdrenoAbiSubmitReply reply{};
        status=submit(&request,&reply,clock);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodWaitFence: {
        NativeAdrenoAbiWaitFence request{};
        memcpy(&request,input,sizeof(request));
        status=waitFence(&request,clock);
        break;
    }
    case NativeAdrenoMethodGetDeviceInfo: {
        NativeAdrenoAbiDeviceInfo reply{};
        status=deviceInfo(&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodGetTimestamp: {
        NativeAdrenoAbiTimestamp reply{};
        status=timestamp(&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodBoCpuMap: {
        NativeAdrenoAbiBoCpuMap request{};
        memcpy(&request,input,sizeof(request));
        NativeAdrenoAbiBoCpuMapReply reply{};
        status=cpuMap(&request,&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodBoCpuUnmap: {
        NativeAdrenoAbiBoCpuUnmap request{};
        memcpy(&request,input,sizeof(request));
        status=cpuUnmap(&request);
        break;
    }
    case NativeAdrenoMethodCreateQueue: {
        NativeAdrenoAbiQueueReply reply{};
        status=createQueue(&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodCloseQueue: {
        NativeAdrenoAbiQueue request{};
        memcpy(&request,input,sizeof(request));
        status=closeQueue(&request);
        break;
    }
    case NativeAdrenoMethodSetMetadata: {
        NativeAdrenoAbiMetadata request{};
        memcpy(&request,input,sizeof(request));
        status=setMetadata(&request);
        break;
    }
    case NativeAdrenoMethodGetMetadata: {
        NativeAdrenoAbiMetadata request{};
        memcpy(&request,input,sizeof(request));
        NativeAdrenoAbiMetadata reply{};
        status=getMetadata(&request,&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    case NativeAdrenoMethodGetStatus: {
        NativeAdrenoAbiStatus reply{};
        status=this->status(&reply);
        if(!status)memcpy(output,&reply,sizeof(reply));
        break;
    }
    default:return kIOReturnUnsupported;
    }
    return status;
}
IOReturn NativeAdrenoUserClient::cpuMap(const NativeAdrenoAbiBoCpuMap *request,
    NativeAdrenoAbiBoCpuMapReply *reply)
{
    if(!request || !reply)return kIOReturnBadArgument;*reply={};
    if(!open_)return kIOReturnNotReady;
    Entry *entry=find(request->handle);
    if(!entry || entry->unreleasable)return kIOReturnBadArgument;
    /* An imported BO is the caller's own memory, already mapped where it came from. */
    if(entry->memory.imported())return kIOReturnUnsupported;
    if(entry->mapping_leases==UINT32_MAX)return kIOReturnNoResources;
    ++entry->mapping_leases;
    reply->token=entry->token;reply->bytes=entry->memory.bytes();
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::cpuUnmap(const NativeAdrenoAbiBoCpuUnmap *request)
{
    if(!request)return kIOReturnBadArgument;
    if(!open_)return kIOReturnNotReady;
    if(request->reserved)return kIOReturnBadArgument;
    Entry *entry=find(request->handle);
    if(!entry || entry->unreleasable || !entry->token)return kIOReturnBadArgument;
    if(request->token!=entry->token)return kIOReturnBadArgument;
    if(!entry->mapping_leases)return kIOReturnBusy;
    --entry->mapping_leases;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::createQueue(NativeAdrenoAbiQueueReply *reply)
{
    if(!reply)return kIOReturnBadArgument;
    if(!open_)return kIOReturnNotReady;
    for(size_t i=1;i<NativeAdrenoAbiMaxQueues;++i)if(!queues_[i].active) {
        queues_[i]={static_cast<uint32_t>(i),true};
        reply->queue=i;
        return kIOReturnSuccess;
    }
    return kIOReturnNoResources;
}
IOReturn NativeAdrenoUserClient::closeQueue(const NativeAdrenoAbiQueue *request)
{
    if(!request)return kIOReturnBadArgument;
    if(!open_)return kIOReturnNotReady;
    if(!request->queue || request->queue>=NativeAdrenoAbiMaxQueues)return kIOReturnBadArgument;
    Queue &queue=queues_[request->queue];
    if(!queue.active)return kIOReturnBadArgument;
    /* A queue must be drained by the caller before it disappears. */
    if(submit_->queueOutstanding(static_cast<uint32_t>(request->queue)))
        return kIOReturnBusy;
    queue={};
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::setMetadata(const NativeAdrenoAbiMetadata *request)
{
    if(!request)return kIOReturnBadArgument;
    if(!open_)return kIOReturnNotReady;
    if(request->reserved || request->bytes>NativeAdrenoAbiMetadataBytes)return kIOReturnBadArgument;
    Entry *entry=find(request->handle);
    if(!entry || entry->unreleasable)return kIOReturnBadArgument;
    if(request->bytes)memcpy(entry->metadata,request->text,request->bytes);
    entry->metadata_bytes=request->bytes;
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::getMetadata(const NativeAdrenoAbiMetadata *request,
    NativeAdrenoAbiMetadata *reply)
{
    if(!request || !reply)return kIOReturnBadArgument;
    /* Snapshot the request so an in/out alias is still well defined. */
    const uint64_t handle=request->handle;
    const uint32_t asked=request->bytes;
    const uint32_t reserved=request->reserved;
    *reply={};
    if(!open_)return kIOReturnNotReady;
    if(reserved || asked>NativeAdrenoAbiMetadataBytes)return kIOReturnBadArgument;
    Entry *entry=find(handle);
    if(!entry || entry->unreleasable)return kIOReturnBadArgument;
    if(asked<entry->metadata_bytes)return kIOReturnBadArgument;
    reply->handle=entry->handle;reply->bytes=entry->metadata_bytes;
    if(entry->metadata_bytes)memcpy(reply->text,entry->metadata,entry->metadata_bytes);
    return kIOReturnSuccess;
}
IOReturn NativeAdrenoUserClient::waitFence(const NativeAdrenoAbiWaitFence *request,
    const NativeAdrenoRingClock *clock)
{
    if(!request || !clock)return kIOReturnBadArgument;
    if(!open_)return kIOReturnNotReady;
    if(!request->fence)return kIOReturnBadArgument;
    const uint32_t timeout_us=request->timeout_ns==0?0:
        (request->timeout_ns>UINT64_C(0xffffffff)*1000u?0xffffffffu:
        (uint32_t)(request->timeout_ns/1000));
    return submit_->wait((uint32_t)request->fence,clock,timeout_us);
}
