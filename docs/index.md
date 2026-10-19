---
layout: home
hero:
  name: UniXNU
  text: Darwin and XNU userland on non-Apple ARM64 hardware
  tagline: A UEFI boot manager, a device tree platform, a patched XNU kernel and a complete boot image.
  image:
    src: /logo-light.svg
    alt: UniXNU logo
  actions:
    - theme: brand
      text: Start here
      link: /guide/overview
    - theme: alt
      text: Port a device
      link: /porting/
    - theme: alt
      text: Device notes
      link: /devices/
---

::: warning Documentation status
The documentation is under active maintenance. Some pages may be incomplete or contain inaccurate details. Check the source repository and the build reports when a detail matters.
:::

## What UniXNU provides

UniXNU is a build system and runtime stack for starting Darwin-based operating systems on ARM64 devices whose firmware is UEFI or can be made UEFI.

| Layer | Component | Role |
| --- | --- | --- |
| Boot image | BootShim, UEFI firmware volume, device tree | Loaded by the stock Android bootloader with `fastboot boot` |
| Boot manager | UniXNU | Loads drivers, applies device tree patches, publishes block slices, runs the menu |
| Loader | DarwinBoot | Validates the kernel Mach-O, ADT and trust cache, exits boot services, enters EL1 |
| Kernel | Patched XNU | Built from Apple source, prelinked against the original kernelcache |
| Devices | XNU drivers and kits | Platform expert, CPU, storage, display, GPU and board power |
| Userland | IPSW disk images | Unchanged OS and SystemOS images on the userdata partition |

## Documentation map

- [Overview](/guide/overview): the stack, the repositories and the build model.
- [Build](/building): clone, configure, fetch, build and pack a boot image.
- [Boot the image](/guide/boot): `fastboot boot`, the menu, serial logs and recovery.
- [Porting](/porting/): add a device repository and a UEFI firmware port.
- [Drivers](/drivers/overview): UEFI drivers, built-in XNU drivers and kext bundles.
- [XNU](/xnu/source-and-patches): source version, patches, four-kilobyte pages and boot arguments.
- [Devices](/devices/): the support model and per-device notes. The sheng page is one device entry.
- [Reference](/reference/cli): every command, configuration key and output file.

## Repositories

| Repository | Contents |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | Build driver, UEFI boot manager, DarwinBoot, generic drivers, kits and patches |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | Device repository for the Xiaomi Pad 6S Pro |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | UEFI firmware port for the Xiaomi Pad 6S Pro |

Device repositories follow the same layout as this repository. Their modules override modules with the same name, and their `package/` becomes a kit in the build tree. The generic documentation applies to every device. Device-specific values live in the repository and in the [Devices](/devices/) section.
