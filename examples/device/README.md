# Device template

This template selects shared SM8550 and iPadOS 17.7.11 support. Complete the
hardware inputs before building.

1. Copy this directory to `UniXNU-<device>` and read `AGENTS.md`.
2. Set the board name, description, firmware paths, and complete component selection.
3. Pin a reviewed SoC commit in `feeds.toml`.
4. Fill the DTS model, compatible values, memory map, CPU selection, and peripherals.
5. List firmware sources and hashes in `proprietary-files.txt`.
6. Set partition geometry and payload inputs from the device's records.
7. Set `example = false`, select the defconfig, fetch, and inspect `status --json`.

The zero memory range and `REQUIRED_` values identify inputs to complete.
`patches/xnu/series` lists board patches in application order. `package/` contains
board packaging. Shared resources come from the selected SoC and component sources.

- [Device repository](https://unixnu.com/porting/device-repository)
- [Device tree](https://unixnu.com/porting/device-tree)
- [Component catalog](https://unixnu.com/reference/components)
- [Migration](https://unixnu.com/porting/migration)
