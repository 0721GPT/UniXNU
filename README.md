<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-light.svg">
    <img alt="UniXNU" src="assets/logo-light.svg" width="200">
  </picture>
</p>

# UniXNU

An independent project to run Darwin/XNU userland on non-Apple ARM64 hardware through UEFI.

Work in progress, released in a hurry. There is no documented build yet: the scripts are not generic,
and you will have to work out how to build it yourself. The development host is macOS.

Device adaptations are separate projects:

- [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng): Xiaomi Pad 6S Pro (sheng)
- [uefi-sheng](https://github.com/UniXNU/uefi-sheng): UEFI firmware for sheng (Mu-Silicium fork)
