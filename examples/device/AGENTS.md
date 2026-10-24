# Device agent guide

This repository owns the board DTS, peripheral wiring, component selections,
firmware inputs, partition layout, and packaging.

## Read first

Read this README and the following UniXNU documentation before changing the port:

| Task | Required reading |
| --- | --- |
| Every port | [Porting](https://unixnu.com/porting/), [device repository](https://unixnu.com/porting/device-repository), [component catalog](https://unixnu.com/reference/components), UniXNU `components.toml` |
| Hardware | [Device tree](https://unixnu.com/porting/device-tree), [matching](https://unixnu.com/drivers/device-tree-matching), selected SoC README, AGENTS, soc.toml and DTSI |
| Component | Its site page, README, catalog build command, dependencies and source |
| Kernel | [Source and patches](https://unixnu.com/xnu/source-and-patches), [patch reference](https://unixnu.com/xnu/patch-reference), selected OS profile |
| Images | [Prelink](https://unixnu.com/concepts/prelink), [payload](https://unixnu.com/concepts/payload), [UEFI](https://unixnu.com/porting/uefi-firmware) |
| Verification | [Building](https://unixnu.com/building), [bring-up](https://unixnu.com/porting/bring-up) |
| Migration | [Migration](https://unixnu.com/porting/migration) |

## Work

Search the catalog and inspect existing component interfaces before adding code.
Match hardware scope, OS ABI, and board dependencies. Shared SoC resources and
operations belong in the selected SoC repository. Reusable drivers and libraries
belong in their component repositories. Use canonical component IDs and preserve
runtime identifiers recorded in the catalog.

Replace every `REQUIRED_` value, set the board name and firmware paths, and select
the complete dependency set. Verify memory ranges, CPU affinities, interrupts,
firmware hashes, and partition geometry from board evidence. Set `example = false`
after completing the inputs. Pin the SoC commit and inspect resolved profiles.

Run from UniXNU, replacing the device path and defconfig:

```sh
./unixnu defconfig example_defconfig -D ../UniXNU-example
./unixnu fetch
./unixnu status --json
./unixnu tree
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-example
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-example --dtb <firmware.dtb>
```

Record commands, revisions, output paths, results, and unresolved inputs.
Report hardware boot, storage, display, input, and GPU results separately.
Update the device page in UniXNU's documentation site and keep this README concise.
