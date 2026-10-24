# ArmDTPlatform

ARM device-tree platform expert. Publishes CPU nubs and every `arm-io` child from the ADT.

| Repository | Maintainer | Scope |
| --- | --- | --- |
| [ArmDTPlatform](https://github.com/UniXNU/ArmDTPlatform) | [UniXNU](https://github.com/UniXNU) | ARM device-tree platform expert. Publishes CPU nubs and every `arm-io` child from the ADT. |

## State

Generic driver. Requires `/cpus`, `/arm-io`, `/defaults`, `/options` and the NVRAM proxy.

## Build

Linked into XNU by `unixnu build kernel`, or built by its own script when the repository is a kit or tool.
