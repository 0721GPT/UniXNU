# Wi-Fi adaptation

Start with [platform interrupts and DMA](/porting/platform). Identify the actual
radio, PCI ID, firmware and board-data variant. Read [Ath12kWifi](/drivers/ath12k-wifi)
and its `include/nw_abi.h` before implementing callbacks.

## 1. Separate the integration layers

| Source | Responsibility |
| --- | --- |
| Ath12kWifi `include/nw_abi.h` | Board callbacks and station API |
| Ath12kWifi `src/` and `shim/` | ath12k, PCI/MHI, QRTR and runtime integration |
| sheng `package/WifiProbe.cpp` | PCIe power, PHY, BAR, MSI, DMA, firmware access and driver loop |
| sheng `package/WifiPcieTables.h` | Board-selected PCIe PHY configuration |
| sheng `package/MhiBoot.c` | Board MHI firmware-startup probe |
| sheng `package/WifiNet.cpp` | XNU network interface, Apple80211 requests and events |

Read the [board sources](https://github.com/UniXNU/UniXNU-sheng/tree/main/package).
sheng uses WCN7850 on SM8550 PCIe0. The component also contains QCC2072 and
QCN9274 definitions. Match the endpoint's hardware revision and firmware support
in the driver before selecting a device definition.

## 2. Bring up the PCIe endpoint

Record controller/PHY windows, lane count, reset sequence, WLAN enable, PERST,
reference clock, regulators, interconnect votes, SMMU streams and MSI routing.
On sheng these are C++ constants and tables in the board package.

1. Inspect powered and clocked register state.
2. Apply the board's rails, clocks, PHY configuration and reset sequence.
3. Train the link and read the endpoint vendor/device ID.
4. Assign and map the BAR; inspect PCI capabilities and enable the required
   memory and bus-master operations.
5. Establish the endpoint DMA range and prove that allocated buffers are
   reachable with the selected cache policy.
6. Attach MSI and confirm delivery before relying on interrupt-driven progress.

**Expected result:** the correct endpoint, readable BAR, usable DMA and delivered
interrupts. sheng's probe stages are selected by `sheng-wifi`; stage 1 inspects
handoff state, stage 2 starts the link and stage 4 inspects the endpoint.
The current endpoint path enters the reusable stack at stage 5 or higher; that
stack performs its MHI and firmware startup. Read the stage's
source before enabling it on a new board.

## 3. Supply the component callbacks

Implement `lx_platform` from `nw_abi.h`: time, sleep, polling, logging, allocation,
DMA allocation/mapping/synchronization and `fw_get`. Implement `nw_board`: mapped
BAR, PCI identity, config-space access and MSI target.

The sheng implementation uses `native_ufs_extra_*` for its DMA arena and
`native_ufs_part_*` for firmware access. A port with another storage or DMA
implementation must supply equivalent board services. Record firmware filenames,
board data and calibration provenance in `proprietary-files.txt`.

Call `nw_wifi_start`, then service `nw_wifi_step` from the board worker. Event
callbacks run from that worker and must return promptly. Inspect `nw_wifi_stats`,
`nw_wifi_where`, board MSI counts and the firmware log when progress stops.

**Expected result:** firmware and QRTR services start, then scanning reports BSS
entries. Firmware startup and successful scanning are separate checkpoints.

## 4. Connect the OS network interface

Adapt `WifiNet.cpp` to the selected XNU and wireless-framework ABI. Connect
`nw_wifi_scan`, `nw_wifi_connect`, `nw_wifi_tx` and receive/link events to the OS.
Provide the device MAC identity, channel/regulatory data and request layouts
required by the selected userland.

Select Ath12kWifi in the complete component list. sheng builds its objects through
`build-ath12k-wifi.py`, called by `build-gop-legacy-display.py`, which also links
`WifiProbe.cpp` and `WifiNet.cpp`. Inspect the assembled target tree:

```sh
rg -n 'struct lx_platform|struct nw_board|nw_wifi_' drivers/Ath12kWifi/include/nw_abi.h
rg -n 'nativewifi|wifi_module_objs|WifiNet|WifiProbe' scripts/build-gop-legacy-display.py
```

Rebuild the kernel, payload and boot image using [Building](/building).

**Expected result:** scan, authentication, association, address configuration,
receive and transmit each succeed. Record access-point settings, firmware,
reconnect behavior and sustained traffic separately in the network record.

## Next step

Continue with [DSP, audio and power](/porting/power), or return to the
[driver checklist](/porting/drivers).
