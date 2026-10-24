# Driver repositories

Every hardware driver has its own repository. A device repository references the drivers it uses as submodules under `drivers/<Name>`. Every driver repository carries the `unixnu` topic.

## Official drivers

These repositories are maintained in the UniXNU organization.

| Driver | Repository | Layer | Page |
| --- | --- | --- | --- |
| NativeAdreno | [UniXNU/NativeAdreno](https://github.com/UniXNU/NativeAdreno) | XNU GPU | [NativeAdreno](/drivers/native-adreno) |
| NativeMetal | [UniXNU/NativeMetal](https://github.com/UniXNU/NativeMetal) | XNU Metal frontend | [NativeMetal](/drivers/native-metal) |
| NativeUfs | [UniXNU/NativeUfs](https://github.com/UniXNU/NativeUfs) | XNU storage | [NativeUfs](/drivers/native-ufs) |
| NativeHidInput | [UniXNU/NativeHidInput](https://github.com/UniXNU/NativeHidInput) | XNU input | [NativeHidInput](/drivers/native-hid-input) |
| NativeRamBlock | [UniXNU/NativeRamBlock](https://github.com/UniXNU/NativeRamBlock) | XNU storage | [NativeRamBlock](/drivers/native-ram-block) |
| NativeMobileDisplay | [UniXNU/NativeMobileDisplay](https://github.com/UniXNU/NativeMobileDisplay) | XNU display | [NativeMobileDisplay](/drivers/native-mobile-display) |
| NativeGeniSpi | [UniXNU/NativeGeniSpi](https://github.com/UniXNU/NativeGeniSpi) | XNU SPI | [NativeGeniSpi](/drivers/native-geni-spi) |
| NativeAES | [UniXNU/NativeAES](https://github.com/UniXNU/NativeAES) | XNU crypto | [NativeAES](/drivers/native-aes) |
| NativeKeyStoreEvents | [UniXNU/NativeKeyStoreEvents](https://github.com/UniXNU/NativeKeyStoreEvents) | XNU keystore | [NativeKeyStoreEvents](/drivers/native-keystore-events) |
| NativeVirtioBlock | [UniXNU/NativeVirtioBlock](https://github.com/UniXNU/NativeVirtioBlock) | XNU storage | [NativeVirtioBlock](/drivers/native-virtio-block) |
| NativeVirtioConsole | [UniXNU/NativeVirtioConsole](https://github.com/UniXNU/NativeVirtioConsole) | XNU console | [NativeVirtioConsole](/drivers/native-virtio-console) |
| NativeNovatekTouch | [UniXNU/NativeNovatekTouch](https://github.com/UniXNU/NativeNovatekTouch) | XNU touch | [NativeNovatekTouch](/drivers/native-novatek-touch) |
| NativePrelink | [UniXNU/NativePrelink](https://github.com/UniXNU/NativePrelink) | Kernel tooling | [NativePrelink](/drivers/native-prelink) |
| NativeWifi | [UniXNU/NativeWifi](https://github.com/UniXNU/NativeWifi) | XNU Wi-Fi | [NativeWifi](/drivers/native-wifi) |
| ArmCpu | [UniXNU/ArmCpu](https://github.com/UniXNU/ArmCpu) | XNU CPU and GIC | [ArmCpu](/drivers/arm-cpu) |
| ArmDTPlatform | [UniXNU/ArmDTPlatform](https://github.com/UniXNU/ArmDTPlatform) | XNU platform expert | [ArmDTPlatform](/drivers/arm-dt-platform) |
| ArmVirtPlatform | [UniXNU/ArmVirtPlatform](https://github.com/UniXNU/ArmVirtPlatform) | XNU platform | [ArmVirtPlatform](/drivers/arm-virt-platform) |
| ArmSmmu4K | [UniXNU/ArmSmmu4K](https://github.com/UniXNU/ArmSmmu4K) | XNU SMMU kit | [ArmSmmu4K](/drivers/arm-smmu4k) |
| AirNir | [UniXNU/AirNir](https://github.com/UniXNU/AirNir) | Compiler kit | [AirNir](/drivers/air-nir) |
| AdrenoPM4 | [UniXNU/AdrenoPM4](https://github.com/UniXNU/AdrenoPM4) | GPU command kit | [AdrenoPM4](/drivers/adreno-pm4) |
| AdrenoFirmware | [UniXNU/AdrenoFirmware](https://github.com/UniXNU/AdrenoFirmware) | GPU firmware kit | [AdrenoFirmware](/drivers/adreno-firmware) |
| DarwinBoot | [UniXNU/DarwinBoot](https://github.com/UniXNU/DarwinBoot) | UEFI loader | [DarwinBoot](/drivers/darwin-boot) |
| VerifiedRoot | [UniXNU/VerifiedRoot](https://github.com/UniXNU/VerifiedRoot) | Root trust tooling | [VerifiedRoot](/drivers/verified-root) |
| ShengPreflight | [UniXNU/ShengPreflight](https://github.com/UniXNU/ShengPreflight) | UEFI preflight | [ShengPreflight](/drivers/sheng-preflight) |

## Community drivers

These repositories are maintained by their owners. They are linked from the device repository that uses them.

| Driver | Device | Repository | Page |
| --- | --- | --- | --- |
| NativeMsdc | Benesse TAB-A04-BR3 (MT8167) | [mouseos/NativeMsdc](https://github.com/mouseos/NativeMsdc) | [NativeMsdc](/drivers/community-native-msdc) |
| NativeAdreno-myron | Redmi K90 Pro Max / Poco F8 Ultra | [zg7681/NativeAdreno-myron](https://github.com/zg7681/NativeAdreno-myron) | [NativeAdreno-myron](/drivers/community-native-adreno-myron) |
| NativeUfs-myron | Redmi K90 Pro Max / Poco F8 Ultra | [zg7681/NativeUfs-myron](https://github.com/zg7681/NativeUfs-myron) | [NativeUfs-myron](/drivers/community-native-ufs-myron) |
| NativeAdreno-fuxi | Xiaomi 13 | [ljhhkx529/NativeAdreno-fuxi](https://github.com/ljhhkx529/NativeAdreno-fuxi) | [NativeAdreno-fuxi](/drivers/community-native-adreno-fuxi) |
| NativeUfs-alioth | Redmi K40 / Poco F3 | [FFT-infra/NativeUfs-alioth](https://github.com/FFT-infra/NativeUfs-alioth) | [NativeUfs-alioth](/drivers/community-native-ufs-alioth) |
| unixnu-nativeufs | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativeufs](https://github.com/RigoLigoRLC/unixnu-nativeufs) | [unixnu-nativeufs](/drivers/community-unixnu-nativeufs) |
| unixnu-nativemobiledisplay | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativemobiledisplay](https://github.com/RigoLigoRLC/unixnu-nativemobiledisplay) | [unixnu-nativemobiledisplay](/drivers/community-unixnu-nativemobiledisplay) |
| unixnu-nativeramblock | Galaxy Tab S7 FE Wi-Fi | [RigoLigoRLC/unixnu-nativeramblock](https://github.com/RigoLigoRLC/unixnu-nativeramblock) | [unixnu-nativeramblock](/drivers/community-unixnu-nativeramblock) |

## Device forks

These repositories are device forks that track an official driver. They are linked from their device repositories and do not have a separate driver page.

| Device | Driver | Repository |
| --- | --- | --- |
| Sony Xperia 1 II | NativeAdreno | [chardidathing/NativeAdreno](https://github.com/chardidathing/NativeAdreno) |
| Xiaomi Pad 7 | NativeAdreno, NativeMobileDisplay | [Allenwdk/NativeAdreno](https://github.com/Allenwdk/NativeAdreno), [Allenwdk/NativeMobileDisplay](https://github.com/Allenwdk/NativeMobileDisplay) |
| Nothing Phone (3a) | NativeAdreno, NativeUfs | [rootstockos/NativeAdreno](https://github.com/rootstockos/NativeAdreno), [rootstockos/NativeUfs](https://github.com/rootstockos/NativeUfs) |
| OnePlus 6T | NativeAdreno, NativeMetal | [johnpanos/NativeAdreno](https://github.com/johnpanos/NativeAdreno), [johnpanos/NativeMetal](https://github.com/johnpanos/NativeMetal) |
| Xiaomi 13 Pro | NativeUfs | [pubglite55/NativeUfs](https://github.com/pubglite55/NativeUfs) |
| OnePlus 5 | NativeUfs | [lss717/NativeUfs](https://github.com/lss717/NativeUfs) |
| QEMU virt | NativeAdreno, NativeMetal, NativeMobileDisplay, NativeUfs | [xVanTuring/NativeAdreno](https://github.com/xVanTuring/NativeAdreno), [xVanTuring/NativeMetal](https://github.com/xVanTuring/NativeMetal), [xVanTuring/NativeMobileDisplay](https://github.com/xVanTuring/NativeMobileDisplay), [xVanTuring/NativeUfs](https://github.com/xVanTuring/NativeUfs) |
| Benesse TAB-A04-BR3 | NativeMetal | [mouseos/NativeMetal](https://github.com/mouseos/NativeMetal) |
| Unisoc devices | NativeUfs | [N1709/NativeUfs](https://github.com/N1709/NativeUfs) |

## Rules

- One hardware driver per repository.
- `UniXNU-<device>` references each driver as a submodule under `drivers/<Name>`.
- A driver for an official device or the generic stack can be contributed to the UniXNU organization.
- Other drivers stay independently maintained and are linked as submodules.
- Every driver repository carries the `unixnu` topic.
