# Xiaomi Pad 5 (nabu)

nabu is the Xiaomi Pad 5 with the SM8150 SoC. The port below is an independent community project. The UniXNU organization does not maintain it.

| Repository | Maintainer | State |
| --- | --- | --- |
| [unixnu-nabu](https://github.com/Lux12138/unixnu-nabu) | [Lux12138](https://github.com/Lux12138) | XNU reaches vfs_mountroot; root device next |

## State

The boot chain reaches XNU: `fastboot boot` starts Mu UEFI, the UniXNU menu reads the cust payload, DarwinBoot validates the 51 MB kernel and hands off, and the green XNU screen appears. The kernel now reaches `vfs_mountroot`. The earlier IOsurface stop was the last line that reached the framebuffer, not the blocker. A root device is next.

## Maintenance

The maintainer owns the repository and its release decisions. UniXNU documentation lists the port and its state; it does not take over maintenance.
