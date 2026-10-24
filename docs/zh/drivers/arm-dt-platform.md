# ArmDTPlatform

ARM 设备树平台专家。从 ADT 发布 CPU nub 和每个 `arm-io` 子节点。

| 仓库 | 维护者 | 范围 |
| --- | --- | --- |
| [ArmDTPlatform](https://github.com/UniXNU/ArmDTPlatform) | [UniXNU](https://github.com/UniXNU) | ARM 设备树平台专家。从 ADT 发布 CPU nub 和每个 `arm-io` 子节点。 |

## 状态

通用驱动。需要 `/cpus`、`/arm-io`、`/defaults`、`/options` 和 NVRAM proxy。

## 构建

通过 `unixnu build kernel` 链接进 XNU；kit 或工具仓库用自己的脚本构建。
