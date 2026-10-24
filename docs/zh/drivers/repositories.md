# 驱动仓库

每个硬件驱动单独一个仓库。设备仓库用 `drivers/<Name>` 下的子模块引用需要的驱动。每个驱动仓库都带 `unixnu` topic。

## 官方驱动

这些仓库由 UniXNU 组织维护。

| 驱动 | 仓库 | 层 | 页面 |
| --- | --- | --- | --- |
| NativeAdreno | [UniXNU/NativeAdreno](https://github.com/UniXNU/NativeAdreno) | XNU GPU | [NativeAdreno](/zh/drivers/native-adreno) |
| NativeMetal | [UniXNU/NativeMetal](https://github.com/UniXNU/NativeMetal) | XNU Metal 前端 | [NativeMetal](/zh/drivers/native-metal) |
| NativeUfs | [UniXNU/NativeUfs](https://github.com/UniXNU/NativeUfs) | XNU 存储 | [NativeUfs](/zh/drivers/native-ufs) |
| NativeHidInput | [UniXNU/NativeHidInput](https://github.com/UniXNU/NativeHidInput) | XNU 输入 | [NativeHidInput](/zh/drivers/native-hid-input) |
| NativeRamBlock | [UniXNU/NativeRamBlock](https://github.com/UniXNU/NativeRamBlock) | XNU 存储 | [NativeRamBlock](/zh/drivers/native-ram-block) |
| NativeMobileDisplay | [UniXNU/NativeMobileDisplay](https://github.com/UniXNU/NativeMobileDisplay) | XNU 显示 | [NativeMobileDisplay](/zh/drivers/native-mobile-display) |
| NativeGeniSpi | [UniXNU/NativeGeniSpi](https://github.com/UniXNU/NativeGeniSpi) | XNU SPI | [NativeGeniSpi](/zh/drivers/native-geni-spi) |
| NativeAES | [UniXNU/NativeAES](https://github.com/UniXNU/NativeAES) | XNU 加密 | [NativeAES](/zh/drivers/native-aes) |
| NativeKeyStoreEvents | [UniXNU/NativeKeyStoreEvents](https://github.com/UniXNU/NativeKeyStoreEvents) | XNU keystore | [NativeKeyStoreEvents](/zh/drivers/native-keystore-events) |
| NativeVirtioBlock | [UniXNU/NativeVirtioBlock](https://github.com/UniXNU/NativeVirtioBlock) | XNU 存储 | [NativeVirtioBlock](/zh/drivers/native-virtio-block) |
| NativeVirtioConsole | [UniXNU/NativeVirtioConsole](https://github.com/UniXNU/NativeVirtioConsole) | XNU 控制台 | [NativeVirtioConsole](/zh/drivers/native-virtio-console) |
| NativeNovatekTouch | [UniXNU/NativeNovatekTouch](https://github.com/UniXNU/NativeNovatekTouch) | XNU 触控 | [NativeNovatekTouch](/zh/drivers/native-novatek-touch) |
| NativePrelink | [UniXNU/NativePrelink](https://github.com/UniXNU/NativePrelink) | 内核工具 | [NativePrelink](/zh/drivers/native-prelink) |
| NativeWifi | [UniXNU/NativeWifi](https://github.com/UniXNU/NativeWifi) | XNU Wi-Fi | [NativeWifi](/zh/drivers/native-wifi) |
| ArmCpu | [UniXNU/ArmCpu](https://github.com/UniXNU/ArmCpu) | XNU CPU 和 GIC | [ArmCpu](/zh/drivers/arm-cpu) |
| ArmDTPlatform | [UniXNU/ArmDTPlatform](https://github.com/UniXNU/ArmDTPlatform) | XNU 平台专家 | [ArmDTPlatform](/zh/drivers/arm-dt-platform) |
| ArmVirtPlatform | [UniXNU/ArmVirtPlatform](https://github.com/UniXNU/ArmVirtPlatform) | XNU 平台 | [ArmVirtPlatform](/zh/drivers/arm-virt-platform) |
| ArmSmmu4K | [UniXNU/ArmSmmu4K](https://github.com/UniXNU/ArmSmmu4K) | XNU SMMU kit | [ArmSmmu4K](/zh/drivers/arm-smmu4k) |
| AirNir | [UniXNU/AirNir](https://github.com/UniXNU/AirNir) | 编译器 kit | [AirNir](/zh/drivers/air-nir) |
| AdrenoPM4 | [UniXNU/AdrenoPM4](https://github.com/UniXNU/AdrenoPM4) | GPU 命令 kit | [AdrenoPM4](/zh/drivers/adreno-pm4) |
| AdrenoFirmware | [UniXNU/AdrenoFirmware](https://github.com/UniXNU/AdrenoFirmware) | GPU 固件 kit | [AdrenoFirmware](/zh/drivers/adreno-firmware) |
| DarwinBoot | [UniXNU/DarwinBoot](https://github.com/UniXNU/DarwinBoot) | UEFI 加载器 | [DarwinBoot](/zh/drivers/darwin-boot) |
| VerifiedRoot | [UniXNU/VerifiedRoot](https://github.com/UniXNU/VerifiedRoot) | 根信任工具 | [VerifiedRoot](/zh/drivers/verified-root) |
| ShengPreflight | [UniXNU/ShengPreflight](https://github.com/UniXNU/ShengPreflight) | UEFI 预检 | [ShengPreflight](/zh/drivers/sheng-preflight) |

## 社区驱动

这些仓库由各自维护者维护，由使用它的设备仓库链接。

| 驱动 | 设备 | 仓库 | 页面 |
| --- | --- | --- | --- |
| NativeMsdc | Benesse TAB-A04-BR3（MT8167） | [mouseos/NativeMsdc](https://github.com/mouseos/NativeMsdc) | [NativeMsdc](/zh/drivers/community-native-msdc) |
| NativeAdreno-myron | Redmi K90 Pro Max / Poco F8 Ultra | [zg7681/NativeAdreno-myron](https://github.com/zg7681/NativeAdreno-myron) | [NativeAdreno-myron](/zh/drivers/community-native-adreno-myron) |
| NativeUfs-myron | Redmi K90 Pro Max / Poco F8 Ultra | [zg7681/NativeUfs-myron](https://github.com/zg7681/NativeUfs-myron) | [NativeUfs-myron](/zh/drivers/community-native-ufs-myron) |
| NativeAdreno-fuxi | 小米 13 | [ljhhkx529/NativeAdreno-fuxi](https://github.com/ljhhkx529/NativeAdreno-fuxi) | [NativeAdreno-fuxi](/zh/drivers/community-native-adreno-fuxi) |
| NativeUfs-alioth | Redmi K40 / Poco F3 | [FFT-infra/NativeUfs-alioth](https://github.com/FFT-infra/NativeUfs-alioth) | [NativeUfs-alioth](/zh/drivers/community-native-ufs-alioth) |
| unixnu-nativeufs | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativeufs](https://github.com/RigoLigoRLC/unixnu-nativeufs) | [unixnu-nativeufs](/zh/drivers/community-unixnu-nativeufs) |
| unixnu-nativemobiledisplay | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativemobiledisplay](https://github.com/RigoLigoRLC/unixnu-nativemobiledisplay) | [unixnu-nativemobiledisplay](/zh/drivers/community-unixnu-nativemobiledisplay) |
| unixnu-nativeramblock | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativeramblock](https://github.com/RigoLigoRLC/unixnu-nativeramblock) | [unixnu-nativeramblock](/zh/drivers/community-unixnu-nativeramblock) |
| NativeEmmc | Redmi Note 9 | [Dracen-W/NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [NativeEmmc](/zh/drivers/community-native-emmc) |
| NativeDwc3 | 小米平板 5 | [Lux12138/NativeDwc3](https://github.com/Lux12138/NativeDwc3) | [NativeDwc3](/zh/drivers/community-native-dwc3) |
| unixnu-nativefocaltechtouch | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativefocaltechtouch](https://github.com/RigoLigoRLC/unixnu-nativefocaltechtouch) | [unixnu-nativefocaltechtouch](/zh/drivers/community-nativefocaltechtouch) |

## 设备分支

这些仓库是跟随官方驱动的设备分支，由对应设备仓库链接，不单独建驱动页。

| 设备 | 驱动 | 仓库 |
| --- | --- | --- |
| Sony Xperia 1 II | NativeAdreno | [chardidathing/NativeAdreno](https://github.com/chardidathing/NativeAdreno) |
| 小米平板 7 | NativeAdreno、NativeMobileDisplay | [Allenwdk/NativeAdreno](https://github.com/Allenwdk/NativeAdreno)、[Allenwdk/NativeMobileDisplay](https://github.com/Allenwdk/NativeMobileDisplay) |
| Nothing Phone (3a) | NativeAdreno、NativeUfs | [rootstockos/NativeAdreno](https://github.com/rootstockos/NativeAdreno)、[rootstockos/NativeUfs](https://github.com/rootstockos/NativeUfs) |
| 一加 6T | NativeAdreno、NativeMetal | [johnpanos/NativeAdreno](https://github.com/johnpanos/NativeAdreno)、[johnpanos/NativeMetal](https://github.com/johnpanos/NativeMetal) |
| 小米 13 Pro | NativeUfs | [pubglite55/NativeUfs](https://github.com/pubglite55/NativeUfs) |
| 一加 5 | NativeUfs | [lss717/NativeUfs](https://github.com/lss717/NativeUfs) |
| QEMU virt | NativeAdreno、NativeMetal、NativeMobileDisplay、NativeUfs | [xVanTuring/NativeAdreno](https://github.com/xVanTuring/NativeAdreno)、[xVanTuring/NativeMetal](https://github.com/xVanTuring/NativeMetal)、[xVanTuring/NativeMobileDisplay](https://github.com/xVanTuring/NativeMobileDisplay)、[xVanTuring/NativeUfs](https://github.com/xVanTuring/NativeUfs) |
| Benesse TAB-A04-BR3 | NativeMetal | [mouseos/NativeMetal](https://github.com/mouseos/NativeMetal) |
| Unisoc 设备 | NativeUfs | [N1709/NativeUfs](https://github.com/N1709/NativeUfs) |

## 规则

- 每个硬件驱动单独一个仓库。
- `UniXNU-<device>` 用 `drivers/<Name>` 子模块引用驱动。
- 官方机型或通用栈的驱动可以贡献到 UniXNU 组织。
- 其他驱动由维护者独立维护，以子模块链接。
- 每个驱动仓库都带 `unixnu` topic。
