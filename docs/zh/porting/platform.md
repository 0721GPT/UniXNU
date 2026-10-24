# 平台、内存与 OS 服务

先完成[设备仓库解析](/zh/porting/)。阅读[设备树绑定](/zh/porting/device-tree)、
[匹配规则](/zh/drivers/device-tree-matching)、[ArmDTPlatform](/zh/drivers/arm-dt-platform)
和 [ArmCpu](/zh/drivers/arm-cpu)。

## 1. 描述 CPU 和中断资源

记录 CPU MPIDR affinity、启动 CPU、启用的核、PSCI 方法、GIC 版本、distributor/redistributor
范围与步长、定时器频率和中断描述。共享拓扑放在 SoC DTSI，板级启用状态放在设备 DTS。

使用选定 SoC 的 include 目录编译板级源码。以下示例使用 sheng 和已检出的 SM8550 common 仓库：

```sh
dtc -I dts -O dtb -i ../UniXNU-sm8550-common/dts \
  -o /tmp/sheng-hardware.dtb ../UniXNU-sheng/dts/sheng.dts
dtc -I dtb -O dts /tmp/sheng-hardware.dtb
```

将路径替换为目标设备，检查 `reg`、`ranges`、phandle、中断 provider 和禁用节点。
[ADT 转换](/zh/porting/device-tree)导出已启用硬件节点并提供 OS 属性。

**预期结果：** platform expert 发布预期 nub，启动核收到定时器中断，启用的次级核完成入口并收到 IPI。
使用其他 GIC 或 PSCI 接口时，启用更多 CPU 前核对 ArmCpu 实现。

## 2. 核对内存和 DMA

对照固件 RAM、安全保留区、帧缓冲、DSP/GPU 固件、payload 和内核分配。
硬件 DTS 使用连续交接 RAM 范围；加载器拥有的内存请求有独立大小与对齐约束。
两者都应与实际 UEFI 内存映射一致。

对每个 DMA 设备记录地址宽度、物理地址到 IOVA 的映射、SMMU 实例/stream/context 所有权和缓存策略。
目标匹配其转换模型时阅读 [ArmSmmu4K](/zh/drivers/arm-smmu4k)。
GPU、UFS、SPI 和 PCIe 可使用不同 stream 与分配策略。核对消费端源码，sheng 的多条板级路径直接读取 C/C++ 常量。

**预期结果：** 每项交接分配有明确所有者，各外设在提交任务前能访问其 DMA 范围。
随后按 [UFS 适配](/zh/porting/ufs)处理存储控制器。

## 3. 接入块设备和 OS 服务 {#block-and-os-services}

| 组件 | 适配工作 | 预期观测 |
| --- | --- | --- |
| [RamBlock](/zh/drivers/ram-block) | 匹配 dense/packed Data 格式与 ADT 占位符，为目标存储策略实现 `native_ufs_store` 持久化回调 | Data 挂载，写入和重启行为符合选定持久化模式 |
| [VirtioMmioBlock](/zh/drivers/virtio-mmio-block) | 提供 VirtIO MMIO 设备、队列、DMA 和中断路由，确认存储 ABI 绑定 | 挂载根卷前确认容量和读取正确 |
| [VerifiedRoot](/zh/drivers/verified-root) | 提供选定镜像的 owner policy 与信任输入，接入存储策略归档 | 目标镜像通过策略验证 |
| [SoftwareAES](/zh/drivers/software-aes) | 匹配 XNU AES 辅助接口与 IOCryptoAcceleratorFamily ABI，为 OS profile 构建绑定 | 加密服务发布，需要的卷操作完成 |
| [AppleKeyStoreState](/zh/drivers/apple-key-store-state) | 根据 OS 确认 selector 和状态回调，接入初始化与事件调用方 | 所需状态查询与通知完成 |
| [XnuPrelink](/zh/drivers/xnu-prelink) | 选择匹配的 XNU、原始 kernelcache、归档绑定和补丁顺序 | 预链接解析选定驱动并生成内核 |
| [DarwinBoot](/zh/drivers/darwin-boot) | 将加载器加入固件或 payload，提供 Mach-O、ADT 和所需 sidecar | 加载器验证输入并报告内核交接 |
| [ShengPreflight](/zh/drivers/sheng-preflight) | 阅读 sheng 检查，再为目标固件和内存布局编写检查 | 目标固件输入通过对应检查 |

所需产物见[预链接](/zh/concepts/prelink)、[完整性策略](/zh/concepts/security)、
[payload](/zh/concepts/payload)和 [userdata](/zh/guide/userdata)。
根卷 UUID、分区布局、Data 持久化和日志分配保留在设备配置中，OS profile 提供对应版本的 ABI。

## 4. 检查组装后的选择

从主仓库执行：

```sh
./unixnu status --json
./unixnu build kernel
```

构建前检查依赖顺序、配置片段和补丁序列。每项归档需要对应 OS 输入及初始化调用方。
[内核指南](/zh/guide/build-kernel)说明构建输出和失败处理。

在[移植记录](/zh/porting/records)中分别记录源码/构建证据与设备结果。
接下来完成[存储](/zh/porting/ufs)，再进入[显示与亮度](/zh/porting/display)。
