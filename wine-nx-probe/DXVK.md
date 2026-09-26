# AMD64 DXVK on Autorun

The AMD64 payload uses DXVK **3.1.1**, pinned to
`b1a1c99ab52b687cf950d62c88bc2fa316b41663`, including its pinned submodules.
It is built from source with LLVM-MinGW; no moving branch or prebuilt DLL
download is used. The DLLs are AMD64 Windows binaries executed by Box64.
Wine's Vulkan bridge and Mesa/NVK remain native AArch64.

## Why 3.1.1

The reviewed mesa-switch build is Mesa 26.2.2, commit
`f0e183151c3974876400964c7afd95d4ed0e912e`. NVK exposes Vulkan 1.3 on the
Switch's Maxwell GPU. Its source advertises DXVK's required integer types,
scalar block layout, descriptor indexing, maintenance5/6, depth clipping,
robustness2, transform feedback and 256-byte push constants.

Although the [DXVK driver guide](https://github.com/doitsujin/dxvk/wiki/Driver-support)
recommends Vulkan 1.4 for 3.x, the
[3.1.1 instance code](https://github.com/doitsujin/dxvk/blob/v3.1.1/src/dxvk/dxvk_instance.h)
requires Vulkan 1.3. Its
[device capability checks](https://github.com/doitsujin/dxvk/blob/v3.1.1/src/dxvk/dxvk_device_info.cpp)
accept the required extensions separately. Missing host image copy / a Vulkan
1.4 version number therefore does not by itself require using 2.7.1.
No Vulkan version override, fabricated features or removed capability checks
are used. 3.1.1 also includes the newer DXBC shader compiler.

This is source-level compatibility and a cross-built integration, not proof
of DS2 rendering on hardware. Tegra NVK is not advertised as Vulkan-conformant
by this Mesa tree. Shader execution, synchronization and presentation still
need the device tests below. 2.7.1 remains a possible controlled comparison
if an actual regression is isolated, not a second DLL set to mix with this one.

The checkpoint's windowed 640x480 variant also passed on native Windows with
exit code 42. That validates its shader/readback logic, not DXVK or Switch NVK.

## Build

Use the existing Autorun build environment, LLVM-MinGW with ARM64EC support,
Meson >= 1.0, Ninja, glslang and Git. LLVM-MinGW's `LICENSE.TXT` must be present.
The tested toolchain is LLVM-MinGW 20260505 / Clang 22.1.5.

```sh
export WINE_NX_LLVM_MINGW=/path/to/llvm-mingw
export WINE_NX_MESA_SWITCH_DIR=/work/wine-nx-probe/build-mesa-switch/install/opt/devkitpro/portlibs/switch/lib
export WINE_NX_DXVK=1
sh wine-nx-probe/build-amd64-components.sh
```

To build only the DXVK payload:

```sh
python3 wine-nx-probe/tools/build-dxvk.py --jobs 8
```

This fetches the pinned source into `wine-nx-probe/vendor/dxvk` and builds in
`wine-nx-probe/build-dxvk-amd64`. An existing wrong-revision or modified source
tree is rejected without resetting it. `--source` and `--build` select separate
directories. A different compiler needs a different build directory.

To package an already rebuilt Mesa NRO with the configured Wine PE build:

```sh
python3 wine-nx-probe/tools/package-amd64.py \
  --pe /path/to/pe-build --build /path/to/switch-build --vulkan \
  --dxvk wine-nx-probe/build-dxvk-amd64/payload
```

Add `--no-build` only after the necessary Wine DLLs have been built. The
packager checks DXVK's revision, DLL architecture, hashes, direct/delayed
imports, API-set targets and forwarded exports. The full archive is named
`wine-nx-amd64-box64-mesa-dxvk.zip`; its manifest records all source revisions
and payload hashes. Third-party licenses are included.

Host checks:

```sh
python3 wine-nx-probe/tests/check_dxvk_payload.py
python3 wine-nx-probe/tests/check_dxvk_requirements.py
python3 wine-nx-probe/tests/check_package_amd64.py
CC=clang UBSAN_OPTIONS=halt_on_error=1 sh wine-nx-probe/check-runtime-console.sh
```

## Install and validate

Back up `switch/wine`, then merge the archive's `switch` folder onto the card.
Keep the matching NRO and Wine system DLLs together. Use the **39-bit application
forwarder**, not applet mode. The archive does not contain the game, Steam
components or any replacement game executable.

The DXVK DLLs live in `C:\dxvk64`, separate from Wine's system DLLs and the
existing x86 payload in `C:\dxvk`. Existing x86 DXVK DLLs are not upgraded or
replaced by this package. Its tests are in `C:\win64-tests`:

1. Run `pe64-vulkan.exe` first to confirm the native Vulkan/Wine bridge.
2. Run `pe64-dxvk-d3d11.exe`. It checks that DXGI and D3D11 load from
   `C:\dxvk64`, enumerates an adapter/output/display modes, requests FL11_0,
   compiles SM5 shaders, samples a texture, depth-tests a triangle and checks
   GPU readback before presenting red, green and blue frames.
3. Run `pe64-dxvk-d3d11-fullscreen.exe` for the 800x600 fullscreen/scaling path.
4. Run `pe64-dxvk-d3d9.exe` and the existing CPU/thread/audio tests for regressions.

The DXVK tests have their own `d3d=dxvk` settings files. Expected D3D11 result:
`[D3D11 TEST] PASS`, exit code `0x2a`, and the three clear colours with a white
triangle visible. A test that only opens a window is not a pass.

The included `vulkan-probe.txt` enables `[NXVK]` startup diagnostics. These now
check the pinned release's required features plus D3D11 FL11_0/stream-output
capabilities, and attempt device creation with those features enabled. A
successful device with no optional/required features enabled is not equivalent.

## Dark Souls II

The supplied log identifies an AMD64 executable and shows Wine's OpenGL adapter
initialization failing before an unhandled game fault. It does not show DXVK
initialization, and cannot establish that graphics is the only remaining issue.

For the x64 game, open its launcher menu (Y) and set **Direct3D 9/10/11** to
**DXVK**, or put this in `DarkSoulsII.wine-nx.txt` beside `DarkSoulsII.exe`:

```ini
d3d=dxvk
```

A template is supplied in `C:\dxvk64\DarkSoulsII.wine-nx.txt`. If a settings
file already exists, edit just the `d3d` key instead of replacing other settings.
The legacy `d3d9=dxvk` key is still read when `d3d` is absent. Saving through the
launcher migrates it to `d3d`. `d3d=wine` explicitly selects Wine again.

The same setting works for a game on USB (`D:` to `H:`). The executable's own
directory is still searched first. Check for pre-existing `dxgi.dll`,
`d3d11.dll`, `d3d10core.dll` or `d3d9.dll` there: an app-local wrapper can override
the central payload or mix incompatible DXVK versions. Back up conflicting
graphics wrappers before moving them out of the game's directory; do not
remove unrelated DLLs or overwrite game/Steam components.

Start at 1280x720 or below, preferably windowed for the first run. Keep DXVK's
default feature/binding choices initially; do not force unsupported extensions
or a fake GPU. For an optional overlay, add `dxvk.hud = fps,frametimes` to a
`dxvk.conf` beside the game. No performance or memory-saving claims are made
until measured on the Switch.

After a failure, retain `logs/autorun_runtime.log`, `logs/stdout.txt`, `logs/stderr.txt`,
`target.txt`, the game's `*.wine-nx.txt`, and DXVK's `DarkSoulsII_d3d11.log` /
`DarkSoulsII_dxgi.log` from the game's directory if present. DXVK also sends
messages through Wine's debug output. The runtime should print
`[DXVK] AMD64 payload C:\dxvk64` when the per-title setting is active.

Do not label DS2 working until the on-device D3D11 tests pass and the game
reaches and renders its menu/gameplay. Packaging and host tests cannot verify
that boundary.
