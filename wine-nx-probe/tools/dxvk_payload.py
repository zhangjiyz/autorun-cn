"""Pinned DXVK payload shared by the builder and AMD64 packager."""
import hashlib
import json
from pathlib import Path
import struct

VERSION = '3.1.1'
REVISION = 'b1a1c99ab52b687cf950d62c88bc2fa316b41663'
DLLS = ('d3d8.dll', 'd3d9.dll', 'd3d10core.dll', 'd3d11.dll', 'dxgi.dll')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate_payload(directory, architecture='x86_64'):
    machines = {'x86_64': (0x8664, 0x20b), 'x86': (0x14c, 0x10b)}
    if architecture not in machines:
        raise ValueError(f'Unsupported DXVK architecture: {architecture}')
    machine, magic = machines[architecture]
    directory = Path(directory)
    manifest = json.loads((directory / 'dxvk-manifest.json').read_text())
    if (manifest.get('version'), manifest.get('revision'), manifest.get('architecture')) != (
            VERSION, REVISION, architecture):
        raise ValueError(f'DXVK payload does not match the pinned {architecture} release')
    for name in DLLS:
        path = directory / name
        data = path.read_bytes()
        if len(data) < 64 or data[:2] != b'MZ':
            raise ValueError(f'Invalid DXVK PE image: {path}')
        offset = struct.unpack_from('<I', data, 0x3c)[0]
        if (offset > len(data) - 26 or data[offset:offset + 4] != b'PE\0\0' or
                struct.unpack_from('<H', data, offset + 4)[0] != machine or
                struct.unpack_from('<H', data, offset + 24)[0] != magic or
                not struct.unpack_from('<H', data, offset + 22)[0] & 0x2000):
            raise ValueError(f'DXVK DLL is not {architecture}: {path}')
        if manifest.get('files', {}).get(name) != digest(path):
            raise ValueError(f'DXVK payload hash mismatch: {path}')
    for name in manifest.get('licenses', []):
        if Path(name).name != name or not (directory / 'licenses' / name).is_file():
            raise ValueError(f'Missing DXVK license: {name}')
    if not manifest.get('licenses'):
        raise ValueError('DXVK payload lacks licenses')
    return manifest


def say_directx9(path):
    """Make d3d9.dll report the version a Direct3D 9 runtime has.

    DXVK's version resource says 10.0.17763.1, the Windows 10 system DLL it
    stands in for. A game from the Direct3D 9 years reads that resource to
    decide whether DirectX 9 is installed, and reads the major and minor of a
    version it was written before: Halo takes 10.0 for something older than
    9.0b and refuses to start. Wine's own d3d9.dll says 5.3.1.904, which is
    what the DirectX 9.0c file says, so this says the same. Patched in the
    built DLL rather than in the DXVK tree, which is not ours.
    """
    data = bytearray(path.read_bytes())
    version = (5, 3, 1, 904)
    ms, ls = (version[0] << 16) | version[1], (version[2] << 16) | version[3]
    fixed = b'\xbd\x04\xef\xfe'
    patched = 0
    at = data.find(fixed)
    while at >= 0:
        # signature, struct version, then file and product version, MS before LS
        struct.pack_into('<IIII', data, at + 8, ms, ls, ms, ls)
        patched += 1
        at = data.find(fixed, at + 4)
    assert patched, f'{path} has no version resource to correct'

    # The strings beside it, kept the same length so the block does not move.
    text = '%d.%d.%d.%d' % version
    for old_text in ('10.0.17763.1 (WinBuild.160101.0800)', '10.0.17763.1'):
        new_text = text + ' ' * (len(old_text) - len(text))
        assert len(new_text) == len(old_text)
        data = bytearray(data.replace(old_text.encode('utf-16-le'), new_text.encode('utf-16-le')))
    path.write_bytes(data)

    # Read back what a game would: the fixed information, which is what
    # GetFileVersionInfo hands to VerQueryValue for the root.
    check = path.read_bytes()
    at = check.find(fixed)
    got = struct.unpack_from('<IIII', check, at + 8)
    assert got == (ms, ls, ms, ls), f'{path} still reports {got}'
    print(f'{path.name}: version resource says %d.%d.%d.%d' % version)
