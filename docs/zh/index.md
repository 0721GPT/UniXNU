---
layout: home
hero:
  text: 在非 Apple ARM64 硬件上运行 Darwin 与 XNU 用户态
  tagline: UEFI 启动管理器、设备树平台、打过补丁的 XNU 内核和完整启动镜像。
  image:
    src: /logo-light.svg
    alt: UniXNU 标志
  actions:
    - theme: brand
      text: 从这里开始
      link: /zh/guide/overview
    - theme: alt
      text: 移植设备
      link: /zh/porting/
    - theme: alt
      text: 设备说明
      link: /zh/devices/
    - theme: alt
      text: GitHub
      link: https://github.com/UniXNU/UniXNU
---

::: info 文档状态
文档随源码修订。具体细节以构建报告和仓库为准。
:::

## UniXNU 提供什么

UniXNU 是一套构建系统和运行时栈，用于在固件为 UEFI 或可移植为 UEFI 的 ARM64 设备上启动 Darwin 系操作系统。

| 层次 | 组件 | 作用 |
| --- | --- | --- |
| 启动镜像 | BootShim、UEFI 固件卷、设备树 | 由设备原有的 Android bootloader 通过 `fastboot boot` 加载 |
| 启动管理器 | UniXNU | 加载驱动、应用设备树补丁、发布块设备切片、运行菜单 |
| 加载器 | DarwinBoot | 校验内核 Mach-O、ADT 和 trust cache，退出 boot services，进入 EL1 |
| 内核 | 打过补丁的 XNU | 从 Apple 源码构建，并与原始 kernelcache 预链接 |
| 设备 | XNU 驱动与 kit | 平台专家、CPU、存储、显示、GPU 和板级电源 |
| 用户态 | IPSW 磁盘镜像 | userdata 分区中未修改的 OS 与 SystemOS 镜像 |

## 文档地图

- [总览](/zh/guide/overview)：整体栈、仓库和构建模型。
- [构建](/zh/building)：克隆、配置、拉取、构建和打包启动镜像。
- [启动镜像](/zh/guide/boot)：`fastboot boot`、菜单、串口日志和恢复。
- [移植](/zh/porting/)：新增设备仓库和 UEFI 固件移植。
- [驱动](/zh/drivers/overview)：UEFI 驱动、内置 XNU 驱动和 kext 包。
- [XNU](/zh/xnu/source-and-patches)：源码版本、补丁、4 KiB 页和启动参数。
- [设备](/zh/devices/)：支持模型和各设备说明。sheng 页面只是其中一个设备条目。
- [参考](/zh/reference/cli)：全部命令、配置项和输出文件。

## 仓库

| 仓库 | 内容 |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | 构建驱动、UEFI 启动管理器、DarwinBoot、通用驱动、kit 和补丁 |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | 小米平板 6S Pro 的设备仓库 |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | 小米平板 6S Pro 的 UEFI 固件移植 |

设备仓库采用与本仓库相同的目录布局。同名模块会覆盖基础仓库的模块，设备仓库的 `package/` 在构建树中成为 kit。通用文档适用于所有设备，设备相关的具体数值放在仓库和 [设备](/zh/devices/) 章节中。
