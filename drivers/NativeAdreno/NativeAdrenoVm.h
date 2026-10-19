/* SPDX-License-Identifier: MIT */
#ifndef NATIVE_ADRENO_VM_H
#define NATIVE_ADRENO_VM_H
#include "NativeAdrenoSmmuMemory.h"
#include "ArmSmmu4KStreams.h"
struct NativeAdrenoVmRegion {
    NativeAdrenoMemory *tables;
    ArmSmmu4KLiveArena *arena;
    ArmSmmu4KLiveState *state;
};
struct NativeAdrenoVmResult {
    ArmSmmu4KStatus operation,quarantine;
    size_t completed_spans;
    bool complete;
};
/* Kernel-owned BO mapping transactions, not a userspace ABI or GPU scheduler.
 * Attach only under the actual bank/ASID/stream and powered-MMIO lease, with
 * fault IRQ handling installed and empty native4K arenas already active.
 * The real table allocations, context/streams and BO objects remain retained
 * by the owning driver. No implicit destructor cleanup; detach must succeed.
 * Serialize all methods, table updates and submission under the DRIVER gate.
 * ALL masters must stay genuinely quiesced through each map/unmap transaction.
 * Job counts are lifetime accounting; neither they nor these APIs are fences.
 * Each active region is audited, including the unmodified peer. Partial failure
 * keeps every old/new BO reference, poisons both arenas and attempts to FAULT
 * only the owned streams. Uncertain resources require independent real reset.
 * No VM may be advertised to apps until per-client isolation, submission
 * validation/protection, genuine IRQ fences and reset are implemented. */
enum : size_t {NativeAdrenoVmMaxObjects=1024};
/* Board hook for sampleWord(): reads one aligned word of physical memory through a fresh uncached mapping; 0 on success. */
extern "C" int (*native_adreno_dram_read32)(uint64_t physical,uint32_t *value);
class NativeAdrenoVm final {
    struct Mapping {
        NativeAdrenoMemory *memory=nullptr;
        uint64_t handle=0,iova=0;
        size_t spans=0,mapped_spans=0;
        unsigned permissions=0,writeback=0,shareability=0,jobs=0;
    };
    Mapping *mappings_=nullptr;
    NativeAdrenoVmRegion regions_[2]{};
    ArmSmmu4KLiveVisibility visibility_[2]{};
    ArmSmmu4KIo io_{};
    ArmSmmu4KContextConfig *context_=nullptr;
    ArmSmmu4KStreamState *streams_=nullptr;
    ArmSmmu4KRange *ranges_=nullptr;
    size_t capacity_=0,lower_count_=0,upper_count_=0,objects_=0;
    unsigned budget_=0;
    uint64_t next_handle_=1;
    bool attached_=false,poisoned_=false;
    NativeAdrenoVmResult result_{};
    ArmSmmu4KLiveRegion region(unsigned) const;
    ArmSmmu4KStatus observe() const;
    IOReturn fail(ArmSmmu4KStatus);
    Mapping *find(uint64_t);
    void insert(const ArmSmmu4KRange &,unsigned);
    void erase(size_t,unsigned);
    IOReturn preflight(const NativeAdrenoMemory *,unsigned,uint64_t,unsigned,unsigned,unsigned) const;
public:
    NativeAdrenoVm()=default;
    ~NativeAdrenoVm()=default;
    NativeAdrenoVm(const NativeAdrenoVm &)=delete;
    NativeAdrenoVm &operator=(const NativeAdrenoVm &)=delete;
    IOReturn attach(const ArmSmmu4KIo *,ArmSmmu4KContextConfig *,ArmSmmu4KStreamState *,
        const NativeAdrenoVmRegion *lower,const NativeAdrenoVmRegion *upper,
        size_t max_spans,unsigned poll_budget);
    /* Frees CPU metadata only, with no BOs remaining. Never releases roots,
     * disables a bank or returns the platform lease; board lifecycle owns it. */
    IOReturn detach();
    IOReturn findAddress(unsigned upper,size_t bytes,uint64_t alignment,uint64_t *iova) const;
    IOReturn map(NativeAdrenoMemory *,unsigned upper,uint64_t iova,
        unsigned permissions,unsigned writeback,unsigned shareability,uint64_t *handle);
    IOReturn unmap(uint64_t handle);
    IOReturn holdJob(uint64_t handle);
    /* TLBIALL for the context bank plus TLBSYNC: drops every cached translation of the bank whatever its ASID. Unmap already
     * synchronizes by ASID; this is the stronger form the submit path uses before each submission (int-070: on sheng the CP read
     * zeros from an IB whose table entry and DRAM were both correct, after the address had been mapped to other pages before). */
    IOReturn invalidateTlb();
    /* Cleans and invalidates the CPU caches over every mapped kernel-owned BO (whole objects; imported BOs are their owner's
     * business), so no CPU cache line can hold data the GPU should see or hide what it wrote. Lines are physically tagged, so
     * this also reaches lines a user mapping of the same pages left behind. int-070 runs it before every submission. */
    IOReturn publishMapped();
    /* Diagnostic, never a proof of coherence: the 32-bit word at a mapped GPU address as the kernel's cacheable alias reads it
     * and, when the board installed native_adreno_dram_read32, as DRAM reads it through an uncached mapping (else ~0). */
    IOReturn sampleWord(uint64_t iova,uint32_t *cached,uint32_t *dram) const;
    /* Read-only description of a live mapping; never pins or extends it. */
    IOReturn describe(uint64_t handle,uint64_t *iova,size_t *bytes,unsigned *permissions) const;
    /* Only a scheduler's actual successful GPU completion may call this.
     * A timeout, interrupt bit alone or CPU-written timestamp is insufficient. */
    IOReturn retireJobAfterCompletion(uint64_t handle);
    bool poisoned() const {return poisoned_;}
    size_t objectCount() const {return objects_;}
    size_t spanCount() const {return lower_count_+upper_count_;}
    NativeAdrenoVmResult lastResult() const {return result_;}
};
#endif
