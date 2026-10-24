# 移植总览

UniXNU 通过共享配置和独立仓库组装设备构建。

| 仓库 | 职责 |
| --- | --- |
| `UniXNU` | 构建工具、组件目录、ARM64 支持、OS 配置、模板、UEFI package 和文档站 |
| `UniXNU-<soc>-common` | SoC 配置、DTSI、共享平台操作和补丁 |
| `UniXNU-<device>` | 板级 DTS、外设、组件选择、分区布局和打包 |
| 组件仓库 | 驱动、库、加载器和工具 |
| `uefi-<device>` | UEFI 平台集成 |

## 工作流程

1. 阅读仓库根目录的 `AGENTS.md`、[设备仓库规范](/zh/porting/device-repository)和[组件目录](/zh/reference/components)。
2. 选择 SoC 仓库和 OS 配置。逐项确认组件的硬件接口、OS ABI、板级输入和已验证设备。
3. 复制 `examples/device/`，填写板级配置、源码固定版本、DTS、固件输入和私有文件清单。
4. 在板级 DTS 中包含 SoC DTSI。[共享生成器](/zh/porting/device-tree)将编译后的硬件树转换成 XNU ADT。
5. [构建](/zh/building)内核、package、固件、payload 和启动镜像。
6. 按[bring-up 顺序](/zh/porting/bring-up)验证，分别记录主机构建结果和设备观察结果。
7. 在本文档站维护设备页面和组件参考。

现有设备仓库使用[迁移指南](/zh/porting/migration)。

## 归属

ARM64 公共支持包含 CPU 接口、页表支持和通用构建设置。SoC 仓库包含同一 SoC
各板共用的控制器资源和平台操作。OS 配置包含 XNU 与 IPSW 版本、预链接配置、ABI
要求和版本相关修复。

设备仓库提供外设连接、固件选择、保留内存调整、电源策略、分区几何和显示集成。
可复用的控制器与协议实现放在独立组件中。

SM8550 的 SoC 配置选择 Adreno740。AdrenoMetal 依赖内核 GPU 接口和所选 OS ABI。
sheng 提供其固件、电源、内存和显示输入。参见 [SM8550 支持](/zh/porting/sm8550)。

## 下一步

- [设备仓库](/zh/porting/device-repository)
- [设备树](/zh/porting/device-tree)
- [组件目录](/zh/reference/components)
- [迁移指南](/zh/porting/migration)
- [UEFI 固件](/zh/porting/uefi-firmware)

## 适配记录

官方设备使用 `porting.toml` 逐项记录硬件、配置、经验、证据和完成条件。维护者与 agent
使用同一份记录，设备页从固定版本生成。实现前阅读[记录指南](/zh/porting/records)，
存储适配同时阅读 [UFS 指南](/zh/porting/ufs)。
