# Touch and button adaptation

Complete [display geometry](/porting/display) and [interrupt/DMA setup](/porting/platform).
Record touch-controller model, firmware, matrix geometry, physical orientation,
bus wiring, reset/IRQ pins and the OS HID ABI.

## 1. Select the transport and sensor protocol

Read [QcomGeniSpi](/drivers/qcom-geni-spi),
[ShengNovatekTouch](/drivers/sheng-novatek-touch) and
[ShengHidInput](/drivers/sheng-hid-input). Their responsibilities are:

| Source | Input to adapt |
| --- | --- |
| `QcomGeniSpi/GeniSpi.c` | GENI engine access and SPI transfer setup |
| `ShengNovatekTouch/NvtProtocol.c` | Supported chip map, firmware format and report protocol |
| `ShengNovatekTouch/NvtTouchCore.c` | Sensor matrix, filtering and contact tracking |
| sheng `package/TouchBoard.h` | Controller windows, GPIOs, clock votes and SMMU stream |
| sheng `package/TouchProbe.cpp` | Resource ownership, firmware loading, frame loop and coordinates |
| `ShengHidInput/NativeHidInput.cpp` | Contacts/buttons to IOHIDFamily reports |

For a different sensor, implement its identification, firmware and report
protocol. For a different bus controller, implement the transfer/delay callbacks
used by `nvt_bus` when reusing the Novatek protocol.

## 2. Adapt the board path

1. Replace `TouchBoard.h` inputs from the target DTS and clock/pin definitions.
   sheng uses QUP wrapper `0xac0000`, engine `0xa90000`, SPI mode 0 up to
   19.2 MHz, GPIOs 44 through 47 for SPI, IRQ GPIO 25 and SMMU stream `0xa3`.
2. Implement power/reset ownership and verify engine firmware, pins and clocks.
3. Set DMA addressing and cache maintenance for the target SMMU context.
4. Read the chip ID, then load its firmware and check the returned geometry.
5. Decode complete frames and map coordinates to the display's orientation.
6. Check stable contact IDs, simultaneous contacts, edge behavior and release.

The Novatek processing core uses a 40 × 60 matrix. Verify geometry and bounds
when adapting its firmware or processing core. Firmware extraction is owned by
the device's `scripts/gen-touch-firmware.py` and proprietary-file inventory.

sheng's `sheng-touch=1`, `2`, `3` and `4` select resource inspection, clocks/pins,
controller identification, then firmware/scanning. Use those stages only after
adapting the hardware values consumed by that stage.

**Expected result:** coherent contact frames in display coordinates, including
lift events. The HID layer consumes `native_touch_*` through a sequence counter;
preserve snapshot consistency when replacing the producer.

## 3. Publish HID reports

Qualify IOHIDFamily methods and object layouts against the selected kernelcache.
The sheng archive recipe uses a prelinked candidate and ABI records; see
[build inputs](/porting/migration#build-inputs). Adapt the report descriptor,
logical coordinate ranges and contact limits to the sensor.

Select the sensor, transport and HID components in the complete component list.
The sheng display recipe links the sensor and bus objects; its HID recipe binds
the OS interface. `sheng-touch-hid=1` enables sheng's touch publication path.
From the assembled target tree, inspect their integration:

```sh
rg -n 'TouchProbe|GeniSpi|NvtProtocol|NvtTouchCore|NvtFirmware' scripts/build-gop-legacy-display.py
rg -n 'native_touch_|ReportDescriptor' drivers/ShengHidInput/NativeHidInput.cpp
```

**Expected result:** the OS receives correct down, move and up reports for each
contact, and UI gestures follow the panel orientation.

## 4. Adapt physical buttons

Read sheng's [ButtonProbe.cpp](https://github.com/UniXNU/UniXNU-sheng/blob/main/package/ButtonProbe.cpp).
Its power and volume-down buttons use PMK8550 PON/RESIN; volume-up uses PM8550
GPIO 6. The SPMI arbiter routes the interrupt through PDC to GIC SPI 481.

Map the target's GPIO or PMIC inputs, polarity, ownership, interrupt routing and
acknowledgement. Access only the PMIC peripherals owned by the selected execution
environment. Adapt `BootResetCore.h` helpers where the SPMI controller differs.
Connect press/release state to the HID consumer report. sheng uses report 2 bits
for Power, Volume Increment and Volume Decrement and enables this with
`sheng-buttons=1`.

**Expected result:** every press and release produces the expected report,
including held and simultaneous buttons, with bounded interrupt activity.

## Next step

Rebuild through [Building](/building), record touch and buttons separately, then
continue with [GPU adaptation](/porting/gpu).
