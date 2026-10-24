# Adapt the drivers

Use this checklist after [creating and resolving the device repository](/porting/).
Start with platform services, then add one peripheral path at a time. Read the
linked guide before selecting that component or copying its board integration.

## 1. Locate the implementation

The [catalog](/reference/components) lists component scope and dependencies.
The sheng [board configuration](https://github.com/UniXNU/UniXNU-sheng/blob/main/board.toml)
selects components and archive recipes. Its
[package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package) also implements
brightness, buttons, wireless board access, DSP services and power controls.
These files enter the build through device scripts.

From the main checkout, inspect the selected sources:

```sh
./unixnu status --json
./unixnu tree
```

Use the reported target tree for component build commands. Its `drivers/` and
`kits/` directories contain the resolved sources. The original device package
is linked at `kits/<board.kit>`.

## 2. Follow the hardware sequence

| Order | Capability | Adaptation guide | Implementation to inspect |
| --- | --- | --- | --- |
| 1 | CPU, interrupts, timer, RAM, DMA | [Platform and memory](/porting/platform) | ArmDTPlatform, ArmCpu, ArmSmmu4K, SoC DTSI |
| 2 | Storage, root, persistent Data | [UFS](/porting/ufs) and [block/root integration](/porting/platform#block-and-os-services) | ShengUfs, RamBlock, VirtioMmioBlock, VerifiedRoot |
| 3 | Framebuffer, refresh, brightness | [Display and brightness](/porting/display) | GopFramebuffer, DisplayTimingProbe, AppleARMBacklight, GeniI2c |
| 4 | Touch and physical buttons | [Input](/porting/input) | QcomGeniSpi, ShengNovatekTouch, ShengHidInput, ButtonProbe |
| 5 | GPU and Metal | [GPU](/porting/gpu) | Adreno740, AdrenoMetal, Sm8550Platform, AdrenoFirmware, AdrenoPM4, AirNir |
| 6 | Wi-Fi | [Network](/porting/network) | Ath12kWifi, WifiProbe, WifiNet, MhiBoot |
| 7 | DSP, audio, battery, CPU frequency, platform power | [DSP and power](/porting/power) | AdspProbe, AudioProbe, ShengBattery, CpuFreqProbe, ShengPlatformPower |
| 8 | OS services and packaging | [OS integration](/porting/platform#block-and-os-services) and [firmware](/porting/uefi-firmware) | SoftwareAES, AppleKeyStoreState, XnuPrelink, DarwinBoot, ShengPreflight |

The guides describe sheng's implementation and the inputs needed on another
board. For another SoC, implement its controller, power and DMA operations
behind the selected interfaces. For another OS build, qualify the private ABI
bindings against that build's kernelcache.

## 3. Connect the build

1. Add the component ID to the complete `[components] include` list. Inspect
   dependency expansion in `status`.
2. Add the archive recipe in dependency order. Inspect the catalog's `build`
   entry and the selected device's `[kernel] archives`.
3. Add board operations, firmware extraction and initialization calls. Check
   where each probe is called and which configuration flag enables its caller.
4. Add resources to DTS only when the consuming driver reads those properties.
   A driver with C++ address constants needs its source adapted as well.
5. Build the kernel, then rebuild the payload and boot image for the selected
   stage. Follow [Building](/building) and [boot arguments](/xnu/boot-arguments#apply-arguments).

In sheng, `scripts/build-gop-legacy-display.py` also assembles touch, brightness,
buttons, Wi-Fi, DSP and battery objects. Reusing that recipe requires its board
sources and ABI inputs. Review its source list when moving a subsystem to a
separate recipe.

## 4. Record the result

For every capability, record the source revision, firmware, boot arguments,
command, artifact path, last successful stage and captured result in
`porting.toml`. Use the [completion levels](/porting/records#completion-levels)
for source review, host checks and device observations.

A missing input belongs in the record with the next action needed to obtain it.
The next device stage starts when its prerequisites are verified. Return to
[bring-up](/porting/bring-up) to integrate the completed paths.
