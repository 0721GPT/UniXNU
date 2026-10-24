# UniXNU agent guide

## Responsibility

UniXNU owns the build tools, component catalog, ARM64 support, OS profiles,
UEFI package, device template, and documentation site. SoC repositories own
shared hardware descriptions and platform operations. Device repositories own
board peripherals, firmware selections, partition layouts, and packaging.
Component repositories own drivers, libraries, loaders, and tools.

## Required reading

Read the root `AGENTS.md` and `README.md` of each repository you edit. Read these
files in UniXNU before selecting components or changing a port:

| Task | Required reading |
| --- | --- |
| Every port | `docs/porting/index.md`, `docs/porting/device-repository.md`, `docs/reference/components.md`, `components.toml` |
| Hardware description | `docs/porting/device-tree.md`, `docs/drivers/device-tree-matching.md`, selected SoC `README.md`, `soc.toml`, DTSI and bindings |
| Component work | Component page under `docs/drivers/`, component `README.md`, catalog build command, dependencies, and relevant source |
| Kernel work | `docs/xnu/source-and-patches.md`, `docs/xnu/patch-reference.md`, selected OS `profile.toml`, XNU source and ordered patch series |
| Image assembly | `docs/concepts/prelink.md`, `docs/concepts/payload.md`, `docs/porting/uefi-firmware.md`, device firmware configuration |
| Verification | `docs/building.md`, `docs/porting/bring-up.md`, selected device page |
| Migration | `docs/porting/migration.md` |
| Documentation | `docs/contributing/documentation.md` |

The published documentation is at https://unixnu.com. Site sources,
navigation, translations, assets, build configuration, and deployment belong
to this repository. Update English and Chinese pages together.

## Component discovery

1. Search `components.toml` by function, hardware, scope, and dependencies.
2. Read the candidate's page, README, build command, and implementation.
3. Check its OS ABI, board inputs, and verified devices separately.
4. Reuse the component when its interfaces match the port. Add shared SoC
   operations to the SoC repository and board policy to the device repository.
5. Add a component when existing implementations leave a concrete requirement
   uncovered. Record its dependencies and required inputs in the catalog.

Use canonical component IDs for selections, paths, commands, and outputs.
Official compatibility names resolve through repository identity. Community
forks keep their repository identity. Preserve source filenames, runtime
classes, bundle identifiers, plugin filenames, symbols, and protocols; record
these in `runtime_identifiers`.

## Configuration

Format 2 uses `schema_version = 2`. Resolve ARM64, SoC, OS, and device profiles
in that order. Tables merge by key; ordinary arrays replace. List every required
component, configuration fragment, and patch series in the effective arrays.
The selected defconfig follows the configuration fragments.

Pin SoC and component sources. Inspect `unixnu status --json` before building.
Place generated sources, patched XNU, and outputs in the resolved target tree.

## Commands

Run from the UniXNU checkout:

```sh
./unixnu defconfig sheng_defconfig -D ../UniXNU-sheng
./unixnu fetch
./unixnu status --json
./unixnu tree
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-sheng
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-sheng --dtb <firmware.dtb>
```

Use the selected device's documented inputs. For documentation:

```sh
cd docs
npm ci
npm run build
```

For a Mu-Silicium initrd payload, include `UniXNURamDiskDxe`, DiskIoDxe and
FAT support in the firmware DSC and FDF. Run `./unixnu build firmware` and
build the payload before `./unixnu bootimg`. Follow
[required firmware integration](https://unixnu.com/porting/uefi-firmware#required-integration).

## Verification and reporting

Run existing checks and targeted build exercises for changed behavior. Add a
test when a credible future edit would cause a specific device failure that
existing checks miss. Report the command, selected profiles, source revisions,
output path, result, and unresolved input. Report hardware boot, storage,
display, input, and GPU observations separately from host builds.

Preserve unrelated working-tree changes. Keep documentation concise and describe
current behavior. Use standard terms, neutral headings, and ordinary punctuation.

## Porting record

Read the device's `porting.toml` and the
[record guide](https://unixnu.com/porting/records) before implementation.
For storage, also read the [UFS guide](https://unixnu.com/porting/ufs).
Update each affected area's findings or evidence with its code change.
The documentation site renders this same record. Follow its schema and
completion levels for both maintainer and agent work.
