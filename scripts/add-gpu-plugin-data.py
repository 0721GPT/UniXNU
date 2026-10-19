#!/usr/bin/env python3
"""Stage the owner-built Adreno Metal plugin into a copy of a packed Data seed.

  add-gpu-plugin-data.py BASE.dbr OUTPUT.dbr

Adds /private/var/ngpu/addons/NativeAdrenoMetal.bundle, /private/var/ngpu/bin/adrenoprobed and /private/var/ngpu/daemons/*.plist (from build/adreno-metal-plugin, see build-adreno-metal-plugin.py). The
kernel's boot-time addon mount (sheng-gpu-addon=1) presents that directory as a read-only union view over
/System/Library/Extensions, which is where the original Metal discovery looks for MetalPluginName. BASE is not modified and the
volume UUID is unchanged. Nothing is written to Git or to the OS inputs."""
import json
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'drivers/DarwinBoot/Test'))
import packed_ramdisk

BUNDLE = ROOT / 'build/adreno-metal-plugin/NativeAdrenoMetal.bundle'


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    base, output = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    if output.exists():
        sys.exit('Refuse to overwrite ' + str(output))
    if not (BUNDLE / 'NativeAdrenoMetal').is_file():
        sys.exit('Build the plugin first: python3 scripts/build-adreno-metal-plugin.py')
    blob = base.read_bytes()
    logical, extents = packed_ramdisk.regions(blob)
    with tempfile.TemporaryDirectory(prefix='data-seed-', dir=str(ROOT / 'build')) as td:
        raw = Path(td) / 'seed.raw'
        with raw.open('wb') as f:
            f.truncate(logical)
            for offset, data in extents:
                f.seek(offset)
                f.write(data)
        mount = Path(td) / 'mount'
        mount.mkdir()
        attached = plistlib.loads(subprocess.check_output(
            ['hdiutil', 'attach', '-imagekey', 'diskimage-class=CRawDiskImage', '-nobrowse', '-owners', 'off',
             '-mountpoint', str(mount), '-plist', str(raw)]))
        device = attached['system-entities'][0]['dev-entry']
        try:
            if not (mount / 'mobile').is_dir():
                sys.exit('The Data seed has no /mobile; wrong base image?')
            target = mount / 'ngpu/addons/NativeAdrenoMetal.bundle'
            if target.exists():
                shutil.rmtree(target)
            target.parent.mkdir(parents=True)
            for directory in (mount / 'ngpu', mount / 'ngpu/addons'):
                os.chmod(directory, 0o755)
            shutil.copytree(BUNDLE, target, symlinks=False)
            daemon_dir, bin_dir = mount / 'ngpu/daemons', mount / 'ngpu/bin'
            for directory in (daemon_dir, bin_dir):
                if directory.exists():
                    shutil.rmtree(directory)
                directory.mkdir(parents=True)
                os.chmod(directory, 0o755)
            shutil.copyfile(BUNDLE.parent / 'adrenoprobed', bin_dir / 'adrenoprobed')
            os.chmod(bin_dir / 'adrenoprobed', 0o755)
            for plist in sorted(BUNDLE.parent.glob('*.plist')):
                if plist.name.endswith('-entitlements.plist'):
                    continue   # build input, not a launchd job
                shutil.copyfile(plist, daemon_dir / plist.name)
                os.chmod(daemon_dir / plist.name, 0o644)
            for path in [target, *target.rglob('*')]:
                os.chmod(path, 0o755 if path.is_dir() or path.name in ('NativeAdrenoMetal', 'libvulkan_freedreno.dylib') else 0o644)
            print('staged', target.relative_to(mount), sorted(p.name for p in target.iterdir()))
        finally:
            subprocess.run(['hdiutil', 'detach', device], check=True)
        if raw.read_bytes()[32:36] != b'NXSB':
            sys.exit('The edited image is no APFS container any more')
        metadata = packed_ramdisk.pack(raw, output)
    report = dict(base_sha256=packed_ramdisk.inspect(base)[0]['sha256'], output=metadata,
                  plugin=json.loads((ROOT / 'build/adreno-metal-plugin/trust.json').read_text()))
    output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
