"""Create an APFS Data seed on macOS from the fuxi board's matching IPSW."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tomllib
from types import SimpleNamespace


def run(*args):
    return subprocess.check_output(args)


def attach(image, mount, readonly=False):
    command = ['hdiutil', 'attach', '-nobrowse', '-owners', 'on', '-mountpoint', str(mount), '-plist']
    if readonly:
        command += ['-readonly']
    else:
        command += ['-imagekey', 'diskimage-class=CRawDiskImage']
    info = plistlib.loads(run(*command, str(image)))
    device = next(e['dev-entry'] for e in info['system-entities'] if e.get('dev-entry'))
    volume = next(e['dev-entry'] for e in info['system-entities'] if e.get('mount-point') == str(mount))
    return device, volume


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--board', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    core, output = args.core.resolve(), args.output.resolve()
    board = tomllib.loads(args.board.read_text())
    if board['ipsw']['build'] != '20H392':
        raise SystemExit('This seed targets the installed 20H392 root only')
    output.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(core / 'scripts'))
    sys.path.insert(0, str(core / 'drivers/DarwinBoot/Test'))
    from unixnu.ipsw import Ipsw
    import packed_ramdisk
    ctx = SimpleNamespace(board=board, out_root=output / 'cache', option_text=lambda *_: None)
    ipsw = Ipsw(ctx)
    os_image = ipsw.extract('OS')
    source_mount, data_mount = output / 'os-mount', output / 'data-mount'
    source_mount.mkdir()
    data_mount.mkdir()
    raw = output / 'data-raw.cdr'
    run('hdiutil', 'create', '-size', '3g', '-layout', 'NONE', '-fs', 'APFS',
        '-volname', 'Data', '-type', 'UDTO', str(raw))
    source_device = data_device = None
    try:
        source_device, _ = attach(os_image, source_mount, readonly=True)
        source = source_mount / 'private/var'
        if not source.is_dir():
            raise RuntimeError('The matching OS has no initial private/var tree')
        version = plistlib.loads((source_mount / 'System/Library/CoreServices/SystemVersion.plist').read_bytes())
        if version['ProductBuildVersion'] != '20H392':
            raise RuntimeError('Mounted OS build differs from the installed root')
        data_device, data_volume = attach(raw, data_mount)
        # ditto preserves source ownership, symlinks, ACLs and extended attributes.
        for child in source.iterdir():
            if child.name != 'staged_system_apps':
                run('ditto', '--rsrc', '--extattr', '--acl', str(child), str(data_mount / child.name))
        directories = ('db/dslocal/nodes/Default/users', 'db/dslocal/nodes/Default/groups',
                       'db/dslocal/nodes/Default/config', 'containers/Data/System',
                       'containers/Data/Application', 'containers/Shared/SystemGroup',
                       'PersonaVolumes', 'preboot', 'root', 'tmp', 'run', 'Library',
                       'keybags/backup', 'mobile/Library/Preferences', 'mobile/Library/Caches',
                       'mobile/Library/Logs', 'mobile/Containers', 'mobile/Documents',
                       'mobile/Media', 'mobile/tmp')
        for name in directories:
            dest = data_mount / name
            dest.mkdir(parents=True, exist_ok=True)
            if name.startswith('mobile/'):
                os.chown(dest, 501, 501)
        os.chmod(data_mount / 'tmp', 0o1777)
        os.chmod(data_mount / 'mobile/tmp', 0o1777)
        info = plistlib.loads(run('diskutil', 'info', '-plist', data_volume))
        volume_uuid = info['VolumeUUID']
    finally:
        if data_device:
            run('hdiutil', 'detach', data_device)
        if source_device:
            run('hdiutil', 'detach', source_device)
    metadata = packed_ramdisk.pack(raw, output / 'data.dbr')
    # Both names are the same DBRDISK1 representation, accepted by DarwinBoot.
    shutil.copyfile(output / 'data.dbr', output / 'scratch-data.img')
    report = dict(device='Xiaomi 13 (fuxi)', os_version=version['ProductVersion'],
                  os_build=version['ProductBuildVersion'], volume_uuid=volume_uuid,
                  format='DBRDISK1', logical_bytes=raw.stat().st_size, packed=metadata,
                  setup_skipped=False, root_authentication_fixed=False,
                  source='matching IPSW private/var, excluding staged_system_apps',
                  owner_keybags_created=False, hardware_tested=False)
    (output / 'data-seed.json').write_text(json.dumps(report, indent=2) + '\n')
    (output / 'README.txt').write_text(
        'Xiaomi 13 / fuxi Data seed for iOS 16.7.16 (20H392)\n'
        'data.dbr and scratch-data.img contain identical DBRDISK1 packed APFS bytes.\n'
        'Logical capacity: 3 GiB. This is NOT an Android userdata partition image.\n'
        'Build payload with: ./unixnu build payload --data-ramdisk /path/to/data.dbr\n'
        'Enable the board packed_data_ramdisk option if the board delivery path requires it.\n'
        'Do not fastboot flash userdata with either seed. Existing SHNGROOT stays separate.\n'
        'Root authentication and the existing BSD panic are not changed by this seed.\n'
        'Setup is unchanged; no activation records, keybags or owner credentials are generated.\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
