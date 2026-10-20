# AdrenoFirmware

Freestanding C planning of the little-endian GMU block stream used by the
Linux MSM Adreno driver. The caller supplies authoritative GMU address ranges;
the planner checks complete records, alignment, address overflow, whole-block
containment, disjoint regions and output capacity before publishing a plan.
It performs no allocation, MMIO, GPU execution or CPU rendering.

Zero-size metadata retains its opaque type/value. It does not authorize any
allocation or hardware operation. Inputs and regions must remain immutable
during a call, and output storage must not overlap inputs. A loader must bind
the complete firmware hash/signature to its approved device before executing
the plan. A structurally valid prefix is not authenticated firmware.

Format reference: Linux `a6xx_gmu.c`, `struct block_header` and
`a6xx_gmu_fw_load`. The host test consumes a separately hash-qualified
original GMU file; no firmware or other build product is stored here. Host
tests and ordinary ARM64/iOS 17 cross-compilation do not qualify GPU
initialization, shaders, Metal or UI.

## Modern GMU HFI transport

AdrenoHfi is the nonlegacy firmware communication transport, separate from
the firmware block planner. It initializes the actual three-queue shared
wire layout in a caller-owned 16KiB extent (four native4K pages), serializes
commands with the firmware sequence/header format and four-word padding,
publishes indices with AArch64 outer-shareable barriers and calls a real
platform notification callback. A full padded packet plus sentinel must fit
before publication. It never truncates a 32-bit GMU IOVA or trusts a changed
ring size/index. It supports one outstanding command, not concurrent queues.

Poll accepts only a matching returned command header and zero-error ACK.
Malformed, stale and firmware-error responses quarantine the session without
publishing a consumed index or changing output payload. Notification failure
after publication and bounded polling timeout also quarantine it: the command
may already have executed, so automatic retry/reset or storage release would
be unsafe. Success means matching wire exchange only. Poll budgets count
iterations, not elapsed microseconds. The debug queue is not drained yet.

The caller must prove coherent CPU/GMU memory, exclusive ownership, actual
native4K mappings, live power/clocks, firmware state, interrupt notification
and shutdown. Initialization is allowed only with the GMU stopped and unable
to access the shared allocation. No function stops hardware or authorizes
freeing/reusing the allocation. There are no device addresses or Linux runtime
dependencies. This transport does not implement GMU power, firmware loading,
performance/bandwidth-table setup, startup policy or shader submission.

The umbrella check-adreno-hfi.py compares every initialized word and normal
wrapped command/padding bytes with separately compiled original pinned HFI
structs/initialization/write functions. That ignored single-thread software
oracle replaces Linux locks/atomics and notification with test stand-ins;
it is not a device backend. Tests cover matching replies, wrapped indices,
sequence rollover, full-packet capacity, malformed/stale/error/timeout refusal
and indeterminate notification. Native ARM64 subtype0/iOS17 cross-build and
actual C++/C boundaries pass, with general registers only and standard stack
protection retained. All replies in these tests come from a software peer.
No actual GMU ACK, hardware mapping/coherency or GPU execution is qualified.

## Modern startup and A7xx power-table construction

`AdrenoHfiStartup` validates and serializes the complete modern startup
sequence: performance table, bandwidth table, optional ACD enable/table,
optional IFPC enable, core firmware start, and start. Every stage uses the
actual transport and must receive its matching zero-error ACK. Per-command
and total monotonic deadlines, a separate polling limit, and platform health
callbacks bound failures, including stopped/backward clocks. An attempted
sequence is never replayed after failure; resource retention remains required.
The library does not issue a device reset or establish power/mapping ownership.

`AdrenoHfiVotes` constructs ARC votes from explicit OPP levels and original
little-endian command-db rail data, and A7xx BCM bandwidth votes from explicit
auxiliary data/catalog policy. It handles secondary-rail saturation, commit
boundaries, minimum/maximum proportional votes, fixed-performance thresholds,
and the complete modern bandwidth wire layout. It never invents resource
addresses, decides GMxC availability, or falls back after a discovery failure.
The supported rail profile is ordered active levels after off=0, optionally
followed by zero padding. Unsupported layouts and incomplete inputs refuse
without publishing a partial output. These are arithmetic operations, not
RPMh power transitions or firmware authenticity checks.

The umbrella `check-adreno-hfi-startup.py` compares four startup profiles and
66 stage-specific failures with separately compiled pinned original functions.
`check-adreno-hfi-votes.py` compares 1000 ARC and 1000 BCM cases, including full
wire layouts, against original source. Tests use software ACKs/rails/BCMs;
none qualify actual hardware. Both implementations cross-compile for ordinary
ARM64 subtype0/iOS17 with general registers only. Board command-db acquisition,
OPP/catalog binding, coherent native4K storage, real register notifications,
power/clock/SCM/AOSS prerequisites and deployment integration belong to the
platform. All original-source oracles, firmware and build products stay
outside this repository.
