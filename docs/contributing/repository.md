# Repository layout

The base repository and every device repository share the same layout.

```
UniXNU/
  unixnu                  entry point
  scripts/unixnu/         build driver
  package/                UniXNU EDK II package
  drivers/                built-in XNU drivers
  kits/                   kernel libraries
  patches/                XNU, libpthread and Mesa patches
  scripts/                build and verification scripts
  docs/                   this site
  assets/                 logos
  feeds.toml              pinned external sources
  examples/device/        sample device repository
```

## Module kinds

| Kind | Directory | Contents |
| --- | --- | --- |
| driver | `drivers/<Name>` | XNU driver source and reference plist |
| kit | `kits/<Name>` | Kernel library |
| patch | `patches/<Name>` | Patch set |
| script | `scripts/` | Build and verification scripts |
| package | `package/` | UniXNU package |
| refer | fetched tree | Apple source or edk2 |

A device repository can supply any kind. Its module overrides the base module with the same name.

## Resolution order

1. `--local-module [KIND:]NAME=PATH`
2. `--local-<kind> DIR`
3. `--local DIR`
4. a `local` key in `feeds.toml` or `local.toml`
5. the device repository
6. the base repository
7. a git source in `feeds.toml`

## Build tree

`unixnu tree` writes `out/<board>/tree`:

```
tree/
  drivers/  kits/  patches/  refer/
  scripts/  package/
  build/    .cache/
```

Symbolic links point at the selected modules. Scripts are copied. The device `package/` appears as `kits/<board.kit>`.

## Scripts

The build scripts locate the tree from their own path. A script in `scripts/` can read `ROOT`, `build/`, `.cache/`, `refer/`, `drivers/`, `kits/`, `patches/` and `package/`.

## Add a module

1. Create the directory under the matching kind.
2. Add the build or verification logic.
3. Reference it from the board file or the kernel script.
4. Document the module in the matching section.

## Commit scope

A commit changes one subject:

| Prefix | Subject |
| --- | --- |
| `feat(<area>)` | New capability |
| `fix(<area>)` | Defect repair |
| `docs` | Documentation |
| `build(<area>)` | Build system |
| `chore` | Cleanup |

The commit message describes the change. The documentation describes the current state.

## Next steps

- [Device repository](/porting/device-repository)
- [Documentation site](/contributing/documentation)
- [Build](/building)
