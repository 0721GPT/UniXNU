# Device migration

Read the root `AGENTS.md`, [device repository specification](/porting/device-repository),
[component catalog](/reference/components), and [device-tree specification](/porting/device-tree).
Run commands from the UniXNU checkout unless a command selects another directory.

## Format detection

```sh
python3 -c 'import pathlib,tomllib; p=pathlib.Path("../UniXNU-sheng/board.toml"); print(tomllib.loads(p.read_text()).get("schema_version", 1))'
git -C ../UniXNU-sheng status --short
git -C ../UniXNU-sheng submodule status --recursive
./unixnu status --json -D ../UniXNU-sheng > /tmp/sheng-before.json
```

A missing `schema_version` selects format 1. The value `2` selects profiles,
component recipes, ordered patches, and target output isolation. Keep local
changes and record the selected revisions before editing.

## Repository inventory

For each source, record its repository URL, commit, checkout path, local changes,
function, runtime identifiers, build command, outputs, and consumers. Include
sources in `.gitmodules`, `feeds.toml`, `local.toml`, archive lists, shell scripts,
CI, firmware packages, and documentation links.

```sh
git -C ../UniXNU-sheng config -f .gitmodules --get-regexp '^submodule\..*\.(path|url)$'
rg -n 'Native|drivers/|kits/|archives|patches|make-.*-adt' ../UniXNU-sheng
```

For each component checkout:

```sh
git -C <component> remote get-url origin
git -C <component> rev-parse HEAD
git -C <component> status --short
```

Match the URL to the catalog's repository identity. Official compatibility
aliases resolve to their canonical IDs. Community forks keep their own source
identity, name, and pinned revision. Record a community component's build inputs
in its maintained catalog entry before selecting it in format 2.

## Component names

| Accepted official input | Canonical ID |
| --- | --- |
| `NativeAdreno` | `Adreno740` |
| `NativeMetal` | `AdrenoMetal` |
| `NativeUfs` | `ShengUfs` |
| `NativeHidInput` | `ShengHidInput` |
| `NativeNovatekTouch` | `ShengNovatekTouch` |
| `NativeGeniSpi` | `QcomGeniSpi` |
| `NativeMobileDisplay` | `GopFramebuffer` |
| `NativeAES` | `SoftwareAES` |
| `NativeKeyStoreEvents` | `AppleKeyStoreState` |
| `NativeRamBlock` | `RamBlock` |
| `NativeVirtioBlock` | `VirtioMmioBlock` |
| `NativeVirtioConsole` | `VirtioMmioConsole` |
| `NativePrelink` | `XnuPrelink` |
| `NativeWifi` | `Ath12kWifi` |

Apply canonical IDs to official repository URLs, checkouts, feeds, dependency
lists, build entry points, archives, build directories, CI, and documentation.
Keep the source filenames, runtime classes, bundle IDs, plugin filenames,
exported symbols, and protocol IDs recorded in `runtime_identifiers`.

Adreno740 provides the A740 kernel GPU interface. AdrenoMetal provides the Metal
frontend. Their metadata records A740/SM8550 scope and sheng verification.
The board supplies firmware, power policy, memory adjustments, and display
integration.

## Field ownership

| Input | Destination |
| --- | --- |
| Common ARM64 configuration and patches | `profiles/arch/arm64/` |
| SoC registers, interrupts, topology, shared operations | `UniXNU-<soc>-common` |
| XNU/IPSW versions, prelink roots, OS ABI, version fixes | `profiles/os/<id>/` |
| Board wiring, firmware selection, power policy, memory overrides | Device DTS and `board.toml` |
| Partition geometry, package and image parameters | Device `board.toml` and `package/` |
| Component source, dependencies, recipe and outputs | `components.toml` and component repository |
| CPU/GIC/timer constants in an ADT script | SoC DTSI, with board overrides in DTS |
| Apple identity and typed platform properties | OS `[adt]` tables |
| Root UUID and payload memory-map inputs | Device `[payload.adt]` |
| Private firmware inputs | `proprietary-files.txt` and local `.cache/firmware/` |

## Configuration

1. Add `schema_version = 2` at the top of `board.toml`.
2. Pin a `kind = "soc"` source in `feeds.toml`; select its name with `board.soc_source`.
3. Select `board.os_profile`. Place OS settings in that profile and board settings in `board.toml`.
4. Set `[components] include` to the complete required selection. Inspect dependencies before adding a new component.
5. Set `[kernel] config_fragments`. Apply shared fragments in order, then the selected device defconfig.
6. Create ordered `patches/xnu/series` files. Preserve patch prerequisites and inspect `status --json` for the effective sequence.
7. Place `component:<ID>` markers in `[kernel] archives` where their recipes must run among auxiliary build commands.
8. Update scripts and CI to consume the resolver's output directory and effective board data.

Tables merge by key in ARM64, SoC, OS, device order. Arrays replace, including
`include`, `config_fragments`, `patch_series`, and `archives`. A series line is a
relative patch path or a profile/component-prefixed path. Each selected component
recipe runs once in dependency order. An explicit archive plan covers every
selected kernel-stage recipe.

The SM8550 selection applies ARM64, SoC, OS, and device patch series in that
order. The OS series applies the boot-progress integration after the SoC
hooks. The device series contains board patches. The template inherits
common kernel archive recipes and selects its required platform components.

## DTS conversion

1. Read the current ADT generator and the driver properties it supplies.
2. Put confirmed SoC topology, register ranges, interrupts, and references in the SoC DTSI.
3. Include that DTSI in the board DTS. Add board memory, enabled CPUs, peripherals, and reserved regions.
4. Add `unixnu,adt-name` to hardware nodes exported to XNU. Keep names and compatible values used by runtime matching.
5. Define parent cell widths, `ranges`, interrupt providers, and reference argument counts.
6. Move typed Apple properties into the OS profile. Keep root UUID and payload options in `[payload.adt]`.
7. Select `[device_tree] source` and `include_dirs`. Compile with `dtc` and inspect the generated ADT properties against the driver requirements.

The common generator adds NVRAM, entropy, platform identity, root matching, and
payload handoff properties. See [bindings](/porting/device-tree#hardware-bindings).

## sheng example

The complete input set is maintained in these repositories:

| Input | Path |
| --- | --- |
| SoC source | [UniXNU-sm8550-common](https://github.com/UniXNU/UniXNU-sm8550-common), `soc.toml`, `configs/`, `dts/`, `patches/`, `kits/` |
| Device | [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng), `board.toml`, `feeds.toml`, `configs/sheng_defconfig`, `dts/sheng.dts`, `patches/xnu/series`, `proprietary-files.txt`, `package/` |
| OS | `profiles/os/ipados-17.7.11-21H461/profile.toml` |
| Firmware | [uefi-sheng](https://github.com/UniXNU/uefi-sheng) |

The device selects `sm8550` and `ipados-17.7.11-21H461`. Its source pin is in
`feeds.toml`; the resolver loads `soc.toml` from that pinned checkout. The board
selects Adreno740 and AdrenoMetal with its storage, input, framebuffer, and
packaging components. CPU topology and controller resources come from the SoC
DTSI. The board DTS supplies the model, memory, CPU selection, and peripheral policy.

```sh
git clone --recurse-submodules https://github.com/UniXNU/UniXNU-sheng ../UniXNU-sheng
git clone https://github.com/UniXNU/uefi-sheng ../uefi-sheng
./unixnu defconfig sheng_defconfig -D ../UniXNU-sheng
./unixnu fetch
./unixnu status --json > /tmp/sheng-resolved.json
./unixnu tree
./unixnu ipsw fetch
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-sheng
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-sheng --dtb <firmware.dtb>
```

Use existing checkouts when available. Supply the platform seed, trust cache,
Data seed, firmware files, and firmware DTB from the device input inventory.
For local SoC development, add `--local-module soc:sm8550=../UniXNU-sm8550-common`.
The status report marks this source as a local override.

Expected paths under `out/sheng/ipados-17.7.11-21H461/sheng/`:

| Path | Result |
| --- | --- |
| `.config` | Merged configuration |
| `tree/.cache/xnu-work/` | Target-local patched XNU |
| `kernel/kernel.macho` | Prelinked kernel |
| `device-tree/hardware.dtb` | Compiled hardware tree |
| `payload/` | Payload image containing `Payloads/kernel.macho.adt` |
| `boot/boot.img` | BootShim, firmware, firmware DTB and payload |
| `logs/` | Build logs |

Inspect `/tmp/sheng-resolved.json` for profile roots, selected component scope,
source revisions, module origins, patch order, and output path. Check the
[sheng page](/devices/sheng) for board inputs and hardware checkpoints.

## Build inputs

The sheng archive and image steps require:

- A full Xcode installation with the iPhoneOS SDK.
- `storage-abi/interface-audit.json` and its matching storage ABI inputs.
- Qualified `build/xnu-prelink/*candidate.macho` files and their JSON ABI records for the HID and display builders.
- VerifiedRoot and verified VirtIO policy archives required by the selected configuration.
- Board firmware listed in `proprietary-files.txt`, including the GPU firmware.
- `.cache/mesa-work`, built Turnip, and metal2vulkan inputs for the Metal plugin.
- The selected IPSW members, platform seed, trust-cache bundle, Data seed, and firmware DTB.

Record each input's source and revision in the migration report. A missing input
keeps its dependent build step pending. Kernel, payload, and boot-image completion
require their actual outputs. Device verification uses those exact images.

## Verification report

```text
Device:
Configuration and OS profile:
Repository revisions and local overrides:
Selected components and board inputs:
Patch sequence:

Host checks:
  Command | Result | Output or log
  Dependency resolution:
  DTS and ADT generation:
  Kernel and prelink:
  Package and firmware:
  Payload and boot image:
  QEMU checkpoint:
  Documentation build and links:

Hardware:
  Image and device:
  Boot:
  Storage:
  Display:
  Input:
  GPU:

Unresolved inputs:
  Required value or file | Source or owner | Affected step
```

Record unavailable hardware checks as pending. Keep build completion and device
observations separate. Update English and Chinese documentation, root README,
root `AGENTS.md`, component metadata, and firmware references with the final names
and commands.
