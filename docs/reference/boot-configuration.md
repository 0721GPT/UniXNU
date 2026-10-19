# Boot configuration

The UniXNU boot manager reads `\EFI\UniXNU\unixnu.toml` from the volume it was started from, then from every other volume. A kit directory contains `Kit.toml` with the same match and injection sections.

## unixnu.toml

```toml
version = 1

[boot]
timeout = 5
rotate = 0
fallback_timeout = 10
connect_all = true
default = "xnu"

[ui]
title = "UniXNU"
scale = 0
logo = true
console = "auto"
```

### [boot]

| Key | Default | Meaning |
| --- | --- | --- |
| `timeout` | 5 | Seconds. Negative waits for input |
| `rotate` | 0 | Idle highlight rotation in seconds. 0 disables it |
| `fallback_timeout` | 10 | Timeout when no input device exists |
| `connect_all` | true | Calls `ConnectController` on every handle |
| `default` | first entry | Entry id or index |

### [ui]

| Key | Default | Meaning |
| --- | --- | --- |
| `title` | `UniXNU` | Menu title |
| `scale` | 0 | Glyph scale. 0 selects automatically |
| `logo` | true | Draws the logo |
| `console` | `auto` | `auto`, `graphics`, `text` or `none` |

### [[driver]]

```toml
[[driver]]
path = "Drivers/MyDriver.efi"
args = "board=1"
connect = true
enabled = true
```

| Key | Default | Meaning |
| --- | --- | --- |
| `path` | required | File path or `fv:<GUID>` |
| `args` | | Load options |
| `connect` | true | Connects controllers after the driver starts |
| `enabled` | true | `false` skips the entry |

A kit driver uses `file` instead of `path`.

### [[dt_patch]]

```toml
[[dt_patch]]
op = "set"
node = "/chosen"
prop = "bootargs"
value = "console=ttyAMA0"
type = "string"
```

| Key | Meaning |
| --- | --- |
| `op` | `set`, `delete-prop`, `add-node`, `delete-node` or `overlay` |
| `node` | Absolute node path |
| `prop` | Property name for `set` and `delete-prop` |
| `value` | Value for `set` |
| `type` | `string`, `stringlist`, `u32`, `u64`, `bytes` or `empty` |
| `file` | Overlay file for `overlay` |

Patches apply to EFI and Linux entries. The Darwin path uses the `.adt` sidecar.

### [[block_slice]]

```toml
[[block_slice]]
partition = "<GPT name>"
name = "<slice name>"
offset = 0
length = 0
writable = false
enabled = true
```

| Key | Meaning |
| --- | --- |
| `partition` | GPT partition name |
| `name` | Label in the device path |
| `offset` | Byte offset from the partition start |
| `length` | Byte length. 0 means to the end |
| `writable` | Publishes a writable block device |
| `enabled` | `false` skips the slice |

The slice appears as a whole disk with its own device path. A child partition scheme or file system driver can bind to it.

### ACPI

```toml
[acpi]
add = ["Tables/DSDT.aml", "Tables/SSDT.aml"]
```

Each table is installed through `EFI_ACPI_TABLE_PROTOCOL`. The header length must match the file size.

### Kits

```toml
[[kit]]
path = "Kits/MyKit.kit"
enabled = true

[kits]
autoload = true
directory = "Kits"
```

Explicit kits load first. With `autoload`, every `*.kit` directory under `directory` loads in name order.

### [[entry]]

```toml
[[entry]]
id = "xnu"
title = "UniXNU"
type = "xnu"
path = "Payloads/kernel.macho"
args = "-v serial=3"
initrd = "Payloads/scratch-data.img"
enabled = true
```

| Key | Meaning |
| --- | --- |
| `id` | Entry identifier |
| `title` | Menu title |
| `type` | `efi`, `linux`, `action` or `xnu` |
| `path` | EFI image, Linux image or kernel path |
| `args` | Load options or kernel command line |
| `dtb` | `firmware`, a file path, or absent |
| `initrd` | Initrd path |
| `action` | `reset`, `shutdown` or `exit` for `action` entries |
| `enabled` | `false` skips the entry |

The built-in configuration has `exit`, `reset` and `shutdown` entries. When no file is found, removable loaders are added in front of them.

### [entry.chosen]

```toml
[entry.chosen]
bootargs = "console=ttyAMA0"
```

Every key becomes a `/chosen` property. These patches apply after the entry patches.

### [[entry.kext]]

```toml
[[entry.kext]]
bundle = "Kexts/MyDriver.kext"
plist = "Contents/Info.plist"
executable = "Contents/MacOS/MyDriver"
enabled = true
```

Entry kexts apply only to the entry that declares them.

### [[entry.variable]]

```toml
[[entry.variable]]
guid = "00000000-0000-0000-0000-000000000000"
name = "MyVariable"
value = "hello"
type = "ascii"
volatile = true
```

| Key | Meaning |
| --- | --- |
| `guid` | Variable GUID |
| `name` | Variable name |
| `value` | Value |
| `type` | `ascii`, `ucs2`, `u8`, `u16`, `u32`, `u64` or `bytes` |
| `volatile` | `false` adds `EFI_VARIABLE_NON_VOLATILE` |

The variables are set before the entry starts.

### [[entry.dt_patch]]

The entry accepts its own `[[dt_patch]]` array. Entry patches apply after global patches.

## Kit.toml

```toml
schema = 1

[kit]
name = "MyKit"
version = "1.0.0"
description = "Board support"

[match]
arch = "AARCH64"
fdt_compatible = ["vendor,board"]
firmware_vendor = ["Vendor"]
requires_protocol = ["00000000-0000-0000-0000-000000000000"]

[[driver]]
file = "MyDriver.efi"
args = "from-kit"

[[overlay]]
file = "board.dtbo"

[[dt_patch]]
op = "set"
node = "/chosen"
prop = "from-kit"
value = "1"
type = "u32"

[[acpi]]
file = "DSDT.aml"

[[kext]]
bundle = "MyDriver.kext"
entry = "xnu"
```

| Section | Meaning |
| --- | --- |
| `[kit]` | Name, version and description |
| `[match]` | All present keys must match |
| `[[driver]]` | Driver file relative to the kit |
| `[[overlay]]` | Device tree overlay relative to the kit |
| `[[dt_patch]]` | Device tree patch |
| `[[acpi]]` | ACPI table relative to the kit |
| `[[kext]]` | Kext bundle scoped to an entry id |

Match keys:

| Key | Meaning |
| --- | --- |
| `arch` | `AARCH64` |
| `fdt_compatible` | Strings in the firmware root `compatible` list |
| `firmware_vendor` | Substring of `gST->FirmwareVendor` |
| `requires_protocol` | GUIDs that must be installed |

Within a key, any listed value matches. Across keys, every key must match.

## Menu behavior

| Condition | Result |
| --- | --- |
| Input device present | Countdown runs to the timeout |
| No input device | `fallback_timeout` applies |
| Negative timeout with input | Waits for a key |
| Negative timeout without input | `fallback_timeout` applies |
| Failed boot | Returns to the menu without a countdown |

The menu shows the configuration source, the entry list, the injection summary and the first error.

## Next steps

- [Payload](/concepts/payload)
- [Kext bundles](/drivers/kexts)
- [Device configuration](/reference/configuration)
