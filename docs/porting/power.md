# DSP, audio and power adaptation

Start after [platform and memory](/porting/platform). The board owns firmware
loading, resource votes and policy. Read the
[sheng package](https://github.com/UniXNU/UniXNU-sheng/tree/main/package) as an
implementation reference and use the target's DTS and firmware inputs.

## 1. Start the DSP services

`AdspProbe.cpp` connects firmware loading, SMEM, SMP2P, GLINK, IPCC and RPMh.
The helpers are `QcomMdt.c`, `QcomSmem.c`, `QcomSmp2p.c`, `QcomGlink.c`,
`QcomQmp.c` and `QcomRpmh.c`. `ShengAdsp.h` exposes channels to battery and audio.

1. Record signed firmware, PAS service IDs, DSP memory/carveouts, command database,
   SMEM items, mailbox interrupts and regulator/clock votes.
2. Adapt those addresses and IDs in the board code. Preserve firmware-owned and
   secure reservations in the DTS and the firmware memory map.
3. Validate firmware loading and authentication before releasing the DSP.
4. Check ready/crash notifications, then GLINK channel negotiation and messages.
5. Connect service callbacks with bounded queues and prompt returns.

sheng's service clients use `sheng-adsp=2`. Inspect `native_adsp_stats` and the
channel states before enabling them. Firmware, reserved regions and remote item
IDs require target-specific verification on another SoC.

**Expected result:** DSP readiness and bidirectional channel traffic, with
failures and restarts visible in the record.

## 2. Adapt the audio path

`AudioProbe.cpp` opens the `adsp_apps` channel and uses `ShengGpr.c` to query the
DSP audio framework. `sheng-audio=1` accepts the remote channel opening;
`sheng-audio=2` initiates it locally. Both require the DSP path above.
`native_audio_stats[0] == 5` records a ready framework response.

To implement playback or capture on a target:

1. Inventory the codec, amplifiers, buses, clocks, regulators and endpoint wiring.
2. Implement the DSP graph and endpoint routing for the selected firmware.
3. Connect an OS audio interface with the required sample formats and buffering.
4. Check route start/stop, buffer completion, audible playback or captured samples,
   then underrun and recovery behavior.

**Current scope:** the sheng source supplies DSP loading, GLINK/GPR transport and
framework readiness probes. Endpoint routing, OS audio publication and
playback/capture qualification are required work for an audio port.

## 3. Publish battery state

`ShengBattery.cpp` implements `IOPMPowerSource`. `ShengBattmgr.c` reads the battery
manager over the DSP's PMIC GLINK channel; `BatteryProbe.cpp` registers the
service. `sheng-battery=1` enables it with `sheng-adsp=2`.

1. Match the target battery-manager protocol, property IDs, units and signed
   values, including temperature, current and capacity.
2. Adapt its channel, request timeouts and notification handling.
3. Publish capacity, charging/external-power state, voltage and current through
   the selected OS power-source ABI.
4. Compare readings with a known reference; observe updates when power is
   connected and disconnected and while capacity changes.

**Expected result:** battery values and notifications reach the OS with correct
units. Charging control remains owned by the DSP firmware in this implementation.
Record charging policy separately from battery reporting.

## 4. Adapt CPU frequency and platform power

`CpuFreqProbe.cpp` reads Qualcomm EPSS frequency tables and controls CPU/L3
performance states. Its sheng domain bases, CPU counts and reference clocks are
board inputs. Map the target CPU affinities to their frequency domains, inspect
the LUT and enabled state, then choose an operating point within the target's
power and thermal policy. Review `sheng-cpufreq` and `sheng-cpufreq-cap` parsing
before applying those modes.

`ShengPlatformPower.cpp` supplies the PMGR integration selected by
`CONFIG_XNU_SHENG_PMGR` and `sheng-pmgr`. Review its initialization and every
consumer before adapting it to another platform. Qualcomm RPMh/QMP/SCM helpers
require the target's firmware interfaces and resource ownership.

**Expected result:** requested frequency changes have observed effects under
load and remain within the selected limits. Qualify peripheral power transitions
and system sleep/resume individually, including restoration of dependent devices.

## 5. Build and record

The sheng display archive includes DSP, audio and battery sources. The separate
platform-power recipe builds PMGR integration. From the assembled tree:

```sh
rg -n 'adsp_|audio_|battery_|cpufreq_' scripts/build-gop-legacy-display.py
rg -n 'sheng-cpufreq|sheng-pmgr' kits/sheng/CpuFreqProbe.cpp kits/sheng/ShengPlatformPower.cpp
```

Adapt the source lists and callers, then [rebuild the kernel and images](/building).
Record DSP readiness, audio routing, battery publication, frequency control and
sleep/resume as separate results. Return to [bring-up](/porting/bring-up).
