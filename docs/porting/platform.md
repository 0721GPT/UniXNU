# Platform, memory and OS services

Start with a [resolved device repository](/porting/). Read the
[device-tree bindings](/porting/device-tree), [matching rules](/drivers/device-tree-matching),
[ArmDTPlatform](/drivers/arm-dt-platform) and [ArmCpu](/drivers/arm-cpu).

## 1. Describe CPU and interrupt resources

Record CPU MPIDR affinities, boot CPU, enabled cores, PSCI method, GIC revision,
distributor/redistributor ranges and stride, timer frequency and interrupt specifiers.
Put shared topology in the SoC DTSI and board enablement in the board DTS.

Compile the board source with its selected SoC include directory. This example
uses the sheng source and a checked-out SM8550 common repository:

```sh
dtc -I dts -O dtb -i ../UniXNU-sm8550-common/dts \
  -o /tmp/sheng-hardware.dtb ../UniXNU-sheng/dts/sheng.dts
dtc -I dtb -O dts /tmp/sheng-hardware.dtb
```

Adapt both paths to the selected board. Inspect `reg`, `ranges`, phandles,
interrupt providers and disabled nodes. The [ADT conversion](/porting/device-tree)
exports enabled hardware nodes and supplies the OS properties.

**Expected result:** the platform expert publishes the intended nubs, the boot
CPU receives timer interrupts, and each enabled secondary CPU reaches its entry
and receives IPIs. On another GIC or PSCI interface, review ArmCpu's implementation
before enabling additional cores.

## 2. Account for memory and DMA

Compare firmware RAM, secure carveouts, framebuffer, DSP/GPU firmware, payload
and kernel allocations. The hardware DTS uses a contiguous handoff RAM range;
the loader's owned-memory request has its own size and alignment constraints.
Keep both consistent with the actual UEFI memory map.

For each DMA device, record address width, physical-to-IOVA mapping, SMMU
instance/stream/context ownership and cache policy. Read [ArmSmmu4K](/drivers/arm-smmu4k)
where its translation model matches the target. GPU, UFS, SPI and PCIe may use
different streams and allocation policies. Verify the consumer's source: several
sheng board paths read C/C++ constants directly.

**Expected result:** every handoff allocation has a defined owner, and each
peripheral can access its allocated DMA range before it submits work.
Continue with [UFS adaptation](/porting/ufs) for the storage controller.

## 3. Connect block and OS services {#block-and-os-services}

| Component | Adaptation work | Expected observation |
| --- | --- | --- |
| [RamBlock](/drivers/ram-block) | Match dense/packed Data format and ADT placeholders. Implement the `native_ufs_store` persistence callbacks for the target storage policy | Data mounts; writes and restart behavior match the selected persistence mode |
| [VirtioMmioBlock](/drivers/virtio-mmio-block) | Supply a VirtIO MMIO device, queues, DMA and interrupt routing; qualify storage ABI bindings | Capacity and reads are correct before mounting root |
| [VerifiedRoot](/drivers/verified-root) | Supply the selected image's owner policy and trust inputs; connect the storage policy archive | Policy validation succeeds for the intended image |
| [SoftwareAES](/drivers/software-aes) | Match XNU AES helpers and IOCryptoAcceleratorFamily ABI; build its bindings for the OS profile | The crypto service publishes and required volume operations complete |
| [AppleKeyStoreState](/drivers/apple-key-store-state) | Qualify selectors and state callbacks against the OS; connect initialization and event callers | Required state queries and notifications complete |
| [XnuPrelink](/drivers/xnu-prelink) | Select matching XNU, original kernelcache, archive bindings and patch order | Prelink resolves the selected drivers and produces the kernel |
| [DarwinBoot](/drivers/darwin-boot) | Include the loader in firmware or payload; provide Mach-O, ADT and selected sidecars | Loader validates the inputs and reports kernel handoff |
| [ShengPreflight](/drivers/sheng-preflight) | Read its sheng checks, then write checks for the target's firmware and memory layout | Target-specific firmware inputs pass their checks |

Use [prelink](/concepts/prelink), [integrity policy](/concepts/security),
[payload](/concepts/payload) and [userdata](/guide/userdata) for the required
artifacts. Keep root UUID, partition geometry, Data persistence and log allocation
in the device configuration. The OS profile supplies its version-specific ABI.

## 4. Check the assembled selection

From the main checkout:

```sh
./unixnu status --json
./unixnu build kernel
```

Inspect dependency order, configuration fragments and patch series before the
build. Each selected archive needs its OS inputs and initialization caller.
The [kernel guide](/guide/build-kernel) describes build outputs and failures.

Record source/build evidence separately from device results in the
[porting record](/porting/records). Next, complete [storage](/porting/ufs), then
[display and brightness](/porting/display).
