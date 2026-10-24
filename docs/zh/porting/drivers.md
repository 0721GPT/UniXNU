# 适配驱动

完成[设备仓库创建与解析](/zh/porting/)后使用本清单。先准备平台服务，再逐项接入外设。
选择组件或复用板级代码前，阅读对应指南。

## 1. 定位实现

[组件目录](/zh/reference/components)列出组件范围和依赖。
sheng 的[板级配置](https://github.com/UniXNU/UniXNU-sheng/blob/main/board.toml)选择组件和归档配方。
其 [package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package)还包含亮度、按键、
无线板级访问、DSP 服务和电源控制，通过设备脚本参与构建。

从主仓库查看选定来源：

```sh
./unixnu status --json
./unixnu tree
```

组件构建命令在输出所示的目标构建树中执行。`drivers/` 和 `kits/` 包含解析后的源码，
原设备 package 链接到 `kits/<board.kit>`。

## 2. 按硬件依赖顺序操作

| 顺序 | 能力 | 适配指南 | 需要检查的实现 |
| --- | --- | --- | --- |
| 1 | CPU、中断、定时器、RAM、DMA | [平台与内存](/zh/porting/platform) | ArmDTPlatform、ArmCpu、ArmSmmu4K、SoC DTSI |
| 2 | 存储、根卷、持久 Data | [UFS](/zh/porting/ufs)与[块设备和根卷集成](/zh/porting/platform#block-and-os-services) | ShengUfs、RamBlock、VirtioMmioBlock、VerifiedRoot |
| 3 | 帧缓冲、刷新、亮度 | [显示与亮度](/zh/porting/display) | GopFramebuffer、DisplayTimingProbe、AppleARMBacklight、GeniI2c |
| 4 | 触控与实体按键 | [输入](/zh/porting/input) | QcomGeniSpi、ShengNovatekTouch、ShengHidInput、ButtonProbe |
| 5 | GPU 与 Metal | [GPU](/zh/porting/gpu) | Adreno740、AdrenoMetal、Sm8550Platform、AdrenoFirmware、AdrenoPM4、AirNir |
| 6 | Wi-Fi | [网络](/zh/porting/network) | Ath12kWifi、WifiProbe、WifiNet、MhiBoot |
| 7 | DSP、音频、电池、CPU 调频、平台电源 | [DSP 与电源](/zh/porting/power) | AdspProbe、AudioProbe、ShengBattery、CpuFreqProbe、ShengPlatformPower |
| 8 | OS 服务和打包 | [OS 集成](/zh/porting/platform#block-and-os-services)与[固件](/zh/porting/uefi-firmware) | SoftwareAES、AppleKeyStoreState、XnuPrelink、DarwinBoot、ShengPreflight |

指南说明 sheng 当前实现，以及适配其他设备所需的输入。更换 SoC 时，需要在选定接口下实现
对应控制器、电源和 DMA 操作。更换 OS 构建时，需要使用该版本 kernelcache 重新确认私有 ABI 绑定。

## 3. 接入构建

1. 将组件 ID 加入完整的 `[components] include`，在 `status` 中查看展开的依赖。
2. 按依赖顺序加入归档配方，核对目录中的 `build` 项和设备 `[kernel] archives`。
3. 接入板级操作、固件提取和初始化调用，检查 probe 调用位置及控制调用方的配置项。
4. 驱动读取 DTS 属性时加入对应资源。使用 C++ 地址常量的驱动还需要修改源码。
5. 构建内核，再为当前阶段重建 payload 和启动镜像。按[构建指南](/zh/building)与
   [启动参数流程](/zh/xnu/boot-arguments#apply-arguments)操作。

sheng 的 `scripts/build-gop-legacy-display.py` 还组装触控、亮度、按键、Wi-Fi、DSP 和电池对象。
复用该配方需要相应板级源码与 ABI 输入。将子系统拆成独立配方时，核对其源码列表。

## 4. 记录结果

每项能力在 `porting.toml` 中记录源码版本、固件、启动参数、命令、产物路径、最后成功阶段和结果。
按[完成级别](/zh/porting/records#completion-levels)区分源码检查、主机检查和设备观测。

输入缺失时，记录需要执行的下一步。确认前置条件后再进入下一设备阶段。
返回[启动验证](/zh/porting/bring-up)集成已完成的路径。
