# AdrenoPM4

Freestanding native C type-4/type-7 PM4 header emission and transactional
structural stream planning for the Adreno command processor. This is a
foundation for native A7xx submission, not a GPU driver or Metal plugin.
It has no Linux runtime dependency, allocation, MMIO or CPU rendering path.

The emission profile supports 18-bit register indices with 1..127 values
and 7-bit opcodes with 0..16383 payload words. Register ranges cannot cross
the supported register-index limit. Invalid fields are refused, never masked.
The decoder requires exactly this profile, including parity and zero reserved
bits. This is a deliberately constrained software profile, not a claim about
all hardware-supported packet variants. It does not recognize legacy packets.

The planner checks every packet, complete payload extent and output capacity
before publishing any record. Payload words are opaque, even if they resemble
headers. On failure, the count is zero and output is untouched. Inputs must
remain immutable, and inputs/outputs/count must not alias. Caller-provided
memory extents must actually be accessible. Empty input succeeds.

**A structurally valid stream is not safe to submit.** This module does not
qualify opcodes, permissions, register effects, addresses, object residency,
indirect-buffer recursion, synchronization, context identity, ordering or
lifetime. A future driver needs explicit operation/resource policies and
real 4-KiB SMMU contexts before it can use planned commands. No fence or GPU
completion is fabricated here.

Protocol references (inputs remain outside this repository):

- Linux `adreno_gpu.h` PKT4/PKT7, revision
  `42f3b40c702abbc37c3a04c95af008cd283aeaf3` of ianchb/sm8550-mainline.
- Mesa `src/freedreno/common/freedreno_pm4.h`, revision
  `85fda3b36405bcaea8bffc64aa4f2651dc90770d`, confirms count extraction,
  parity and type-7 reserved fields. The owning umbrella pins full hashes.

The native C tests exercise the entire 2,097,152 type-7 field domain,
all 262,144 supported register indices, all 127 register counts, known wire
vectors, 12,256 single-bit header corruptions, mixed opaque payloads,
truncation, late invalid headers, capacity and argument refusals. Run with
AddressSanitizer and UndefinedBehaviorSanitizer; cross-build separately for
ordinary ARM64 / iOS 17. These checks establish CPU-side protocol behavior,
not actual A740 execution, firmware acceptance, shader correctness or Metal.

`AdrenoConstants.h/.c` adds a separate exact-A740 constant upload encoder.
It accepts only the explicitly catalogued 64-bit A740 identities
`0x43050a01` and `0xffff43050a01`, six explicit shader-stage identities and
up to 64 disjoint, vec4-aligned destination spans within a shader's actual
constant allocation (at most 512 vec4). Byte sources may be unaligned.
Only declared intervals are copied into inline `CP_LOAD_STATE6_GEOM/FRAG`
packets using `ST6_CONSTANTS`, `SS6_DIRECT` and zero external-source address.
Internal/preamble/primitive gaps are never uploaded implicitly. It does not
accept GPU addresses or encode indirect fetches. Profile, logical source
bounds, destination overlap, capacity, pointer wrap and metadata/source/output
alias checks complete before any output write; failure preserves output.

This uses the pinned Mesa `fd6_const.cc` direct-upload path, `fd6_emit.h`
stage selection and `adreno_pm4.xml` field definitions. Its byte transport
is a real native encoder, not a GPU model. Successful command encoding still
requires a driver to establish shader/context authorization, pipeline constant
sharing, actual native-4KiB GPU SMMU residency, synchronization and resource
lifetimes before real submission. No completion, rendering or Metal device
is fabricated.

`test_constants.c` checks 9,202 positive cases across all six stages and all
512 destination offsets, full remaining extents, 64-packet streams, guarded
source boundaries, unaligned byte sources and 31 transactional refusals.
Define `MESA_REFERENCE` and provide the pinned generated register headers plus
`freedreno_pm4.h` to compare every control/header against Mesa's actual encoder.
The umbrella additionally packs and independently XML-decodes all fourteen
currently accepted original AIR/IR3 layouts and cross-builds native ordinary
ARM64/iOS17 C/C++ objects/archive with stack protection retained. These tests
qualify parameter command transport, not actual A740 execution or a UI.

The complete 64-bit chip identity is preserved. The existing sheng Linux driver
reports `0xffff43050a01`; the fixed Mesa catalog names this exact value as its
no-speedbin FD740 fallback. It selects the same encoding profile explicitly.
A high-bit-corrupted ID whose low32 bits look correct is refused; an arbitrary
kernel value must never silently truncate into an accepted chip identity.

`AdrenoShader.h/.c` now encodes the native A740 shader-register binding subset
for actual float32 vertex/fragment variants with no private memory or appended
constant-data blob. It writes the 13 exact SP/TPL1 registers in 11 type-4
packets (24 words), preserving actual register footprints, hardware branch
stack, merged/early-preamble/thread mode and fragment helper requirements.
Shader base uses a bounded immutable resource view in a declared native-4K
context. A 16K view, bad extent/alignment/identity, unsupported half/private
state, aliases, wraps and insufficient capacity refuse before output changes.
An inactive view is not proof of real SMMU residency or context authorization;
the caller must establish those and shader/pipeline/cache/lifetime ownership
before submission. Only vertex/fragment binding is currently qualified.

Unlike A6xx, the pinned A740 path does not emit a CP_LOAD_STATE6 instruction
preload. TPL1's A8xx-only CLAMP_DISABLE bit is also excluded. SP instruction
size follows the actual compiler's instruction groups, independently from the
padded binary allocation size (which may contain an additional NOP group).
Private-memory bases/size are zero for this explicit no-private-memory profile;
there is no arbitrary GPU pointer or spill fallback.

`test_shader.c` passes 16,644 ASan/UBSan cases across both full chip identities,
all supported full-register/branch fields, vertex/fragment control combinations,
NOP padding and resource extent boundaries. Forty-two transactional refusals
pass. The host oracle uses actual generated Mesa register/header functions.
The umbrella independently XML-decodes all 17 actual original IR3 binding states
and cross-builds ordinary ARM64/iOS17 C/C++ objects/archive with stack protection.
These are register encoding tests, not a complete pipeline, draw, active GPU
mapping, completed submission, Metal device or desktop observation.

## A740 vertex-to-VPC linkage

`AdrenoVPC.h` / `AdrenoVPC.c` match exact generated varying identities to
actual float32 IR3 VS registers and FS component masks/inloc. Declaration
order is not used as an interstage identity. Required missing or mismatched
varyings refuse; no value is synthesized. The profile has one float4 position,
at most 15 active smooth-center fragment varyings, no other builtins and no
half/view/aliased output metadata. Position follows the actual padded fragment
footprint. Inputs are canonicalized by their actual interpolation location.

The encoder emits the source A740 VS/VPC linkage register subset, including
transfer masks, packed register/destination maps, position/stride/count,
clip/cull/SIV disables and every smooth interpolation/replacement word.
It clears stale state within that subset. Complete primitive/raster/input/
resource/cache state, shader ownership and a live native-4K GPU context are
still the driver's responsibility. These words do not submit or complete a
draw and do not qualify GPU interpolation or a complete pipeline.

The reference unit compiles Mesa's actual ir3_link_shaders/ir3_link_add and
register-header functions as an independent oracle. It covers 16597 profiles
and 36 transactional refusals under ASan/UBSan. Integration tests cover 42
actual original VS/constant-FS pairs, an original VS with an authored fragment
whose declarations are reordered, genuine missing-varying refusals and
ordinary ARM64/iOS17 C/C++ plus a complete combined helper link. Vendor code,
original shader inputs, binaries and generated reference headers stay outside
this repository.

## A740 rasterizer state

`AdrenoRaster.h` / `AdrenoRaster.c` encode the pinned A740 rasterizer-state
register set: depth clip controls, culling/front face, line and point sizes,
polygon offset, both primitive-restart/provoking-vertex controls, all three
polygon-mode controls and the default depth clamps for all 16 viewports.
Disabling either depth clip plane requires the separate dynamic viewport
path to supply the correct GRAS/RB clamp bounds. Per-vertex point size also
requires a compatible shader/VPC export; this encoder does not supply it.

Size fields use explicit hardware fixed-point units; polygon offsets retain
exact finite binary32 bits. The encoder performs no floating-point arithmetic
or allocation. Both complete A740 chip identities are checked. Invalid
metadata, nonfinite offsets, field overflow, aliases, wrap and insufficient
capacity refuse before any output changes. The result contains 24 or 60
words; this state does not establish a complete pipeline or submit work.

The umbrella compiles the actual pinned Mesa A740 rasterizer function and
its generated pack functions, replacing only its register-output transport
with a collector. All 114916 cases match every emitted register address and
value under ASan/UBSan, including every flag combination/polygon mode for
both IDs and all supported point/line field values. Forty-one transactional
refusals pass. Ordinary ARM64/iOS17 C/C++ objects and the combined helper
library link successfully. Actual target runtime execution of this raster
suite and GPU execution have not been observed.
