# 小米平板 6S Pro（sheng）

sheng 是搭载 SM8550 SoC 的小米平板 6S Pro。它是[支持的设备](/zh/devices/)中的一个设备移植。

| 仓库 | 内容 |
| --- | --- |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | 板级文件、defconfig、设备脚本、触控驱动和 package |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | Mu-Silicium UEFI 移植 |

## 板级

```toml
[board]
name = "sheng"
soc = "SM8550"
description = "Xiaomi Pad 6S Pro"
kit = "sheng"
```

输出目录为 `out/sheng/`。

## IPSW

```toml
[ipsw]
url = "https://updates.cdn-apple.com/2025SpringFCS/fullrestores/122-77166/DCA6CFE8-90E9-4B92-880D-3DE171798E40/iPad_Pro_HFR_17.7.11_21H461_Restore.ipsw"
product = "iPad7,1"
version = "17.7.11"
build = "21H461"
device_class = "j120ap"
chip_id = 0x8011
board_id = 12
fetch = ["KernelCache", "DeviceTree", "StaticTrustCache", "Cryptex1,SystemTrustCache"]
```

板级使用未修改的 iPad7,1 iPadOS 17.7.11 恢复镜像。成员哈希：

```toml
[ipsw.sha256]
"044-76040-038.dmg" = "3f99b859c9728379110b66657c7b15285bcb1925ad8b5726542e2cdcf8b1028d"
"044-76034-038.dmg" = "b4908cde41d23e6bc05e6af51a2c9eeb00cf0f863a6a18b1600c96a3fd70414f"
```

## userdata

```toml
[userdata]
first_lba = 0x2f3840
last_lba = 0x3d0abfa
lba_bytes = 4096
windows = [
  { name = "OS", component = "OS" },
  { name = "SystemOS", component = "Cryptex1,SystemOS" },
]
```

LBA 来自设备上名为 `userdata` 的 GPT 条目。

## 内核

```toml
[kernel]
defconfig = "sheng_defconfig"

[kernel.prelink]
tool = "kits/NativePrelink/emit_prelink.py"
kernel = "build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837.unstripped.noctf"
kernelcache_sha256 = "f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7"
slide = "-0x1000000"
early_security_init = "com.apple.driver.AppleMobileFileIntegrity=0xfffffff005ab4ae0"
```

预链接 roots：

```
com.apple.AppleFSCompression.AppleFSCompressionTypeZlib
com.apple.driver.AppleARMPlatform
com.apple.driver.AppleARMWatchdogTimer
com.apple.driver.AppleAudioClockLibs
com.apple.driver.AppleEffaceableStorage
com.apple.driver.AppleIISController
com.apple.driver.AppleM2ScalerCSCDriver
com.apple.driver.AppleMobileFileIntegrity
com.apple.driver.ApplePMGR
com.apple.driver.IODARTFamily
com.apple.filesystems.apfs
com.apple.iokit.CoreAnalyticsFamily
com.apple.iokit.IOAVFamily
com.apple.iokit.IOAudio2Family
com.apple.iokit.IOCECFamily
com.apple.iokit.IOCryptoAcceleratorFamily
com.apple.iokit.IOGPUFamily
com.apple.iokit.IOHDCPFamily
com.apple.iokit.IOMobileGraphicsFamily
com.apple.iokit.IOReportFamily
com.apple.iokit.IOSlowAdaptiveClockingFamily
com.apple.iokit.IOStorageFamily
com.apple.iokit.IOSurface
com.apple.kec.Libm
com.apple.kec.corecrypto
com.apple.kext.AppleMatch
com.apple.kext.CoreTrust
com.apple.security.AppleImage4
com.apple.security.sandbox
com.apple.iokit.IOHIDFamily
com.apple.iokit.IOHIDEventDriver
```

## defconfig

`sheng_defconfig` 启用：

```
CONFIG_XNU_SOURCE_CORECRYPTO14=y
CONFIG_XNU_SOURCE_PTHREAD=y
CONFIG_XNU_ARM_DT_PLATFORM=y
CONFIG_XNU_ARMCPU=y
CONFIG_XNU_NATIVE_UFS=y
CONFIG_XNU_NATIVE_AES=y
CONFIG_XNU_NATIVE_VIRTIO_BLOCK=y
CONFIG_XNU_NATIVE_RAM_BLOCK=y
CONFIG_XNU_TRUST_CACHE_RUNTIME=y
CONFIG_XNU_SOURCE_MOCKFS=y
CONFIG_XNU_NATIVE_CS_HASH_CACHE=y
CONFIG_XNU_VERIFIED_ROOT_PROBE=y
CONFIG_XNU_OWNER_ROOT_AUDIT=y
CONFIG_XNU_OWNER_ROOT_TRUST=y
CONFIG_XNU_SHENG_PMGR=y
CONFIG_XNU_OWNER_CACHE_MOUNT=y
CONFIG_XNU_OWNER_CACHE_STARTUP=y
CONFIG_XNU_OWNER_BOOT_MANIFEST=y
CONFIG_XNU_OWNER_DATA_MOUNT=y
CONFIG_XNU_OWNER_UNION_SYSTEMOS=y
CONFIG_XNU_DT_PANIC_REGION=y
CONFIG_XNU_ZONE_PGZ_ALIGNMENT_PROBE=y
CONFIG_XNU_NATIVE_ARCH_TIMER=y
CONFIG_XNU_NATIVE_PAN=y
CONFIG_XNU_BOOT_PROGRESS=y
CONFIG_XNU_NATIVE_LEGACY_DISPLAY=y
CONFIG_XNU_NATIVE_ADRENO=y
CONFIG_XNU_CRYPTO_BOOTSTRAP="prelinked"
```

板级设置 scratch Data 卷 UUID：

```
CONFIG_XNU_OWNER_DATA_VOLUME_UUID="18AB5557-DC2D-469B-8758-855E60F5B0DF"
```

## 固件

```toml
[firmware]
config = "sheng"
dsc = "Platforms/Xiaomi/shengPkg/sheng.dsc"
fd = "Build/shengPkg/{target}_CLANGPDB/FV/SILICIUM_UEFI.fd"
preflight = ["scripts/check-sheng.py"]
submodules = ["Common/Mu", "Common/Mu_OEM_Sample", "Silicium-ACPI", "Silicon/Silicium/OpensslPkg/Library/OpensslLib/openssl"]
patches = ["Resources/MuPatches/Gic-Spurious.patch"]
```

package 路径覆盖 Mu-Silicium 树、UniXNU package、DarwinBoot、edk2、Mu、Mu OEM、Qualcomm、Silicium 和 ACPI。

defines：

```toml
[firmware.defines]
ENABLE_SECUREBOOT = "1"
DEVICE_MODEL = "sheng"
MEMORY_PROTECTION = "TRUE"
SHIP_MODE = "FALSE"
BUILDID_STRING = "sheng-research"
```

## 负载

```toml
[payload]
volume = "SHENGESP"
size_mib = 192
loader = "firmware"
title = "sheng first boot: source 4K XNU to console"
dram_size = "0x80000000"
skip_setup_tool = "scripts/seed-data-skip-setup.py"
panic_screen = true
trust_cache_sha256 = [
  "7416ca4f8377146626bb284e2133c76bee6cb4e3ee0af5d648b186235a7323a7",
  "8900d6a93d7a59ce1a872a5c081e559f4240a218f97e12d44d22dda46644d68f",
]
```

内核参数覆盖平台、存储、显示、输入、GPU、诊断和 owner root 路径。完整列表在板级文件中。

ADT：

```toml
[payload.adt]
tool = "scripts/make-sheng-adt.py"
chip_id = "207"
owned_memory_size = "0x130000000"
root_match_volume_uuid = "04769D26-2CA0-4825-B443-E2E5F88300F2"
cpu_regs = "0,0x100,0x200,0x300,0x400,0x500,0x600,0x700"
packed_data_ramdisk = true
ipad_identity = true
unique_chip_id = "required"
```

启动镜像加入原有 ABL 需要的 splash 区域：

```toml
[[bootimg.dtb_node]]
path = "/reserved-memory/splash_region"
props = [
  { name = "label", type = "s", value = "cont_splash_region" },
  { name = "reg", type = "x", value = ["0x0", "0xb8000000", "0x0", "0x2b00000"] },
]
```

## 构建

```sh
git clone https://github.com/UniXNU/UniXNU
git clone https://github.com/UniXNU/UniXNU-sheng
git clone https://github.com/UniXNU/uefi-sheng
cd UniXNU

./unixnu defconfig sheng_defconfig -D ../UniXNU-sheng
./unixnu fetch
./unixnu ipsw fetch
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-sheng
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-sheng --dtb <sheng.dtb>
```

平台种子、trust cache 和 Data 种子是设备输入。板级文件报告接受的 trust cache 哈希。

## 启动

```sh
fastboot boot out/sheng/boot/boot.img
```

首次启动使用 userdata 镜像：

```sh
./unixnu build userdata
fastboot flash userdata out/sheng/userdata/userdata-root.img
fastboot reboot bootloader
fastboot boot out/sheng/boot/boot.img
```

## macOS

板级文件指向 iPadOS 恢复镜像。要在同一设备上启动 macOS，把 `[ipsw]` 指向 macOS 恢复镜像，并更新产品、版本、build、device class、chip ID、board ID 和成员哈希。把 `[userdata] windows` 设为 `OS` 和 `Cryptex1,SystemOS`。内核、负载、启动镜像和 userdata 步骤保持不变。

## 设备支持

启动链可以到达 XNU。下表记录操作系统中用户可见的设备支持。

| 功能 | 状态 | 说明 |
| --- | --- | --- |
| 触控 | ✅ | Novatek NT36532E |
| 显示 | ✅ | |
| Wi-Fi | ❌ | |
| 蓝牙 | ❌ | |
| 相机 | ❌ | |
| GPU | ❌ | |
| USB | ❌ | |
| 音频 | ❌ | |
| 传感器 | ❌ | |
| 电池驱动 | ❌ | |
| MiPPS | ❌ | |
| 休眠与唤醒 | ❌ | |

✅ 可用，❌ 不可用。

## 观察状态

| 阶段 | 状态 |
| --- | --- |
| 原有 ABL 加载启动镜像，BootShim 启动 UEFI 固件 | 工作 |
| Silicium UEFI 和 UniXNU 启动 | 工作；Silicium 偶尔停住，再次启动同一镜像可继续 |
| UniXNU 读取负载卷 | 工作 |
| XNU 与用户态 | 早期工作流的镜像已到达 |
| 触控驱动 | NT36532E 控制器的分阶段探针 |

触控驱动见[触控驱动示例](/zh/devices/sheng-touch-driver)。

## 设备细节

- 设备树没有 splash 区域时，原有 ABL 会关闭面板。启动镜像加入 `/reserved-memory/splash_region`。
- 平台种子位于仓库之外，用 `--unique-chip-id` 提供。
- 原生 UFS 根路径需要 `sheng-pmgr=2`。`native-ufs-root=1` 时，负载步骤拒绝没有链接 PMGR 集成的内核。
- trust cache 包必须是两个接受哈希之一。
- Data 种子是负载步骤的输入。

## 下一步

- [移植](/zh/porting/)
- [触控驱动示例](/zh/devices/sheng-touch-driver)
- [调试与日志](/zh/guide/debugging)
- [故障排查](/zh/reference/troubleshooting)
