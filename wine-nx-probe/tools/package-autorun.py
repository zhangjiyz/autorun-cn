#!/usr/bin/env python3
"""Build one SD-card archive with the x86 and AMD64 graphics runtimes."""
from pathlib import Path
from pathlib import PurePosixPath
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

probe = Path(__file__).resolve().parents[1]
tools = probe / 'tools'
build = probe / 'build-switch-wow64-dynarec'
stage_root = build / 'full-sd-card'
stage = stage_root / 'switch/wine'
default_amd64 = probe / 'build-switch-amd64/wine-nx-amd64-box64-mesa-dxvk-vkd3d.zip'
parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument('--amd64', type=Path,
                    default=Path(os.environ.get('WINE_NX_AMD64_PACKAGE', default_amd64)))
parser.add_argument('--no-example-games', action='store_true',
                    help='Package the dual-architecture runtime without the old test-game inputs')
parser.add_argument('--x86-dxvk-overlay', type=Path,
                    help='Add the matching x86 DXVK overlay without replacing the AMD64 runtime')
args = parser.parse_args()
if args.x86_dxvk_overlay and not args.no_example_games:
    parser.error('--x86-dxvk-overlay requires --no-example-games')


def merge_amd64(archive, root, keep_launch_files=True):
    keep = {
        'switch/wine/run-entry.txt',
        'switch/wine/target.txt',
        'switch/wine/vulkan-probe.txt',
    } if keep_launch_files else set()
    required = {
        'switch/wine/build-manifest.json',
        'switch/wine/wine-nx-runtime.nro',
        'switch/wine/drive_c/windows/system32/winebox64ec.dll',
        'switch/wine/drive_c/dxvk64/dxgi.dll',
        'switch/wine/drive_c/vkd3d64/d3d12.dll',
    }
    with ZipFile(archive) as z:
        assert z.testzip() is None, f'{archive} is damaged'
        names = set()
        for info in z.infolist():
            path = PurePosixPath(info.filename)
            assert path.parts[:2] == ('switch', 'wine') and '..' not in path.parts, info.filename
            assert not ((info.external_attr >> 16) & 0o170000) == 0o120000, info.filename
            folded = info.filename.rstrip('/').casefold()
            assert folded not in names, info.filename
            names.add(folded)
        assert {name.casefold() for name in required} <= names, f'{archive} is not a full AMD64 graphics package'
        manifest = json.loads(z.read('switch/wine/build-manifest.json'))
        features = manifest.get('features', {})
        for feature in ('amd64', 'dynarec', 'vulkan', 'dxvk', 'vkd3d', 'lsfg'):
            assert features.get(feature) is True, f'{archive} has no {feature} support'
        for info in z.infolist():
            if info.filename.rstrip('/') in keep:
                continue
            destination = root.joinpath(*PurePosixPath(info.filename).parts)
            if info.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                with z.open(info) as source, destination.open('wb') as output:
                    shutil.copyfileobj(source, output)
    nro = (root / 'switch/wine/wine-nx-runtime.nro').read_bytes()
    match = re.search(rb'nx-amd64-box64-(\d+)\0', nro)
    assert match, f'{archive} does not contain the AMD64 runtime'
    return match.group(1).decode()

if args.no_example_games:
    assert args.amd64.is_file(), f'{args.amd64} is missing; build the AMD64 DXVK/VKD3D package first'
    generic_root = build / 'cn-generic-sd-card'
    shutil.rmtree(generic_root, ignore_errors=True)
    generic_root.mkdir(parents=True)
    marker = merge_amd64(args.amd64, generic_root, keep_launch_files=False)
    generic_stage = generic_root / 'switch/wine'
    for name in ('run-entry.txt', 'target.txt', 'vulkan-probe.txt'):
        (generic_stage / name).unlink(missing_ok=True)
    shutil.copy2(probe.parent / 'README.zh-CN.md', generic_stage / 'README.zh-CN.md')
    zhaoyun_profile = generic_stage / 'profiles/zhaoyun'
    zhaoyun_profile.mkdir(parents=True, exist_ok=True)
    for name in ('Game.keys.txt', 'README.zh-CN.md', 'patch-game.py'):
        shutil.copy2(probe / 'profiles/zhaoyun' / name, zhaoyun_profile / name)
    (generic_stage / 'INSTALL.zh-CN.txt').write_text(
        '将压缩包内的 switch 文件夹复制到 SD 卡根目录。\n'
        '自行把已安装的 Windows 游戏复制到 switch/wine/drive_c，'
        '在 Autorun 中按 + 添加游戏并选择 EXE。\n'
        '启动 Switch 游戏时按住 R 打开自制程序菜单，进入 wine 文件夹，'
        '选择 Autorun（wine-nx-runtime.nro），以获得完整内存。\n'
        '本包不附带游戏；通过上游在线更新会覆盖中文界面。\n', encoding='utf-8')
    if args.x86_dxvk_overlay:
        with ZipFile(args.x86_dxvk_overlay) as overlay:
            assert overlay.testzip() is None, f'{args.x86_dxvk_overlay} is damaged'
            assert 'switch/wine/drive_c/dxvk/d3d9.dll' in overlay.namelist(), \
                f'{args.x86_dxvk_overlay} has no x86 d3d9.dll'
            for info in overlay.infolist():
                name = info.filename
                if info.is_dir() or not (name.startswith('switch/wine/drive_c/dxvk/') or
                                         name == 'switch/wine/DXVK-README.txt'):
                    continue
                path = PurePosixPath(name)
                assert '..' not in path.parts, name
                destination = generic_root.joinpath(*path.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                with overlay.open(info) as source, destination.open('wb') as output:
                    shutil.copyfileobj(source, output)
    manifest_path = generic_stage / 'build-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    for name, digest in manifest['files'].items():
        path = PurePosixPath(name)
        assert '..' not in path.parts and not path.is_absolute(), name
        source = generic_stage.joinpath(*path.parts)
        if name not in ('run-entry.txt', 'target.txt', 'vulkan-probe.txt'):
            assert source.is_file() and hashlib.sha256(source.read_bytes()).hexdigest() == digest, name
    current_commit = subprocess.check_output(
        ['git', '-C', str(probe.parent), 'rev-parse', 'HEAD'], text=True).strip()
    assert manifest['wine'] == current_commit, f'{args.amd64} was built from another source commit'
    manifest['localization'] = {
        'language': 'zh-CN',
        'branch': 'main_cn',
        'translation_sha256': hashlib.sha256(
            (probe / 'source/launcher_zh_cn.h').read_bytes()).hexdigest(),
        'source_sha256': {
            str(path.relative_to(probe.parent)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (probe / 'source/key_names.h', probe / 'source/launcher.c',
                         probe / 'source/launcher_ui.c', probe / 'source/launcher_ui.h',
                         probe / 'source/launcher_zh_cn.h')
        },
    }
    manifest['features']['x86_dxvk'] = bool(args.x86_dxvk_overlay)
    if not args.x86_dxvk_overlay:
        config = generic_stage / 'config'
        config.mkdir(exist_ok=True)
        (config / 'settings.json').write_text('{"dxvk-for-new-games": false}\n')
    manifest['files'] = {
        str(path.relative_to(generic_stage)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(generic_stage.rglob('*'))
        if path.is_file() and path != manifest_path and path.suffix != '.log'
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    archive = build / f'autorun-cn-main_cn-{marker}.zip'
    with ZipFile(archive, 'w', ZIP_DEFLATED) as output:
        for path in sorted(generic_stage.rglob('*')):
            if path.is_file() and path.suffix != '.log':
                output.write(path, path.relative_to(generic_root))
    with ZipFile(archive) as output:
        assert output.testzip() is None, f'{archive} is damaged'
    print(f'{archive} ({archive.stat().st_size / 2**20:.1f} MiB)')
    sys.exit(0)

subprocess.run([sys.executable, str(tools / 'package-wow64-full.py')], check=True)
subprocess.run([sys.executable, str(tools / 'package-wow64-dxvk.py')], check=True)

wow64_marker = re.search(r'nx-wow64-dynarec-(\d+)', (probe / 'source/runtime.c').read_text()).group(1)
full = build / f'wine-nx-full-dynarec-{wow64_marker}.zip'
overlay = build / f'wine-nx-dxvk-overlay-dynarec-{wow64_marker}.zip'
assert full.is_file() and overlay.is_file(), 'a half is missing'

# The overlay's paths are the card's own, so it unpacks onto the staged payload
# the way it would onto the card: a newer runtime and the DXVK files.
with ZipFile(overlay) as z:
    for name in z.namelist():
        assert name.startswith('switch/wine/'), name
    z.extractall(stage_root)

assert args.amd64.is_file(), f'{args.amd64} is missing; build the AMD64 DXVK/VKD3D package first'
marker = merge_amd64(args.amd64, stage_root)
subprocess.run([sys.executable, str(tools / 'verify-wow64-package.py'), str(stage)], check=True)

archive = build / f'autorun-{marker}.zip'
with ZipFile(archive, 'w', ZIP_DEFLATED) as z:
    for f in sorted(stage.rglob('*')):
        if f.is_file() and f.name != '.DS_Store' and f.suffix != '.log':
            z.write(f, f.relative_to(stage_root))
        # Empty folders are places to copy a game into, such as drive_c/WarCraft III.
        elif f.is_dir() and not any(f.iterdir()):
            z.write(f, f.relative_to(stage_root))
with ZipFile(archive) as z:
    assert z.testzip() is None
    files = len(z.infolist())

full.unlink()
overlay.unlink()
print(f'{archive} ({archive.stat().st_size / 2**20:.1f} MiB, {files} files)')
