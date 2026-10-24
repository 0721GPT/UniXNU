# GPU and Metal adaptation

Complete [platform DMA](/porting/platform) and basic [display presentation](/porting/display)
first. Read the [Adreno740 runtime path](/drivers/adreno740) and
[AdrenoMetal interface](/drivers/adreno-metal).

## 1. Match the GPU generation and OS ABI

Adreno740 implements the A740 kernel interface. AdrenoMetal uses that interface,
Turnip, metal2vulkan and the selected OS GPU/IOSurface ABI. Identify the target
GPU revision and compare register, firmware, packet and memory interfaces.
For another GPU generation, implement the matching kernel backend and compiler
support before selecting its frontend.

For another A740 board, collect GPU/GMU registers, interrupts, SMMU context and
stream IDs, firmware, power domains, clock/voltage levels, interconnect votes,
RAM reservations and scanout layout. Check them against the selected firmware.

## 2. Adapt power and firmware inputs

Read the [SM8550 platform sources](https://github.com/UniXNU/UniXNU-sm8550-common/tree/main/kits)
and the [sheng package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package).
`Sm8550Platform` supplies shared GMU operations and command-database handling.
`ShengGmuVotes.c` and `ShengGmuVotesSource.h` supply the board's OPP and bandwidth
policy. Port those inputs to the target's rails and command database.

The sheng recipe `scripts/build-adreno740-kits.py` embeds these local inputs:

| Firmware | Purpose |
| --- | --- |
| `qcom/gmu_gen70200.bin` | GMU image |
| `qcom/a740_sqe.fw` | Command processor firmware |
| `qcom/sm8550/sheng/a740_zap.mbn` | Board's signed ZAP image |

The recipe checks pins in `scripts/data/adreno-inputs.json` and reads
`.cache/firmware/sheng-gpu/`. Adapt the extraction paths and pins to the target's
verified firmware; record provenance in `proprietary-files.txt`.

**Expected result:** board power inputs and firmware match the target GPU.
Confirm GMU startup and HFI response before submitting commands.

## 3. Establish memory and command submission

1. Review ArmSmmu4K context ownership, stream selection, address width and cache
   maintenance. sheng uses GPU streams 0/1 and GMU stream 5 on its GPU SMMU.
2. Adapt reserved memory and GPU virtual-address windows. Keep scanout, firmware,
   ring and client allocations within their owned ranges.
3. Confirm CP initialization, ring progress and fence completion with bounded work.
4. Render a known image and inspect the result before using UI workloads.
5. Verify client teardown, mapping lifetime and fence failures.

Use `AdrenoFirmware` for firmware/HFI operations and `AdrenoPM4` for supported
packet encoding. The sheng recipe links these kits, ArmSmmu4K and board votes
into `libAdreno740.a`, then builds the accelerator through
`build-adreno740-accelerator.py`. Preserve that order when creating the target recipe.

**Expected result:** commands complete, fences advance and the known image is
correct. Record GPU results independently from desktop presentation.

## 4. Build and stage Metal

Run these commands from the resolved target tree after preparing the matching
SDK, compiler dependencies and ABI inputs:

```sh
python3 scripts/build-turnip-darwin.py --target ios
python3 scripts/build-adreno-metal-plugin.py
python3 scripts/add-gpu-plugin-data.py --publish /path/to/base.dbr /path/to/gpu.dbr
```

The staging command uses macOS image tools and writes a copy of the Data seed.
Use that output as `--data-ramdisk` when building the payload, then rebuild the
boot image. For selected app processes, the staging command also accepts
`--clients NAME,...`. The runtime plugin name is `NativeAdrenoMetal`.

Check the selected OS profile's sandbox, user-client and trust-cache integration.
AirNir is the offline compiler kit; the runtime frontend uses the
AIR → SPIR-V → NIR → IR3 chain described in the component page.

**Expected result:** Metal device discovery, client connection, pipeline creation,
rendering and presentation each succeed. Record firmware, compiler and plugin
revisions with the frame result. Cold pipeline compilation and cache reuse need
separate timing observations.

## Next step

Continue with [Wi-Fi](/porting/network), or return to the
[driver checklist](/porting/drivers).
