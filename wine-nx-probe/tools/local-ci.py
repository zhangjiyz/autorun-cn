#!/usr/bin/env python3
"""Local build/check/package pipeline. Never commits, uploads or creates a Release."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
from zipfile import BadZipFile, ZipFile

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / 'wine-nx-probe'
MESA_REVISION = 'c68987266979dc7b4105877f2bf27543e76a1622'
DEVKIT_IMAGE = 'devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282'


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def capture(*command):
    return subprocess.check_output(command, cwd=ROOT, text=True).strip()


def source_state():
    names = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=ROOT)
    files = {}
    for name in sorted(set(names.decode().split('\0')) - {''}):
        path = ROOT / name
        files[name] = sha256(path) if path.is_file() else None
    digest = hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()
    return {'commit': capture('git', 'rev-parse', 'HEAD'),
            'status': capture('git', 'status', '--porcelain'),
            'source_sha256': digest, 'files': files}


def default_repository():
    header = (PROBE / 'source/autorun_update.h').read_text()
    return re.search(r'^#define AUTORUN_DEFAULT_REPOSITORY "([^"]+)"$', header, re.MULTILINE).group(1)


def profile_index_url(repository, tag):
    if not repository:
        return ''
    channel = f'download/{tag}' if tag else 'latest/download'
    return f'https://github.com/{repository}/releases/{channel}/autorun-profiles.tsv'


def verify_profiles(index):
    """Validate the release table and its exact set of independent game archives."""
    data = index.read_bytes()
    if len(data) > 1024 * 1024 or b'\0' in data:
        raise ValueError('Invalid profile index size/content')
    lines = data.decode('utf-8').splitlines()
    if not lines or lines[0] != 'autorun-profile-index-v1' or not 2 <= len(lines) <= 129:
        raise ValueError('Invalid profile index header/count')
    profiles, ids = [], set()
    for line in lines[1:]:
        fields = line.split('\t')
        if len(fields) != 9:
            raise ValueError('Invalid profile index columns')
        ident, name, version, api, keywords, description, url, digest, size = fields
        limits = (64, 96, 11, 11, 192, 512, 768, 65, 11)
        if any(len(value.encode('utf-8')) >= limit or any(ord(c) < 32 for c in value)
               for value, limit in zip(fields, limits)):
            raise ValueError('Invalid profile index field')
        if not re.fullmatch('[a-z0-9-]+', ident) or ident in ids or not name:
            raise ValueError('Invalid/duplicate profile ID or empty name')
        ids.add(ident)
        if any(not re.fullmatch('[1-9][0-9]*', value) or int(value) > 2147483647
               for value in (version, api, size)) or int(size) > 16 * 1024 * 1024:
            raise ValueError('Invalid profile version/API/size')
        parsed = urlsplit(url)
        if (not url.isascii() or parsed.scheme != 'https' or not parsed.hostname
                or any(c.isspace() or c in '\\#@' for c in url)):
            raise ValueError('Invalid profile download URL')
        if not re.fullmatch('[0-9a-f]{64}', digest):
            raise ValueError('Invalid profile SHA-256')
        filename = f'profile-{ident}-v{version}.zip'
        path = index.parent / filename
        if not path.is_file() or path.is_symlink() or path.stat().st_size != int(size) or sha256(path) != digest:
            raise ValueError(f'Game package differs from indexed release: {filename}')
        with ZipFile(path) as archive:
            entries = archive.infolist()
            if (sum(e.file_size for e in entries) > 3 * 1024 * 1024
                    or any((e.external_attr >> 16) & 0o170000 == 0o120000 for e in entries)):
                raise ValueError(f'Invalid profile ZIP entries: {filename}')
            if archive.testzip():
                raise ValueError(f'Profile ZIP CRC validation failed: {filename}')
            catalog = archive.read('catalog.tsv').decode('utf-8').splitlines()
            if len(catalog) != 2 or catalog[0] not in ('autorun-profiles-v1', 'autorun-profiles-v2'):
                raise ValueError(f'Profile ZIP must contain exactly one game: {filename}')
            columns = catalog[1].split('\t')
            v2 = catalog[0] == 'autorun-profiles-v2'
            if len(columns) != (8 if v2 else 6) or columns[:6] != fields[:6]:
                raise ValueError(f'Profile ZIP metadata differs from index: {filename}')
            expected = {'catalog.tsv', f'{ident}/settings.txt', f'{ident}/keys.txt'}
            if v2:
                if int(api) < 2 or any(flag not in ('0', '1') for flag in columns[6:]):
                    raise ValueError(f'Invalid profile feature flags: {filename}')
                for flag, resource in zip(columns[6:], ('cheats.txt', 'cover.png')):
                    if flag == '1':
                        expected.add(f'{ident}/{resource}')
            if len(entries) != len(expected) or {e.filename for e in entries} != expected:
                raise ValueError(f'Unexpected/missing/duplicate profile files: {filename}')
        profiles.append({'id': ident, 'name': name, 'version': int(version), 'min_api': int(api),
                         'filename': filename, 'url': url, 'sha256': digest, 'bytes': int(size)})
    actual = {p.name for p in index.parent.iterdir() if p.suffix.lower() == '.zip' and p.name != 'autorun.zip'}
    if actual != {p['filename'] for p in profiles}:
        raise ValueError('Release contains stale or unindexed profile ZIPs')
    return profiles


def verify_runtime(path, profiles, commit, index_url=None):
    with ZipFile(path) as archive:
        if archive.testzip():
            raise ValueError('Runtime ZIP CRC validation failed')
        seen = set()
        for entry in archive.infolist():
            p = PurePosixPath(entry.filename)
            if (p.parts[:2] != ('switch', 'wine') or '..' in p.parts or '\\' in entry.filename
                    or entry.filename.casefold() in seen or (entry.external_attr >> 16) & 0o170000 == 0o120000):
                raise ValueError(f'Unsafe/duplicate runtime ZIP entry: {entry.filename}')
            seen.add(entry.filename.casefold())
        prefix = 'switch/wine/'
        manifest = json.loads(archive.read(prefix + 'build-manifest.json'))
        if manifest['wine'] != commit:
            raise ValueError('Runtime package was built from another commit')
        for feature in ('amd64', 'dynarec', 'vulkan', 'dxvk', 'vkd3d', 'lsfg', 'x86_dxvk'):
            if manifest['features'].get(feature) is not True:
                raise ValueError(f'Missing runtime feature: {feature}')
        actual = {i.filename[len(prefix):] for i in archive.infolist() if not i.is_dir()}
        if actual != set(manifest['files']) | {'build-manifest.json'}:
            raise ValueError('Runtime manifest does not cover the complete archive')
        for name, expected in manifest['files'].items():
            if hashlib.sha256(archive.read(prefix + name)).hexdigest() != expected:
                raise ValueError(f'Runtime package hash mismatch: {name}')
        for name in ('target.txt', 'run-entry.txt', 'vulkan-probe.txt'):
            if name in actual:
                raise ValueError(f'Test autostart leaked into the release: {name}')
        nro = archive.read(prefix + 'wine-nx-runtime.nro')
        if nro[16:20] != b'NRO0':
            raise ValueError('Invalid NRO header')
        if archive.read(prefix + 'profiles/autorun-profiles.tsv') != profiles.read_bytes():
            raise ValueError('Bundled and separate profile indexes differ')
        if any(name.startswith('profiles/') and name.lower().endswith('.zip') for name in actual):
            raise ValueError('Main package should only bundle the index, not game ZIPs')
        if index_url is not None:
            expected = f'index-url={index_url}\nauto-update=1\n'.encode()
            if archive.read(prefix + 'profile-updates.txt') != expected:
                raise ValueError('Bundled profile update source differs from CI configuration')
        for arch in ('dxvk', 'dxvk64'):
            if prefix + f'drive_c/{arch}/d3d9.dll' not in archive.namelist():
                raise ValueError(f'Missing {arch} d3d9.dll')
    return manifest


class Pipeline:
    def __init__(self, args, output):
        self.args, self.output = args, output
        self.steps = []

    def run(self, *command, env=None):
        title = shlex.join(map(str, command))
        print(f'\n[{len(self.steps) + 1}] {title}', flush=True)
        with (self.output / 'build.log').open('a') as log:
            log.write('\n$ ' + title + '\n'); log.flush()
            result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                    stdout=log, stderr=subprocess.STDOUT)
        self.steps.append({'command': list(map(str, command)), 'exit_code': result.returncode})
        if result.returncode:
            tail = (self.output / 'build.log').read_text(errors='replace').splitlines()[-50:]
            print('\n'.join(tail), file=sys.stderr)
            raise RuntimeError(f'Step failed ({result.returncode}); see {self.output / "build.log"}')

    def image(self, target):
        suffix = sha256(ROOT / 'ci/Dockerfile')[:16]
        image = f'autorun-local-ci-{target}:{suffix}'
        exists = subprocess.run(['docker', 'image', 'inspect', image], stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL).returncode == 0
        if not exists:
            self.run('docker', 'build', '--platform', 'linux/arm64', '--target', target,
                     '-t', image, '-f', ROOT / 'ci/Dockerfile', ROOT / 'ci')
        return image

    def container(self, image, *command):
        self.run('docker', 'run', '--rm', '--platform', 'linux/arm64',
                 '-v', f'{ROOT}:/work', '-w', '/work',
                 '-e', f'WINE_NX_JOBS={self.args.jobs}',
                 '-e', f'AUTORUN_PROFILE_REPOSITORY={self.args.profile_repository}',
                 '-e', f'AUTORUN_PROFILE_TAG={self.args.profile_tag}',
                 '-e', 'UBSAN_OPTIONS=halt_on_error=1', '-e', 'SDL_VIDEODRIVER=dummy',
                 image, *command)

    def mesa(self):
        source = PROBE / 'vendor/mesa-switch'
        if not (source / '.git').is_dir():
            if source.exists():
                raise ValueError(f'Refusing to overwrite non-Git Mesa source: {source}')
            source.mkdir(parents=True)
            self.run('git', '-C', source, 'init', '-q')
            self.run('git', '-C', source, 'remote', 'add', 'origin', 'https://github.com/danfromtico/mesa-switch.git')
            self.run('git', '-C', source, 'fetch', '--depth=1', 'origin', MESA_REVISION)
            self.run('git', '-C', source, 'checkout', '--detach', 'FETCH_HEAD')
        if (capture('git', '-C', str(source), 'rev-parse', 'HEAD') != MESA_REVISION
                or capture('git', '-C', str(source), 'status', '--porcelain', '--untracked-files=no')):
            raise ValueError(f'Mesa must be clean at {MESA_REVISION}; existing files were not reset')
        mesa = PROBE / 'build-mesa-switch'
        lib = mesa / 'install/opt/devkitpro/portlibs/switch/lib'
        revision = mesa / 'source-revision.txt'
        libraries = ('EGL', 'GL', 'glapi', 'vulkan', 'mesa_util_c11', 'blake3', 'mesa_util', 'mesa_util_simd', 'xmlconfig')
        ready = (revision.is_file() and revision.read_text().strip() == MESA_REVISION
                 and all((lib / f'lib{name}.a').is_file() for name in libraries))
        if ready and not self.args.rebuild_mesa:
            print('Reusing the verified Mesa SDK; use --rebuild-mesa to rebuild it.', flush=True)
        else:
            dockerfile = self.output / 'Dockerfile.mesa'
            recipe = (source / 'Docker.rust').read_text().replace('FROM devkitpro/devkita64:latest', f'FROM {DEVKIT_IMAGE}')
            # Pin the Rust frontend used by this local recipe; apt packages remain distribution-managed.
            recipe = recipe.replace('--default-toolchain nightly', '--default-toolchain nightly-2026-09-20')
            recipe = recipe.replace('cargo install bindgen-cli cbindgen',
                                    'cargo install --locked bindgen-cli --version 0.73.2 && cargo install --locked cbindgen --version 0.29.4')
            dockerfile.write_text(recipe)
            image = 'autorun-local-ci-mesa:' + sha256(dockerfile)[:16]
            self.run('docker', 'build', '--platform', 'linux/arm64', '-t', image, '-f', dockerfile, ROOT / 'ci')
            env = dict(os.environ, WINE_NX_MESA_SWITCH_SRC=str(source), WINE_NX_MESA_IMAGE=image)
            self.run('sh', PROBE / 'build-mesa-switch.sh', env=env)
        return {name: sha256(lib / f'lib{name}.a') for name in libraries}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('all', 'profiles', 'check'), nargs='?', default='all')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--profile-repository', default=default_repository(), help='GitHub owner/repo; empty selects offline catalog')
    parser.add_argument('--profile-tag', default='', help='Fixed profile Release tag; empty uses latest stable Release')
    parser.add_argument('--rebuild-mesa', action='store_true')
    parser.add_argument('--output', type=Path, help='New run directory; must not already exist')
    parser.add_argument('--plan', action='store_true', help='Print the pipeline without Docker, builds or file changes')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    component = r'[A-Za-z0-9_-](?:[A-Za-z0-9_.-]{0,98}[A-Za-z0-9_-])?'
    if args.profile_repository and not re.fullmatch(f'{component}/{component}', args.profile_repository):
        parser.error('--profile-repository must be owner/repo')
    if args.profile_tag and not re.fullmatch(component, args.profile_tag):
        parser.error('Invalid --profile-tag')
    if args.plan:
        print(f'Mode: {args.mode}; jobs: {args.jobs}; profile source: {args.profile_repository or "offline"}; tag: {args.profile_tag or "latest"}')
        print('Prepare Docker image -> profile transaction/menu tests -> DXVK/package tests')
        if args.mode == 'all':
            print('Verify/build Mesa -> build NRO + matching Wine DLLs + x86/AMD64 DXVK + VKD3D -> package autorun.zip')
        if args.mode != 'check':
            print('Build autorun-profiles.tsv + per-game ZIPs -> verify archive/provenance -> SHA256SUMS + BUILD.json + RELEASE.md')
        print('Logs: dist/local-ci/<timestamp>/build.log; successful artifacts: <run>/release/; no upload')
        return 0
    if not shutil.which('docker'):
        parser.error('Docker is required; install/start Docker Desktop first')
    lock_root = PROBE / 'build-local-ci'
    lock_root.mkdir(parents=True, exist_ok=True)
    lock = lock_root / 'pipeline.lock'
    try:
        lock.mkdir()
    except FileExistsError:
        parser.error(f'Another local CI may be running: {lock}. Remove this empty lock only after confirming it stopped.')
    output = None
    try:
        base = ROOT / 'dist/local-ci'
        base.mkdir(parents=True, exist_ok=True)
        if args.output:
            requested = args.output.resolve()
            if requested.is_relative_to(ROOT):
                ignored = subprocess.run(['git', 'check-ignore', '-q', str(requested / 'build.log')], cwd=ROOT)
                if ignored.returncode:
                    raise ValueError('--output inside the repository must be Git-ignored')
            requested.mkdir(parents=True, exist_ok=False)
            output = requested
        else:
            stamp = datetime.now().strftime('%Y%m%d-%H%M%S-')
            output = Path(tempfile.mkdtemp(prefix=stamp, dir=base))
        print(f'Run: {output}\nFollow progress: tail -f {output / "build.log"}', flush=True)
        pipeline = Pipeline(args, output)
        pipeline.run('docker', 'info', '--format', '{{.Architecture}}')
        before = source_state()
        (output / 'input-source.json').write_text(json.dumps(before, ensure_ascii=False, indent=2) + '\n')
        image = pipeline.image('runtime' if args.mode == 'all' else 'checks')
        pipeline.container(image, 'python3', 'wine-nx-probe/tests/check-game-profiles.py')
        pipeline.container(image, 'python3', 'wine-nx-probe/tests/check_dxvk_payload.py')
        pipeline.container(image, 'python3', 'wine-nx-probe/tests/check_package_amd64.py')
        pipeline.container(image, 'python3', 'wine-nx-probe/tests/check_local_ci.py')
        pending = output / 'pending'
        pending.mkdir()
        metadata = {'mode': args.mode, 'built_at': datetime.now(timezone.utc).isoformat(),
                    'source': before, 'container': capture('docker', 'image', 'inspect', image, '--format', '{{.Id}}'),
                    'profile_repository': args.profile_repository, 'profile_tag': args.profile_tag,
                    'profile_index_url': profile_index_url(args.profile_repository, args.profile_tag),
                    'validation': 'Host regression tests and package integrity only; Switch acceptance is separate.'}
        if args.mode == 'all':
            metadata['mesa'] = {'revision': MESA_REVISION, 'libraries': pipeline.mesa()}
            pipeline.container(image, 'sh', 'ci/build-runtime.sh')
            source = (PROBE / 'source/runtime.c').read_text()
            marker = re.search(r'nx-amd64-box64-(\d+)', source)[1]
            archive = PROBE / f'build-switch-wow64-dynarec/autorun-cn-main_cn-{marker}.zip'
            shutil.copy2(archive, pending / 'autorun.zip')
        if args.mode != 'check':
            pipeline.run(sys.executable, PROBE / 'tools/package-profiles.py', '--output-dir', pending,
                         '--repository', args.profile_repository or default_repository(), '--release-tag', args.profile_tag)
            metadata['profiles'] = verify_profiles(pending / 'autorun-profiles.tsv')
            if args.mode == 'all':
                metadata['runtime'] = verify_runtime(pending / 'autorun.zip', pending / 'autorun-profiles.tsv',
                                                     before['commit'], metadata['profile_index_url'])
        after = source_state()
        if after != before:
            changed = sorted(name for name in before['files'].keys() | after['files'].keys()
                             if before['files'].get(name) != after['files'].get(name))
            detail = ', '.join(changed[:10]) or 'Git commit/index/status'
            raise ValueError(f'Source changed during CI ({detail}); outputs are not marked ready. Rerun after editing finishes.')
        metadata['steps'] = pipeline.steps
        metadata['artifacts'] = {p.name: sha256(p) for p in sorted(pending.iterdir()) if p.suffix in ('.zip', '.tsv')}
        (pending / 'BUILD.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n')
        notes = ['# Autorun 本地 CI 产物', '', f'源码提交：`{before["commit"]}`',
                 f'工作区含未提交修改：{"是，请同时提交对应源码" if before["status"] else "否"}', '',
                 '主机检查通过；Switch 实机运行效果需单独验收。', '']
        if args.mode == 'check':
            notes += ['本轮仅运行检查，没有生成主程序、管理表或游戏适配包，无需上传 Release。', '']
        else:
            notes += ['## 游戏适配包', '']
            notes += [f'- {p["name"]}：v{p["version"]}，最低适配 API {p["min_api"]}；`{p["filename"]}`'
                      for p in metadata['profiles']]
            notes += ['', '## 手动发布', '',
                  '先上传表中所有游戏 ZIP、主程序 autorun.zip（如有），确认附件可下载后，最后上传 autorun-profiles.tsv。',
                  '同时上传 SHA256SUMS 和 BUILD.json；RELEASE.md 可用作发布说明。',
                  '管理表固定名 autorun-profiles.tsv，游戏包按 profile-游戏ID-v版本.zip 命名。只发布配置时无需上传主程序。',
                  f'主程序在线更新使用 {default_repository()} 最新正式 Release 的 autorun.zip。',
                  f'管理表地址（all 模式写入主程序）：{metadata["profile_index_url"] or "空（关闭在线来源）"}。',
                  '如使用固定配置标签，需要将配置包上传到该标签；空标签读取最新正式 Release。',
                  '不要将仅含配置包的 Release 设为主程序的 Latest。每个最新正式 Release 都必须带齐表中使用 latest 地址的 ZIP。',
                  '公共发布应同时提供对应源码，保留第三方许可证；LSFG 对应源码及补丁见仓库 wine-nx-probe/lsfg。', '']
        (pending / 'RELEASE.md').write_text('\n'.join(notes))
        checksums = [f'{sha256(p)}  {p.name}\n' for p in sorted(pending.iterdir()) if p.is_file()]
        (pending / 'SHA256SUMS').write_text(''.join(checksums))
        pending.rename(output / 'release')
        (output / 'SUCCESS').write_text('All requested steps passed.\n')
        print(f'\nPASS: {output / "release"}', flush=True)
        return 0
    except (OSError, ValueError, KeyError, BadZipFile, RuntimeError, subprocess.CalledProcessError) as error:
        if output and output.is_dir():
            (output / 'FAILED').write_text(str(error) + '\n')
        print(f'Local CI failed: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        if output and output.is_dir():
            (output / 'FAILED').write_text('Interrupted by user.\n')
        return 130
    finally:
        lock.rmdir()


if __name__ == '__main__':
    sys.exit(main())
