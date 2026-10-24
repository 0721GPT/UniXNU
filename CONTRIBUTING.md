# Contributing to UniXNU

Read [AGENTS.md](AGENTS.md) and the [documentation](https://unixnu.com) before changing the build driver, the kernel patches or a device port. The [repository layout](https://unixnu.com/contributing/repository) page describes the module system.

## Repository scope

| Repository | Changes |
| --- | --- |
| `UniXNU` | Build tools, component catalog, ARM64 and OS profiles, template, UEFI package and documentation site |
| `UniXNU-<soc>-common` | SoC configuration, DTSI, platform operations and patches |
| `UniXNU-<device>` | Board DTS, peripheral inputs, component selection, partition layout and packaging |
| Component repository | Driver, library, loader or tool with a defined hardware, protocol or functional scope |
| `uefi-<device>` | Mu-Silicium UEFI port |

Board policy belongs in the device repository. Shared SoC operations belong in the SoC repository. Reusable drivers and libraries belong in component repositories. Build tools, ARM64 support, OS profiles and site documentation belong in UniXNU.

Profiles select catalog components by canonical ID. Feeds and submodules supply source repositories. Inspect component scope, OS ABI and board dependencies before adding an implementation.

## Repository topics

Every repository in the UniXNU organization carries the `unixnu` topic. Every independently maintained driver repository also carries the topic before a device repository references it. Add the topic before a repository is linked from the documentation or used as a module.

## Private blobs

Do not commit Apple IPSW files, kernelcaches, disk images, trust caches, Data seeds, vendor firmware or other proprietary binaries. Keep them in a local cache or external storage. The macOS runner deletes these inputs after the artifacts are uploaded.

## Development

```sh
git clone https://github.com/UniXNU/UniXNU
cd UniXNU
./unixnu defconfig <name> -D ../UniXNU-<device>
./unixnu fetch
./unixnu build kernel package firmware payload
./unixnu bootimg --uefi ../uefi-<device> --dtb <board>.dtb
```

Use `--local-module` or `--local` to test a module from a working directory without committing it.

## Change guidelines

- State the current behavior in the code and documentation.
- Keep one subject per commit.
- Add a test when it prevents a failure a future edit would introduce. A behavior visible on every run needs no assertion.
- Put device-specific values in the device repository.
- Put shared SoC operations in the SoC repository and reusable implementations in component repositories.
- Keep the documentation site, navigation, assets and deployment in `UniXNU`.
- Keep the English and Simplified Chinese documentation pages in sync.

## Commit messages

```
feat(<area>): <summary>
fix(<area>): <summary>
docs: <summary>
build(<area>): <summary>
chore: <summary>
```

The message describes the change. The documentation describes the resulting state.

## Kernel patches

1. Edit `out/<device>/<os_profile>/<config>/tree/.cache/xnu-work` after kernel preparation.
2. Generate the patch with `git diff`.
3. Add it to the owning profile's patch series in dependency order.
4. Rebuild the kernel and the prelink.
5. Record the kernelcache pin if the ABI changed.

`unixnu status --json` reports the resolved patch order.

## Drivers

- One hardware driver per repository.
- SoC and device profiles select components through `components.toml`.
- A driver is contributed to the UniXNU organization only when it serves an official device or the generic stack. Otherwise it stays independently maintained and is linked as a submodule.
- Every driver repository carries the `unixnu` topic.
- Built-in XNU drivers compile against the recorded XNU flags in `*.cpo.json`.
- UEFI drivers use the EDK II module layout.
- Kext bundles keep the standard bundle layout.
- A driver that touches hardware reports each stage through a boot argument while it is under bring-up.

## Documentation

After a device port, contribute the device page and the porting notes to this repository.

The documentation source is in `docs/`. The site is bilingual:

| Locale | Path |
| --- | --- |
| English | `docs/` |
| Simplified Chinese | `docs/zh/` |

```sh
cd docs
npm install
npm run dev
npm run build
```

Add a page to both locales and to the sidebar in `docs/.vitepress/config.mts`.

## Security

Report a security issue privately to the maintainers. Do not open a public issue for a trust cache, code signing or boot policy defect.

## License

Contributions are licensed under BSD-2-Clause-Patent. The `LICENSE` file in each directory applies to that directory.
