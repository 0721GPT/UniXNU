# 设备仓库

先完成[移植设备的第 1 至 4 步](/zh/porting/)，再用本页查阅字段，随后继续[硬件与驱动适配](/zh/porting/drivers)。

设备仓库选择共享 SoC 与 OS 支持，并描述一块板。添加驱动前阅读[组件目录](/zh/reference/components)。

## 布局

```text
UniXNU-<device>/
  AGENTS.md
  README.md
  board.toml
  porting.toml
  .github/workflows/porting-record.yml
  feeds.toml
  configs/<device>_defconfig
  dts/<device>.dts
  patches/xnu/series
  proprietary-files.txt
  package/
  local.toml
  unixnu.lock
```

`local.toml` 保存本地覆盖，`unixnu fetch` 将源码版本写入 `unixnu.lock`。组件源码
来自目录、feeds 或本地检出。`package/` 在构建树中成为 `kits/<board.kit>`。

## 配置层

```toml
schema_version = 2

[board]
name = "sheng"
soc = "SM8550"
soc_source = "sm8550"
os_profile = "ipados-17.7.11-21H461"
kit = "sheng"

[components]
include = ["ArmDTPlatform", "ArmCpu", "Adreno740", "AdrenoMetal", "ShengUfs", "ShengHidInput", "ShengNovatekTouch", "GopFramebuffer"]

[device_tree]
source = "device:dts/sheng.dts"
include_dirs = ["soc:dts"]
```

解析器按以下顺序读取：

1. `UniXNU/profiles/arch/arm64/profile.toml`
2. 所选 SoC 仓库的 `soc.toml`
3. `UniXNU/profiles/os/<os_profile>/profile.toml`
4. 设备 `board.toml`

表按键合并，普通数组替换继承数组。组件依赖按构建顺序补全。`unixnu status --json`
显示最终设置、源码、覆盖项和补丁顺序。

省略 `schema_version` 的文件使用格式 1 及其模块路径。格式 2 的输出位于
`out/<device>/<os_profile>/<config>/`，其中 `<config>` 为去掉 `_defconfig` 的配置名。

## 有序配置和补丁

```toml
[kernel]
defconfig = "sheng_defconfig"
config_fragments = ["arch:configs/common.config", "soc:configs/sm8550.config"]
patch_series = ["arch:patches/xnu/series", "soc:patches/xnu/series", "os:patches/xnu/series", "device:patches/xnu/series"]
```

配置片段按列表顺序应用，随后应用所选设备 defconfig。同名设置使用后面的值。
series 每行列出一个补丁，按行序应用，`#` 开始注释。相对路径从 series 所在目录
解析。`arch:`、`soc:`、`os:` 和 `device:` 指向对应配置根目录。
`component:<ID>/path` 指向组件文件。内核构建将解析后的补丁序列应用到目标独立的 XNU 源码。

SoC 源码在设备 `feeds.toml` 中固定：

```toml
[[source]]
name = "sm8550"
kind = "soc"
git = "https://github.com/UniXNU/UniXNU-sm8550-common"
commit = "<40-character SoC commit>"
```

`board.soc_source` 选择该源码的名称，`board.os_profile` 选择 UniXNU 的 OS 配置目录。
`proprietary-files.txt` 记录固件文件、来源和哈希。格式 2 的 `[device_tree]` 选择硬件
DTS，共享生成器结合 `[payload.adt]` 输入和 OS 身份生成 ADT。

## board.toml

### 板级

```toml
[board]
name = "<device>"
soc = "<SoC>"
description = "<device description>"
kit = "<kit>"
# example = true
```

| 键 | 含义 |
| --- | --- |
| `name` | `out/` 下的输出目录名 |
| `soc` | 报告和脚本使用的 SoC 标识 |
| `description` | 可读描述 |
| `kit` | 设备 package 在 `kits/` 下的目录名 |
| `example` | 标记示例仓库，`build` 和 `bootimg` 会拒绝 |

### IPSW

```toml
[ipsw]
url = "https://example.invalid/restore.ipsw"
product = "<product>"
version = "<version>"
build = "<build>"
device_class = "<device class>"
chip_id = 0x0000
board_id = 0

fetch = ["KernelCache", "DeviceTree", "StaticTrustCache", "Cryptex1,SystemTrustCache"]

[ipsw.sha256]
"<member file>" = "<sha256>"
```

`url` 是默认来源，`--ipsw` 或 `UNIXNU_IPSW` 覆盖。镜像必须匹配产品、版本、build、device class、chip ID 和 board ID。

`fetch` 列出 `unixnu ipsw fetch` 无参数时提取的组件。`[ipsw.sha256]` 按成员文件名固定哈希。

### userdata

```toml
[userdata]
first_lba = 0x0
last_lba = 0x0
lba_bytes = 4096
windows = [
  { name = "OS", component = "OS" },
  { name = "SystemOS", component = "Cryptex1,SystemOS" },
]
```

`first_lba` 和 `last_lba` 来自名为 `userdata` 的 GPT 条目，`lba_bytes` 是逻辑块大小。每个窗口指定 IPSW 磁盘镜像组件。

### 内核

```toml
[kernel]
defconfig = "<name>_defconfig"
archives = [
  "build-software-aes-bindings.py",
  "build-libpthread-kernel.py",
  "build-corecrypto-14.py",
  "build-corecrypto-14.py --namespace",
]

[kernel.prelink]
tool = "kits/XnuPrelink/emit_prelink.py"
kernel = "build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837.unstripped.noctf"
kernelcache_sha256 = "..."
slide = "-0x1000000"
early_security_init = "com.apple.driver.AppleMobileFileIntegrity=0xfffffff005ab4ae0"
roots = [
  "com.apple.driver.AppleARMPlatform",
  "com.apple.filesystems.apfs",
]
extra_args = []
```

| 键 | 含义 |
| --- | --- |
| `defconfig` | `configs/` 下的文件名 |
| `archives` | 静态库脚本，在链接内核前按顺序执行 |
| `tool` | 构建树中的预链接工具 |
| `kernel` | 构建树中的原生内核 |
| `kernelcache_sha256` | 固定原始 kernelcache |
| `slide` | 选中组件的地址 slide |
| `early_security_init` | 早期初始化器的组件标识和地址 |
| `roots` | 保留的原始 kernelcache bundle identifier |
| `extra_args` | 预链接工具的额外参数 |

### 固件

```toml
[firmware]
config = "<name>"
dsc = "Platforms/<Vendor>/<device>Pkg/<device>.dsc"
fd = "Build/<device>Pkg/{target}_CLANGPDB/FV/SILICIUM_UEFI.fd"
preflight = ["scripts/check-<device>.py"]
submodules = []
patches = ["Resources/MuPatches/<patch>.patch"]
packages = [
  "{uefi}",
  "{tree}/package",
  "{tree}/drivers/DarwinBoot",
  "{edk2}",
]

[firmware.defines]
DEVICE_MODEL = "<device>"
```

| 键 | 含义 |
| --- | --- |
| `config` | Mu-Silicium 中的 `Resources/Configs/<name>.toml` |
| `dsc` | 相对仓库根目录的 DSC |
| `fd` | 固件卷路径，`{target}` 替换为 `DEBUG` 或 `RELEASE` |
| `preflight` | 构建树中的预检脚本 |
| `submodules` | 板级需要的 Mu-Silicium 子模块 |
| `patches` | 构建期间应用到共享 edk2 树、之后还原的补丁 |
| `packages` | `PACKAGES_PATH` 条目，`{uefi}`、`{edk2}`、`{tree}` 会被替换 |
| `defines` | EDK II 额外 `-D` 定义 |

### 启动镜像

```toml
[bootimg.image]
base = 0x0
kernel_offset = 0x8000
tags_offset = 0x1e00000
pagesize = 4096
header_version = 0

[[bootimg.dtb_node]]
path = "/reserved-memory/<region>"
props = [
  { name = "label", type = "s", value = "<label>" },
  { name = "reg", type = "x", value = ["0x0", "0x80000000", "0x0", "0x100000"] },
]
```

镜像参数默认来自 Mu-Silicium 设备配置，`[bootimg.image]` 覆盖。每个 `[[bootimg.dtb_node]]` 在设备树追加到启动镜像前加入节点或属性。原有 bootloader 有时需要 splash 或 reserved-memory 节点。

### 负载

```toml
[payload]
volume = "<label>"
size_mib = 192
loader = "firmware"
title = "UniXNU"
dram_size = "0x80000000"
panic_screen = true
skip_setup_tool = "scripts/seed-data-skip-setup.py"
trust_cache_sha256 = ["<sha256>"]
timeout = 3

kernel_args = ["-v", "serial=3"]

[payload.adt]
chip_id = "0"
owned_memory_size = "0x10000000"
root_match_volume_uuid = "REQUIRED_ROOT_VOLUME_UUID"
cpu_regs = "0,0x100"
psci_method = "smc"
packed_data_ramdisk = true
ipad_identity = true
unique_chip_id = "required"
```

| 键 | 含义 |
| --- | --- |
| `volume` | FAT 卷标，最多 11 字符 |
| `size_mib` | 卷大小 |
| `loader` | DarwinBoot 在固件卷中时用 `firmware` |
| `title` | 菜单标题 |
| `dram_size` | 传给 ADT 工具的 DRAM 大小 |
| `panic_screen` | 把 panic 屏幕打进负载内核副本 |
| `entry_probe_reset_*` | 保留入口探针的定时复位，可改到某个调用之后（见[调试](/zh/guide/debugging)） |
| `skip_setup_tool` | `--skip-setup` 使用的脚本 |
| `skip_setup_args` | 传给该脚本的额外参数，例如 `["--no-auto-dim"]` |
| `trust_cache_sha256` | 接受的 trust cache 哈希 |
| `timeout` | 菜单超时 |
| `kernel_args` | 默认内核命令行 |

`--owner-trust-cache` 传入 owner 签名的包，包含原始缓存和额外模块。它替换 `--trust-cache`，并跳过 `trust_cache_sha256`。

格式 2 使用 `[device_tree]` 和共享 ADT 生成器，`[payload.adt]` 提供 payload 输入，OS 配置提供 Apple 身份。格式 1 使用声明的 `tool`。参见[设备树](/zh/porting/device-tree)。

## defconfig

`configs/<name>_defconfig` 使用 Linux defconfig 语法：

```
CONFIG_XNU_ARM_DT_PLATFORM=y
CONFIG_XNU_ARMCPU=y
CONFIG_XNU_NATIVE_UFS=y
CONFIG_XNU_NATIVE_CS_HASH_CACHE=y
# CONFIG_XNU_ARMVIRT_PLATFORM is not set
CONFIG_XNU_CRYPTO_BOOTSTRAP="prelinked"
CONFIG_XNU_OWNER_DATA_VOLUME_UUID="00000000-0000-0000-0000-000000000000"
CONFIG_XNU_OWNER_CACHE_VOLUME_UUID="00000000-0000-0000-0000-000000000000"
```

取值可以是 `y`、`n`、数字或带引号字符串。每个 `CONFIG_XNU_<NAME>` 变成内核构建的 `XNU_<NAME>`，字符串中的 `{tree}` 替换为构建树路径。

`CONFIG_XNU_OWNER_CACHE_VOLUME_UUID` 指定 owner 链作为缓存载体挂载的 SystemOS cryptex 卷，即签名 SystemOS 策略所覆盖镜像中的 APFS 卷 UUID。不设置时，内核使用 sheng 镜像的卷。

## feeds.toml

设备仓库可以追加源码：

```toml
[[source]]
name = "DeviceKit"
kind = "kit"
git = "https://github.com/ORG/REPO"
ref = "main"
path = "kits/DeviceKit"
commit = "..."
local = "~/Code/device-kit"
```

kind 为 `driver`、`kit`、`patch`、`package` 或 `refer`。`path` 只拉取子目录，`local` 使用本地目录。

基础 `feeds.toml` 提供 `xnu`、`libplatform`、`libdispatch`、`AvailabilityVersions`、`edk2`、`libpthread` 和 `corecrypto-2023`。

## 专有文件

设备仓库把专有输入放在 `.cache/firmware`。构建树链接到该目录，每个构建树和每次重建都使用同一份文件，不把它们复制进仓库。

## 本地开发

第一个匹配生效：

1. `--local-module [KIND:]NAME=PATH`
2. `--local-<kind> DIR`
3. `--local DIR`
4. `feeds.toml` 的 `local` 键或 `local.toml` 条目
5. 设备仓库
6. 所选 SoC 仓库
7. 基础仓库
8. `feeds.toml` 的 git 源码

格式 2 在设备的 `local.toml` 中接受源码覆盖：

```toml
[[source]]
kind = "soc"
name = "sm8550"
local = "../UniXNU-sm8550-common"
```

本地相对路径从包含该条目的配置文件目录解析。路径解析前展开 `~` 和环境变量。

本地模块以符号链接进入 `out/<device>/<os_profile>/<config>/tree`。`unixnu status` 会标记它们并报告锁文件未固定。

## package 与 kit

设备 `package/` 在构建树中成为 `kits/<board.kit>`。package 可以包含 `unixnu.toml`、kit 目录、驱动、设备树 overlay 和 kext 包。

kit 目录包含 `Kit.toml`：

```toml
schema = 1

[kit]
name = "<kit>"
version = "1.0.0"
description = "<description>"

[match]
arch = "AARCH64"
fdt_compatible = ["<compatible>"]
firmware_vendor = ["<vendor>"]
requires_protocol = ["<guid>"]

[[driver]]
file = "<driver>.efi"
args = "<load options>"

[[overlay]]
file = "<overlay>.dtbo"

[[kext]]
bundle = "<name>.kext"
entry = "<entry id>"
```

`[match]` 中每个已提供的键都需要满足，同一个键内任意值匹配。不匹配的 kit 会被跳过并计入菜单摘要。

## 下一步

- [UEFI 固件](/zh/porting/uefi-firmware)
- [设备树](/zh/porting/device-tree)
- [设备配置参考](/zh/reference/configuration)
- [设备](/zh/devices/)

## 适配记录

`porting.toml` 与 `board.toml` 位于设备仓库根目录。每个部分包含开发经验、证据和
完成条件，记录字段与 `board.toml` 及组件依赖匹配。设备 CI 检查代码路径对应部分
的经验或证据更新。字段、状态和发布流程见[记录指南](/zh/porting/records)。

板级驱动子模块归属设备仓库。sheng 在设备仓库固定 ShengUfs、ShengHidInput、
ShengNovatekTouch 和 ShengPreflight。
