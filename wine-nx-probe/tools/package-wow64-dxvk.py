#!/usr/bin/env python3
"""Stage the DXVK checkpoint as an overlay for a card that holds a full
package: the runtime linked with mesa-switch (build-switch-wow64-mesa-switch),
Wine's i386 vulkan-1.dll and winevulkan.dll, and DXVK's d3d9.dll built for i386
from WINE_NX_DXVK_DIR (default ~/dxvk) next to C:\\dxvk\\pe32-d3d9.exe.

A program's own folder comes first in its DLL search, and the card has no
builtin d3d9 under lib/wine for Wine's load order to prefer, so only programs
in a folder holding DXVK's d3d9.dll use it; everything else keeps wined3d."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import functools
import os
import re
import subprocess

from dxvk_payload import say_directx9

probe = Path(__file__).resolve().parents[1]
pe = probe / 'build-wine-wow64-pe'
build = probe / 'build-switch-wow64-dynarec'
nro = probe / 'build-switch-wow64-mesa-switch/wine-nx-runtime.nro'
base = build / 'full-sd-card/switch/wine'
dxvk = Path(os.environ.get('WINE_NX_DXVK_DIR', Path.home() / 'dxvk'))
dxvk_build = probe / 'build-dxvk-i386'
tools = probe / 'toolchains/llvm-mingw-20260505-ucrt-macos-universal/bin'
env = dict(os.environ, PATH=f'{tools}:/opt/homebrew/opt/bison/bin:' + os.environ['PATH'])
marker = re.search(r'nx-wow64-dynarec-(\d+)', (probe / 'source/runtime.c').read_text()).group(1)

parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument('--name', default='dxvk', help='label in the zip name, new for each rebuild')
args = parser.parse_args()

assert b'a Vulkan surface has the screen' in nro.read_bytes(), \
    f'{nro} has no Vulkan display driver; configure it with -DWINE_NX_MESA_SWITCH_DIR'
assert f'nx-wow64-dynarec-{marker}'.encode() + b'\0' in nro.read_bytes(), \
    f'{nro} is stale; rebuild the runtime for build {marker}'
assert b'winemem\0' in nro.read_bytes(), \
    f'{nro} predates sections with no file, which DXVK\'s 32-bit d3d9 needs; rebuild the runtime'
assert (base / 'drive_c/windows/syswow64').is_dir(), f'{base} is not a full package; run package-wow64-full.py first'
assert (base / 'drive_c/windows/system32/apisetschema.dll').is_file(), \
    f'{base} has no API set schema, which DXVK\'s api-ms-win-crt imports need'
assert (dxvk / 'VP_DXVK_requirements.json').is_file(), f'{dxvk} is not a DXVK tree; set WINE_NX_DXVK_DIR'

# DXVK's d3d9 alone. DXVK links its C++ runtime statically, and llvm-mingw's
# UCRT imports resolve to ucrtbase.dll through the API set schema.
if not (dxvk_build / 'build.ninja').is_file():
    dxvk_build.mkdir(parents=True, exist_ok=True)
    cross = dxvk_build / 'llvm-mingw-i386.txt'
    cross.write_text(f'''[binaries]
c = '{tools}/i686-w64-mingw32-clang'
cpp = '{tools}/i686-w64-mingw32-clang++'
ar = '{tools}/i686-w64-mingw32-ar'
strip = '{tools}/i686-w64-mingw32-strip'
windres = '{tools}/i686-w64-mingw32-windres'

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'windows'
cpu_family = 'x86'
cpu = 'x86'
endian = 'little'
''')
    subprocess.run(['meson', 'setup', str(dxvk_build), '--cross-file', str(cross), '--buildtype', 'release',
                    '--strip', '-Denable_d3d9=true', '-Denable_d3d8=false', '-Denable_d3d10=false',
                    '-Denable_d3d11=false', '-Denable_dxgi=false'], cwd=dxvk, env=env, check=True)
subprocess.run(['ninja', '-C', str(dxvk_build), 'src/d3d9/d3d9.dll'], env=env, check=True)
d3d9 = dxvk_build / 'src/d3d9/d3d9.dll'
say_directx9(d3d9)
dxvk_version = subprocess.run(['git', '-C', str(dxvk), 'describe', '--tags', '--always', '--dirty'],
                              capture_output=True, text=True).stdout.strip() or 'unknown'

dlls = {}
for name in ('vulkan-1.dll', 'winevulkan.dll'):
    target = f'dlls/{name.removesuffix(".dll")}/i386-windows/{name}'
    subprocess.run(['make', '-C', str(pe), '-j8', target, 'dlls/vulkan-1/i386-windows/libvulkan-1.a'],
                   env=env, check=True)
    dlls[name] = pe / target

exe = pe / 'pe32-d3d9.exe'
subprocess.run([str(tools / 'i686-w64-mingw32-clang'), '-Os', '-Wall', '-Wextra', '-Werror', '-fno-builtin',
                '-nostdlib', '-Wl,--entry,_start@0', '-Wl,--image-base,0x10000000', '-Wl,--dynamicbase',
                '-o', str(exe), str(probe / 'tests/pe32_d3d9.c'),
                '-ld3d9', '-lgdi32', '-luser32', '-lkernel32', '-lntdll'], check=True, env=env)
# The same test in NFSU2's mode: an 800x600 full-screen back buffer.
exe_800 = pe / 'pe32-d3d9-800.exe'
subprocess.run([str(tools / 'i686-w64-mingw32-clang'), '-Os', '-Wall', '-Wextra', '-Werror', '-fno-builtin',
                '-DTEST_WIDTH=800', '-DTEST_HEIGHT=600', '-DTEST_WINDOWED=FALSE',
                '-nostdlib', '-Wl,--entry,_start@0', '-Wl,--image-base,0x10000000', '-Wl,--dynamicbase',
                '-o', str(exe_800), str(probe / 'tests/pe32_d3d9.c'),
                '-ld3d9', '-lgdi32', '-luser32', '-lkernel32', '-lntdll'], check=True, env=env)
vulkan_exe = pe / 'pe32-vulkan.exe'
subprocess.run([str(tools / 'i686-w64-mingw32-clang'), '-Os', '-Wall', '-Wextra', '-Werror',
                '-Wno-missing-field-initializers', '-fno-builtin',
                '-nostdlib', '-Wl,--entry,_start@0', '-Wl,--image-base,0x10000000', '-Wl,--dynamicbase',
                # after mingw's own headers: only wine/vulkan.h comes from Wine's tree
                '-idirafter', str(probe.parent / 'include'),
                '-o', str(vulkan_exe), str(probe / 'tests/pe32_vulkan.c'),
                '-L', str(pe / 'dlls/vulkan-1/i386-windows'),
                '-lvulkan-1', '-luser32', '-lkernel32', '-lntdll'], check=True, env=env)
section_exe = pe / 'pe32-section.exe'
subprocess.run([str(tools / 'i686-w64-mingw32-clang'), '-Os', '-Wall', '-Wextra', '-Werror', '-fno-builtin',
                '-nostdlib', '-Wl,--entry,_start@0', '-Wl,--image-base,0x10000000', '-Wl,--dynamicbase',
                '-o', str(section_exe), str(probe / 'tests/pe32_section.c'),
                '-lkernel32', '-lntdll'], check=True, env=env)

def readobj(option, path):
    return subprocess.check_output([str(tools / 'llvm-readobj'), option, str(path)], text=True)

apisets = dict(re.findall(r'^apiset (\S+) = (\S+)$', (probe.parent / 'dlls/apisetschema/apisetschema.spec').read_text(), re.M))
syswow64 = base / 'drive_c/windows/syswow64'
# Where each import resolves on the card: the program's folder first (d3d9.dll
# beside pe32-d3d9.exe), then syswow64 with this overlay's DLLs over it.
resolved = {p.name.lower(): p for p in syswow64.iterdir()} | dlls | {'d3d9.dll': d3d9}

@functools.lru_cache(maxsize=None)
def exports_of(path):
    return set(re.findall(r'^  Name: (\S+)', readobj('--coff-exports', path), re.M))

# Every imported function, not only every DLL, so a missing export shows here
# rather than as a load failure on the Switch.
for path in (exe, exe_800, section_exe, vulkan_exe, d3d9, *dlls.values()):
    assert 'Arch: i386\n' in readobj('--file-headers', path), f'{path.name} is not i386'
    for block in re.findall(r'^Import \{\n(.*?)^\}', readobj('--coff-imports', path), re.M | re.S):
        name = re.search(r'Name: (.+)', block).group(1).lower()
        if name == 'ntdll.dll':
            continue
        target = apisets.get(name.removesuffix('.dll'), name).lower()
        assert target in resolved, f'{path.name} imports {name} ({target}), which the card lacks'
        missing = set(re.findall(r'Symbol: (\S+) \(', block)) - exports_of(resolved[target])
        assert not missing, f'{path.name} imports from {target} what it does not export: {sorted(missing)}'

readme = f'''Wine-NX build {marker}: the DXVK checkpoint, as an overlay for a card that
holds a full package. It replaces the runtime NRO with the one linked with
Mesa 26 (mesa-switch: OpenGL through nvc0, Vulkan through NVK), adds Wine's
Vulkan (vulkan-1.dll and winevulkan.dll), and adds the folder C:\\dxvk with
DXVK {dxvk_version}'s d3d9.dll and the Direct3D 9 test.

Keep your current switch/wine/wine-nx-runtime.nro somewhere else to go back,
then copy the switch folder to the SD card, merging folders.

Only programs in a folder that holds DXVK's d3d9.dll use DXVK: a program's own
folder comes first when Windows DLLs are found. Everything else keeps Wine's
d3d9, which draws through wined3d and OpenGL.

Frames of any size other than the screen's (800x600 or 640x480 in full screen)
are scaled into 1280x720 screen buffers, keeping the aspect ratio. Vulkan memory
of 32-bit programs stays in the driver's own mappings, outside their address
space.

- C:\\dxvk\\pe32-d3d9.exe and pe32-d3d9-800.exe: red, green and blue frames with
  a white triangle, read back every frame, ending in [D3D9 TEST] PASS.
- C:\\dxvk\\pe32-vulkan.exe: Vulkan memory, surface and swapchain checks.
- C:\\dxvk\\pe32-section.exe: sections with no file, which DXVK's d3d9 uses.
- Need for Speed Underground 2: copy C:\\dxvk\\d3d9.dll next to SPEED2.EXE
  (delete that copy to return to wined3d). C:\\dxvk\\nfsu2-hud\\dxvk.conf, copied
  next to it too, draws DXVK's frame rate.
'''

archive = build / f'wine-nx-{args.name}-overlay-dynarec-{marker}.zip'
with ZipFile(archive, 'w', ZIP_DEFLATED) as z:
    z.write(nro, 'switch/wine/wine-nx-runtime.nro')
    for name, path in dlls.items():
        z.write(path, f'switch/wine/drive_c/windows/syswow64/{name}')
    z.write(d3d9, 'switch/wine/drive_c/dxvk/d3d9.dll')
    z.write(exe, 'switch/wine/drive_c/dxvk/pe32-d3d9.exe')
    z.write(exe_800, 'switch/wine/drive_c/dxvk/pe32-d3d9-800.exe')
    z.writestr('switch/wine/drive_c/dxvk/pe32-d3d9-800.args.txt', '\n')
    z.writestr('switch/wine/drive_c/dxvk/pe32-d3d9.args.txt', '\n')
    z.write(vulkan_exe, 'switch/wine/drive_c/dxvk/pe32-vulkan.exe')
    z.writestr('switch/wine/drive_c/dxvk/pe32-vulkan.args.txt', '\n')
    z.write(section_exe, 'switch/wine/drive_c/dxvk/pe32-section.exe')
    z.writestr('switch/wine/drive_c/dxvk/pe32-section.args.txt', '\n')
    z.writestr('switch/wine/drive_c/dxvk/nfsu2-hud/dxvk.conf',
               '# Copy next to SPEED2.EXE for the second NFSU2 run (DXVK-README.txt, step 4).\n'
               '# DXVK draws its frame rate over every frame it presents.\n'
               'dxvk.hud = fps,frametimes\n')
    z.writestr('switch/wine/DXVK-README.txt', readme)
with ZipFile(archive) as z:
    assert z.testzip() is None
    print('\n'.join(f'{i.file_size:>10} {i.filename}' for i in z.infolist()))
print(f'{archive} ({archive.stat().st_size / 2**20:.1f} MiB)')
