#!/usr/bin/env python3
"""Build a game-local proxy from the exact Swordman Miles export table.

No proprietary DLL bytes are included in the proxy. The input remains intact.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess

MILES_SHA256 = '6a128953250b3d142245a9ca6facabf308666703c1f9c8c5b6ac95acce95403d'


def pe_image(path):
    raw = Path(path).read_bytes()
    pe = struct.unpack_from('<I', raw, 60)[0]
    assert raw[pe:pe + 4] == b'PE\0\0'
    assert struct.unpack_from('<H', raw, pe + 4)[0] == 0x14c
    opt = pe + 24
    assert struct.unpack_from('<H', raw, opt)[0] == 0x10b
    mem = bytearray(struct.unpack_from('<I', raw, opt + 56)[0])
    headers = struct.unpack_from('<I', raw, opt + 60)[0]
    mem[:headers] = raw[:headers]
    for i in range(struct.unpack_from('<H', raw, pe + 6)[0]):
        section = opt + struct.unpack_from('<H', raw, pe + 20)[0] + i * 40
        va, size, offset = struct.unpack_from('<III', raw, section + 12)
        mem[va:va + size] = raw[offset:offset + size]
    return raw, mem, opt


def exports(path):
    raw, mem, opt = pe_image(path)
    u32 = lambda offset: struct.unpack_from('<I', mem, offset)[0]
    string = lambda offset: bytes(mem[offset:]).split(b'\0', 1)[0].decode('ascii')
    exp, length = struct.unpack_from('<II', raw, opt + 96)
    base, count, names, funcs, strings, ords = struct.unpack_from('<IIIIII', mem, exp + 16)
    assert names == count  # This version has no unnamed ordinal-only exports.
    result = []
    for i in range(names):
        name = string(u32(strings + i * 4))
        ordinal = struct.unpack_from('<H', mem, ords + i * 2)[0]
        rva = u32(funcs + ordinal * 4)
        result.append((name, base + ordinal, string(rva) if exp <= rva < exp + length else None))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--cc', default='i686-w64-mingw32-clang')
    args = parser.parse_args()
    assert hashlib.sha256(args.original.read_bytes()).hexdigest() == MILES_SHA256, 'Unsupported Miles DLL'
    names = exports(args.original)
    assert len(names) == 315
    args.output.mkdir(parents=True, exist_ok=True)
    definition = args.output / 'mss32.def'
    override = {'_AIL_startup@0': 'proxy_startup@0', '_AIL_set_preference@8': 'proxy_set_preference@8'}
    lines = ['LIBRARY mss32.dll', 'EXPORTS']
    for name, ordinal, _ in names:
        target = override.get(name, 'mss32_autorun_original.' + name)
        lines.append(f'  "{name}"="{target}" @{ordinal}')
    definition.write_text('\n'.join(lines) + '\n')
    source = Path(__file__).with_name('proxy.c')
    dll = args.output / 'mss32.dll'
    subprocess.run([args.cc, '-Os', '-Wall', '-Wextra', '-Werror', '-nostdlib', '-shared',
                    '-Wl,--entry,_DllMain@12', '-Wl,--dynamicbase', '-Wl,--no-insert-timestamp',
                    '-o', str(dll), str(source), str(definition), '-lkernel32'], check=True)
    produced = exports(dll)
    assert [(n, o) for n, o, _ in produced] == [(n, o) for n, o, _ in names]
    for name, _, forwarder in produced:
        assert forwarder == (None if name in override else 'mss32_autorun_original.' + name)
    print(f'Validated {len(produced)} exports: 2 overrides, 313 unchanged forwarders')
    print(f'{dll}: {dll.stat().st_size} bytes, SHA256 {hashlib.sha256(dll.read_bytes()).hexdigest()}')


if __name__ == '__main__':
    main()
