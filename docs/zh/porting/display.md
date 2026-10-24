# 显示与亮度

先完成[平台与内存](/zh/porting/platform)。收集固件 GOP 地址、大小、分辨率、行跨度、
像素格式和保留内存范围，确认面板、显示控制器、背光控制器及其电源控制方。

## 1. 确认固件扫描输出

构建[集成后的 UEFI 固件](/zh/porting/uefi-firmware)，确认菜单显示正确。
将 GOP 参数和帧缓冲范围与固件内存映射、板级 DTS 对照，在内核交接期间保留当前扫描缓冲区。

**预期结果：** 固件画面稳定，颜色和尺寸正确。完成后再启用 XNU 显示后端。

## 2. 适配 XNU 显示后端

阅读 [GopFramebuffer](/zh/drivers/gop-framebuffer)及其 `NativeLegacyDisplay.cpp`、
`NativeGOPPresenter.cpp` 和 `NativeGOPWire.h`。它从 `PE_current_console` 获取帧缓冲，
并接入选定的 MobileFramebuffer 和 IOSurface ABI。

在 [sheng package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package) 中检查
`LegacyDisplayProbe.cpp` 与 `DisplayTimingProbe.cpp`。
设备脚本 `scripts/build-gop-legacy-display.py` 使用预链接候选内核验证 ABI 方法，再生成后期显示归档。
按[构建输入](/zh/porting/migration#build-inputs)提供匹配的候选内核和审计文件。

1. 选择 GopFramebuffer 和显示归档配方，保留所需依赖。
2. 启用配方需要的内核配置和初始化调用。sheng 使用 `CONFIG_XNU_NATIVE_LEGACY_DISPLAY=y`
   及相应的 `offline-legacy-*` 参数。
3. 在 CPU 呈现路径中确认像素布局、行跨度、分配大小和缓存处理，检查已知画面能否显示。
4. 确认交换完成通知与 IOSurface 生命周期后再启用 [GPU 呈现](/zh/porting/gpu)。
5. 按目标控制器适配时序读取。sheng 的 `sheng-dpu=1` 通过 `DisplayTimingProbe.cpp`
   采样 SM8550 DPU 和双 DSI 计数器。根据实际面板模式设置刷新策略；`sheng-vsync=120` 是 sheng 的请求值。

**预期结果：** XNU 发布显示服务、呈现画面并完成交换。分别记录请求刷新率和测得的帧时序。

## 3. 确认亮度控制接口

sheng 使用两个 I2C 地址为 `0x11` 的 KTZ8866，分别位于 GENI 引擎 `0xa84000` 和 `0x988000`。
`package/AppleARMBacklight.cpp` 实现 `ShengBacklight`，`GeniI2c.c` 实现传输状态机，
`BacklightProbe.cpp` 启动服务。

复用前记录目标设备的控制器数量、寄存器编码、亮度范围、I2C 引擎、从地址、引脚、时钟、中断和供电顺序。
使用 DSI 命令或 PWM 背光的设备，需要在面向 OS 的亮度服务下实现相应传输与亮度编码。

## 4. 适配并启用亮度

1. 更新板级引擎映射、时钟投票和中断路由。sheng 的 GENI 路径采用固件加载的引擎代码与时序；
   核对该交接状态，或实现目标所需初始化。
2. 实现控制器亮度写入与回读。sheng 将最大 2047 的 11 位值分写到 `0x04`、`0x05`，同时控制两个芯片。
3. 保留有超时的传输、取消和工作循环调度，确认错误与超时情况后再启用渐变调光。
4. 根据选定 OS ABI 配置 `IODisplayParameters` 亮度范围和 `backlight-control`。
   服务接受 brightness 与 `FadePeriod` 属性。
5. 构建服务和注册封装。sheng 的显示配方包含两者，`sheng-backlight=1` 启用 probe。

在解析后的构建树定位集成点：

```sh
rg -n 'kSeA|kSeB|kSpi|kMaxLevel|backlight-control|FadePeriod' kits/sheng/AppleARMBacklight.cpp
rg -n 'backlight|GeniI2c' scripts/build-gop-legacy-display.py
```

新设备使用对应 kit 路径，按[构建指南](/zh/building)重建内核、payload 和启动镜像。

**预期结果：** 最低、中间和最高设置有对应回读与可见亮度，渐变调光完成，超时计数稳定。
sheng 的 `native_backlight_stats` 记录发布、请求、回读、中断和失败。
控制器写入与系统 UI 亮度调整分别验证。

## 下一步

分别记录显示与亮度结果，再进入[触控和按键](/zh/porting/input)。
