#!/usr/bin/env python3
"""Host regression suite. Needs a C compiler, pkg-config, minizip, libpng, curl, OpenSSL and SDL2/SDL2_ttf."""
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import struct
import zlib
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

PROBE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_profiles', PROBE / 'tools/package-profiles.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def flags(*packages):
    return shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', *packages], text=True))


with tempfile.TemporaryDirectory(prefix='autorun-profile-tests-') as directory:
    root = Path(directory).resolve()
    valid = root / 'valid.zip'
    assert pack.build(PROBE / 'profiles/catalog.json', valid) >= 2
    repeat = root / 'repeat.zip'
    pack.build(PROBE / 'profiles/catalog.json', repeat)
    assert valid.read_bytes() == repeat.read_bytes(), 'profile packaging must be reproducible'
    with ZipFile(valid) as archive:
        entries = {name: archive.read(name) for name in archive.namelist()}
    bad = []

    def invalid(name, items):
        output = root / f'{name}.zip'
        with ZipFile(output, 'w', compression=ZIP_DEFLATED) as archive:
            for path, data in items:
                archive.writestr(path, data)
        bad.append(output)

    invalid('path-traversal', [(name if name != 'newpal/keys.txt' else '../Game.keys.txt', data) for name, data in entries.items()])
    invalid('unknown-setting', [(name, b'run-command=evil\n' if name == 'newpal/settings.txt' else data) for name, data in entries.items()])
    invalid('duplicate-setting', [(name, data + b'controller=keyboard\n' if name == 'newpal/settings.txt' else data) for name, data in entries.items()])
    invalid('oversized', [(name, b'#' * 8192 if name == 'newpal/keys.txt' else data) for name, data in entries.items()])
    invalid('missing-file', [(name, data) for name, data in entries.items() if name != 'newpal/keys.txt'])
    invalid('embedded-nul', [(name, data + b'\0' if name == 'catalog.tsv' else data) for name, data in entries.items()])
    invalid('duplicate-id', [(name, data.replace(b'zhaoyun-2002ls\t', b'newpal\t') if name == 'catalog.tsv' else data) for name, data in entries.items()])
    link = ZipInfo('newpal/keys.txt'); link.create_system = 3; link.external_attr = 0o120777 << 16
    invalid('symlink', [(link if name == 'newpal/keys.txt' else name, data) for name, data in entries.items()])
    # Test mapping maintenance errors before any artifact is emitted.
    maintenance = root / 'maintenance'
    shutil.copytree(PROBE / 'profiles', maintenance)
    data = json.loads((PROBE / 'profiles/catalog.json').read_text())
    for mutator in (lambda d: d['profiles'][0].update(version=0),
                    lambda d: d['profiles'][0].update(id='../bad'),
                    lambda d: d['profiles'][0].update(name='a\tb'),
                    lambda d: d['profiles'][0].update(settings='/tmp/arbitrary-file')):
        broken = json.loads(json.dumps(data)); mutator(broken)
        fixture = maintenance / 'catalog.json'; fixture.write_text(json.dumps(broken))
        try:
            pack.build(fixture, root / 'bad-output.zip')
        except ValueError:
            pass
        else:
            raise AssertionError('invalid mapping accepted')

    common = [os.environ.get('CC', 'cc'), '-std=gnu11', '-Wall', '-Wextra', '-Werror',
              '-O1', '-g', '-fsanitize=undefined', '-fno-omit-frame-pointer', '-I' + str(PROBE / 'source')]
    if 'clang' not in subprocess.check_output([common[0], '--version'], text=True).lower():
        common.append('-Wno-format-truncation')
    core = root / 'core'
    run(*common, PROBE / 'tests/game_profiles.c', PROBE / 'source/game_cheats.c',
        *flags('minizip', 'libpng', 'openssl'), '-lz', '-o', core)
    run(core, valid, *bad)
    # v21 packages remain readable by the new runtime.
    legacy = json.loads(json.dumps(data)); legacy['schema'] = 1
    for entry in legacy['profiles']:
        entry.pop('cheats', None); entry.pop('cover', None); entry.pop('binary_patch', None); entry['min_api'] = 1
    (maintenance / 'catalog.json').write_text(json.dumps(legacy))
    pack.build(maintenance / 'catalog.json', root / 'legacy.zip')
    run(core, root / 'legacy.zip')

    fixture = json.loads(json.dumps(data))
    fixture['profiles'][0]['cheats'] = 'test-cheats.json'
    fixture['profiles'][0]['cover'] = 'test-cover.png'
    definition = dict(schema=1, cheats=[
        dict(id='test-toggle', name='测试开关', description='仅测试框架，不对游戏产生效果', type='toggle', backend='example.toggle'),
        dict(id='test-value', name='测试数值', description='测试整数范围', type='integer', backend='example.value',
             min=0, max=100, step=5, default=10)])
    (maintenance / 'test-cheats.json').write_text(json.dumps(definition, ensure_ascii=False))
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 24, 8, 2, 0, 0, 0)) +
           chunk(b'IDAT', zlib.compress((b'\0' + b'\x18\xb4\xd8' * 16) * 24)) + chunk(b'IEND', b''))
    (maintenance / 'test-cover.png').write_bytes(png)
    (maintenance / 'catalog.json').write_text(json.dumps(fixture))
    pack.build(maintenance / 'catalog.json', root / 'framework.zip')
    with ZipFile(root / 'framework.zip') as archive:
        extended = {name: archive.read(name) for name in archive.namelist()}
    bad.clear()
    invalid('bad-png', [(name, data[:-5] if name == 'newpal/cover.png' else data) for name, data in extended.items()])
    invalid('huge-png', [(name, b'x' * (2 * 1024 * 1024 + 1) if name == 'newpal/cover.png' else data) for name, data in extended.items()])
    invalid('cheat-step-zero', [(name, data.replace(b'\t0\t100\t5\t10\t', b'\t0\t100\t0\t10\t') if name == 'newpal/cheats.txt' else data) for name, data in extended.items()])
    invalid('duplicate-cheat', [(name, data + data.splitlines(keepends=True)[1] if name == 'newpal/cheats.txt' else data) for name, data in extended.items()])
    invalid('extra-cover', list(extended.items()) + [('extra/cover.png', png)])
    run(core, root / 'framework.zip', *bad)
    framework = root / 'framework'
    run(*common, PROBE / 'tests/game_cheats.c', PROBE / 'source/game_cheats.c', *flags('minizip', 'libpng', 'openssl'), '-lz', '-o', framework)
    run(framework, root / 'framework.zip')
    # The publisher rejects invalid numeric definitions and PNGs before release.
    for change in ({'step': 0}, {'default': 11}, {'max': 2147483648}, {'enabled': True}):
        broken = json.loads(json.dumps(definition)); broken['cheats'][1].update(change)
        (maintenance / 'test-cheats.json').write_text(json.dumps(broken))
        try:
            pack.build(maintenance / 'catalog.json', root / 'bad-output.zip')
        except ValueError:
            pass
        else:
            raise AssertionError('invalid cheat definition accepted')
    (maintenance / 'test-cheats.json').write_text(json.dumps(definition))
    (maintenance / 'test-cover.png').write_bytes(png[:-5])
    try:
        pack.build(maintenance / 'catalog.json', root / 'bad-output.zip')
    except ValueError:
        pass
    else:
        raise AssertionError('truncated PNG accepted')

    def asset(name, repository='PalmMuse/autorun-cn'):
        return dict(name=name, browser_download_url=f'https://cnb.cool/{repository}/-/releases/download/v1/{name}',
                    size=1234, hash_algo='sha256', hash_value='a' * 64)
    metadata = dict(tag_name='v1', name='Release', published_at='2026-09-21T00:00:00Z',
                    body='Test only', draft=False, prerelease=False,
                    assets=[asset('autorun.zip'), asset('autorun-profiles.zip'), asset('other.zip')])
    release = root / 'release.json'; release.write_text(json.dumps(metadata))
    metadata['prerelease'] = True
    prerelease = root / 'prerelease.json'; prerelease.write_text(json.dumps(metadata))
    metadata['prerelease'] = False
    metadata['assets'] = [asset('autorun-profiles.zip')]
    profiles = root / 'profiles.json'; profiles.write_text(json.dumps(metadata))
    metadata['assets'] = [asset('autorun-profiles.zip', 'danfromtico/autorun'), asset('autorun.zip', 'danfromtico/autorun')]
    wrong = root / 'wrong.json'; wrong.write_text(json.dumps(metadata))
    transport = root / 'transport'
    shim = '-I' + str(PROBE / 'tests/profiles-shims')
    run(*common, '-Wno-deprecated-declarations', shim, PROBE / 'tests/autorun_profile_update.c',
        *flags('libcurl', 'openssl'), '-o', transport)
    run(transport, release, profiles, prerelease, wrong)
    debug_transport = root / 'debug-transport'
    run(*common, '-Wno-deprecated-declarations', '-DAUTORUN_DEBUG_BUILD',
        '-DAUTORUN_RUNTIME_RELEASE_TAG="profile-test-002"', shim,
        PROBE / 'tests/autorun_profile_update.c', *flags('libcurl', 'openssl'), '-o', debug_transport)
    run(debug_transport, release, profiles, prerelease, wrong)

    runtime = root / 'runtime'; (runtime / 'profiles').mkdir(parents=True)
    pack.build_release(PROBE / 'profiles/catalog.json', runtime / 'profiles')
    versions = {profile['id']: profile['version'] for profile in data['profiles']}
    base_version = versions['newpal']
    other_version = versions['zhaoyun-2002ls']
    # Start with no source file to test the new default management UI.
    updated = root / 'updated'
    newer = json.loads(json.dumps(data)); newer['profiles'][0]['version'] += 1
    (maintenance / 'catalog.json').write_text(json.dumps(newer))
    pack.build_release(maintenance / 'catalog.json', updated)
    ui = root / 'ui'
    run(*common, '-Wno-deprecated-declarations', shim, PROBE / 'tests/launcher_profiles.c',
        PROBE / 'source/launcher_profiles.c', PROBE / 'source/game_profiles.c', PROBE / 'source/game_cheats.c',
        PROBE / 'source/autorun_update.c',
        *flags('sdl2', 'SDL2_ttf', 'minizip', 'libpng', 'libcurl', 'openssl'), '-lz', '-o', ui)
    run(ui, runtime, updated, base_version, other_version)
    debug_runtime = root / 'debug-runtime'; (debug_runtime / 'profiles').mkdir(parents=True)
    pack.build_release(PROBE / 'profiles/catalog.json', debug_runtime / 'profiles')
    debug_ui = root / 'debug-ui'
    run(*common, '-Wno-deprecated-declarations', '-DAUTORUN_DEBUG_BUILD', shim,
        PROBE / 'tests/launcher_profiles.c', PROBE / 'source/launcher_profiles.c',
        PROBE / 'source/game_profiles.c', PROBE / 'source/game_cheats.c', PROBE / 'source/autorun_update.c',
        *flags('sdl2', 'SDL2_ttf', 'minizip', 'libpng', 'libcurl', 'openssl'), '-lz', '-o', debug_ui)
    run(debug_ui, debug_runtime, updated, base_version, other_version)
    cheats_ui = root / 'cheats-ui'
    run(*common, PROBE / 'tests/launcher_cheats.c', PROBE / 'source/launcher_cheats.c',
        PROBE / 'source/game_profiles.c', PROBE / 'source/game_cheats.c',
        *flags('sdl2', 'SDL2_ttf', 'minizip', 'libpng', 'openssl'), '-lz', '-o', cheats_ui)
    run(cheats_ui, runtime, root / 'framework.zip')
    network = root / 'network-test'
    run(*common, '-Wno-deprecated-declarations', shim, PROBE / 'tests/profile_network.c',
        PROBE / 'source/launcher_profiles.c', PROBE / 'source/game_profiles.c', PROBE / 'source/game_cheats.c', PROBE / 'source/autorun_update.c',
        '-Wl,--wrap=autorun_update_text', '-Wl,--wrap=autorun_update_file',
        *flags('sdl2', 'SDL2_ttf', 'minizip', 'libcurl', 'openssl', 'libpng'), '-lz', '-o', network)
    run(network, runtime, runtime / f'profiles/profile-newpal-v{base_version}.zip', updated, base_version)
    print('Profile package, cover, cheats, recovery, CNB metadata and menu regression suite passed.')
