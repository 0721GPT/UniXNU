#!/usr/bin/env python3
"""Build Turnip (Mesa freedreno Vulkan) with the native Darwin kernel backend (patches/mesa/turnip-darwin-kmd.patch).

--target host builds for the Mac only to prove the backend compiles and links against the real Turnip; --target ios
cross-builds the ARM64/iOS 17 library. Neither runs a GPU: the backend talks to the NativeAdreno IOKit service, which
exists only on the tablet. Output stays under build/ and is never committed."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', choices=('host', 'ios'), default='host')
parser.add_argument('--configure-only', action='store_true')
args = parser.parse_args()
SOURCE = ROOT / '.cache/mesa-work'
OUT = ROOT / ('build/mesa-tu' if args.target == 'host' else 'build/mesa-tu-ios')
LOGS = ROOT / '.cache'


def read(cmd):
    return subprocess.check_output([str(c) for c in cmd], text=True).strip()


if shutil.disk_usage(ROOT).free < 8 * 1024**3:
    sys.exit('Require 8 GiB free before the Turnip build to preserve the 5-GiB floor')
pins = json.loads((ROOT / 'scripts/data/adreno-compiler-inputs.json').read_text())
if read(['git', '-C', SOURCE, 'rev-parse', 'HEAD']) != pins['mesa']['commit']:
    sys.exit('Pinned Mesa source differs')
for patch in pins['patches']:
    if subprocess.run(['git', '-C', SOURCE, 'apply', '--reverse', '--check', ROOT / patch], capture_output=True).returncode:
        sys.exit('Recorded patch is not applied to the Mesa tree: ' + patch)
if not shutil.which('glslangValidator'):
    sys.exit('glslangValidator is required by the Vulkan runtime build (brew install glslang)')

env = {k: v for k, v in os.environ.items() if not k.startswith(('IR3_', 'FD_MESA_', 'NIR_', 'MESA_SHADER_', 'TU_'))}
env['PATH'] = str(ROOT / '.cache/mesa-tools/bin') + ':' + str(ROOT / '.cache/mesa-venv/bin') + ':' + env['PATH']
options = ['-Dgallium-drivers=', '-Dvulkan-drivers=freedreno', '-Dfreedreno-kmds=darwin',
           '-Dtu-darwin-abi-dir=' + str(ROOT / 'drivers/NativeAdreno'), '-Dplatforms=macos', '-Dglx=disabled',
           '-Degl=disabled', '-Dgbm=disabled', '-Dopengl=false', '-Dgles1=disabled', '-Dgles2=disabled',
           '-Dllvm=disabled', '-Dshared-glapi=disabled', '-Dxmlconfig=disabled', '-Dshader-cache=disabled',
           '-Dtools=', '-Dbuild-tests=false', '-Dvideo-codecs=', '-Dvulkan-layers=']
setup = [ROOT / '.cache/mesa-venv/bin/meson', 'setup', OUT, SOURCE, '--wrap-mode=nodownload']
if args.target == 'ios':
    sdk = read(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'])
    find = lambda tool: read(['xcrun', '--sdk', 'iphoneos', '--find', tool])
    flags = ['-target', 'arm64-apple-ios17.0', '-isysroot', sdk, '-fstack-protector-strong']
    cross = ROOT / 'build/mesa-tu-ios.cross'
    cross.parent.mkdir(exist_ok=True)
    cross.write_text(
        '[binaries]\nc = %r\ncpp = %r\nobjc = %r\nar = %r\nstrip = %r\npkg-config = \'/usr/bin/false\'\n'
        % (find('clang'), find('clang++'), find('clang'), find('ar'), find('strip')) +
        "[host_machine]\nsystem = 'darwin'\ncpu_family = 'aarch64'\ncpu = 'arm64'\nendian = 'little'\n"
        "[properties]\nneeds_exe_wrapper = true\n[built-in options]\n" +
        ''.join('%s = %r\n' % (k, flags) for k in ('c_args', 'cpp_args', 'objc_args', 'c_link_args', 'cpp_link_args',
                                                    'objc_link_args')))
    setup += ['--cross-file', cross]
    options += ['-Dlibunwind=disabled', '-Dlmsensors=disabled', '-Dvalgrind=disabled', '-Dexpat=disabled',
                '-Dzstd=disabled']
    env.update(CC=find('clang'), CXX=find('clang++'), CFLAGS='', CXXFLAGS='', CPPFLAGS='', LDFLAGS='')
else:
    env.update(CC='/usr/bin/clang', CXX='/usr/bin/clang++')
log = LOGS / ('mesa-tu-%s-configure.log' % args.target)
if not (OUT / 'build.ninja').exists():
    with log.open('w') as stream:
        if subprocess.run([str(c) for c in setup + options], env=env, stdout=stream, stderr=subprocess.STDOUT).returncode:
            sys.exit('Configure failed; see ' + str(log) + '\n' + '\n'.join(log.read_text().splitlines()[-12:]))
if args.configure_only:
    print('PASS: Turnip darwin-kmd configuration (%s)' % args.target)
    sys.exit(0)
build_log = LOGS / ('mesa-tu-%s-build.log' % args.target)
with build_log.open('w') as stream:
    result = subprocess.run(['ninja', '-C', str(OUT), '-j6'], env=env, stdout=stream, stderr=subprocess.STDOUT)
if result.returncode:
    sys.exit('Build failed; see ' + str(build_log) + '\n' + '\n'.join(
        l for l in build_log.read_text().splitlines() if 'error' in l.lower())[:3000])
library = OUT / 'src/freedreno/vulkan/libvulkan_freedreno.dylib'
symbols = subprocess.run(['nm', '-gU', str(library)], capture_output=True, text=True).stdout
if ' T _vk_icdGetInstanceProcAddr' not in symbols:
    sys.exit('The library does not export vk_icdGetInstanceProcAddr')
print('PASS: Turnip with the darwin kernel backend builds for %s (%d bytes); no GPU or IOKit service was exercised'
      % (args.target, library.stat().st_size))
