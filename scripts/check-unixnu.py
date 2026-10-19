#!/usr/bin/env python3
"""Offline self-test of the unixnu build driver: defconfig parsing and module precedence."""
import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unixnu import cli, feeds, payload
from unixnu.config import Context, parse_config
from unixnu.util import Error


def expect(condition, message):
    if not condition:
        raise SystemExit(f'FAIL: {message}')


def main():
    config = parse_config('# c\nCONFIG_A=y\nCONFIG_B=n\n# CONFIG_C is not set\nCONFIG_D="x y"\nCONFIG_E=0x10\n')
    expect(config == {'A': True, 'B': False, 'C': False, 'D': 'x y', 'E': 16}, 'defconfig values')
    try:
        parse_config('CONFIG_A=maybe\n')
        expect(False, 'bad defconfig value accepted')
    except Error:
        pass
    with tempfile.TemporaryDirectory() as name:
        root = Path(name)
        for path in ('dev/drivers/Both', 'dev/drivers/DevOnly', 'loc/drivers/Both', 'loc/drivers/LocOnly', 'kd/Both', 'one/Both'):
            (root / path).mkdir(parents=True)
        (root / 'dev/board.toml').write_text('[board]\nname = "t"\n')
        parsed = cli.parser().parse_args(['status', '-D', str(root / 'dev'), '--out', str(root / 'out')])
        ctx = Context(parsed)
        found = feeds.resolve(ctx)
        expect(found[('driver', 'Both')].origin == 'device', 'device beats base')
        expect(('driver', 'ArmCpu') in found and found[('driver', 'ArmCpu')].origin == 'base', 'base modules found')
        parsed = cli.parser().parse_args(['status', '-D', str(root / 'dev'), '--out', str(root / 'out'), '--local', str(root / 'loc')])
        found = feeds.resolve(Context(parsed))
        expect(found[('driver', 'Both')].origin == 'local (--local)' and ('driver', 'LocOnly') in found, '--local beats device')
        parsed = cli.parser().parse_args(['status', '-D', str(root / 'dev'), '--out', str(root / 'out'), '--local', str(root / 'loc'),
                                          '--local-drivers', str(root / 'kd')])
        found = feeds.resolve(Context(parsed))
        expect(found[('driver', 'Both')].origin == 'local (--local-drivers)', '--local-drivers beats --local')
        parsed = cli.parser().parse_args(['status', '-D', str(root / 'dev'), '--out', str(root / 'out'), '--local', str(root / 'loc'),
                                          '--local-drivers', str(root / 'kd'), '--local-module', f'Both={root / "one"}'])
        found = feeds.resolve(Context(parsed))
        expect(found[('driver', 'Both')].origin == 'local (--local-module)', '--local-module wins')
        try:
            feeds.resolve(Context(cli.parser().parse_args(['status', '-D', str(root / 'dev'), '--out', str(root / 'out'),
                                                            '--local', str(root / 'missing')])))
            expect(False, 'missing --local directory accepted')
        except Error:
            pass
    expect(payload.check_args(['-v', 'a=1', 'b=x,y']) == '-v a=1 b=x,y', 'kernel arguments')
    for bad in (['a b'], ['x' * 1100], ['a;b']):
        try:
            payload.check_args(bad)
            expect(False, f'bad kernel argument accepted: {bad[0][:10]}')
        except Error:
            pass
    with tempfile.TemporaryDirectory() as name:
        root = Path(name)
        stage = root / 'stage/EFI/CrabApple'
        stage.mkdir(parents=True)
        (stage / 'crabapple.toml').write_text('version = 1\n')
        import os
        for path in (root / 'stage', *(root / 'stage').rglob('*')):
            os.utime(path, (payload.EPOCH, payload.EPOCH))
        payload.write_volume(root / 'stage', root / 'a.img', 'TEST', 64)
        payload.write_volume(root / 'stage', root / 'b.img', 'TEST', 64)
        expect((root / 'a.img').read_bytes() == (root / 'b.img').read_bytes(), 'payload volume is reproducible')
    print('PASS: unixnu defconfig parsing, module precedence and payload helpers')


if __name__ == '__main__':
    main()
