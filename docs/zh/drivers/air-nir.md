# AirNir

离线 AIR 到 NIR 到 IR3 编译器 kit。在没有 GPU 提交的情况下验证 shader 路径。

| 仓库 | 维护者 | 范围 |
| --- | --- | --- |
| [AirNir](https://github.com/UniXNU/AirNir) | [UniXNU](https://github.com/UniXNU) | 离线 AIR 到 NIR 到 IR3 编译器 kit。在没有 GPU 提交的情况下验证 shader 路径。 |

## 状态

离线工具。运行时 Metal 路径使用 metal2vulkan 和 Turnip。

## 构建

通过 `unixnu build kernel` 链接进 XNU；kit 或工具仓库用自己的脚本构建。
