#!/usr/bin/env python3
"""Exercise the release verifier's failure boundaries without compiling a Switch runtime."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
from zipfile import BadZipFile, ZipFile

spec = importlib.util.spec_from_file_location('local_ci', Path(__file__).resolve().parents[1] / 'tools/local-ci.py')
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)

spec = importlib.util.spec_from_file_location('package_profiles', ci.PROBE / 'tools/package-profiles.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)

assert '标签 profile-test-001' in ci.runtime_update_note('Debug', 'profile-test-001')
assert '可使用预发布' in ci.runtime_update_note('Debug', 'profile-test-001')
assert '最新正式 Release' in ci.runtime_update_note('Release', '')

with tempfile.TemporaryDirectory(prefix='autorun-ci-profiles-') as directory:
    release = Path(directory)
    index = release / 'autorun-profiles.tsv'
    packager.build_release(ci.PROBE / 'profiles/catalog.json', release)
    expected = ci.verify_profiles(index)
    original = index.read_bytes()
    assets = {p.name: p.read_bytes() for p in release.glob('*.zip')}
    fields = original.decode().splitlines()[1].split('\t')
    selected = expected[0]['filename']

    def reject_profile(change):
        for path in release.iterdir():
            path.unlink()
        index.write_bytes(original)
        for name, data in assets.items():
            (release / name).write_bytes(data)
        change()
        try:
            ci.verify_profiles(index)
        except (ValueError, KeyError, BadZipFile):
            return
        raise AssertionError('invalid profile release accepted')

    def replace_field(column, value):
        lines = original.decode().splitlines()
        changed = fields.copy()
        changed[column] = value
        lines[1] = '\t'.join(changed)
        index.write_text('\n'.join(lines) + '\n')

    def corrupt_archive(kind):
        path = release / selected
        with ZipFile(path) as archive:
            content = {e.filename: archive.read(e) for e in archive.infolist()}
        if kind == 'metadata':
            content['catalog.tsv'] = content['catalog.tsv'].replace(fields[0].encode(), b'another-game')
        elif kind == 'multiple':
            content['catalog.tsv'] += content['catalog.tsv'].splitlines(keepends=True)[1]
        elif kind == 'missing':
            del content[f'{fields[0]}/keys.txt']
        else:
            content['../unexpected.txt'] = b'outside'
        with ZipFile(path, 'w') as archive:
            for name, data in content.items():
                archive.writestr(name, data)
        lines = original.decode().splitlines()
        changed = fields.copy()
        changed[7:9] = [ci.sha256(path), str(path.stat().st_size)]
        lines[1] = '\t'.join(changed)
        index.write_text('\n'.join(lines) + '\n')

    reject_profile(lambda: index.write_text('autorun-profile-index-v1\n'))
    reject_profile(lambda: index.write_bytes(original.replace(b'index-v1', b'index-v9')))
    reject_profile(lambda: index.write_bytes(original + original.splitlines(keepends=True)[1]))
    for column, value in ((0, '../escape'), (2, '0'), (3, '-1'), (6, 'http://example.com/game.zip'),
                          (6, 'https://user@example.com/game.zip'), (7, 'bad-hash'), (8, '16777217')):
        reject_profile(lambda c=column, v=value: replace_field(c, v))
    reject_profile(lambda: (release / selected).unlink())
    reject_profile(lambda: (release / selected).write_bytes(b'corrupted'))
    reject_profile(lambda: (release / 'profile-old-v1.zip').write_bytes(b'stale'))
    reject_profile(lambda: (release / 'autorun-profiles.zip').write_bytes(b'legacy aggregate'))
    for kind in ('metadata', 'multiple', 'missing', 'path'):
        reject_profile(lambda k=kind: corrupt_archive(k))

print('PASS: split profile index, exact asset set, SHA/size, single-game metadata and file boundaries')

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    profiles = root / 'autorun-profiles.tsv'
    profiles.write_bytes(b'autorun-profile-index-v1\n')
    files = {'wine-nx-runtime.nro': bytes(16) + b'NRO0',
             'profiles/autorun-profiles.tsv': profiles.read_bytes(),
             'drive_c/dxvk/d3d9.dll': b'x86 fixture', 'drive_c/dxvk64/d3d9.dll': b'x64 fixture'}
    manifest = {'wine': 'abc', 'features': dict.fromkeys(('amd64', 'dynarec', 'vulkan', 'dxvk', 'vkd3d', 'lsfg', 'x86_dxvk'), True),
                'files': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}

    def archive(entries, metadata):
        path = root / 'runtime.zip'
        with ZipFile(path, 'w') as output:
            for name, data in entries.items():
                output.writestr('switch/wine/' + name, data)
            output.writestr('switch/wine/build-manifest.json', json.dumps(metadata))
        return path

    def reject(entries, metadata=manifest, commit='abc'):
        try:
            ci.verify_runtime(archive(entries, metadata), profiles, commit)
        except ValueError:
            return
        raise AssertionError('invalid runtime accepted')

    assert ci.verify_runtime(archive(files, manifest), profiles, 'abc') == manifest
    for name in ('profiles/autorun-profiles.zip', 'profiles/profile-old-v1.zip'):
        bundled = files | {name: b'unwanted archive'}
        reject(bundled, manifest | {'files': {n: hashlib.sha256(d).hexdigest() for n, d in bundled.items()}})
    reject(files, commit='other')
    reject(files | {'wine-nx-runtime.nro': b'corrupted'})
    reject(files | {'unlisted.txt': b'extra'})
    reject(files | {'../escape.txt': b'bad'})
    reject(files | {'WINE-NX-RUNTIME.NRO': b'duplicate'})
    reject(files, manifest | {'features': manifest['features'] | {'x86_dxvk': False}})
    profiles.write_bytes(b'another catalog')
    reject(files)

print('PASS: release provenance, full manifest, hash validation, paths, architecture feature and bundled catalog')

# Run the real final packager in an isolated miniature checkout. This catches
# lost x86 graphics, stale manifests and mismatched embedded profile catalogs.
with tempfile.TemporaryDirectory(prefix='autorun-ci-package-') as directory:
    repo = Path(directory)
    probe = repo / 'wine-nx-probe'
    tools = probe / 'tools'
    tools.mkdir(parents=True)
    for name in ('package-autorun.py', 'package-profiles.py', 'dxvk_payload.py'):
        shutil.copy2(ci.PROBE / 'tools' / name, tools / name)
    shutil.copytree(ci.PROBE / 'profiles', probe / 'profiles')
    shutil.copy2(ci.ROOT / 'README.zh-CN.md', repo / 'README.zh-CN.md')
    (probe / 'source').mkdir()
    shutil.copy2(ci.PROBE / 'source/autorun_update.h', probe / 'source/autorun_update.h')
    for name in ('key_names.h', 'launcher.c', 'launcher_ui.c', 'launcher_ui.h', 'launcher_zh_cn.h'):
        (probe / 'source' / name).write_text('fixture\n')
    subprocess.run(['git', 'init', '-q', str(repo)], check=True)
    subprocess.run(['git', '-C', str(repo), '-c', 'user.name=CI fixture', '-c', 'user.email=ci@example.invalid',
                    'commit', '-q', '--allow-empty', '-m', 'fixture'], check=True)
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    payload = {'wine-nx-runtime.nro': bytes(16) + b'NRO0\0nx-amd64-box64-1\0' +
               ci.profile_index_url('example/autorun', 'profiles').encode() + b'\0' +
               ci.profile_index_url(ci.default_repository(), '').encode() + b'\0',
               'drive_c/windows/system32/winebox64ec.dll': b'fixture',
               'drive_c/dxvk64/dxgi.dll': b'fixture', 'drive_c/dxvk64/d3d9.dll': b'fixture',
               'drive_c/vkd3d64/d3d12.dll': b'fixture'}
    metadata = {'wine': commit, 'features': dict(manifest['features']),
                'files': {name: hashlib.sha256(data).hexdigest() for name, data in payload.items()}}
    amd64 = repo / 'amd64.zip'
    with ZipFile(amd64, 'w') as output:
        for name, data in payload.items():
            output.writestr('switch/wine/' + name, data)
        output.writestr('switch/wine/build-manifest.json', json.dumps(metadata))
    sys.path.insert(0, str(ci.PROBE / 'tools'))
    from dxvk_payload import DLLS, REVISION, VERSION
    x86 = repo / 'x86'
    (x86 / 'licenses').mkdir(parents=True)
    (x86 / 'licenses/test.txt').write_text('fixture license')
    image = bytearray(256)
    image[:2] = b'MZ'
    struct.pack_into('<I', image, 0x3c, 128)
    image[128:132] = b'PE\0\0'
    for offset, value in ((132, 0x14c), (150, 0x2000), (152, 0x10b)):
        struct.pack_into('<H', image, offset, value)
    for name in DLLS:
        (x86 / name).write_bytes(image)
    (x86 / 'dxvk-manifest.json').write_text(json.dumps({
        'version': VERSION, 'revision': REVISION, 'architecture': 'x86',
        'files': {name: ci.sha256(x86 / name) for name in DLLS}, 'licenses': ['test.txt']}))
    subprocess.run([sys.executable, str(tools / 'package-autorun.py'), '--no-example-games',
                    '--amd64', str(amd64), '--x86-dxvk', str(x86),
                    '--profile-repository', 'example/autorun', '--profile-release-tag', 'profiles'], check=True)
    release = repo / 'release'
    profiles = release / 'autorun-profiles.tsv'
    subprocess.run([sys.executable, str(tools / 'package-profiles.py'), '--output-dir', str(release),
                    '--repository', 'example/autorun', '--release-tag', 'profiles'], check=True)
    archive = probe / 'build-switch-wow64-dynarec/autorun-cn-main_cn-1.zip'
    assert len(ci.verify_profiles(profiles)) == len(json.loads((probe / 'profiles/catalog.json').read_text())['profiles'])
    result = ci.verify_runtime(archive, profiles, commit, ci.profile_index_url('example/autorun', 'profiles'))
    assert result['x86_dxvk']['architecture'] == 'x86'
    with ZipFile(archive) as output:
        assert output.read('switch/wine/profile-updates.txt') == b'index-url=https://cnb.cool/example/autorun/-/releases/download/profiles/autorun-profiles.tsv\nauto-update=1\nbuild-index-url=https://cnb.cool/example/autorun/-/releases/download/profiles/autorun-profiles.tsv\n'
        assert output.read('switch/wine/licenses/x86-test.txt') == b'fixture license'
        assert output.read('switch/wine/drive_c/dxvk/d3d9.dll') == image
        assert not any(name.startswith('switch/wine/profiles/profile-') and name.endswith('.zip') for name in output.namelist())

    subprocess.run([sys.executable, str(tools / 'package-autorun.py'), '--no-example-games',
                    '--amd64', str(amd64), '--x86-dxvk', str(x86)], check=True)
    subprocess.run([sys.executable, str(tools / 'package-profiles.py'), '--output-dir', str(release)], check=True)
    ci.verify_profiles(profiles)
    ci.verify_runtime(archive, profiles, commit, ci.profile_index_url(ci.default_repository(), ''))
    try:
        ci.verify_runtime(archive, profiles, commit, ci.profile_index_url('wrong/repository', ''))
    except ValueError:
        pass
    else:
        raise AssertionError('wrong bundled update source accepted')
    with ZipFile(archive) as output:
        assert output.read('switch/wine/profile-updates.txt') == b'index-url=https://cnb.cool/PalmMuse/autorun-cn/-/releases/latest/download/autorun-profiles.tsv\nauto-update=1\nbuild-index-url=https://cnb.cool/PalmMuse/autorun-cn/-/releases/latest/download/autorun-profiles.tsv\n'
    assert ci.default_repository() == 'PalmMuse/autorun-cn'
    subprocess.run([sys.executable, str(tools / 'package-autorun.py'), '--no-example-games',
                    '--amd64', str(amd64), '--x86-dxvk', str(x86), '--profile-repository', ''], check=True)
    ci.verify_runtime(archive, profiles, commit, ci.profile_index_url('', ''))

print('PASS: real generic packager includes x86 graphics, licenses, profile channel and consistent manifests')
