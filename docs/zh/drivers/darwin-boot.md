# DarwinBoot

UEFI 启动加载器。安装 loader 协议，把内核、ADT、trust cache 和 ramdisk 交给 XNU。

| 仓库 | 维护者 | 范围 |
| --- | --- | --- |
| [DarwinBoot](https://github.com/UniXNU/DarwinBoot) | [UniXNU](https://github.com/UniXNU) | UEFI 启动加载器。安装 loader 协议，把内核、ADT、trust cache 和 ramdisk 交给 XNU。 |

## 状态

不是驱动。每个 XNU 启动路径都使用它。

## 构建

通过 `unixnu build kernel` 链接进 XNU；kit 或工具仓库用自己的脚本构建。
