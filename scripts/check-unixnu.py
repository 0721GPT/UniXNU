#!/usr/bin/env python3
"""Offline self-test of the unixnu build driver: defconfig parsing and module precedence."""
import argparse
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unixnu import cli, feeds
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
    print('PASS: unixnu defconfig parsing and module precedence')


if __name__ == '__main__':
    main()
