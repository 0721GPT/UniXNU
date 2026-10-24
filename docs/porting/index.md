# Port a device

UniXNU runs iOS or macOS on ARM64 devices through UEFI. Follow the eight steps
below to collect inputs, select shared support, adapt drivers and build images.
Each step states its expected result.

- **Add a device:** start at step 1.
- **Build an existing device:** check its inputs and current support in [Devices](/devices/), then follow [Building](/building).
- **Migrate an existing repository:** update its configuration with the [migration guide](/porting/migration), then continue with the required driver adaptation and build steps.

## 1. Collect the device inputs

Prepare the [host tools](/guide/requirements). Record the board's SoC, CPU
layout, RAM reservations, interrupt controllers, storage, panel, touch sensor,
wireless chip and firmware versions. Use the stock DTS, firmware source and
hardware observations as sources. Read the target repositories' root `AGENTS.md`.

Create a [porting record](/porting/records) as you collect these facts. For each
subsystem, record its input source, implementation, current result and next
measurement. The [driver adaptation checklist](/porting/drivers) lists all
subsystems used by sheng, including code that lives inside the device package.

**Result:** a hardware inventory and a list of missing inputs. Hardware addresses
and partition geometry must come from the target board.

## 2. Choose shared support

Read the [component catalog](/reference/components) and candidate implementations
before selecting drivers. Match hardware revision, firmware protocol and OS ABI.

| Location | Contents to select or implement |
| --- | --- |
| `UniXNU/profiles/arch/arm64` | Common ARM64 build configuration and patches |
| `UniXNU-<soc>-common` | SoC resources, DTSI, platform operations and patches |
| `UniXNU/profiles/os/<profile>` | XNU/IPSW revisions, ABI, prelink and OS patches |
| `UniXNU-<device>` | Board wiring, component selection, firmware and partition policy |
| Component repository | Reusable driver, library, loader or tool |
| `uefi-<device>` | Firmware platform and peripheral initialization |

For SM8550, read [SM8550 support](/porting/sm8550). A new SoC needs its own
`soc.toml`, DTSI, configuration fragments and patch series, following that
repository's layout. Review every resource and operation against the new SoC.
A new OS profile needs the [XNU and patch inputs](/xnu/source-and-patches) and
[prelink ABI work](/concepts/prelink) before hardware drivers can be qualified.

**Result:** selected repositories and exact source revisions, with unresolved
hardware or ABI work recorded.

## 3. Create the device repository

The template selects SM8550 and the iPadOS 17.7.11 profile. Set `DEVICE` to the
new board's codename. Run from the UniXNU repository:

```sh
DEVICE=myboard
cp -R examples/device "../UniXNU-$DEVICE"
```

Complete these files using the [device repository reference](/porting/device-repository):

1. `board.toml`: set the board name, kit, SoC source, OS profile, DTS entry point
   and firmware paths. Review the complete component and archive selections.
2. `feeds.toml`: set the SoC repository URL and commit. For another SoC, also
   change `board.soc`, `board.soc_source` and the DTS include.
3. Rename `configs/example_defconfig` and `dts/board.dts`; update their references.
4. Fill the RAM range, enabled CPUs, peripherals and reservations in the DTS.
5. List required firmware files and their sources in `proprietary-files.txt`.
6. Complete `porting.toml`, the root README and `AGENTS.md` for the board.
7. Remove `board.example = true` after the required placeholders are filled.
   The build commands reject repositories marked as examples.

Arrays replace inherited arrays. Keep the complete required component,
archive, configuration-fragment and patch-series lists in each override.

**Result:** a device repository whose source pins and hardware values are explicit.

## 4. Resolve and inspect the build

```sh
./unixnu defconfig "${DEVICE}_defconfig" -D "../UniXNU-$DEVICE"
./unixnu fetch
./unixnu status --json
./unixnu tree
```

Inspect the selected profiles, component sources, overrides and patch order in
`status`. The assembled sources appear under
`out/<device>/<os_profile>/<config>/tree/`; the device's `package/` appears there
as `kits/<board.kit>`. Run component scripts from this assembled tree when their
instructions use `scripts/`, `drivers/` and `kits/` together.

**Result:** resolved dependencies and a build tree with the selected revisions.
Use [configuration](/reference/configuration) for field definitions and
[CLI reference](/reference/cli) for options.

## 5. Prepare hardware description and firmware

Follow [device-tree conversion](/porting/device-tree) to compile the board DTS
and inspect the ADT resources consumed by drivers. The payload command generates
`kernel.macho.adt` from this hardware description and the selected OS profile.
The boot image separately needs the DTB required by the device's firmware.

Follow [UEFI integration](/porting/uefi-firmware#required-integration) to include
UniXNU, DarwinBoot and payload filesystem support. For a Mu-Silicium initrd,
include `UniXNURamDiskDxe`, DiskIoDxe and FAT support in the DSC and FDF.

Include the [firmware log exporter](/porting/log-capture#firmware-setup) so
you can scan logs from the menu during initial bring-up.

**Result:** a buildable firmware configuration and two identified tree inputs:
the board DTS for XNU's ADT, and the firmware DTB for boot-image assembly.

## 6. Adapt drivers in dependency order

Work through [driver adaptation](/porting/drivers): platform and memory,
storage, display and brightness, input, GPU, Wi-Fi, then DSP, audio and power.
Each guide identifies source files, board inputs, build integration and expected
observations. Select only implementations whose required inputs are satisfied.

**Result:** each selected capability has its own configuration and verification
entry. A component selected in TOML still needs its archive recipe and board hooks.

## 7. Build and start the image

Save the paths from [requirements](/guide/requirements) in
[local build configuration](/reference/configuration#local-build-inputs).
Follow [Building](/building) in order: kernel, package, firmware, payload,
userdata when required, then boot image. Check each output before proceeding.

Follow [bring-up](/porting/bring-up) for firmware, menu, loader, kernel and root
checkpoints. Use [boot and recovery](/guide/boot) for device commands. After
changing boot arguments, rebuild both the payload and boot image as described
in [apply arguments](/xnu/boot-arguments#apply-arguments).

**Result:** build outputs and separately recorded device observations. For a
failure, record the last completed checkpoint and use [debugging](/guide/debugging)
and [troubleshooting](/reference/troubleshooting).

Set up [QR log export](/porting/log-capture#firmware-setup) during firmware integration
and save the firmware log before kernel handoff. After storage verification, follow
the same guide through persistent capture, a manual cycle and automated tests.

## 8. Publish the port

Validate the record from the main checkout:

```sh
python3 scripts/porting-record.py check --device "../UniXNU-$DEVICE"
```

Publish the device repository and pin its revision in `devices.toml` when adding
it to the central site. Follow [records](/porting/records) and
[documentation contribution](/contributing/documentation). Link the new device
page from [Devices](/devices/) in both languages. Keep the detailed guide in the
main repository and link it from the device README.

## Related documentation

Use these references for the step you are working on:

| Topic | Documentation |
| --- | --- |
| Boot process and image inputs | [Boot chain](/concepts/boot-chain), [prelink](/concepts/prelink), [payload](/concepts/payload), [userdata layout](/concepts/userdata), [integrity and boot policy](/concepts/security) |
| Component implementation and development | [Driver layers](/drivers/overview), [driver repositories and component pages](/drivers/repositories) |
| Kernel changes | [Source and patches](/xnu/source-and-patches), [patch reference](/xnu/patch-reference), [4 KiB page tables](/xnu/four-k-page-tables) |
| Build and boot configuration | [Command line](/reference/cli), [device configuration](/reference/configuration), [boot configuration](/reference/boot-configuration), [output layout](/reference/output-layout) |
| Host verification | [QEMU workflow](/reference/qemu) |
| Project maintenance | [Repository layout](/contributing/repository), [documentation workflow](/contributing/documentation), [glossary](/reference/glossary) |

Use [Technical reference](/reference/) for fields, driver interfaces, kernel patches and project maintenance.
