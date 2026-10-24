<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-light.svg">
    <img alt="UniXNU" src="assets/logo-light.svg" width="200">
  </picture>
</p>

<p align="center">
  <a href="https://unixnu.com"><img alt="文档" src="https://img.shields.io/badge/docs-unixnu.com-2A5BD7?logo=readthedocs&logoColor=white"></a>
  <a href="https://t.me/unixnu"><img alt="Telegram" src="https://img.shields.io/badge/Telegram-@unixnu-26A5E4?logo=telegram&logoColor=white"></a>
  <a href="https://qun.qq.com/universal-share/share?ac=1&authKey=Pubz23DOCY76nxjlGuSms2zesk4jRvas4zrXJRYk7omf7CprXbGQigLEI%2BCoYR8D&busi_data=eyJncm91cENvZGUiOiIxMTI2MTE2NzU5IiwidG9rZW4iOiJwSHJsUDN1UnN2Vi8yM210TjJRbzBIcERmcUpyOS9lb0hHVVpadWdvVFdScVpSYUFLT1YzTk5TWC9mN2xWYmtIIiwidWluIjoiMzE1MTEwOTc0MSJ9&data=_cJgr-jX9yJ2-GXwVjRN5FOhDyk5B-HAfIk5LSunbW_XkkmKqNXbpH33wzP93J0p9DI1fbjgBUrUfTVhwDcxjg&svctype=4&tempid=h5_group_info"><img alt="QQ 群" src="https://img.shields.io/badge/QQ%20Group-join-12B7F5?logo=qq&logoColor=white"></a>
  <a href="https://x.com/unixnu"><img alt="X" src="https://img.shields.io/badge/X-@unixnu-000000?logo=x&logoColor=white"></a>
</p>

<p align="center">
  <a href="README.md">English</a> | 简体中文
</p>

# UniXNU

> **迁移通知：** 请按[迁移指南](https://unixnu.com/zh/porting/migration)更新设备仓库，接入共享 SoC 支持并统一组件名称。

一个独立项目，通过 UEFI 在非 Apple ARM64 硬件上运行 iOS 与 macOS。

UniXNU 为不同 SoC 和设备的移植提供共享构建工具、ARM64 支持、OS 配置和可复用组件。SoC 仓库提供公共平台支持，设备仓库提供板级配置和硬件集成。

**🎉 News:** Adreno740 和 AdrenoMetal 在 sheng 上提供 GPU 加速。render server 通过 Adreno 740 合成系统界面，列入白名单的进程会获得各自的 Metal 设备。这两个组件提供共享 GPU 实现，各设备适配提供所需的固件、电源、内存和显示集成。详见 [Adreno 740 驱动文档](https://unixnu.com/zh/drivers/adreno740)。

## 固件要求

运行 `./unixnu bootimg` 前，先用 `./unixnu build firmware --uefi ../uefi-<device>` 构建设备固件，并将 UniXNU、UniXNUDxe、DarwinBoot 和共享负载驱动 `UniXNURamDiskDxe` 纳入固件。步骤见[必需的固件集成](https://unixnu.com/zh/porting/uefi-firmware#required-integration)。

在 [`unixnu.local.toml`](https://unixnu.com/zh/reference/configuration#local-build-inputs) 保存路径和启动参数，即可使用 `./unixnu kernel`、`./unixnu firmware`、`./unixnu payload` 和 `./unixnu bootimg`。修改启动参数后重新构建负载和启动镜像。检查步骤见[启动参数](https://unixnu.com/zh/xnu/boot-arguments#apply-arguments)和[绿条诊断](https://unixnu.com/zh/guide/debugging#green-bar)。

## 文档

Linux 安装脚本支持 apt、dnf、pacman、zypper，以及单独准备依赖的环境。详见 [Linux 要求](https://unixnu.com/zh/guide/requirements#linux)。

文档位于 [unixnu.com](https://unixnu.com)，源码、导航、翻译、资源、配置和部署工作流由本仓库维护。文档覆盖启动链、构建驱动、设备移植、驱动开发、XNU 4 KiB 页补丁、启动参数和设备说明。

- [总览](https://unixnu.com/zh/guide/overview)
- [构建](https://unixnu.com/zh/building)
- [启动镜像](https://unixnu.com/zh/guide/boot)
- [调试与日志](https://unixnu.com/zh/guide/debugging)
- [移植](https://unixnu.com/zh/porting/)
- [驱动](https://unixnu.com/zh/drivers/overview)
- [XNU 与 4 KiB 页](https://unixnu.com/zh/xnu/four-k-page-tables)
- [设备](https://unixnu.com/zh/devices/)

## 仓库

| 仓库 | 内容 |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | 构建工具、组件目录、ARM64 支持、OS 配置、模板、UEFI package 和文档站 |
| [UniXNU-sm8550-common](https://github.com/UniXNU/UniXNU-sm8550-common) | SM8550 共享配置、DTSI、平台操作和补丁系列 |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | 小米平板 6S Pro 的设备仓库 |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | 小米平板 6S Pro 的 UEFI 固件移植 |

主仓库检出共享驱动和 kit。设备仓库维护板级驱动子模块，sheng 提供 ShengUfs、ShengHidInput、触控和 preflight 组件。组件目录描述可用组件，构建工具组装所选设备的源码。克隆仓库时使用 `--recurse-submodules`。

设备仓库选择固定版本的 SoC 源码、OS 配置和组件。构建工具合并配置、解析依赖、编译板级 DTS 并生成 ADT。先读 [AGENTS.md](AGENTS.md)、[设备模板](examples/device/README.md)和[迁移指南](https://unixnu.com/zh/porting/migration)。

## 官方设备开发

每个官方设备维护 `porting.toml`，为每个子系统记录硬件输入、配置、步骤、经验、
证据和完成条件。维护者与 agent 共用这份记录。文档站按 `devices.toml` 固定的设备版本生成页面。

必需 CI 检查覆盖记录结构、组件依赖，以及代码改动对应部分的经验或证据更新。
从[记录指南](https://unixnu.com/zh/porting/records)、[UFS 移植指南](https://unixnu.com/zh/porting/ufs)
和[设备模板](examples/device/README.md)开始。

## 参与贡献

阅读 [CONTRIBUTING.md](CONTRIBUTING.md)（英文），或查看[文档站的贡献页面](https://unixnu.com/zh/contributing/documentation)。

## 私有 blob

不要把 Apple IPSW、kernelcache、磁盘镜像、trust cache、Data 种子或厂商固件提交到本仓库。把它们放在本地缓存或外部存储。

## 许可证

BSD-2-Clause-Patent。见 [LICENSE](LICENSE)。
