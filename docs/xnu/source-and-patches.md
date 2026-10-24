# Source and patches

The selected OS profile pins XNU and its ABI inputs. The iPadOS 17.7.11 profile
uses `xnu-10063.141.1`, Darwin 23.6.0, DriverKit headers from `xnu-10063.121.3`,
and TrustCache headers from `xnu-8796.101.5`.

`feeds.toml` supplies libplatform, libdispatch, AvailabilityVersions, libpthread,
corecrypto, IOStorageFamily, and edk2. The format 2 OS profile's explicit XNU
revision takes priority over a recorded source lock.

## Patch application

Format 2 resolves `[kernel] patch_series` in declared order. A series lists one
patch path per line, relative to the series file or prefixed with `arch:`, `soc:`,
`os:`, `device:`, or `component:<ID>/`. Component patches participate in the
same resolved order. `unixnu status --json` prints that order.

Kernel preparation creates `tree/.cache/xnu-work` under the selected target,
checks each patch, applies it, and prepares missing source headers. The target
also owns generated headers and `tree/build` outputs.

Format 1 selects `patches/xnu/*.patch` and `kits/XnuPrelink/patches/*.patch` in
filename order. Its shell preparation handles the late `zzw`, `zzy`, and `zzz`
patches and checks already-applied patches.

## Ownership

| Location | Contents |
| --- | --- |
| ARM64 profile | Common architecture configuration and patch series |
| SoC repository | Hardware operations and SoC patch series |
| OS profile | Version-specific fixes and prelink patch selection |
| Device repository | Board-specific patch series |
| Component repository | Patches required by that component |
| `patches/libpthread/`, `patches/mesa/` | Library and compiler source patches |

## Add a patch

1. Build the kernel once so `.cache/xnu-work` exists.
2. Edit the source in `.cache/xnu-work`.
3. Generate the patch:

```sh
git -C .cache/xnu-work diff -- <paths> > patches/xnu/my-change.patch
```

4. Add the patch to its profile series and run `unixnu build kernel`.

A patch that adds a build option follows the pattern in `native-aes-personality.patch`:

```c
#if defined(XNU_MY_OPTION) && XNU_MY_OPTION
    "   ,{"
    "     'IOClass'         = MyDriver;"
    "     'IOProviderClass' = IOPlatformDevice;"
    "   }"
#endif
```

The matching `-DXNU_MY_OPTION=1` comes from `EXTRA_MAKE_ARGS` in the kernel script.

## Build flags

The kernel build uses these switches:

| Switch | Value |
| --- | --- |
| `SDKROOT` | `iphoneos` |
| `ARCH_CONFIGS` | `ARM64` |
| `MACHINE_CONFIGS` | `BCM2837` |
| `KERNEL_CONFIGS` | `DEVELOPMENT` |
| `ARM64_LINK_ADDR` | `0xfffffff007004000` |
| `ARM64_PLKSEG_ADDR` | `0xfffffff004004000` |
| `DEPLOYMENT_TARGET_FLAGS` | `-miphoneos-version-min=17.0 -DXNU_TARGET_OS_IOS` |
| `BUILD_LTO` | `0` |
| `BOUND_CHECKS` | `0` |

`BCM2837` carries the 4 KiB page layout. `BUILD_LTO=0` keeps the object files and the compile records readable and link-compatible with the external archives.

## Defconfig mapping

Each `CONFIG_XNU_<NAME>` becomes `XNU_<NAME>`:

| Defconfig value | Build value |
| --- | --- |
| `y` | `1` |
| `n` | `0` |
| `"text"` | `text` |
| `123` | `123` |
| `{tree}` in a string | Build tree path |

The kernel script reads the variables and adds object flags, archives and link inputs.

## Source tree

```
out/<device>/<os_profile>/<config>/tree/
  refer/xnu/                 pinned source
  .cache/xnu-work/           working clone
  .cache/fakeroot/           kernel SDK root
  .cache/sources/            restored header manifests
  build/xnu/ios-obj/         objects and compile records
  build/xnu/ios-sym/         symbols
  build/xnu/ios-dst/         install headers
  patches/xnu/               linked patches
  kits/XnuPrelink/        prelink tool and patches
```

## Updating the source

1. Set a new `ref` and `commit` in `feeds.toml`.
2. Update `darwin`, `version`, `driverkit_tag` and `trustcache_tag`.
3. Rebase the patches onto the new tag.
4. Update the archive builders in `[kernel] archives` for the new compile records.
5. Rebuild the prelink and re-qualify the kernelcache pin.

The prelink output depends on the original kernelcache ABI. A new source version can require a new kernelcache pin and new roots.

## Next steps

- [Four-kilobyte pages](/xnu/four-k-page-tables)
- [Patch reference](/xnu/patch-reference)
- [Build the kernel](/guide/build-kernel)
