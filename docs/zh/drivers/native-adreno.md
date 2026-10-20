# Adreno 740 驱动

sheng 通过原生 XNU 服务驱动 Adreno 740。图形栈使用原生 4 KiB SMMU 翻译、GMU 固件、CP ring、真实 fence 和 GPU 到 scanout 的复制。

## 运行时链路

```mermaid
flowchart LR
  A[iPadOS 应用] --> B[QuartzCore 和 RenderBox]
  B --> C[NativeMetal 插件]
  C --> D[metal2vulkan]
  D --> E[Turnip]
  E --> F[NIR 和 IR3]
  F --> G[Darwin KMD]
  G --> H[NativeAdreno]
  H --> I[GMU、CP、SMMU 和 ring]
  I --> J[Adreno 740]
  J --> K[GPU blit 到 scanout]
  K --> L[NativeMobileDisplay]
```

1. NativeMetal 通过系统 `MTLAddDevice` 发布 Metal 设备，管理 Metal 对象、编码器、IOSurface、event、fence 和 pipeline archive。
2. metal2vulkan 把 AIR 翻译成 SPIR-V，并给出 pipeline 描述、binding 计划和 function constant。
3. Turnip 把 SPIR-V 经过 NIR 编译成 A740 IR3。Darwin KMD 通过 NativeAdreno user client 访问内核。
4. NativeAdreno 管理 BO、GPU 地址、提交 ring、fence 和 SMMU context。GMU 让 GPU 退出 reset 并提交功耗票，CP 执行 IB。
5. 插件把完成帧 blit 到 scanout 页，NativeMobileDisplay 提供并回收 swap。

运行时编译链路是 Metal、AIR、SPIR-V、NIR、IR3。AirNir 是离线的 AIR 到 NIR 到 IR3 编译器，不是部署路径。Apple 的 `applegpu_g9g` 切片是 Apple GPU 代码，不能在 Adreno 上运行。

## 组件

| 组件 | 作用 |
| --- | --- |
| `drivers/NativeMetal` | Metal 前端、metal2vulkan、命令编码、IOSurface 和 scanout client |
| `drivers/NativeAdreno` | GPU 服务、GMU/CP 启动、user client、BO、VM、ring、提交和 fence |
| `drivers/NativeMobileDisplay` | swap 提供、校验、回收和 scanout hand-off |
| `kits/AdrenoFirmware` | GMU HFI、启动、功耗票和 SQE 固件数据 |
| `kits/AdrenoPM4` | A740 PM4 状态、常量、VPC 和 rasterizer 包 |
| `kits/ArmSmmu4K` | 原生 4 KiB 表、context、stream、live mapping 和 TLB 维护 |
| `kits/AirNir` | 离线 AIR 到 NIR 到 IR3 编译器 |

## 最新提交，从新到旧

| 日期 | 提交 | 变化 |
| --- | --- | --- |
| 2026-10-20 | NativeMetal `7cd766b` | 折叠 constant-global 字段加载。Uber fragment SPIR-V 从 915584 降到 11212 字节；60 个 recipe 从 36.0 s 降到 6.5 s；58/60 输出逐字节相同。 |
| 2026-10-20 | NativeMetal `9a0a771` | 把 pipeline cache 导出分片写入 256 MiB 板级日志区。 |
| 2026-10-20 | NativeMetal `9b1e514` | 导出 render server pipeline cache，并在设备打开前导入 `m2vpack`。导出是增量的。 |
| 2026-10-20 | NativeMetal `364701a` | 验证 iOS priming cache 和全新镜像的首次消费者。 |
| 2026-10-20 | NativeAdreno `0018649` | 把 fence wait 移出连接锁，并通过方法 `0x7100` 转发 cache 导出分片。 |
| 2026-10-20 | NativeAdreno `12439ae` | 用离线 peer 验证同一 iOS 驱动的 pipeline cache。 |
| 2026-10-20 | NativeAdreno `9a45113` | 在 GPU mapping 前索引跨 class 物理 alias。 |
| 2026-10-20 | ArmSmmu4K `dea5eb6` | 用有界查找处理混合大小物理 alias。 |
| 2026-10-20 | UniXNU `c537282` | 应用翻译器折叠，重复 coherence probe，记录 int-150 到 int-152。 |
| 2026-10-20 | UniXNU `8472a96` | 板级日志 v3，通过 UFS 导出 pipeline cache，并在 Data seed 中合并 pack。 |
| 2026-10-20 | UniXNU `0d5357d` | 跟踪内部 Turnip 编译。设备启动在应用 pipeline 前编译 18 个 clear/blit 和 2 个 empty shader。 |

## 状态

| 步骤 | 结果 |
| --- | --- |
| GMU 冷启动和 HFI | 工作 |
| GPU context 和 CP 命令 | 工作 |
| 原生 4 KiB SMMU context | active |
| Metal 设备发现 | 工作 |
| GPU 帧复制到 scanout | 工作，像素一致 |
| GPU 合成桌面 | 工作，慢 |
| 稳定 120 FPS | 未达到 |

active GPU 4 KiB 证明、显示 vblank、IOSurface 格式约定和首次使用 shader 路径仍未完成。

## 着色器编译

驱动没有预编译的 Adreno UI shader pack，在设备上编译原始 UI shader：

1. `default.metallib` 带有 AIR 和 Apple GPU 机器码。Apple 代码不能在 Adreno 上运行。
2. metal2vulkan 把 AIR 翻译成 SPIR-V，Turnip 在 pipeline 创建时编译 IR3。
3. 冷启动会编译它遇到的每个 UI 变体。int-148：77 个 pipeline，约 43 s，cache hit 0。int-150：103 个 Metal pipeline，共 40.1 s，最大 1.40 s；94 个 Vulkan pipeline，共 23.3 s。
4. Apple 的预编译 A10X 变体不能复用，也没有厂商预编译的 Adreno pack。

已实现的缓解：

| 缓解 | 效果 |
| --- | --- |
| NDEBUG release 构建 | 去掉每 pass 后的 Mesa assertion 和 NIR 校验。 |
| metal2vulkan patch `0004` | 在 Vulkan 编译前特化 QuartzCore uber shader。 |
| priming cache | 缓存运行时启动 entry point 工作。 |
| `m2vpack` 导出和导入 | 复用之前编译的 pipeline cache；导出是增量的。 |
| Turnip builtin pack | 内嵌 clear、blit 和 empty shader 变体；不覆盖 UI 变体。 |

剩余开销是某个 UI 变体的第一次编译，且之前的启动没有导出过它。pack 复用前，cache key 必须包含翻译器身份。int-152 使用 patch `0004` 且没有 pipeline pack，所以每次编译都是冷的；结果是大幅改善，未彻底修复。

## 构建

```sh
python3 scripts/build-turnip-darwin.py --target ios
python3 scripts/check-turnip-pipeline-cache-host.py
python3 scripts/check-priming-cache-host.py --output build/priming-cache/run
python3 scripts/probe-turnip-internal-compiler-host.py --qualification <old> --output <new>
```

设备仓库增加原始输入检查：

```sh
python3 scripts/build-adreno-compiler-host.py --probe
python3 scripts/build-turnip-ios-compiler-peer.py
python3 scripts/check-turnip-ios-compiler-cache.py
python3 scripts/check-quartzcore-turnip-cache-host.py
```

## 相关

- [小米平板 6S Pro（sheng）](/zh/devices/sheng)
- [XNU 驱动](/zh/drivers/xnu-drivers)
- [4 KiB 页表](/zh/xnu/four-k-page-tables)
