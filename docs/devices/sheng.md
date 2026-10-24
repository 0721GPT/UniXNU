# Xiaomi Pad 6S Pro (sheng)

sheng uses the SM8550 SoC and the `ipados-17.7.11-21H461` OS profile.

| Repository | Responsibility |
| --- | --- |
| [UniXNU-sm8550-common](https://github.com/UniXNU/UniXNU-sm8550-common) | SoC configuration, DTSI, GPU platform operations, shared patches |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | Board DTS, peripherals, selected components, firmware policy, partitions and packaging |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | UEFI platform integration |

## Configuration

```toml
schema_version = 2

[board]
name = "sheng"
soc = "SM8550"
soc_source = "sm8550"
os_profile = "ipados-17.7.11-21H461"
kit = "sheng"

[device_tree]
source = "device:dts/sheng.dts"
include_dirs = ["soc:dts"]
```

The device feed pins the SoC commit. ARM64, SoC, OS, and device settings merge in
that order. The selected `sheng_defconfig` follows their configuration fragments.
The output root is `out/sheng/ipados-17.7.11-21H461/sheng/`.

The OS profile supplies XNU 10063.141.1, iPad7,1 iPadOS 17.7.11 build 21H461,
IPSW member pins, prelink roots, Apple identity, and OS ABI requirements.
`payload.adt` supplies the device root UUID, memory request, and platform seed policy.

## Hardware description

`dts/sheng.dts` includes `sm8550.dtsi`. It supplies the model, compatible values,
DRAM range, splash reservation, CPU selection, and enabled peripherals. The
shared ADT generator converts it with the OS profile and payload inputs.

| Input | Value |
| --- | --- |
| DRAM range used by the handoff | Base `0x80000000`, size `0x80000000` |
| Splash region | Base `0xb8000000`, size `0x2b00000` |
| Contiguous memory request | `0x130000000` |
| Root UUID | `04769D26-2CA0-4825-B443-E2E5F88300F2` |
| Platform seed | Supplied with `--unique-chip-id` |

## Components

The device selects ArmDTPlatform, ArmCpu, SoftwareAES, ShengUfs, VirtioMmioBlock,
RamBlock, Adreno740, AdrenoMetal, GopFramebuffer, ShengHidInput, ShengNovatekTouch,
QcomGeniSpi, AppleKeyStoreState, XnuPrelink, DarwinBoot, VerifiedRoot, ShengPreflight,
AirNir, and Ath12kWifi. The resolver adds dependencies including the shared SM8550 GPU kit.

Adreno740 and AdrenoMetal implement the A740/SM8550 graphics stack. sheng supplies
signed firmware, power policy, memory, and display inputs. The [GPU page](/drivers/adreno740)
describes runtime submission and plugin staging. Plugin filenames and runtime
classes retain their ABI identifiers.

ShengUfs uses the SM8550 UFS resources and sheng storage policy. The userdata
partition has `first_lba = 0x2f3840`, `last_lba = 0x3d0abfa`, and 4096-byte blocks.
The OS and SystemOS windows come from the selected IPSW. With
`sheng-data-persist=2`, the Data store writes through to the end of userdata,
below the 1024 MiB log reserve.

Ath12kWifi uses the board PCIe, MSI, DMA, and network interfaces. The board
selects a 208 MiB payload, sRGB display properties, and a 120 Hz timing request.

## Build

```sh
git clone --recurse-submodules https://github.com/UniXNU/UniXNU
git clone --recurse-submodules https://github.com/UniXNU/UniXNU-sheng
git clone https://github.com/UniXNU/uefi-sheng
cd UniXNU
./unixnu defconfig sheng_defconfig -D ../UniXNU-sheng
./unixnu fetch
./unixnu status --json
./unixnu ipsw fetch
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-sheng
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-sheng --dtb <firmware.dtb>
```

The platform seed, trust cache, Data seed, and vendor firmware come from the
device input inventory. `proprietary-files.txt` records firmware sources and
hashes. The OS profile records accepted trust-cache hashes.

The kernel selects ARM64, SoC, OS, and device patch series in that order. Its archive plan interleaves component recipes
with ABI bindings and board helper scripts. Inspect both in `status --json`.

## Boot

```sh
fastboot boot out/sheng/ipados-17.7.11-21H461/sheng/boot/boot.img
```

The userdata installation uses the partition described above:

```sh
./unixnu build userdata
fastboot flash userdata out/sheng/ipados-17.7.11-21H461/sheng/userdata/userdata-root.img
fastboot reboot bootloader
fastboot boot out/sheng/ipados-17.7.11-21H461/sheng/boot/boot.img
```

## Verification

Run `unixnu status --json` to inspect source revisions, profiles, components,
and patch order. Build the selected target with the
[required inputs](/porting/migration#build-inputs). Inspect DTS/ADT output,
kernel and prelink logs, payload contents, and firmware packaging.

Record image revisions, commands, outputs, and hardware observations with the
[migration report](/porting/migration#verification-report). The complete board
inputs are maintained in the device repository.

- [SM8550 support](/porting/sm8550)
- [Adreno740](/drivers/adreno740)
- [Novatek touch](/drivers/sheng-novatek-touch)
- [Unattended checks](/devices/sheng-unattended-test)
- [Migration](/porting/migration)
