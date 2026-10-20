# Adreno 740 driver

sheng drives the Adreno 740 through native XNU services. The stack uses native 4 KiB SMMU translation, GMU firmware, a CP ring, real fences and a GPU copy into the scanout.

## Runtime path

```mermaid
flowchart LR
  A[iPadOS app] --> B[QuartzCore and RenderBox]
  B --> C[NativeMetal plugin]
  C --> D[metal2vulkan]
  D --> E[Turnip]
  E --> F[NIR and IR3]
  F --> G[Darwin KMD]
  G --> H[NativeAdreno]
  H --> I[GMU, CP, SMMU and ring]
  I --> J[Adreno 740]
  J --> K[GPU blit to scanout]
  K --> L[NativeMobileDisplay]
```

1. NativeMetal publishes a Metal device through the system `MTLAddDevice` path. It owns the Metal objects, encoders, IOSurfaces, events, fences and pipeline archives.
2. metal2vulkan translates AIR to SPIR-V and supplies pipeline descriptors, binding plans and function constants.
3. Turnip compiles SPIR-V through NIR to A740 IR3. Its Darwin KMD talks to the NativeAdreno user client.
4. NativeAdreno owns BOs, GPU addresses, the submission ring, fences and SMMU contexts. The GMU brings the GPU out of reset and applies power votes. The CP executes the IBs.
5. The plugin blits the finished frame into scanout pages. NativeMobileDisplay offers and retires the swap.

The runtime compiler chain is Metal, AIR, SPIR-V, NIR, IR3. AirNir is an offline direct AIR to NIR to IR3 compiler, not the deployment path. Apple's `applegpu_g9g` slice is Apple GPU code and cannot run on Adreno.

## Components

| Component | Role |
| --- | --- |
| `drivers/NativeMetal` | Metal frontend, metal2vulkan, command encoding, IOSurface and scanout client |
| `drivers/NativeAdreno` | GPU service, GMU/CP startup, user client, BO, VM, ring, submit and fence |
| `drivers/NativeMobileDisplay` | Swap offer, validation, retirement and scanout hand-off |
| `kits/AdrenoFirmware` | GMU HFI, startup, power votes and SQE firmware data |
| `kits/AdrenoPM4` | A740 PM4 state, constants, VPC and rasterizer packets |
| `kits/ArmSmmu4K` | Native 4 KiB tables, context, streams, live mappings and TLB maintenance |
| `kits/AirNir` | Offline AIR to NIR to IR3 compiler |

## Recent changes, newest first

| Date | Commit | Change |
| --- | --- | --- |
| 2026-10-20 | NativeMetal `7cd766b` | Folds constant-global field loads. Uber fragment SPIR-V drops from 915584 to 11212 bytes; 60 recipes drop from 36.0 s to 6.5 s. 58 of 60 outputs stay byte-identical. |
| 2026-10-20 | NativeMetal `9a0a771` | Streams the pipeline-cache export into the 256 MiB board-log region. |
| 2026-10-20 | NativeMetal `9b1e514` | Exports render-server pipeline caches and imports `m2vpack` before the device opens. The export is incremental. |
| 2026-10-20 | NativeMetal `364701a` | Qualifies the iOS priming cache and the first consumer from a fresh image. |
| 2026-10-20 | NativeAdreno `0018649` | Moves fence waits outside the connection lock and forwards cache-export pieces through method `0x7100`. |
| 2026-10-20 | NativeAdreno `12439ae` | Qualifies same-iOS-driver pipeline caching with an offline peer. |
| 2026-10-20 | NativeAdreno `9a45113` | Indexes cross-class physical aliases before GPU mappings. |
| 2026-10-20 | ArmSmmu4K `dea5eb6` | Bounds mixed-size physical alias lookups. |
| 2026-10-20 | UniXNU `c537282` | Applies the translator fold, retries the coherence probe and records int-150 to int-152. |
| 2026-10-20 | UniXNU `8472a96` | Board log v3, pipeline-cache export through UFS and merged packs in the Data seed. |
| 2026-10-20 | UniXNU `0d5357d` | Traces internal Turnip compilation. Device startup compiles 18 clear/blit and 2 empty shaders before application pipelines. |

## State

| Step | Result |
| --- | --- |
| GMU cold start and HFI | works |
| GPU context and CP commands | works |
| Native 4 KiB SMMU context | active |
| Metal device discovery | works |
| GPU frame copy into scanout | works, pixel-identical |
| GPU-composited desktop | works, slow |
| Stable 120 FPS | not achieved |

Active GPU 4 KiB proof, display vblank, the IOSurface format contract and the first-use shader path remain open.

## Shader compilation

The driver has no precompiled Adreno UI shader pack. It compiles the original UI shaders on the device:

1. `default.metallib` carries AIR plus Apple GPU machine code. The Apple code cannot run on Adreno.
2. metal2vulkan translates AIR to SPIR-V, then Turnip compiles IR3 at pipeline creation.
3. A cold boot compiles every UI variant it reaches. int-148: 77 pipelines, about 43 s, cache hit 0. int-150: 103 Metal pipelines, 40.1 s total, 1.40 s max; 94 Vulkan pipelines, 23.3 s.
4. Apple's precompiled A10X variants cannot be reused. No vendor precompiled Adreno pack exists.

Implemented mitigations:

| Mitigation | Effect |
| --- | --- |
| NDEBUG release build | Removes per-pass Mesa assertions and NIR validation. |
| metal2vulkan patch `0004` | Specializes the QuartzCore uber shaders before Vulkan compilation. |
| Priming cache | Caches runtime startup entry-point work. |
| `m2vpack` export and import | Reuses earlier compiled pipeline caches; the export is incremental. |
| Turnip builtin pack | Embeds clear, blit and empty shader variants; it does not cover UI variants. |

The remaining cost is the first compile of a UI variant that no earlier boot exported. The cache key must include the translator identity before packs are reused. int-152 used patch `0004` without a pipeline pack, so every compile was cold; it was a large improvement, not a fixed result.

## Build

```sh
python3 scripts/build-turnip-darwin.py --target ios
python3 scripts/check-turnip-pipeline-cache-host.py
python3 scripts/check-priming-cache-host.py --output build/priming-cache/run
python3 scripts/probe-turnip-internal-compiler-host.py --qualification <old> --output <new>
```

The device repository adds the original-input checks:

```sh
python3 scripts/build-adreno-compiler-host.py --probe
python3 scripts/build-turnip-ios-compiler-peer.py
python3 scripts/check-turnip-ios-compiler-cache.py
python3 scripts/check-quartzcore-turnip-cache-host.py
```

## Related

- [Xiaomi Pad 6S Pro (sheng)](/devices/sheng)
- [XNU drivers](/drivers/xnu-drivers)
- [Four-kilobyte pages](/xnu/four-k-page-tables)
