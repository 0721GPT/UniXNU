# 小米平板 6S Pro（sheng）

sheng 使用 SM8550 SoC 和 `ipados-17.7.11-21H461` OS 配置。

| 仓库 | 职责 |
| --- | --- |
| [UniXNU-sm8550-common](https://github.com/UniXNU/UniXNU-sm8550-common) | SoC 配置、DTSI、GPU 平台操作和共享补丁 |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | 板级 DTS、外设、组件选择、固件策略、分区和打包 |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | UEFI 平台集成 |

## 配置

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

设备 feed 固定 SoC 提交。配置按 ARM64、SoC、OS、设备顺序合并，所选
`sheng_defconfig` 在配置片段之后应用。输出根目录为
`out/sheng/ipados-17.7.11-21H461/sheng/`。

OS 配置提供 XNU 10063.141.1、iPad7,1 iPadOS 17.7.11 build 21H461、IPSW 成员固定
哈希、预链接根组件、Apple 身份和 OS ABI 要求。`payload.adt` 提供设备根卷 UUID、
内存请求和平台种子规则。

## 硬件描述

`dts/sheng.dts` 包含 `sm8550.dtsi`，提供型号、compatible、DRAM 范围、splash
保留区域、CPU 选择和启用的外设。共享 ADT 生成器结合 OS 配置与 payload 输入完成转换。

| 输入 | 值 |
| --- | --- |
| 交接使用的 DRAM 范围 | 基址 `0x80000000`，长度 `0x80000000` |
| Splash 区域 | 基址 `0xb8000000`，长度 `0x2b00000` |
| 连续内存请求 | `0x130000000` |
| 根卷 UUID | `04769D26-2CA0-4825-B443-E2E5F88300F2` |
| 平台种子 | 通过 `--unique-chip-id` 提供 |

## 组件

设备选择 ArmDTPlatform、ArmCpu、SoftwareAES、ShengUfs、VirtioMmioBlock、RamBlock、
Adreno740、AdrenoMetal、GopFramebuffer、ShengHidInput、ShengNovatekTouch、QcomGeniSpi、
AppleKeyStoreState、XnuPrelink、DarwinBoot、VerifiedRoot、ShengPreflight、AirNir 和 Ath12kWifi。
解析器补全共享 SM8550 GPU kit 等依赖。

Adreno740 和 AdrenoMetal 实现 A740/SM8550 图形栈。sheng 提供签名固件、电源策略、
内存和显示输入。[GPU 页面](/zh/drivers/adreno740)说明运行时提交与插件发布。
插件文件名和运行时类保持其 ABI 标识符。

ShengUfs 使用 SM8550 UFS 资源和 sheng 存储策略。userdata 分区为
`first_lba = 0x2f3840`、`last_lba = 0x3d0abfa`，逻辑块大小为 4096 字节。
OS 和 SystemOS 窗口来自所选 IPSW。`sheng-data-persist=2` 将 Data 写入 userdata
末尾、1024 MiB 日志保留区之前的存储区域。

Ath12kWifi 使用板级 PCIe、MSI、DMA 和网络接口。设备配置选择 208 MiB payload、
sRGB 显示属性和 120 Hz 时序请求。

## 构建

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

平台种子、trust cache、Data seed 和厂商固件来自设备输入清单。
`proprietary-files.txt` 记录固件来源与哈希，OS 配置记录接受的 trust cache 哈希。

内核依次选择 ARM64、SoC、OS 和设备补丁 series。
archive 计划将组件构建命令与 ABI 绑定、板级辅助脚本按顺序排列。使用
`status --json` 检查两者。

## 启动

```sh
fastboot boot out/sheng/ipados-17.7.11-21H461/sheng/boot/boot.img
```

userdata 安装使用上述分区布局：

```sh
./unixnu build userdata
fastboot flash userdata out/sheng/ipados-17.7.11-21H461/sheng/userdata/userdata-root.img
fastboot reboot bootloader
fastboot boot out/sheng/ipados-17.7.11-21H461/sheng/boot/boot.img
```

## 验证

运行 `unixnu status --json` 检查源码版本、配置、组件和补丁顺序。使用
[所需输入](/zh/porting/migration#构建输入)构建所选目标。检查 DTS/ADT 输出、
内核与预链接日志、payload 内容和固件打包结果。

使用[迁移报告](/zh/porting/migration#验证报告)记录镜像版本、命令、输出和硬件观察。
完整板级输入由设备仓库维护。

- [SM8550 支持](/zh/porting/sm8550)
- [Adreno740](/zh/drivers/adreno740)
- [Novatek 触控](/zh/drivers/sheng-novatek-touch)
- [无人值守检查](/zh/devices/sheng-unattended-test)
- [迁移指南](/zh/porting/migration)
