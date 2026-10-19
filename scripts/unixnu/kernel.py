"""XNU kernel: build from the patched Apple source, then prelink against the original kernelcache."""
import os
from pathlib import Path

from .util import Error, expand, log, run, sha256_file, tool


def xnu_env(ctx):
    """CONFIG_XNU_* of the defconfig become XNU_* variables of the legacy kernel build script."""
    env = {}
    for key, value in ctx.need_config().items():
        if not key.startswith('XNU_'):
            continue
        if isinstance(value, str):
            value = value.replace('{tree}', str(ctx.tree))
        env[key] = '1' if value is True else '0' if value is False else str(value)
    return env


def build(ctx):
    log('kernel: XNU')
    for name in ('git', 'make', 'clang', 'cmake', 'ninja', 'unifdef'):
        tool(name, 'brew install ' + name)
    script = ctx.tree / 'scripts/build-xnu-4k-baseline.sh'
    if not script.is_file():
        raise Error(f'{script} missing')
    env = dict(os.environ)
    env.update(xnu_env(ctx))
    env['JOBS'] = str(ctx.jobs)
    record = ctx.tree / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/osfmk/DEVELOPMENT/startup.o.json'
    if env.get('XNU_BOOT_PROGRESS') == '1' and not record.is_file():
        # The boot progress layer is generated from the compile record of startup.o, which only a
        # previous build leaves behind, so a first build from scratch runs once without it.
        log('kernel: bootstrap pass without boot progress')
        run(['bash', script], cwd=ctx.tree, env={**env, 'XNU_BOOT_PROGRESS': '0'}, log_file=ctx.logs / 'kernel-bootstrap.log')
    run(['bash', script], cwd=ctx.tree, env=env, log_file=ctx.logs / 'kernel.log')


def prelink(ctx):
    """Produce out/<device>/kernel/kernel.macho from the XNU build and the original kernelcache."""
    log('kernel: prelink')
    spec = ctx.board.get('kernel', {}).get('prelink')
    if not spec:
        raise Error('board.toml has no [kernel.prelink]')
    cache = ctx.option('kernelcache', 'UNIXNU_KERNELCACHE') or (expand(ctx.tree / spec['kernelcache']) if spec.get('kernelcache') else None)
    if not cache or not cache.is_file():
        raise Error('the original decompressed kernelcache is an input you provide: --kernelcache FILE or UNIXNU_KERNELCACHE')
    wanted = spec.get('kernelcache_sha256')
    if wanted and sha256_file(cache) != wanted:
        raise Error(f'{cache} does not match the kernelcache this board was qualified against (sha256 {wanted})')
    kernel = ctx.tree / spec['kernel']
    if not kernel.is_file():
        raise Error(f'{kernel} not found; run "unixnu build kernel" first')
    output = ctx.work / 'kernel/kernel.macho'
    output.parent.mkdir(parents=True, exist_ok=True)
    command = ['python3', ctx.tree / spec['tool'], '--kernel', kernel, '--prelinked', cache,
               f'--slide={spec["slide"]}', '--output', output]
    for root in spec.get('roots', []):
        command += ['--root', root]
    if spec.get('early_security_init'):
        command += ['--early-security-init', spec['early_security_init']]
    command += spec.get('extra_args', [])
    run(command, cwd=ctx.tree, log_file=ctx.logs / 'prelink.log')
    log(f'kernel: {output} sha256 {sha256_file(output)}')
    return output
