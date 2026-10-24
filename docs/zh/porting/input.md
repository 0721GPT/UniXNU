# 触控与按键适配

先完成[显示几何参数](/zh/porting/display)和[中断/DMA 配置](/zh/porting/platform)。
记录触控芯片、固件、矩阵尺寸、物理方向、总线连接、复位/中断引脚及 OS HID ABI。

## 1. 选择传输与传感器协议

阅读 [QcomGeniSpi](/zh/drivers/qcom-geni-spi)、
[ShengNovatekTouch](/zh/drivers/sheng-novatek-touch)和 [ShengHidInput](/zh/drivers/sheng-hid-input)。

| 源码 | 需要适配的输入 |
| --- | --- |
| `QcomGeniSpi/GeniSpi.c` | GENI 引擎访问与 SPI 传输配置 |
| `ShengNovatekTouch/NvtProtocol.c` | 芯片地址表、固件格式和报告协议 |
| `ShengNovatekTouch/NvtTouchCore.c` | 传感器矩阵、过滤和接触跟踪 |
| sheng `package/TouchBoard.h` | 控制器地址、GPIO、时钟投票和 SMMU stream |
| sheng `package/TouchProbe.cpp` | 资源所有权、固件加载、帧循环和坐标 |
| `ShengHidInput/NativeHidInput.cpp` | 接触/按键到 IOHIDFamily 报告 |

不同传感器需要实现其识别、固件和报告协议。复用 Novatek 协议但更换总线控制器时，
实现 `nvt_bus` 所需的传输与延时回调。

## 2. 适配板级路径

1. 根据目标 DTS 和时钟/引脚定义替换 `TouchBoard.h` 输入。sheng 使用 QUP wrapper `0xac0000`、
   引擎 `0xa90000`、最高 19.2 MHz 的 SPI mode 0、SPI GPIO 44 至 47、中断 GPIO 25 和 SMMU stream `0xa3`。
2. 实现电源/复位控制，确认引擎固件、引脚和时钟。
3. 按目标 SMMU context 设置 DMA 寻址和缓存维护。
4. 读取芯片 ID，再加载固件并确认返回的几何参数。
5. 解码完整帧，将坐标变换到显示方向。
6. 检查稳定接触 ID、多点、边缘行为和抬起事件。

Novatek 处理核心使用 40 × 60 矩阵。修改固件或处理核心时核对几何尺寸和边界。
固件提取由设备的 `scripts/gen-touch-firmware.py` 与私有文件清单管理。

sheng 的 `sheng-touch=1`、`2`、`3`、`4` 分别选择资源检查、时钟/引脚、控制器识别和固件/扫描。
先适配该阶段使用的硬件参数，再启用相应阶段。

**预期结果：** 得到显示坐标系中的一致接触帧，包含抬起事件。
HID 层通过序列计数器读取 `native_touch_*`；替换生产方时保留快照一致性。

## 3. 发布 HID 报告

使用选定 kernelcache 确认 IOHIDFamily 方法和对象布局。
sheng 归档配方使用预链接候选内核与 ABI 记录，见[构建输入](/zh/porting/migration#build-inputs)。
按传感器适配报告描述符、逻辑坐标范围和接触数量上限。

在完整组件列表选择传感器、传输和 HID 组件。sheng 显示配方链接传感器与总线对象，
HID 配方绑定 OS 接口；`sheng-touch-hid=1` 启用触控发布路径。在目标构建树检查：

```sh
rg -n 'TouchProbe|GeniSpi|NvtProtocol|NvtTouchCore|NvtFirmware' scripts/build-gop-legacy-display.py
rg -n 'native_touch_|ReportDescriptor' drivers/ShengHidInput/NativeHidInput.cpp
```

**预期结果：** OS 收到各接触点正确的按下、移动和抬起报告，UI 手势与面板方向一致。

## 4. 适配实体按键

阅读 sheng 的 [ButtonProbe.cpp](https://github.com/UniXNU/UniXNU-sheng/blob/main/package/ButtonProbe.cpp)。
电源和音量减使用 PMK8550 PON/RESIN，音量加使用 PM8550 GPIO 6；
SPMI arbiter 经 PDC 将中断路由到 GIC SPI 481。

映射目标 GPIO 或 PMIC 输入、极性、所有权、中断路由和应答操作。
仅访问当前执行环境拥有的 PMIC 外设。SPMI 控制器变化时适配 `BootResetCore.h` 辅助接口。
将按下/释放状态接入 HID consumer 报告。sheng 使用 report 2 的 Power、Volume Increment
和 Volume Decrement 位，参数 `sheng-buttons=1` 启用该路径。

**预期结果：** 每次按下和释放产生正确报告，包括长按和同时按键，中断活动保持有界。

## 下一步

按[构建指南](/zh/building)重建，分别记录触控与按键结果，再进入 [GPU 适配](/zh/porting/gpu)。
