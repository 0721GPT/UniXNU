# Porting overview

UniXNU assembles a device from shared profiles and independent repositories.

| Repository | Responsibility |
| --- | --- |
| `UniXNU` | Build tools, catalog, ARM64 support, OS profiles, templates, UEFI package, documentation site |
| `UniXNU-<soc>-common` | SoC configuration, DTSI, shared platform operations, patches |
| `UniXNU-<device>` | Board DTS, peripherals, component selection, partition layout, packaging |
| Component repositories | Drivers, libraries, loaders, tools |
| `uefi-<device>` | UEFI platform integration |

## Workflow

1. Read the repository's root `AGENTS.md`, [device repository specification](/porting/device-repository), and [component catalog](/reference/components).
2. Select a SoC repository and OS profile. Check the hardware interfaces, OS ABI, board inputs, and verified devices of each component.
3. Copy `examples/device/`. Fill in the board configuration, source pins, DTS, firmware inputs, and proprietary-file inventory.
4. Include the SoC DTSI in the board DTS. The [shared generator](/porting/device-tree) converts the compiled hardware tree into XNU's ADT.
5. [Build](/building) the kernel, package, firmware, payload, and boot image.
6. Follow the [bring-up order](/porting/bring-up). Record host build results and device observations separately.
7. Publish the device page and component references in this documentation site.

Use the [migration guide](/porting/migration) for an existing device repository.

## Ownership

Common ARM64 support includes CPU interfaces, page-table support, and generic
build settings. SoC support contains controller resources and operations shared
by boards using that SoC. OS profiles contain XNU and IPSW versions, prelink
configuration, ABI requirements, and version-specific fixes.

The device supplies peripheral wiring, firmware selection, reserved-memory
adjustments, power policy, partition geometry, and display integration. Reusable
controller and protocol implementations remain components.

For SM8550, the SoC profile selects Adreno740. AdrenoMetal uses the kernel GPU
interface and the selected OS ABI. The sheng device supplies its firmware,
power, memory, and display inputs. See [SM8550 support](/porting/sm8550).

## Next steps

- [Device repository](/porting/device-repository)
- [Device tree](/porting/device-tree)
- [Component catalog](/reference/components)
- [Migration](/porting/migration)
- [UEFI firmware](/porting/uefi-firmware)

## Porting records

Official devices record hardware, configuration, findings, evidence and completion
criteria in `porting.toml`. Maintainers and agents use the same record; device
pages render its pinned revision. Read the [record guide](/porting/records)
and the [UFS guide](/porting/ufs) before storage work.
