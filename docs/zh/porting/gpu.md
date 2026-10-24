# GPU 与 Metal 适配

先完成[平台 DMA](/zh/porting/platform)和基础[显示呈现](/zh/porting/display)。
阅读 [Adreno740 运行路径](/zh/drivers/adreno740)与 [AdrenoMetal 接口](/zh/drivers/adreno-metal)。

## 1. 匹配 GPU 代际与 OS ABI

Adreno740 实现 A740 内核接口。AdrenoMetal 使用该接口、Turnip、metal2vulkan
和选定的 OS GPU/IOSurface ABI。确认目标 GPU 版本，对照寄存器、固件、命令包和内存接口。
其他 GPU 代际需要先实现对应内核后端和编译器支持，再选择前端。

另一块 A740 设备需要收集 GPU/GMU 寄存器、中断、SMMU context 与 stream ID、固件、
电源域、时钟/电压档位、互连投票、RAM 保留区和扫描输出布局，并与选定固件核对。

## 2. 适配电源和固件输入

阅读 [SM8550 平台源码](https://github.com/UniXNU/UniXNU-sm8550-common/tree/main/kits)
和 [sheng package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package)。
`Sm8550Platform` 提供共享 GMU 操作和 command database 处理，
`ShengGmuVotes.c` 与 `ShengGmuVotesSource.h` 提供板级 OPP 和带宽策略。
根据目标电源和 command database 适配这些输入。

sheng 的 `scripts/build-adreno740-kits.py` 嵌入以下本地输入：

| 固件 | 用途 |
| --- | --- |
| `qcom/gmu_gen70200.bin` | GMU 镜像 |
| `qcom/a740_sqe.fw` | 命令处理器固件 |
| `qcom/sm8550/sheng/a740_zap.mbn` | 板级签名 ZAP 镜像 |

配方读取 `.cache/firmware/sheng-gpu/`，核对 `scripts/data/adreno-inputs.json` 中的固定值。
按目标设备已确认的固件修改提取路径与固定值，在 `proprietary-files.txt` 中记录来源。

**预期结果：** 板级电源输入和固件匹配目标 GPU。提交命令前确认 GMU 启动和 HFI 响应。

## 3. 建立内存和命令提交路径

1. 核对 ArmSmmu4K context 所有权、stream 选择、地址宽度和缓存维护。
   sheng 在 GPU SMMU 上使用 GPU stream 0/1 与 GMU stream 5。
2. 适配保留内存和 GPU 虚拟地址窗口，确保扫描输出、固件、ring 和客户端分配位于各自拥有的范围。
3. 使用有界任务确认 CP 初始化、ring 推进和 fence 完成。
4. 渲染已知图像并检查结果，再运行 UI 工作负载。
5. 验证客户端退出、映射生命周期和 fence 失败处理。

使用 AdrenoFirmware 的固件/HFI 操作和 AdrenoPM4 支持的命令包编码。
sheng 配方把这些 kit、ArmSmmu4K 和板级投票链接到 `libAdreno740.a`，再通过
`build-adreno740-accelerator.py` 构建 accelerator。创建目标配方时保留该顺序。

**预期结果：** 命令完成、fence 推进、已知图像正确。GPU 结果与桌面呈现结果分别记录。

## 4. 构建并加入 Metal

准备匹配的 SDK、编译器依赖和 ABI 输入后，在目标构建树执行：

```sh
python3 scripts/build-turnip-darwin.py --target ios
python3 scripts/build-adreno-metal-plugin.py
python3 scripts/add-gpu-plugin-data.py --publish /path/to/base.dbr /path/to/gpu.dbr
```

加入插件的命令使用 macOS 镜像工具，生成 Data seed 副本。
构建 payload 时将输出传给 `--data-ramdisk`，再重建启动镜像。
如需指定应用进程，还可传入 `--clients NAME,...`。运行时插件名为 `NativeAdrenoMetal`。

核对所选 OS profile 的 sandbox、user-client 和 trust-cache 集成。
AirNir 是离线编译器 kit；运行时前端使用组件页说明的 AIR → SPIR-V → NIR → IR3 路径。

**预期结果：** Metal 设备发现、客户端连接、pipeline 创建、渲染和呈现分别成功。
随画面结果记录固件、编译器和插件版本，分别测量首次 pipeline 编译和缓存复用时间。

## 下一步

继续 [Wi-Fi](/zh/porting/network)，或返回[驱动清单](/zh/porting/drivers)。
