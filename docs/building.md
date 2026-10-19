# Building UniXNU

This describes the `unixnu` build driver. The project was released in a hurry: the driver is new,
only the boot image step has been checked end to end, and the kernel, package and firmware steps
call scripts that still carry assumptions from the old single repository. Expect to read logs.

The driver is developed and tested on macOS. Whether another host works is not known: the kernel step builds
with Xcode (`xcodebuild`, the iphoneos SDK) and `--skip-setup` uses `hdiutil`, so those two need macOS. The firmware,
payload and boot image steps use Python, git, LLVM, EDK II, mtools and dtc, and have only been run on macOS.
Boot images are only ever started with `fastboot boot`. Nothing here flashes a device.

## Requirements

- macOS (see above), Python 3.11 or newer (standard library only, nothing to install with pip)
- Xcode for the kernel step, `hdiutil` for `--skip-setup`, `mtools` for the payload
- `git`, `make`, `cmake`, `ninja`, `unifdef`
- LLVM and lld from Homebrew (`clang`, `llvm-objcopy`, `llvm-nm`, `ld.lld`). The keg-only
  `/opt/homebrew/opt/llvm/bin` is found without being on `PATH`.
- `dtc` (provides `fdtput` and `fdtget`)
- Network access for `unixnu fetch`

You also provide inputs that are never part of a repository:

| Input | Used by | Option |
| --- | --- | --- |
| A Mu-Silicium checkout for the device (for sheng: uefi-sheng) | firmware, bootimg | `--uefi DIR` or `UNIXNU_UEFI` |
| The target IPSW, a local file or an https URL | kernel (prelink), userdata | `--ipsw PATH_OR_URL` or `UNIXNU_IPSW`; defaults to the url of `board.toml` |
| The decompressed kernelcache (optional, taken from the IPSW when not given) | kernel (prelink) | `--kernelcache FILE` or `UNIXNU_KERNELCACHE` |
| A compiled device tree | bootimg | `--dtb FILE` or `UNIXNU_DTB` |
| A trust cache bundle (optional) | payload | `--trust-cache FILE` |
| A Data ramdisk seed (optional) | payload | `--data-ramdisk FILE` |
| The platform seed of the device (not stored in any repository) | payload | `--unique-chip-id HEX` or `UNIXNU_UNIQUE_CHIP_ID` |
| Vendor firmware inputs of the board | firmware | board preflight scripts |
| An edk2 tree (Mu_Basecore) | package, firmware | fetched by default, or `--edk2 DIR` / `UNIXNU_EDK2` |

## Layout

```
UniXNU/                    this repository: the driver, generic modules and the package
  unixnu                   entry point
  scripts/unixnu/          the driver
  package/                 UniXNU itself (CrabApple, an EDK II package)
  drivers/ kits/ patches/ scripts/
  feeds.toml               external sources, pinned by commit
  examples/device/         a sample device repository (marked as an example)

UniXNU-sheng/              a device repository
  board.toml               board description
  configs/*_defconfig      kernel configuration
  feeds.toml               optional extra sources
  drivers/ kits/ patches/ scripts/ package/
```

A device repository has the same folder layout as this repository. Its modules override modules of the
same name here. Its `package/` becomes `kits/<board.kit>` in the build tree.

All output goes under `out/` (ignored by Git): `out/<board>/` for the board and `out/feeds/` for fetched
sources. Nothing is written into the repositories, except `unixnu.lock` in the device repository.

## Quick start

```sh
git clone https://github.com/UniXNU/UniXNU
git clone https://github.com/UniXNU/UniXNU-sheng
git clone https://github.com/UniXNU/uefi-sheng
cd UniXNU

./unixnu defconfig sheng_defconfig -D ../UniXNU-sheng      # the device is remembered afterwards
./unixnu fetch                                             # shallow, sparse clones, writes unixnu.lock
./unixnu ipsw fetch                                        # kernelcache, device tree and trust caches from the IPSW
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-sheng
./unixnu bootimg --uefi ../uefi-sheng --dtb /path/to/sheng.dtb
```

The boot image is `out/sheng/boot/boot.img`, with a `boot.json` report next to it. Options go after
the subcommand.

## Commands

| Command | What it does |
| --- | --- |
| `defconfig [NAME]` | Lists `configs/*_defconfig` of the device repository, or selects one into `out/<board>/.config`. |
| `fetch [NAME...] [--update]` | Fetches git sources and records the commits in `unixnu.lock`. `--update` ignores the lock. |
| `tree` | Assembles `out/<board>/tree`, the layout the build scripts expect. |
| `ipsw info\|fetch [COMPONENT...]` | Lists the components of the target IPSW, or extracts them into `out/ipsw/<build>/`. |
| `build [kernel] [package] [firmware] [payload] [prelink]` | Builds the steps (default: all four). `prelink` repeats only the prelink. |
| `build userdata [--dry-run]` | Builds the raw image for the userdata partition (large, only when named). |
| `bootimg` | Packs BootShim, the firmware volume and the device tree into an Android boot image. |
| `status` | Shows the device, the configuration, and where every module comes from. |
| `clean` | Removes the build output of the board and keeps fetched sources. |

Common options: `-D DIR` device repository, `--out DIR` output root, `-j N` jobs, `--target DEBUG|RELEASE`,
`--uefi`, `--edk2`, `--kernelcache`, and the local source options below.
Environment: `UNIXNU_DEVICE`, `UNIXNU_OUT`, `UNIXNU_UEFI`, `UNIXNU_EDK2`, `UNIXNU_KERNELCACHE`,
`UNIXNU_DTB`, `JOBS`.

## Steps

**ipsw** is not a step but the source of the original OS files. `--ipsw` takes a local file or an https URL.
A remote IPSW is never downloaded as a whole: the zip directory and each needed member are read with HTTP range
requests. Members are cached in `out/ipsw/<build>/` and checked against the sha256 pins of `board.toml [ipsw.sha256]`.
The image must match `[ipsw]` (product, version, build, device class, chip and board id).

**kernel** applies `patches/xnu` and the prelink patches to Apple's xnu-10063.141.1, builds the kernel with
the switches of the defconfig, then prelinks it against the original kernelcache with
`kits/NativePrelink`. The kernelcache is taken from the IPSW (the LZFSE payload of its IM4P, arm64 slice, checked against
`kernelcache_sha256`) unless `--kernelcache` gives one; decoding it uses macOS `compression_tool`. The result is `out/<board>/kernel/kernel.macho`. If the board lists a
`kernelcache_sha256`, any other kernelcache is refused. The XNU sources, libplatform, libdispatch and
AvailabilityVersions are fetched on demand.

**package** builds `package/` (UniXNU itself) with EDK II into `out/<board>/package/Build/Out`.

**firmware** builds the device firmware from the Mu-Silicium checkout. It runs the preflight scripts of the
board, applies the board's patches to edk2 for the build only (they are always reverted), and runs `build` with
the board's DSC and defines. `FD_BASE`, `FD_SIZE` and `FD_BLOCKS` come from the Mu-Silicium device config.

**payload** builds the ramdisk of the boot image: a FAT volume with the CrabApple config, the prelinked kernel
(`--kernel FILE`, default the kernel step output; optionally with the panic screen patched into a copy), the
device tree sidecar generated by the board script, and the optional trust cache and Data seed. The kernel command
line comes from `board.toml` plus any `--kernel-arg NAME=VALUE`. The result is `out/<board>/payload/payload.img`
with a `payload.json` report. The volume is written with mtools without mounting. The device tree contains a
random seed, so the payload differs between runs. The loader (DarwinBoot) is part of the firmware volume, and the
IPSW is not read by this step: its inputs are the kernel, which comes from the original kernelcache, and the
files you provide. `--skip-setup` marks Setup as done in a copy of the Data seed given with `--data-ramdisk`, so the Hello pages are skipped.
The Data seed itself is an input.

**userdata** builds the raw image for the `userdata` partition: the unchanged OS and SystemOS disk images of the
IPSW, byte for byte, behind a manifest (`SHNGROOT`). The disk images are extracted from the IPSW (about 8 GiB) and
attached read-only with `hdiutil`. The result is `out/<board>/userdata/userdata-root.img` with a report. Nothing is
flashed: the report prints the `fastboot flash userdata` command for you to run. `--dry-run` only checks the IPSW and
the sizes.

**bootimg** compiles the BootShim, appends the firmware volume, compresses it with gzip, appends the device
tree (after adding the board's `dtb_node` entries), and runs `mkbootimg.py` with the parameters of the
Mu-Silicium device config. It unpacks the result and compares every part with its input, so a mismatch fails
the build. The output is deterministic. The ramdisk is the output of the payload step when it exists, or `--ramdisk FILE`,
or empty with `--ramdisk none`.

## Local development

Any module can come from a local directory instead of the repository or git. The first match wins:

1. `--local-module [KIND:]NAME=PATH`, one module
2. `--local-drivers|kits|patches|scripts|package DIR`, one category (each subdirectory is a module; for
   `package` the directory is the package itself)
3. `--local DIR`, a directory laid out like this repository
4. a `local = "DIR"` entry in a `feeds.toml`, or an ignored `local.toml` in the device repository
   (see `examples/device/local.toml.example`)
5. the device repository
6. this repository
7. a git source of a `feeds.toml`

Local modules are linked into the build tree, not copied, so an edit is used by the next build. A missing
directory is an error. `unixnu status` marks local modules and warns that they are not pinned by the lock, so
a build that uses them is not reproducible.

## Configuration files

### board.toml (device repository)

| Section | Keys |
| --- | --- |
| `[board]` | `name`, `soc`, `description`, `kit` (folder name of the device package under `kits/`), `example` |
| `[ipsw]` | `url` (default remote source), `product`, `version`, `build`, `device_class`, `chip_id`, `board_id`, `fetch` (components `ipsw fetch` extracts by default) |
| `[ipsw.sha256]` | sha256 pins by member file name |
| `[userdata]` | `first_lba`, `last_lba`, `lba_bytes` (the GPT entry of the partition) and `windows` (name and IPSW component of each disk image) |
| `[kernel]` | `defconfig` |
| `[kernel.prelink]` | `tool`, `kernel`, `slide`, `roots` (list), `early_security_init`, `kernelcache`, `kernelcache_sha256`, `extra_args` |
| `[firmware]` | `config` (name of `Resources/Configs/<name>.toml`), `dsc`, `fd` (path of the result, `{target}` is replaced), `packages` (`{uefi}`, `{edk2}`, `{tree}` are replaced), `preflight`, `patches` |
| `[firmware.defines]` | extra `-D` definitions |
| `[bootimg.image]` | overrides of the image parameters (`base`, `kernel_offset`, `tags_offset`, `pagesize`, `header_version`) |
| `[payload]` | `volume`, `size_mib`, `loader` (`firmware`), `title`, `dram_size`, `panic_screen`, `trust_cache_sha256` (accepted bundles), `kernel_args` |
| `[payload.adt]` | `tool`, `chip_id`, `owned_memory_size`, `root_match_volume_uuid`, `cpu_regs`, `psci_method`, `packed_data_ramdisk`, `ipad_identity`, `unique_chip_id = "required"` |
| `[[bootimg.dtb_node]]` | `path` and `props` (`name`, `type`, `value`) added to the device tree |

`examples/device` is a complete sample. It is marked `example = true`, and `build` and `bootimg` refuse to run
on a repository with that mark.

### configs/NAME_defconfig

Lines of the form `CONFIG_NAME=y`, `=n`, `="text"` or `=123`, and `# CONFIG_NAME is not set`. Each
`CONFIG_XNU_<NAME>` is passed to the kernel build as `XNU_<NAME>`, with `y` as 1 and `n` as 0. `{tree}` in a
string is replaced by the build tree.

### feeds.toml

```toml
[[source]]
name = "SomeDriver"
kind = "driver"          # driver, kit, patch, package, refer
git = "https://github.com/ORG/REPO"
ref = "main"
path = "drivers/SomeDriver"   # optional: fetch only this directory
commit = "..."                # optional pin
local = "~/Code/dev/x"        # optional: use a local directory instead
```

Fetching uses `git fetch --depth 1 --filter=blob:none` and a cone sparse checkout, one clone per
repository and ref. UniXNU repositories do not use submodules; the `submodules` key exists only for upstream
trees such as edk2. `unixnu.lock` in the device repository records the commit of every fetched source.

## Status

Observed on the Xiaomi Pad 6S Pro with `fastboot boot` of an image built by the driver (firmware compiled from
source, payload with a prelinked kernel, trust cache and Data seed supplied as inputs):

| Stage | State |
| --- | --- |
| Stock ABL loads the boot image, BootShim starts the UEFI firmware | works |
| UEFI (Silicium) and CrabApple | works; Silicium stalls at random and a retry of the same image starts it |
| CrabApple finds and loads the payload volume (ramdisk) | works |
| XNU and userland | reached with images from the earlier workflow; not yet recorded for driver-built images |

## Limits

- The kernel step needs prebuilt static libraries (corecrypto, ArmCpu, NativeUfs, NativeAES, NativeAdreno and
  others) that separate scripts produce. Running them from the driver is planned. Meanwhile pass an existing
  prelinked kernel with `--kernel`.
- The kernel and userdata steps read the IPSW. Only the iPad7,1 iPadOS 17.7.11 (21H461) image is described by a board.
- The firmware step needs the vendor binaries of the board (`Binaries/<board>`), and the board preflight script
  still expects the old layout, so use `--skip-preflight`.
- The firmware volume differs between two builds from the same sources. The boot image is deterministic for the
  same firmware volume, device tree and ramdisk.
- Many scripts in `scripts/` still use the old layout (`build/`, `.cache/`, `refer/`) and are being generalized.
- The Data seed of the boot payload is an input.
- The userdata image reproduced a previously built image byte for byte. It has not been flashed from this driver.
