# NativeAdreno

Native ARM64 XNU Adreno backend for sheng. The repository provides owned
backing memory, physical spans, explicit PoC cache maintenance and a
real-allocation adapter for ArmSmmu4K live table arenas. It also contains the
GMU and GPU services and the user client; the Metal plugin lives in the
NativeMetal repository. Hardware: the GPU executes commands, copies a
pixel-identical frame into the scanout and drives the render server through the
Metal plugin. Shader compilation is slow and no complete precompiled UI pack
exists. The memory backend remains the foundation described below. No Linux
ioctl emulation, host transport, synthetic fence, software rendering, shadow
table or 16K translation conversion is used.

`NativeAdrenoMemory` allocates a prepared IOBufferMemoryDescriptor with no
system mapper, reads raw physical segments with kIOMemoryMapperNone, checks
all native4K alignment/length/address-width constraints and coalesces adjacent
physical pages into resource spans. The optional contiguous contract is
checked against every page. Its current per-object driver budget is at most
256MiB; this is an implementation limit, not an advertised Vulkan capability.
Memory is explicitly zeroed and published before returning it to the driver.

The allocator observes actual CPU TCR/TTBR/SCTLR and the kernel page-size ABI
before allocation. This does not independently qualify early boot mappings,
the userspace page-size ABI or GPU translation. The cache backend pins the
current CPU, reads CTR_EL0, uses DC CVAC/CIVAC/IVAC over whole owned pages and
DSB SY completion. It does not assume Apple's coherent-I/O API establishes
noncoherent Adreno DMA visibility. CPU/device exclusion and physical cache
attributes must still be established by the platform.

Every mapping and queued use must hold a device-lifetime reference before
publication. Retirement requires genuine DMA quiescence and completed native
TLB removal; a reference counter is not evidence of either condition. Release
refuses live references or quarantine. A failed descriptor complete retains
the descriptor and metadata rather than releasing possibly wired pages.
There is deliberately no implicit destructor release or automatic reset.
The owning driver must retain the object until explicit release succeeds,
including failed allocate calls that leave quarantined state. Calls require
one driver-owned resource/queue gate; these objects are not a userspace ABI.

`NativeAdrenoPrepareSmmuMemory` uses the actual contiguous allocation's raw PA
and CPU mapping as the live native4K table pool. It prepares/audits an inactive
empty arena, publishes used table pages through the same allocation and holds
its lifetime before context exposure. The live updater's cache callback is
bound to that real allocation and checks every requested page. The caller
still owns bank/ASID/stream leases, MMIO, power/clocks/security, IRQs and the
physical startup/stop lifecycle. Existing compact-tree board startup requires
separate integration for reserved live arenas.

The umbrella checker `scripts/check-native-adreno-memory.py` runs the actual
memory and table-binding implementation with authored host IOKit doubles,
then cross-compiles the production sources against the private XNU source
kernel headers. It checks 18 allocation profiles, 16 lifetime/allocation/cache
failures, 20 argument/CPU refusals, 34 table bindings and four binding refusals.
It hashes all native compiler dependencies, audits ordinary ARM64 subtype0 /
iOS17 objects, checks actual CPU/cache instructions and resolves imports
against the existing source-kernel checkpoint. Static analysis is clean.

Those checks do not run a kernel allocator or privileged CPU/cache operations,
establish physical provenance/coherence, activate a real GPU SMMU, execute
Adreno commands, signal actual GPU fences or validate Metal/UI/presentation.
Buffers, submissions/IRQ fences, Turnip's native kernel interface, IOSurface
sharing, original iPadOS discovery/AMFI and native display remain work ahead.

References read, not transplanted: Mesa Turnip `tu_knl`/MSM at
`c823a83e8f373a5c36c2b9ed1d7dc98a4f3360ee`; nullmoth's native XNU allocation
and GPUVA code at `637282334617d27bccf8aa2448497c0f19465dc8`. The latter's
PolyForm code and macOS/AMFI-disabled deployment are not incorporated here.
ArmSmmu4K dependency: `8c687592c2c63d9f0010df575b5ba4d2b1f1a239`.
The umbrella records original source hashes and exact local kernel inputs;
original/vendor inputs and all build products remain outside this repository.

The umbrella's `check-turnip-darwin-host.py` also runs `tests/hostsim/deferred_bo.cpp`
through Vulkan, the actual Turnip Darwin KMD and this repository's native client
implementation. The software CP can hold completion while a separate atomic
release flag is changed; model registers and completion memory are published
only during a locked service operation. Method faults occur before mutation.
Eight bounded cases cover deferred BO retirement, retained CPU leases/IOVAs,
temporary busy, fence/free/lease/view failures, failed map cleanup and the
256-entry pressure boundary. The old KMD fails the unrelated-buffer control
by waiting for the held work. These are transport/lifetime checks with no shader
execution, active Adreno SMMU proof, hardware timing or display qualification.

`tests/hostsim/pipeline_cache.cpp` runs actual Turnip/IR3 compilation without
submitting a command. The umbrella cache checker uses separate processes:
empty cache plus FAIL_ON_PIPELINE_COMPILE_REQUIRED refuses, saved cache import
succeeds with that flag and application-hit feedback, and changed UUID/shader
controls refuse again. Only an authored GLSL fixture is compiled. Cache bytes
and binary identities stay in ignored build output. This qualifies host cache
import, not iOS cache portability, shader execution or cold system-UI deadlines.

`tests/hostsim/pipeline_cache_ios.cpp` adds an explicitly offline iOS instrument.
Its parent identifies the own RAM Data volume by UUID and provider ancestry,
mounts a read-only union extension view, then starts four independent workers.
The worker links the software peer as an ordinary dependency; it does not rely
on a DYLD_INSERT_LIBRARIES exception. Original Sandbox and AMFI enforce the
own signed helper/library permissions. The unchanged iOS Turnip compiler then
passes the same strict-hit and UUID/shader-miss sequence. A bounded saved-state
capture contains the complete authored-fixture cache and worker ledger. This
qualifies compilation/import with that exact iOS driver and cache UUID in
QEMU. It does not execute shaders, prove active native-4K GPU translation,
provide a complete initial UI cache pack, or measure hardware performance.

VM mapping preflight also uses the caller's existing alias scratch to index
the smaller of the new physical spans and the conflicting-class live spans.
`NativeAdrenoVmAlias.h` sorts temporary integer indices and discards contained
intervals, so starts and ends are monotonic and overlap queries use binary
search. Resource spans and actual native4K page tables are unchanged. Absent
or insufficient scratch retains the pairwise fallback; the GPU context already
supplies sufficient scratch for its VM capacity. Attribute-class refusals still
precede MMIO and device references. The full table/context/route/cache/TLB
checks still execute after preflight.

An independent interval oracle covers 2048 profiles / 524288 queries plus
65536 reversed spans and gap boundaries. Full VM controls cover both smaller
sides, scratch/fallback, same-class aliases, conflicting cache/share/privilege
classes and 8192-span map/unmap transactions. Map timing slot 5 now includes
the previously omitted preflight and the map maximum includes that work.
Host timing comparisons remain diagnostics, not Adreno timing or a UI FPS bound.
