# Display and brightness

Start after [platform and memory](/porting/platform). Collect the firmware GOP
base, size, dimensions, stride, pixel format and reserved-memory range. Identify
the panel, display controller, backlight controller and their power owners.

## 1. Establish firmware scanout

Build the [integrated UEFI firmware](/porting/uefi-firmware). Confirm that its
menu displays correctly. Compare GOP geometry and framebuffer bounds with the
firmware memory map and board DTS. Preserve the active scanout allocation through
kernel handoff.

**Expected result:** a stable firmware image with correct colors and dimensions.
Resolve this before enabling the XNU presentation backend.

## 2. Adapt the XNU display backend

Read [GopFramebuffer](/drivers/gop-framebuffer) and its `NativeLegacyDisplay.cpp`,
`NativeGOPPresenter.cpp` and `NativeGOPWire.h`. It consumes `PE_current_console`
and integrates with the selected MobileFramebuffer and IOSurface ABI.

In the [sheng package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package),
inspect `LegacyDisplayProbe.cpp` and `DisplayTimingProbe.cpp`. The device's
`scripts/build-gop-legacy-display.py` qualifies ABI methods against a prelinked
candidate before producing the late display archive. Supply the matching
candidate and audit inputs listed in [build inputs](/porting/migration#build-inputs).

1. Select GopFramebuffer and the display archive recipe, preserving its dependencies.
2. Enable the recipe's kernel configuration and initialization caller. sheng uses
   `CONFIG_XNU_NATIVE_LEGACY_DISPLAY=y` and its `offline-legacy-*` arguments.
3. Check pixel layout, row stride, allocation size and cache handling in CPU
   presentation. Confirm that a known frame reaches the display.
4. Check swap completion and IOSurface lifetime before enabling
   [GPU presentation](/porting/gpu).
5. Adapt timing reads to the target display controller. sheng's `sheng-dpu=1`
   samples the SM8550 DPU and dual-DSI counters through `DisplayTimingProbe.cpp`.
   Set refresh policy from the observed panel mode. `sheng-vsync=120` is sheng's
   requested timing value.

**Expected result:** XNU publishes the display, presents frames and completes
swaps. Record requested refresh and measured frame timing separately.

## 3. Identify the brightness transport

sheng uses two KTZ8866 controllers at I2C address `0x11`, on GENI engines
`0xa84000` and `0x988000`. `package/AppleARMBacklight.cpp` contains the service
`ShengBacklight`; `GeniI2c.c` provides the transfer state machine and
`BacklightProbe.cpp` starts the service.

Before reuse, record the target's controller count, register encoding, brightness
range, I2C engine, slave address, pins, clocks, interrupts and power sequence.
For a DSI-command or PWM backlight, implement that transport and its level encoding
behind the OS-facing brightness service.

## 4. Adapt and enable brightness

1. Update the board's engine mappings, clock votes and interrupt routing.
   sheng's GENI path adopts firmware-loaded engine code and timing; verify that
   handoff or implement the target's initialization sequence.
2. Implement the controller's level write and readback. sheng writes an 11-bit
   value across registers `0x04` and `0x05`, with maximum level 2047, to both chips.
3. Preserve bounded transfers, cancellation and work-loop scheduling. Inspect
   errors and timeouts before enabling fades.
4. Match `IODisplayParameters` brightness limits and `backlight-control` to the
   selected OS ABI. The service accepts brightness and `FadePeriod` properties.
5. Build the service and its registration wrapper. sheng's display recipe does
   both; `sheng-backlight=1` enables its probe.

From the resolved target tree, locate these integration points:

```sh
rg -n 'kSeA|kSeB|kSpi|kMaxLevel|backlight-control|FadePeriod' kits/sheng/AppleARMBacklight.cpp
rg -n 'backlight|GeniI2c' scripts/build-gop-legacy-display.py
```

Use the equivalent kit path for the new board. Rebuild the kernel, payload and
boot image through [Building](/building).

**Expected result:** minimum, middle and maximum settings produce corresponding
readback and visible levels; fades complete; timeout counters remain stable.
On sheng, `native_backlight_stats` records publication, requests, readback,
interrupts and failures. Verify UI brightness changes as a separate step from
raw controller writes.

## Next step

Record display and brightness results separately, then continue with
[touch and buttons](/porting/input).
