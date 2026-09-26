"""Enumerate every configured Wine PE module, following upstream's full DLL pack.

Use the current checkout's configured make rules, never foreign prebuilt DLLs:
the AMD64 runtime needs ARM64X system32 rather than the upstream x86 pack's tree.
"""
from pathlib import Path
import re
import subprocess

MODULE = r'(?:dll|drv|exe|sys|ocx|cpl|acm|ax|tlb|ds|msstyles|dll16|drv16|exe16|mod16|vxd)'


def component_targets(database, arch):
    if arch not in ('aarch64', 'i386'):
        raise ValueError(f'Unsupported component architecture: {arch}')
    pattern = rf'^((?:dlls|programs)/[^ :/]+)/{arch}-windows/([^ :/]+\.{MODULE}):'
    found = sorted({(directory, name) for directory, name in re.findall(pattern, database, re.M)
                    if name.lower() != 'winetest.exe'})
    names = [name.lower() for directory, name in found]
    if len(set(names)) != len(names):
        raise ValueError(f'Duplicate {arch} component destinations')
    if not {'ntdll.dll', 'kernel32.dll', 'avifil32.dll'} <= set(names):
        raise ValueError(f'Incomplete {arch} make rules')
    return [(name, f'{directory}/{arch}-windows/{name}') for directory, name in found]


def configured_database(pe, env):
    result = subprocess.run(['make', '-C', str(pe), '-pnq', 'all'], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    # -q returns 1 when targets need rebuilding; 2 is a make/configuration error.
    if result.returncode not in (0, 1):
        raise ValueError(f'Cannot enumerate configured Wine modules: {result.stderr[-2000:]}')
    return result.stdout


def copy_library_licenses(root, destination):
    import shutil
    destination.mkdir(parents=True, exist_ok=True)
    for library in sorted((Path(root) / 'libs').iterdir()):
        if not library.is_dir():
            continue
        for pattern in ('LICENSE*', 'COPYING*', 'COPYRIGHT*'):
            for path in sorted(library.glob(pattern)):
                if path.is_file():
                    shutil.copy2(path, destination / f'{library.name}-{path.name}')
