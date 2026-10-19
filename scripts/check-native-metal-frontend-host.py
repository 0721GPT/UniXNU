#!/usr/bin/env python3
"""Host qualification of the native Metal frontend over the bridge (drivers/NativeMetal/NativeBridgeMetalFrontend.mm, bridge/NativeMetalObjects.rs).

Builds the bridge for the host, the frontend and its public-API test (bridge/frontend_host.mm), and runs the test over a host Vulkan driver given
as the runtime's loader: MoltenVK (brew install molten-vk) on this Mac. That is an offline instrument for the frontend's and the runtime's
behaviour; it says nothing of Turnip, the Adreno 740 or its SMMU. With the target's QuartzCore library on a local mount (never committed) the
test also translates it. The frontend is cross-compiled for arm64-apple-ios17.0 as well. Output stays under build/native-metal-frontend-host."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/native-metal-frontend-host'
KIT = ROOT / 'drivers/NativeMetal'
QUARTZCORE = ROOT / '.cache/firmware/target-volumes/os-mount/System/Library/Frameworks/QuartzCore.framework/default.metallib'
loader = next((p for p in [os.environ.get('NMB_HOST_LOADER', ''), '/opt/homebrew/lib/libMoltenVK.dylib'] if p and Path(p).is_file()), None)
if not loader:
    raise SystemExit('A host Vulkan driver is required (brew install molten-vk, or NMB_HOST_LOADER)')
subprocess.run([sys.executable, ROOT / 'scripts/build-native-metal-bridge.py', '--target', 'host'], check=True)
OUT.mkdir(parents=True, exist_ok=True)
flags = ['-std=c++17', '-fobjc-arc', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-deprecated-declarations', '-I', KIT]
test = OUT / 'frontend_host'
subprocess.run(['xcrun', 'clang++', *flags, KIT / 'NativeBridgeMetalFrontend.mm', KIT / 'bridge/frontend_host.mm',
                ROOT / 'build/native-metal-bridge/host/libnative_metal_bridge.a', '-framework', 'Foundation', '-framework', 'Metal',
                '-framework', 'IOSurface', '-framework', 'CoreFoundation', '-o', test], check=True)
sdk = subprocess.check_output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'], text=True).strip()
subprocess.run(['xcrun', 'clang++', '-target', 'arm64-apple-ios17.0', '-isysroot', sdk, *flags, '-c', KIT / 'NativeBridgeMetalFrontend.mm',
                '-o', OUT / 'NativeBridgeMetalFrontend-ios.o'], check=True)
cache = OUT / 'shader-cache'
cache.mkdir(exist_ok=True)
args = [str(test), loader, str(cache), str(QUARTZCORE) if QUARTZCORE.is_file() else '']
result = subprocess.run(args, capture_output=True, text=True, timeout=1800)
(OUT / 'frontend_host.log').write_text(result.stdout + result.stderr)
lines = [l for l in result.stdout.splitlines() if l.startswith(('PASS', 'FAIL', 'QuartzCore', 'missing'))]
print('\n'.join(lines))
if result.returncode or not any(l.startswith('PASS') for l in lines):
    sys.exit('native Metal frontend host check failed (exit %d); see %s' % (result.returncode, OUT / 'frontend_host.log'))
# The plugin's bridge phase in order over the host simulation (scripts/check-turnip-darwin-host.py builds it): self-test, then the frontend on
# a second Turnip device. Results are not checked there (the software CP executes nothing); every step must run and retire.
HOSTSIM = ROOT / 'build/turnip-darwin-host/libhostiokit.dylib'
HOST_TURNIP = ROOT / 'build/mesa-tu/src/freedreno/vulkan/libvulkan_freedreno.dylib'
if not HOSTSIM.is_file() or not HOST_TURNIP.is_file():
    sys.exit('Build the host simulation first: python3 scripts/check-turnip-darwin-host.py')
sequence = OUT / 'plugin_sequence_host'
subprocess.run(['xcrun', 'clang++', *flags, KIT / 'NativeBridgeMetalFrontend.mm', KIT / 'bridge/plugin_sequence_host.mm',
                ROOT / 'build/native-metal-bridge/host/libnative_metal_bridge.a', '-framework', 'Foundation', '-framework', 'Metal',
                '-framework', 'IOSurface', '-framework', 'CoreFoundation', '-o', sequence], check=True)
hostcache = OUT / 'hostsim-cache'
hostcache.mkdir(exist_ok=True)
env = dict(os.environ, DYLD_INSERT_LIBRARIES=str(HOSTSIM), MESA_LOG='', TU_DEBUG='nocb')
env.pop('TU_DARWIN_SERVICE', None)
run = subprocess.run([str(sequence), str(HOST_TURNIP), str(hostcache)], env=env, capture_output=True, text=True, timeout=1800)
(OUT / 'plugin_sequence_host.log').write_text(run.stdout + run.stderr)
summary = [l for l in run.stdout.splitlines() if l.startswith(('selftest', 'frontend'))]
print('\n'.join(summary))
if run.returncode or not any(l.startswith('selftest code 0 ') for l in summary) or not any(l.endswith('last 660f') for l in summary):
    sys.exit('the plugin bridge phase did not run through on the host simulation; see %s' % (OUT / 'plugin_sequence_host.log'))
print('PASS: plugin bridge phase over the host simulation: self-test, second device, every frontend step submitted and retired (results unchecked)')
