# DarwinBoot

UEFI bootloader. Installs the loader protocol and hands the kernel, ADT, trust cache and ramdisk to XNU.

| Repository | Maintainer | Scope |
| --- | --- | --- |
| [DarwinBoot](https://github.com/UniXNU/DarwinBoot) | [UniXNU](https://github.com/UniXNU) | UEFI bootloader. Installs the loader protocol and hands the kernel, ADT, trust cache and ramdisk to XNU. |

## State

Not a driver. Used by every XNU boot path.

## Build

Linked into XNU by `unixnu build kernel`, or built by its own script when the repository is a kit or tool.
