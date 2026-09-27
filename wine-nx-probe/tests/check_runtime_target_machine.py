#!/usr/bin/env python3
"""Exercise the runtime's actual EXE identification code on the host.

Packed PE files may put the NT header inside the DOS header (Swordman uses
offset 0x10). Optional command-line paths are real x86 EXEs to identify; they
are read without executing or changing them.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'wine-nx-probe/source/runtime.c').read_text()
start = source.index('static NTSTATUS runtime_target_machine(')
function = source[start:source.index('\nstatic int launcher_machine(', start)]
fixture = '''
#include <stdio.h>
#define WINE_NX_PE_LOADER
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#undef WIN32_NO_STATUS
#include "ntstatus.h"
''' + function + '''
int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
    {
        USHORT machine = 0xffff;
        NTSTATUS status = runtime_target_machine(argv[i], &machine);
        printf("%08x %04x\\n", (unsigned int)status, machine);
    }
    return 0;
}
'''


def image(offset=0x80, machine=0x14c, magic=0x10b):
    data = bytearray(max(64, offset + 28))
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, offset)
    data[offset:offset + 4] = b'PE\0\0'
    struct.pack_into('<H', data, offset + 4, machine)
    struct.pack_into('<H', data, offset + 24, magic)
    return data


with tempfile.TemporaryDirectory(prefix='wine-nx-target-machine-') as temporary:
    tmp = Path(temporary)
    c = tmp / 'test.c'
    c.write_text(fixture)
    cases = {
        'x86': image(),
        'overlap': image(0x10),
        'arm64': image(machine=0xaa64, magic=0x20b),
        'amd64': image(machine=0x8664, magic=0x20b),
        'bad-magic': image(magic=0x20b),
        'bad-machine': image(machine=0x1c4),
        'truncated-dos': image()[:63],
        'truncated-pe': image()[:0x80 + 25],
        'bad-mz': image(),
        'bad-pe': image(0x10),
        'negative-offset': image(),
        'past-end': image(),
    }
    cases['bad-mz'][:2] = b'NO'
    cases['bad-pe'][0x10:0x14] = b'NOPE'
    struct.pack_into('<I', cases['negative-offset'], 0x3c, 0xffffffff)
    struct.pack_into('<I', cases['past-end'], 0x3c, 0x7fffffff)
    paths = [tmp / (name + '.exe') for name in cases]
    for path, data in zip(paths, cases.values()):
        path.write_bytes(data)
    paths.append(tmp / 'missing.exe')
    real = [Path(path).resolve() for path in sys.argv[1:]]

    for label, defines, accepted in [
        ('arm64', [], {'arm64': 0xaa64}),
        ('wow64', ['WINE_NX_BOX64_INTERPRETER'],
         {'x86': 0x14c, 'overlap': 0x14c, 'arm64': 0xaa64}),
        ('amd64-wow64', ['WINE_NX_BOX64_INTERPRETER', 'WINE_NX_AMD64'],
         {'x86': 0x14c, 'overlap': 0x14c, 'arm64': 0xaa64, 'amd64': 0x8664}),
    ]:
        executable = tmp / label
        subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=undefined', '-I', str(root / 'include'),
                        *['-D' + define for define in defines], str(c),
                        '-o', str(executable)], check=True)
        output = subprocess.run([str(executable), *map(str, paths + real)],
                                check=True, capture_output=True, text=True,
                                timeout=30).stdout.splitlines()
        expected = [(0, accepted[name]) if name in accepted else (0xc000007b, 0xffff)
                    for name in cases]
        expected.append((0xc0000034, 0xffff))
        expected.extend([(0, 0x14c) if defines else (0xc000007b, 0xffff)] * len(real))
        actual = [tuple(int(value, 16) for value in line.split()) for line in output]
        assert actual == expected, (label, list(zip(paths + real, actual, expected)))
        print(f'{label}: overlapping and standard headers, invalid files and architecture checks passed')
    for path in real:
        print(f'{path}: identified as x86 in WoW64 builds')
