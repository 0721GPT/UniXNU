# Building UniXNU

This describes the `unixnu` build driver. The project was released in a hurry: the driver is new,
only the boot image step has been checked end to end, and the kernel, package and firmware steps
call scripts that still carry assumptions from the old single repository. Expect to read logs.

The development host is macOS. Boot images are only ever started with `fastboot boot`. Nothing here
flashes a device.

## Requirements

- macOS, Python 3.11 or newer (standard library only, nothing to install with pip)
- `git`, `make`, `cmake`, `ninja`, `unifdef`
- LLVM and lld from Homebrew (`clang`, `llvm-objcopy`, `llvm-nm`, `ld.lld`). The keg-only
  `/opt/homebrew/opt/llvm/bin` is found without being on `PATH`.
- `dtc` (provides `fdtput` and `fdtget`)
- Network access for `unixnu fetch`

You also provide inputs that are never part of a repository:

| Input | Used by | Option |
| --- | --- | --- |
| A Mu-Silicium checkout for the device (for sheng: uefi-sheng) | firmware, bootimg | `--uefi DIR` or `UNIXNU_UEFI` |
| The original decompressed kernelcache of the target OS | kernel (prelink) | `--kernelcache FILE` or `UNIXNU_KERNELCACHE` |
| A compiled device tree | bootimg | `--dtb FILE` or `UNIXNU_DTB` |
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
./unixnu build kernel --kernelcache /path/to/kernelcache.decompressed
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
| `build [kernel] [package] [firmware] [prelink]` | Builds the steps (default: kernel, package, firmware). `prelink` repeats only the prelink. |
| `bootimg` | Packs BootShim, the firmware volume and the device tree into an Android boot image. |
| `status` | Shows the device, the configuration, and where every module comes from. |
| `clean` | Removes the build output of the board and keeps fetched sources. |

Common options: `-D DIR` device repository, `--out DIR` output root, `-j N` jobs, `--target DEBUG|RELEASE`,
`--uefi`, `--edk2`, `--kernelcache`, and the local source options below.
Environment: `UNIXNU_DEVICE`, `UNIXNU_OUT`, `UNIXNU_UEFI`, `UNIXNU_EDK2`, `UNIXNU_KERNELCACHE`,
`UNIXNU_DTB`, `JOBS`.

## Steps

**kernel** applies `patches/xnu` and the prelink patches to Apple's xnu-10063.141.1, builds the kernel with
the switches of the defconfig, then prelinks it against the original kernelcache with
`kits/NativePrelink`. The result is `out/<board>/kernel/kernel.macho`. If the board lists a
`kernelcache_sha256`, any other kernelcache is refused. The XNU sources, libplatform, libdispatch and
AvailabilityVersions are fetched on demand.

**package** builds `package/` (UniXNU itself) with EDK II into `out/<board>/package/Build/Out`.

**firmware** builds the device firmware from the Mu-Silicium checkout. It runs the preflight scripts of the
board, applies the board's patches to edk2 for the build only (they are always reverted), and runs `build` with
the board's DSC and defines. `FD_BASE`, `FD_SIZE` and `FD_BLOCKS` come from the Mu-Silicium device config.

**bootimg** compiles the BootShim, appends the firmware volume, compresses it with gzip, appends the device
tree (after adding the board's `dtb_node` entries), and runs `mkbootimg.py` with the parameters of the
Mu-Silicium device config. It unpacks the result and compares every part with its input, so a mismatch fails
the build. The output is deterministic. A ramdisk (the payload image) is optional with `--ramdisk FILE`; the
payload step is not part of the driver yet.

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
| `[kernel]` | `defconfig` |
| `[kernel.prelink]` | `tool`, `kernel`, `slide`, `roots` (list), `early_security_init`, `kernelcache`, `kernelcache_sha256`, `extra_args` |
| `[firmware]` | `config` (name of `Resources/Configs/<name>.toml`), `dsc`, `fd` (path of the result, `{target}` is replaced), `packages` (`{uefi}`, `{edk2}`, `{tree}` are replaced), `preflight`, `patches` |
| `[firmware.defines]` | extra `-D` definitions |
| `[bootimg.image]` | overrides of the image parameters (`base`, `kernel_offset`, `tags_offset`, `pagesize`, `header_version`) |
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

## Known limits

- Only the boot image step has been verified end to end: with the same inputs it reproduced a previously built
  image byte for byte. It has no ramdisk and has not been started on hardware.
- The kernel, package and firmware steps pass the work to the existing scripts through the build tree. They
  were wired up but not run from scratch here. The xnu patch series applies to a pristine tag.
- Many scripts in `scripts/` still use the old layout (`build/`, `.cache/`, `refer/`) and are meant to be
  generalized later.
- The payload (ramdisk) packaging is not part of the driver yet.
