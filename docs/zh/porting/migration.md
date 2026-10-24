# 设备迁移

先读根目录 `AGENTS.md`、[设备仓库规范](/zh/porting/device-repository)、
[组件目录](/zh/reference/components)和[设备树规范](/zh/porting/device-tree)。
以下命令从 UniXNU 检出目录运行，带目录参数的命令使用指定目录。

迁移固件时，把 `UniXNURamDiskDxe` 及其文件系统依赖加入设备 DSC 和 FDF，再运行 `./unixnu build firmware --uefi ../uefi-<device>`。组装启动镜像前，完成[必需的固件集成](/zh/porting/uefi-firmware#required-integration)。

## 格式识别

```sh
python3 -c 'import pathlib,tomllib; p=pathlib.Path("../UniXNU-sheng/board.toml"); print(tomllib.loads(p.read_text()).get("schema_version", 1))'
git -C ../UniXNU-sheng status --short
git -C ../UniXNU-sheng submodule status --recursive
./unixnu status --json -D ../UniXNU-sheng > /tmp/sheng-before.json
```

省略 `schema_version` 时使用格式 1。值为 `2` 时使用配置层、组件构建命令、有序补丁
和按目标隔离的输出。保留本地改动，编辑前记录选中的源码版本。

## 仓库清单

逐项记录仓库 URL、提交、检出路径、本地改动、功能、运行时标识符、构建命令、输出
和使用者。检查 `.gitmodules`、`feeds.toml`、`local.toml`、archive 列表、shell 脚本、
CI、固件 package 和文档链接中的源码。

```sh
git -C ../UniXNU-sheng config -f .gitmodules --get-regexp '^submodule\..*\.(path|url)$'
rg -n 'Native|drivers/|kits/|archives|patches|make-.*-adt' ../UniXNU-sheng
```

逐个检查组件：

```sh
git -C <component> remote get-url origin
git -C <component> rev-parse HEAD
git -C <component> status --short
```

将 URL 与目录中的仓库身份对应。官方兼容输入解析为规范 ID。社区分支保留自己的
源码身份、名称和固定版本。在格式 2 中选择社区组件前，将其构建输入记录在维护的
组件目录条目中。

## 组件名称

| 官方兼容输入 | 规范 ID |
| --- | --- |
| `NativeAdreno` | `Adreno740` |
| `NativeMetal` | `AdrenoMetal` |
| `NativeUfs` | `ShengUfs` |
| `NativeHidInput` | `ShengHidInput` |
| `NativeNovatekTouch` | `ShengNovatekTouch` |
| `NativeGeniSpi` | `QcomGeniSpi` |
| `NativeMobileDisplay` | `GopFramebuffer` |
| `NativeAES` | `SoftwareAES` |
| `NativeKeyStoreEvents` | `AppleKeyStoreState` |
| `NativeRamBlock` | `RamBlock` |
| `NativeVirtioBlock` | `VirtioMmioBlock` |
| `NativeVirtioConsole` | `VirtioMmioConsole` |
| `NativePrelink` | `XnuPrelink` |
| `NativeWifi` | `Ath12kWifi` |

规范 ID 用于官方仓库 URL、检出目录、feeds、依赖、构建入口、archive、构建目录、
CI 和文档。保留 `runtime_identifiers` 记录的源文件名、运行时类名、bundle ID、
插件文件名、导出符号和协议 ID。

Adreno740 提供 A740 内核 GPU 接口，AdrenoMetal 提供 Metal 前端。元数据分别记录
A740/SM8550 实现范围和 sheng 验证结果。设备提供固件、电源策略、内存调整和显示集成。

## 字段归属

| 输入 | 目标 |
| --- | --- |
| ARM64 公共配置和补丁 | `profiles/arch/arm64/` |
| SoC 寄存器、中断、拓扑和共享操作 | `UniXNU-<soc>-common` |
| XNU/IPSW 版本、预链接根组件、OS ABI、版本修复 | `profiles/os/<id>/` |
| 板级连接、固件选择、电源策略、内存覆盖 | 设备 DTS 和 `board.toml` |
| 分区几何、package 和镜像参数 | 设备 `board.toml` 和 `package/` |
| 组件源码、依赖、构建命令和输出 | `components.toml` 与组件仓库 |
| ADT 脚本中的 CPU/GIC/计时器常量 | SoC DTSI，板级覆盖放在 DTS |
| Apple 身份和带类型的平台属性 | OS 的 `[adt]` 表 |
| 根卷 UUID 和 payload 内存映射输入 | 设备 `[payload.adt]` |
| 私有固件输入 | `proprietary-files.txt` 和本地 `.cache/firmware/` |

## 配置

1. 在 `board.toml` 顶层加入 `schema_version = 2`。
2. 在 `feeds.toml` 固定 `kind = "soc"` 的源码，用 `board.soc_source` 选择其名称。
3. 选择 `board.os_profile`，将 OS 设置放在对应配置中，板级设置留在 `board.toml`。
4. 在 `[components] include` 完整列出所需组件。添加组件前检查现有依赖。
5. 设置 `[kernel] config_fragments`，依次应用共享片段，再应用所选设备 defconfig。
6. 创建有序 `patches/xnu/series`，保留补丁前置关系，用 `status --json` 检查最终顺序。
7. 在 `[kernel] archives` 中用 `component:<ID>` 指定组件构建命令在辅助脚本之间的位置。
8. 更新脚本和 CI，使用解析器提供的输出目录和最终板级数据。

配置层按 ARM64、SoC、OS、设备顺序合并表。`include`、`config_fragments`、
`patch_series` 和 `archives` 等数组整体替换。series 每行是相对补丁路径或带配置/组件
前缀的路径。每个组件构建命令按依赖顺序运行一次。显式 archive 计划覆盖所有选中的
内核阶段构建命令。

SM8550 配置依次应用 ARM64、SoC、OS 和设备补丁序列。OS 序列在 SoC 接口补丁
之后应用启动进度集成。设备序列包含板级补丁。模板继承公共内核归档构建命令，
并选择所需的平台组件。

## DTS 转换

1. 阅读当前 ADT 生成器和驱动读取的属性。
2. 将已确认的 SoC 拓扑、寄存器范围、中断和引用放入 SoC DTSI。
3. 板级 DTS 包含 DTSI，添加内存、启用的 CPU、外设和保留区域。
4. 用 `unixnu,adt-name` 标记导出到 XNU 的硬件节点，保持运行时匹配使用的节点名和 compatible。
5. 声明父节点 cell 宽度、`ranges`、中断提供者和引用参数数量。
6. 将带类型的 Apple 属性放在 OS 配置，根卷 UUID 和 payload 选项放在 `[payload.adt]`。
7. 选择 `[device_tree] source` 和 `include_dirs`，用 `dtc` 编译，并按驱动要求核对生成的 ADT 属性。

共享生成器添加 NVRAM、熵、平台身份、根卷匹配和 payload 交接属性。
参见[硬件绑定](/zh/porting/device-tree#硬件绑定)。

## sheng 示例

完整输入由以下仓库维护：

| 输入 | 路径 |
| --- | --- |
| SoC | [UniXNU-sm8550-common](https://github.com/UniXNU/UniXNU-sm8550-common)，`soc.toml`、`configs/`、`dts/`、`patches/`、`kits/` |
| 设备 | [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng)，`board.toml`、`feeds.toml`、`configs/sheng_defconfig`、`dts/sheng.dts`、`patches/xnu/series`、`proprietary-files.txt`、`package/` |
| OS | `profiles/os/ipados-17.7.11-21H461/profile.toml` |
| 固件 | [uefi-sheng](https://github.com/UniXNU/uefi-sheng) |

设备选择 `sm8550` 和 `ipados-17.7.11-21H461`。`feeds.toml` 固定 SoC 版本，解析器
从该检出读取 `soc.toml`。板级配置选择 Adreno740、AdrenoMetal，以及存储、输入、
framebuffer 和打包组件。CPU 拓扑和控制器资源来自 SoC DTSI，板级 DTS 提供型号、
内存、CPU 选择和外设策略。

```sh
git clone --recurse-submodules https://github.com/UniXNU/UniXNU-sheng ../UniXNU-sheng
git clone https://github.com/UniXNU/uefi-sheng ../uefi-sheng
./unixnu defconfig sheng_defconfig -D ../UniXNU-sheng
./unixnu fetch
./unixnu status --json > /tmp/sheng-resolved.json
./unixnu tree
./unixnu ipsw fetch
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-sheng
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-sheng --dtb <firmware.dtb>
```

已有检出可直接使用。根据设备输入清单提供平台种子、trust cache、Data seed、固件
文件和固件 DTB。本地开发 SoC 时增加
`--local-module soc:sm8550=../UniXNU-sm8550-common`，状态报告会标出本地覆盖。

`out/sheng/ipados-17.7.11-21H461/sheng/` 下的预期输出：

| 路径 | 结果 |
| --- | --- |
| `.config` | 合并后的配置 |
| `tree/.cache/xnu-work/` | 目标独立的补丁后 XNU |
| `kernel/kernel.macho` | 预链接内核 |
| `device-tree/hardware.dtb` | 编译后的硬件树 |
| `payload/` | 包含 `Payloads/kernel.macho.adt` 的 payload 镜像 |
| `boot/boot.img` | BootShim、固件、固件 DTB 和 payload |
| `logs/` | 构建日志 |

检查 `/tmp/sheng-resolved.json` 的配置根目录、组件范围、源码版本、模块来源、补丁
顺序和输出路径。[sheng 页面](/zh/devices/sheng)列出板级输入和硬件检查点。

## 构建输入

sheng 的 archive 和镜像步骤需要：

- 含 iPhoneOS SDK 的完整 Xcode。
- `storage-abi/interface-audit.json` 及匹配的存储 ABI 输入。
- HID 和显示构建器使用的已确认 `build/xnu-prelink/*candidate.macho` 及其 JSON ABI 记录。
- 所选配置需要的 VerifiedRoot 和 verified VirtIO 策略 archive。
- `proprietary-files.txt` 列出的板级固件，包括 GPU 固件。
- Metal 插件使用的 `.cache/mesa-work`、已构建的 Turnip 和 metal2vulkan 输入。
- 所选 IPSW 成员、平台种子、trust cache、Data seed 和固件 DTB。

在迁移报告中记录各输入的来源和版本。缺少输入时，依赖它的构建步骤记为待完成。
内核、payload 和启动镜像完成状态以实际输出为准，设备验证使用这些具体镜像。

## 验证报告

```text
设备：
配置和 OS profile：
仓库版本与本地覆盖：
所选组件与板级输入：
补丁顺序：

主机检查：
  命令 | 结果 | 输出或日志
  依赖解析：
  DTS 和 ADT 生成：
  内核与预链接：
  Package 和固件：
  Payload 和启动镜像：
  QEMU 检查点：
  文档构建与链接：

硬件：
  镜像和设备：
  启动：
  存储：
  显示：
  输入：
  GPU：

待补输入：
  所需值或文件 | 来源或维护者 | 受影响步骤
```

无法执行的硬件检查记为待验证。构建完成与设备观察分别记录。
用最终名称和命令更新中英文文档、根目录 README、根目录 `AGENTS.md`、组件元数据
和固件引用。

## 结构化适配记录

复制模板中的 `porting.toml` 和 `.github/workflows/porting-record.yml`。按实际代码
为每个部分填写硬件、配置、操作、经验、完成条件和证据，并覆盖所选组件及依赖。
把板级驱动子模块放入设备仓库。运行记录检查，将 `Porting record` 设为官方设备分支
的必需检查，并更新 UniXNU `devices.toml` 的发布版本。设备页由这份记录生成。
详见[记录指南](/zh/porting/records)和 [UFS 指南](/zh/porting/ufs)。
