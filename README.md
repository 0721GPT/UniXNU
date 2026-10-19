<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-light.svg">
    <img alt="UniXNU" src="assets/logo-light.svg" width="200">
  </picture>
</p>

<p align="center">
  <a href="https://t.me/unixnu"><img alt="Telegram" src="https://img.shields.io/badge/Telegram-@unixnu-26A5E4?logo=telegram&logoColor=white"></a>
  <a href="https://x.com/unixnu"><img alt="X" src="https://img.shields.io/badge/X-@unixnu-000000?logo=x&logoColor=white"></a>
</p>

# UniXNU

An independent project to run Darwin/XNU userland on non-Apple ARM64 hardware through UEFI.

Work in progress. The `unixnu` driver builds the kernel, the UniXNU package, the device firmware and the
boot image (developed on macOS, other hosts untested), see [docs/building.md](docs/building.md). The boot payload (kernel, device tree and
config) and the userdata image for the device are built by the same driver. The target IPSW can be a local file or
a URL.

Device adaptations are separate projects:

- [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng): Xiaomi Pad 6S Pro (sheng)
- [uefi-sheng](https://github.com/UniXNU/uefi-sheng): UEFI firmware for sheng (Mu-Silicium fork)
