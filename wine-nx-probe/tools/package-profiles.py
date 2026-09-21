#!/usr/bin/env python3
"""Validate the maintained catalog and build the CNB Release profile asset."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib
from urllib.parse import urlsplit
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

PROBE = Path(__file__).resolve().parents[1]
SETTINGS = set('title d3d d3d9 own-controls controller verbose profile window-fit sdl-audio sd-stat-cache sd-clean-writer-cache locale wined3d-renderer wined3d-frontbuffer-swap wined3d-explicit-buffer-flush wined3d-csmt '
               'aspect-fit touch-coordinates left-stick-run left-stick-eight-way left-stick-aim left-stick-move '
               'windows dxvk-version vkd3d-version dxvk-hud frame-limit vsync'.split())
KEYS = set('LSTICK RSTICK DPAD TOUCH UP DOWN LEFT RIGHT LUP LDOWN LLEFT LRIGHT RUP RDOWN RLEFT RRIGHT '
           'A B X Y L R ZL ZR PLUS MINUS STICKL STICKR'.lower().split())


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate JSON field: {key}')
        result[key] = value
    return result


def field(value, limit, name):
    if not isinstance(value, str) or len(value.encode('utf-8')) >= limit or any(ord(c) < 32 for c in value):
        raise ValueError(f'invalid or too long {name}')
    return value


def resource(root, relative):
    if not isinstance(relative, str):
        raise ValueError('resource path must be text')
    path = root / relative
    if Path(relative).is_absolute() or '..' in Path(relative).parts or not path.resolve().is_relative_to(root.resolve()):
        raise ValueError(f'path outside profiles directory: {relative}')
    if any(parent.is_symlink() for parent in (path, *path.parents)):
        raise ValueError(f'symlink is not allowed: {relative}')
    return path.read_bytes()


def defaults(root, relative, controls):
    data = resource(root, relative)
    if len(data) >= 8192 or b'\0' in data:
        raise ValueError(f'configuration too large or contains NUL: {relative}')
    text = data.decode('utf-8')
    found = set()
    for line in text.splitlines():
        if len(line.encode('utf-8')) >= 512:
            raise ValueError(f'configuration line too long: {relative}')
        line = line.strip()
        if not line or line.startswith(('#', ';')):
            continue
        key, separator, value = line.partition('=')
        key, value = key.strip().lower(), value.strip()
        if not separator or key not in (KEYS if controls else SETTINGS) or key in found or not value or len(value.encode()) >= 384:
            raise ValueError(f'invalid or duplicate setting in {relative}: {line}')
        if any(ord(c) < 32 for c in value):
            raise ValueError(f'control character in {relative}')
        found.add(key)
    return data


def cheats(root, relative):
    data = json.loads(resource(root, relative).decode('utf-8'), object_pairs_hook=unique_object)
    if set(data) != {'schema', 'cheats'} or type(data['schema']) is not int or data['schema'] != 1:
        raise ValueError('cheats schema must be 1')
    if not isinstance(data['cheats'], list) or len(data['cheats']) > 32:
        raise ValueError('at most 32 cheats per profile')
    lines, ids = ['autorun-cheats-v1\n'], set()
    for entry in data['cheats']:
        required = {'id', 'name', 'description', 'type', 'backend'}
        if entry.get('type') == 'integer':
            required |= {'min', 'max', 'step', 'default'}
        if set(entry) != required or entry['type'] not in ('toggle', 'integer'):
            raise ValueError('invalid cheat fields/type')
        ident = field(entry['id'], 48, 'cheat id')
        backend = field(entry['backend'], 64, 'cheat backend')
        if not re.fullmatch('[a-z0-9_.-]+', ident) or ident in ids or not re.fullmatch('[a-z0-9_.-]+', backend):
            raise ValueError('invalid or duplicate cheat id/backend')
        ids.add(ident)
        name = field(entry['name'], 96, 'cheat name')
        desc = field(entry['description'], 256, 'cheat description')
        if not name:
            raise ValueError('empty cheat name')
        low, high, step, initial = (entry[k] for k in ('min', 'max', 'step', 'default')) if entry['type'] == 'integer' else (0, 1, 1, 1)
        if any(type(v) is not int or not -2147483648 <= v <= 2147483647 for v in (low, high, step, initial)) or step <= 0 or not low <= initial <= high or (initial - low) % step:
            raise ValueError('invalid cheat range, step or default')
        lines.append('\t'.join(map(str, (ident, name, desc, entry['type'], low, high, step, initial, backend))) + '\n')
    result = ''.join(lines).encode('utf-8')
    if len(result) >= 8192:
        raise ValueError('cheat definitions too large')
    return result


def cover(root, relative):
    data = resource(root, relative)
    if len(data) > 2 * 1024 * 1024 or not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError('cover must be a PNG of at most 2 MiB')
    pos, ihdr, ended, idat = 8, False, False, False
    while pos < len(data):
        if pos + 12 > len(data):
            raise ValueError('truncated PNG')
        size, kind = struct.unpack_from('>I4s', data, pos)
        if pos + 12 + size > len(data):
            raise ValueError('truncated PNG chunk')
        body = data[pos + 8:pos + 8 + size]
        if zlib.crc32(kind + body) != struct.unpack_from('>I', data, pos + 8 + size)[0]:
            raise ValueError('PNG checksum mismatch')
        if not ihdr:
            if kind != b'IHDR' or size != 13:
                raise ValueError('PNG must start with IHDR')
            width, height = struct.unpack_from('>II', body)
            if not 1 <= width <= 2048 or not 1 <= height <= 2048:
                raise ValueError('cover dimensions must be at most 2048 x 2048')
            ihdr = True
        elif kind == b'IHDR':
            raise ValueError('duplicate PNG header')
        if kind == b'IDAT':
            idat = True
        pos += 12 + size
        if kind == b'IEND':
            if size or pos != len(data):
                raise ValueError('invalid PNG end')
            ended = True
            break
    if not ihdr or not idat or not ended:
        raise ValueError('incomplete PNG')
    # The runtime also fully decodes the image before committing any files.
    return data


def binary_patch(root, relative):
    patch = json.loads(resource(root, relative).decode('utf-8'), object_pairs_hook=unique_object)
    required = {'schema', 'original_sha256', 'patched_sha256', 'offset', 'old', 'new'}
    if set(patch) != required or type(patch['schema']) is not int or patch['schema'] != 1:
        raise ValueError('binary patch schema must be 1 with the exact required fields')
    for key in ('original_sha256', 'patched_sha256'):
        if not isinstance(patch[key], str) or not re.fullmatch('[0-9a-f]{64}', patch[key]):
            raise ValueError(f'invalid binary patch {key}')
    if patch['original_sha256'] == patch['patched_sha256']:
        raise ValueError('binary patch digests must differ')
    if type(patch['offset']) is not int or not 0 <= patch['offset'] <= 0x7fffffffffffffff:
        raise ValueError('invalid binary patch offset')
    for key in ('old', 'new'):
        if not isinstance(patch[key], str) or not re.fullmatch('[0-9a-f]+', patch[key]) or len(patch[key]) % 2:
            raise ValueError(f'invalid binary patch {key} bytes')
    if len(patch['old']) != len(patch['new']) or not 2 <= len(patch['old']) <= 128 or patch['old'] == patch['new']:
        raise ValueError('binary patch must replace 1 to 64 bytes with a different equal-length value')
    return ('autorun-binary-patch-v1\n'
            f'original-sha256={patch["original_sha256"]}\n'
            f'patched-sha256={patch["patched_sha256"]}\n'
            f'offset={patch["offset"]}\n'
            f'old={patch["old"]}\n'
            f'new={patch["new"]}\n').encode('ascii')


def build(catalog_path, output, selected=None):
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    if set(catalog) != {'schema', 'profiles'} or type(catalog['schema']) is not int or catalog['schema'] not in (1, 2, 3):
        raise ValueError('catalog schema must be 1, 2 or 3')
    entries = catalog['profiles']
    if not isinstance(entries, list) or not 1 <= len(entries) <= 128:
        raise ValueError('catalog must contain 1 to 128 profiles')
    if selected is not None:
        entries = [entry for entry in entries if entry.get('id') == selected]
        if len(entries) != 1:
            raise ValueError('profile selection must identify one entry')
    v2 = catalog['schema'] >= 2
    v3 = catalog['schema'] >= 3
    lines = [f'autorun-profiles-v{catalog["schema"]}\n']
    files, ids = {}, set()
    for entry in entries:
        required = {'id', 'name', 'version', 'min_api', 'keywords', 'description', 'settings', 'keys'}
        optional = ({'cheats', 'cover', 'url'} if v2 else set()) | ({'binary_patch'} if v3 else set())
        if not required <= set(entry) or set(entry) - required - optional:
            raise ValueError('unexpected or missing profile fields')
        ident = field(entry['id'], 64, 'id')
        if not re.fullmatch('[a-z0-9-]+', ident) or ident in ids:
            raise ValueError(f'invalid or duplicate id: {ident}')
        ids.add(ident)
        name = field(entry['name'], 96, 'name')
        if not name:
            raise ValueError('name must not be empty')
        for key in ('version', 'min_api'):
            if type(entry[key]) is not int or not 1 <= entry[key] <= 2147483647:
                raise ValueError(f'{key} must be a positive integer')
        keywords = field(entry['keywords'], 192, 'keywords')
        description = field(entry['description'], 512, 'description')
        if v2 and entry['min_api'] < 2:
            raise ValueError('schema 2 requires min_api >= 2')
        columns = [ident, name, str(entry['version']), str(entry['min_api']), keywords, description]
        if v2:
            columns += [str(int('cheats' in entry)), str(int('cover' in entry))]
        if v3:
            columns += [str(int('binary_patch' in entry))]
            if 'binary_patch' in entry and entry['min_api'] < 3:
                raise ValueError('binary patches require min_api >= 3')
        lines.append('\t'.join(columns) + '\n')
        if 'cheats' in entry:
            files[f'{ident}/cheats.txt'] = cheats(catalog_path.parent, entry['cheats'])
        if 'cover' in entry:
            files[f'{ident}/cover.png'] = cover(catalog_path.parent, entry['cover'])
        if 'binary_patch' in entry:
            files[f'{ident}/patch.txt'] = binary_patch(catalog_path.parent, entry['binary_patch'])
        for kind in ('settings', 'keys'):
            files[f'{ident}/{kind}.txt'] = defaults(catalog_path.parent, entry[kind], kind == 'keys')
    if sum(len(data) for name, data in files.items() if name.endswith('/cover.png')) > 16 * 1024 * 1024:
        raise ValueError('total covers exceed 16 MiB')
    files['catalog.tsv'] = ''.join(lines).encode('utf-8')
    if len(files['catalog.tsv']) >= 128 * 1024:
        raise ValueError('catalog too large')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.zip.new')
    with ZipFile(temporary, 'w', compression=ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = ZipInfo(name, (2020, 1, 1, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    if temporary.stat().st_size > 16 * 1024 * 1024:
        temporary.unlink()
        raise ValueError('profile ZIP exceeds download limit of 16 MiB')
    with ZipFile(temporary) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP verification failed')
    temporary.replace(output)
    return len(entries)


def default_repository():
    return re.search(r'^#define AUTORUN_DEFAULT_REPOSITORY "([^"]+)"$',
                     (PROBE / 'source/autorun_update.h').read_text(), re.MULTILINE).group(1)


def release_base(repository, tag=''):
    component = r'[A-Za-z0-9_-][A-Za-z0-9_.-]{0,98}[A-Za-z0-9_-]|[A-Za-z0-9_-]'
    if not re.fullmatch(f'(?:{component})/(?:{component})', repository):
        raise ValueError('repository must be owner/repo')
    if tag and not re.fullmatch(component, tag):
        raise ValueError('invalid release tag')
    return f'https://cnb.cool/{repository}/-/releases/' + (f'download/{tag}/' if tag else 'latest/download/')


def build_release(catalog_path, directory, repository=None, tag=''):
    """Publish a single index and independent game ZIPs; never a combined download."""
    base = release_base(repository or default_repository(), tag)
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    entries = catalog['profiles']
    if not isinstance(entries, list) or not 1 <= len(entries) <= 128:
        raise ValueError('catalog must contain 1 to 128 profiles')
    directory.mkdir(parents=True, exist_ok=True)
    lines, names, ids = ['autorun-profile-index-v1\n'], [], set()
    for entry in entries:
        ident = field(entry['id'], 64, 'id')
        if not re.fullmatch('[a-z0-9-]+', ident) or ident in ids:
            raise ValueError('invalid or duplicate profile id')
        ids.add(ident)
        if type(entry['version']) is not int or not 1 <= entry['version'] <= 2147483647:
            raise ValueError('invalid profile version')
        name = f'profile-{ident}-v{entry["version"]}.zip'
        output = directory / name
        build(catalog_path, output, selected=ident)
        url = entry.get('url', base + name)
        field(url, 768, 'package URL')
        parts = urlsplit(url)
        if parts.scheme != 'https' or not parts.netloc or any(ord(c) <= 32 or ord(c) >= 127 or c in '\\#@' for c in url):
            raise ValueError('package URL must be an HTTPS URL without credentials or fragment')
        digest = hashlib.sha256(output.read_bytes()).hexdigest()
        lines.append('\t'.join(map(str, (ident, entry['name'], entry['version'], entry['min_api'],
                         entry['keywords'], entry['description'], url, digest, output.stat().st_size))) + '\n')
        names.append(name)
    index = directory / 'autorun-profiles.tsv'
    text = ''.join(lines).encode('utf-8')
    if len(text) > 1024 * 1024:
        raise ValueError('profile index exceeds 1 MiB')
    temporary = index.with_suffix('.tsv.new'); temporary.write_bytes(text); temporary.replace(index)
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, default=PROBE / 'profiles/catalog.json')
    parser.add_argument('--output-dir', type=Path, default=PROBE / 'build-switch-amd64/profiles-release')
    parser.add_argument('--repository', default=default_repository())
    parser.add_argument('--release-tag', default='')
    # Compatibility for the old format's regression fixtures, not a release artifact.
    parser.add_argument('--output', type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        if args.output:
            count = build(args.catalog, args.output)
            print(f'{args.output}: {count} profiles (legacy fixture)')
        else:
            names = build_release(args.catalog, args.output_dir, args.repository, args.release_tag)
            print(f'{args.output_dir / "autorun-profiles.tsv"}: {len(names)} games')
            for name in names:
                print(f'  {name}')
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(1, f'profile package: {error}\n')


if __name__ == '__main__':
    main()
