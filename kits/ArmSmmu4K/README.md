# ArmSmmu4K

Freestanding C builder for inactive ARM LPAE stage-1 page tables. It supports
low TTBR0 and canonical upper TTBR1 widths 32..48, output widths
32/36/40/42/44/48 and only native
4-KiB granules with level-3 4-KiB leaves. It selects the correct initial level
and keeps every unmapped entry invalid. The upper builder uses the architectural
partial-root index width, including the last page ending at 2^64. It refuses
truncated low addresses; it does not convert another page size or remap an
address region. Results retain their explicit lower/upper region identity.

Callers provide a physically contiguous owned table pool, immutable sorted
IOVA ranges, physical resource extents, explicit read/write/device-execute/privilege permissions,
normal NC or WB/WA attributes, explicit inner/outer shareability and a
leaf-work budget. The builder
counts all needed tables and checks all input bounds/capacity before writing
anything. Failure clears the result and leaves table storage untouched.
Success writes only the used pool pages. Inputs, outputs and results must
not alias. This requires real accessible storage and matching ownership;
software cannot establish that a caller-supplied physical address is owned.

Mapped resources cannot overlap the entire table pool. Physical aliases with
conflicting cache, shareability or privilege policies refuse. Write-only, MMIO,
wrong lower/upper address regions, unsupported widths, unsorted/overlapping IOVAs and non-4K
extents refuse. All leaves are non-global and accessed. Permission values 1/3
retain the existing unprivileged, PXN/UXN behavior. Explicit Execute and
Privileged flags enable the device-stage1 permissions needed by native GMU
firmware mappings. They do not change CPU permissions or authenticate code;
the caller must bind actual firmware/resources to trusted platform policy.
NC selects MAIR index 0=0x44; WB/WA selects index 1=0xff. Shareability is
mandatory per range (2=outer, 3=inner); neither cache mode implies a domain. The result reports MAIR=0xff44 and the initial level.
The caller must validate CPU attributes and perform real cache maintenance.

**Inactive table construction is not active hardware mapping.** In the builder,
no allocation,
register writes, barriers, TLB invalidation, stream/context assignment, ASID
management, translation activation, fault handling or GPU execution occurs.
The separate context encoder generates a register image only.
Before deployment, a driver must prove actual SMMU
capabilities/address widths, stage/format, both granules, table residency,
walk attributes, permissions, cache ordering, valid context ownership and
real fault/refusal behavior. Live map/unmap for bounded single-region arenas is
implemented separately below; context switching is not implemented. Optional register
programming is a separate API described below, with mandatory driver preconditions.
The two software table-pool tests are not hardware context isolation proof.

Protocol reference: Linux io-pgtable-arm.c and arm-smmu headers, exact revision
42f3b40c702abbc37c3a04c95af008cd283aeaf3 of ianchb/sm8550-mainline.
Complete source hashes are pinned in `scripts/data/adreno-inputs.json`; no
Linux code or vendor binaries are included in this kit.

Host tests use an independent table graph and walker to check every generated
leaf, permissions/attributes, absent gaps, no block descriptors, exact table
use and untouched unused capacity. They cover 2-MiB/1-GiB/512-GiB boundaries,
all 17 input widths, all six output widths, highest-address pages, two
separate table pools for the same IOVA, consistent/conflicting cache/shareability aliases and
transactional refusal paths. Run with AddressSanitizer/UndefinedBehaviorSanitizer.
The C implementation also cross-builds for ordinary ARM64 / iOS 17. Standard
stack-protector imports are preserved; no Linux runtime or heap is required.

## SMMUv2 stage1 context encoding and readback comparison

ArmSmmu4KContext accepts trusted lower/upper builder configs/results and raw
ID0/ID2 values. It checks stage1/native4K support, recognized VA/PA widths,
requested bounds, builder region identity and nonoverlapping physical table
pools. It encodes AArch64 CBAR/CBA2R, TCR/TCR2, both TTBRs with the same explicit
16-bit ASID, MAIR and SCTLR. Stage2 is bypassed. TG0/TG1 both select 4K even if
one region is disabled; A1 remains zero. The caller explicitly chooses coherent
inner WBWA or noncoherent outer NC walks and fault policies. Unknown width
encodings or unsupported/malformed inputs refuse transactionally.

ValidateContextReadback freshly derives the expected image and compares every
supplied register field, including stage/format, translation enables, granules,
ASID, root addresses and attributes. This does not read hardware. A caller
must supply an actual consistent hardware snapshot, physically walk its actual
TTBR tables and prove ownership/coherency/stream assignment and TLB lifetime.
Matching a software fixture or inactive table never qualifies active mappings.
Register programming must establish power/context ownership and preserve the
required TCR-before-TTBR ordering; the encoder performs none of those operations.

The umbrella checker compiles the actual two inline TCR helpers from the pinned
original arm-smmu source into a separate ignored oracle. It checks 7344 lower,
upper and split images across 17 input widths, six output widths, two walk
policies, four fault policies and three ASIDs. All 2820096 individual stored-bit
changes refuse readback comparison. Independent upper-table walks cover partial
roots, 2MiB/1GiB/512GiB transitions and the last canonical page, with unchanged
storage on refusal. Existing lower-table regression remains intact. Both C
objects cross-build as ARM64 subtype0/iOS17 with general registers only, and a
real C++ consumer imports the unmangled C interfaces. No hardware ID values,
active context, MMIO, GPU execution or Metal are qualified by these checks.

## Owned SMMUv2 context programming and disable

ArmSmmu4KProgram performs actual register I/O through explicit callbacks.
ArmSmmu4KMmio supplies aligned volatile 32/64-bit accesses and dsb sy barriers
for an already mapped, powered, little-endian AArch64 Device window. It does
not discover a device, map physical addresses or recover external bus aborts.
There are no board addresses or synthesized hardware capability values.

The caller must exclusively own the bank, ASID and streams, keep power/clocks
on, quiesce/fault streams, establish global/platform/security configuration,
install a fault handler and clean/publish physically owned native4K tables.
This primitive does not establish those conditions. Actual ID0/ID1/ID2 reads
determine capabilities and register spacing. 64K register spacing is supported
independently of the mandatory 4K translation granule. An enabled bank refuses
before writing. TCR2/TCR precede TTBR accesses; ASID invalidation and bounded
TLB synchronization plus disabled readback precede enabling SCTLR last.
Enabled readback and fault status must then match. No other bank or global
configuration is written. Correct register readback alone is not proof of
active stream translation, physical table ownership or actual GPU execution.

Disable first compares the actual context's stage, format, ASID, roots and
attributes with the expected owned context. Errors attempt disable and sync;
the original failure remains visible. The caller must retain tables, bank
and leases while active or uncertain. Only confirmed SCTLR=0 and completed
TLB sync clear must_retain_tables; a timeout never authorizes recycling.
No resources are released by this API. Unknown readback state is not success.

The umbrella check-smmu4k-program.py compiles exact pinned source register
definitions as a separate oracle, runs the real programmer against explicitly
software I/O fixtures, injects errors before/after possible write effects,
and checks ordering, corruption, busy banks, bounded timeout and cleanup
failure. The direct adapter is tested on private host memory only. All four
C objects cross-build for ARM64 subtype0/iOS17 with general registers only,
no unexpected runtime imports and a real C++ caller. These tests perform no
hardware MMIO and do not qualify active Adreno mappings or Metal.


## Owned exact stream matching and live context observation

ArmSmmu4KStreams binds explicitly leased SMR/S2CR slots to a real enabled
native4K context, using actual ID0/global/context register reads. Both ordinary
and EXIDS matching formats are supported. The global SMMU must already enable
fault reporting and fault unmatched streams; indexing-only or bypass policy
refuses. Only exact SIDs are attached. Every other valid mapping is inspected
for matching-mask collisions and foreign SIDs targeting the owned bank.
No unrelated bank, stream slot or global register is modified.

All masters remain quiesced under an exclusive global configuration lease.
Preflight requires owned S2CRs in FAULT. The writer checks S2CR before SMR,
keeps EXIDVALID clear until the exact SID is read back, then validates the
entire live context and stream set again. Any attempted mutation failure
quarantines only the touched leased slots and permanently poisons the state.
Quarantine verifies FAULT before restoring/publishing the exact SID, so a
failed SMR write cannot activate a stale SID. A retained lease is mandatory.

CheckFaultedStreams enables a board orchestrator to preflight streams before
programming a context. ObserveOwnedContext and ObserveStreams perform only
real register reads/barriers; matching flags supplied by a caller do not pass.
FaultStreams requires the exact owned routes and puts them in FAULT, retaining
valid SID matching. It does not drain bus transactions/TLBs, disable the bank,
free tables or authorize retry. The board must fault streams before context
disable, since disabling stage1 alone can permit bypass through that context.

The umbrella source oracle compiles original pinned SMR/S2CR writers. Software
register tests cover both layouts, 324 per-call I/O failure cases, 28 pre-write
refusals and unrelated-route isolation. Board integration additionally tests
actual context-program/attach/fault/disable ordering with GPU bank 0 and a
separate GMU bank. Permission tests compare 544 lower/upper profiles against
the original LPAE descriptor algorithm, including privileged executable device
memory. No actual hardware mapping, physical table ownership/coherency or GPU
execution is proven by these tests. The complete platform power/clock/interrupt/SCM lifecycle remains an
integration requirement; physical traversal is implemented below.


## Actual physical descriptor graph auditing

ArmSmmu4KAudit walks the actual declared physical pools through descriptor
reads. It does not rebuild reference tables. Every reachable page and all
512 slots are inspected, including partial-root unused slots. Only 4K level-3
leaves with exact declared physical resources, AP/XN/AF/nG/cache/shareability
are accepted. Blocks, reserved bits, cycles, shared table pages, foreign table
addresses, missing/extra mappings and empty intermediate tables refuse.
All expected leaves must be consumed exactly once. A visited workspace,
explicit read budget and at most four stack levels bound traversal.

Both regions are validated before physical reads, including full pool
capacities rather than only used pages, access to either table pool, and
conflicting physical aliases across TTBR0/TTBR1. Owned tables may be physically
reordered within their reserved pools; equality with builder allocation order
is deliberately not required. Upper IOVAs retain native sign extension,
including the final page ending at UINT64_MAX. No page-size conversion occurs.

AuditDeclaredContext is an inactive preflight. ObserveAuditedContext obtains
actual IDs and complete enabled context state, traverses its observed TTBR
roots, then rechecks context/capabilities/faults. Its trusted caller must hold
exclusive leases and keep masters quiesced and tables immutable/coherent.
Two observations cannot detect an unowned writer changing/restoring state.
Readback and declared ownership bounds are not physical provenance proofs.

ArmSmmu4KTableMemory supplies actual aligned volatile 64-bit reads through
one/two existing CPU mappings, checks CPU/physical range overlap and bounds,
and has no shadow/fallback table. The platform must establish and retain the
real VA-to-PA relationship, allocator ownership and cache visibility. This
adapter does not allocate, map, clean caches or claim hardware observation.

## Incremental mappings in an owned native4K arena

ArmSmmu4KLive prepares a bounded inactive VA aperture with real preallocated
intermediate tables and invalid 4KiB leaf slots. The fixed topology remains
resident throughout the context lifetime. An empty arena is a valid context:
the context encoder now accepts zero mapped leaves without disabling its
translation region. The result retains its actual mapped-leaf count.
LiveAudit checks every actual slot, including reserved empty intermediate
tables. It is a separate contract from AuditTables, whose original compact
nonempty-tree contract is unchanged. Board startup must use the appropriate
auditor; existing ShengGpuSmmuStart does not accept these reserved arenas yet.

LiveUpdate validates all declared resource extents and the complete actual
descriptor graph, observes the exact enabled context before/after preflight,
then directly stores aligned volatile 64-bit leaves into the owned CPU mapping.
Only invalid-to-valid maps and matching valid-to-invalid unmaps are accepted.
Replacement requires a completed unmap before mapping the new physical pages.
No shadow tables, copied translations, blocks or 16K conversion are involved.
Physical aliases require consistent cache/shareability/privilege policy;
resources cannot overlap the entire table pool, including spare pages.

Every changed leaf-table page passes an explicit platform cache-publication
callback. SynchronizeOwnedContext then reads actual context/capability/fault
state, issues an ordered ASID TLBI and bounded context TLBSYNC, and observes
the context again. LiveUpdate checks the resulting complete physical graph
and final context before committing metadata/generation. It never changes
SCTLR, TTBRs, stream routes or another bank while updating mappings.
Timeout, I/O error, publication failure or unexpected graph/context state
poisons the arena. There is no automatic rollback/replay or unsafe release.

The caller must retain a real exclusive bank/ASID/stream lease, power/clocks,
fault IRQ handling, actual allocator-established CPU-to-PA mappings and every
old/new memory object. All masters remain quiesced under queue serialization
until the operation completes. Data-cache ordering is separate from table
publication. Successful unmap removes translation; it does not signal a GPU
fence, drain DMA, free an allocation or justify releasing context tables.
The current API supports one enabled lower OR upper region per context and
a contiguous physical extent per update. Discontiguous BOs require multiple
owned range operations with higher-level transaction/lifetime management.

Pinned original Linux arm_smmu_tlb_inv_context_s1 establishes the ordering
and register protocol; its exact revision remains the one above. Host tests
compare live descriptors with the actual original LPAE permission function.
3264 lower/upper/VA-width/PA-width/permission/cache profiles, partial unmaps,
highest canonical addresses and IOVA reuse pass. 313 I/O/publication/timeout/
state-change controls and 5120 graph mutations refuse with retained resources
and no replay. Production code cross-builds for ordinary ARM64/iOS17 and passes
static analysis. These are software fixtures, not real cache visibility, live
hardware TLB operation, physical allocation provenance or GPU execution proof.

The umbrella audit test covers 1632 lower/upper/IAS/OAS/permission profiles,
9704 descriptor mutations and 8192 per-read failures, with sanitizers and
real volatile loads on host buffers labeled with synthetic physical addresses.
Board lifecycle requires physical audit before writes and after attaching
streams, and brackets active audit with complete stream observations. Its
native ARM64/iOS17 archive includes the auditor and memory adapter, passes
C++/C boundary and static analysis checks, and never authorizes hardware
readiness from software fixtures.

Physical-alias lookup uses the caller's scratch for sorted integer indices.
Each pass compares one attribute class with its complement; every compared
pair therefore has different cache/shareability/privilege attributes. After
sorting by physical start, contained intervals are discarded so both retained
starts and ends are monotonic. Binary search checks overlap without a backward
scan whose extent depends on the longest interval. One distant long range
therefore cannot make thousands of gap queries quadratic. Original ranges,
descriptors, actual native4K tables and all context/TLB/publication checks are
unchanged. Missing/insufficient scratch retains pairwise validation.

The alias test compares 15000 self/cross profiles across up to eight attribute
classes with an independent pairwise predicate, with full, absent and insufficient scratch.
A mixed 16387-interval control includes one distant long interval and checks
gaps and a true long-range conflict. Compiler basic-block coverage in the host
test bounds work; the original longest-interval scan fails that bound. These
test callbacks are absent from production builds. Host counts/timings are not
hardware SMMU timing, GPU execution or display-deadline qualification.
