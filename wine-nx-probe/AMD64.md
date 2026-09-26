# AMD64 bring-up

This build adds an AMD64 path alongside the existing x86/WoW64 path. The NRO
and Unix backends remain native AArch64. Wine's ARM64EC loader handles mixed
AMD64/ARM64EC modules; `winebox64ec.dll` runs AMD64 code through Box64. Common
system DLLs are built as ARM64X, with both native ARM64 and ARM64EC code.

Box64 is pinned to **v0.4.4**, commit
`2f130fab1d6e1a4ee8a71dc60cfdfcc839ad192a`. The bootstrap script fetches that
revision, not the latest branch. CMake checks the revision and clean vendor
tree, then applies checked adaptations to generated copies in the build
directory. It uses the execution core, not Box64's Linux ELF loader or wrappers.

## Launch profiles

Use a **39-bit address-space application forwarder** for AMD64. The runtime
checks this before initializing Wine. An NRO chainload cannot change the
address-space mode of the running Horizon process.

The same NRO selects x86/WoW64 or AMD64/ARM64EC from the executable's PE header.
Keep a separate 32-bit-address-space forwarder for x86 programs that require
fixed low addresses. Relocatable x86 programs can also use the 39-bit launch
profile when enough low address space is available. Changing profiles requires
launching a different application process, not another NRO in the same process.
The launcher can install both the main 39-bit forwarder and the 32-bit no-alias
forwarder from Settings.

## Build

Requirements: an ARM64 Linux/macOS host, LLVM-MinGW with ARM64EC support,
Wine's normal build prerequisites, Docker and `devkitpro/devkita64` for ARM64.
The tested toolchain is LLVM-MinGW 20260505 / Clang 22.1.5.

```sh
export WINE_NX_LLVM_MINGW=/path/to/llvm-mingw
sh wine-nx-probe/build-amd64-components.sh
```

The script configures `aarch64,arm64ec,i386`, enables both CPU DLLs, builds the
NRO, follows direct/delayed imports and used forwarders in both ARM64X views,
and produces
`wine-nx-probe/build-switch-amd64/wine-nx-amd64-box64.zip`.
`WINE_NX_PE_BUILD_DIR`, `WINE_NX_BUILD_DIR`, and `WINE_NX_JOBS` override build
locations and parallelism. Set `WINE_NX_BOX64_DYNAREC=OFF` for interpreter
bring-up. `--minimal` on `tools/package-amd64.py` stages only console-test DLLs.
Use `--interpreter-nro /path/to/wine-nx-runtime.nro` to include a separately
built interpreter runtime in the archive as a diagnostic alternative.
After a successful build, `--no-build` repackages existing DLLs without Makefile
scans. It still validates the complete dependency closure and rejects missing
files, but does not check source freshness; rebuild first after source changes.

To link an already built mesa-switch installation, set
`WINE_NX_MESA_SWITCH_DIR` to its library directory **inside the container**,
for example `/work/wine-nx-probe/build-mesa-switch/install/opt/devkitpro/portlibs/switch/lib`.
The existing `build-mesa-switch.sh` accepts `WINE_NX_MESA_SWITCH_SRC` for the
local Mesa checkout. Set `WINE_NX_DXVK=1` to build and include the pinned AMD64
DXVK 3.1.1 payload; see [DXVK.md](DXVK.md) for requirements and DS2 validation.
With Mesa enabled, the package is named
`wine-nx-amd64-box64-mesa-vulkan.zip` and records the exact mesa-switch commit.
The DXVK option searches `C:\dxvk` for x86 and `C:\dxvk64` for AMD64, so it
cannot accidentally load the existing x86 payload into a 64-bit application.
The archive's `build-manifest.json` records the source revisions, enabled
features and file hashes. A default build uses the devkitPro OpenGL driver;
Vulkan requires the separate mesa-switch build above.
The DXVK-enabled archive is `wine-nx-amd64-box64-mesa-dxvk.zip`. Select DXVK
per title with `d3d=dxvk` or the launcher's Direct3D option; Wine remains the
default. The older `d3d9=dxvk` setting remains readable.

`tools/package-autorun.py` merges the standard
`wine-nx-amd64-box64-mesa-dxvk-vkd3d.zip` into the full x86 package. Set
`WINE_NX_AMD64_PACKAGE` or pass `--amd64` when the archive is elsewhere.

## First hardware checks

Back up `switch/wine` before replacing the NRO and system DLLs together. Do not
mix a new NRO with an older CPU DLL. Copy the archive's `switch` directory to
the SD card, then start the 39-bit forwarder and select `pe64-smoke.exe`.

The smoke test checks imports, GS/TEB access, allocations above 4 GiB, SSE2,
rounding-mode preservation across native calls, callbacks, TLS, and thread
creation/join. Expected result: `RESULT PASS` in
`pe64_smoke.log` and process exit code zero. Runtime diagnostics are in
`logs/autorun_runtime.log` and `logs/horizon-trace.log`.

The full package also includes these native x64 programs in
`C:\win64-tests`: functional, threads, lifecycle, messages, timers,
video-startup, section, wasapi, audio, OpenGL, D3D9, and Vulkan when Mesa is
enabled. The wasapi test performs waveOutOpen's steps through mmdevapi
directly, from a message-only window on a second apartment, and reports
each result before the audio test uses winmm.
The default target is `pe64-functional.exe`. Run them in that order; successful
tests print `PASS` and exit with `0x2a`. OpenGL, D3D9, and Vulkan show red,
green, then blue as their hardware check. The Vulkan test also validates NVK
memory mapping, synchronization, image readback, a Win32 surface, swapchain,
and presentation. Then run `pe32-functional.exe`, `pe32-threads.exe`, and
`pe32-lifecycle.exe` to check the existing x86 path.
If the archive includes `wine-nx-runtime-interpreter.nro`, keep a copy of the
normal NRO, then replace `wine-nx-runtime.nro` with that file to repeat a
failing test without the dynarec. It uses the same DLL payload and still needs
the 39-bit forwarder for AMD64.

## USB drives

FAT32 and exFAT USB drives mount at startup through libusbhsfs, with the same
UASP transport and BOT fallback as Cemu-nx. The first five volumes are `D:` to
`H:` in Wine, and the launcher lists programs found in each volume's `Wine`
folder, two folder levels deep: `Wine\Game\Game.exe` or
`Wine\Game\Bin\Game.exe`. Use exFAT for games with files over 4 GiB, which
FAT32 cannot hold. NTFS and ext4 are not supported. Plug the drive in before
starting Autorun, and close the running program before unplugging it.

## Host regression checks

On AArch64 Linux, run `sh wine-nx-probe/check-amd64.sh` for both CPU modes,
ARM64EC assembly/exception checks, the Unix bridge, and image/address tests.
`WINE_NX_TEST_BUILD_DIR` selects the host test build directory. Run
`CC=clang sh wine-nx-probe/check-runtime-console.sh` for the broader platform
suite. `check-audio.sh` also needs `WINE_NX_PE_BUILD_DIR` set to the configured
PE build directory for generated headers. Sanitizer failures can be made
fatal with `UBSAN_OPTIONS=halt_on_error=1`.

## Validation boundary

The user reports that the Win64 validation suite passes on Switch. This is not
a claim of general Win64 game compatibility or verification of the new DXVK
path. Host tests cover the CPU core, transition assembly,
Unix ABI, image parsing and address-space bookkeeping; cross-compilation checks
the PE DLLs and NRO. They cannot establish hardware graphics/audio behavior.
Both x86 and AMD64 core suites passed in interpreter and dynarec modes on
AArch64 Linux. The assembly fixtures passed; platform and Unix-bridge tests
also passed with fatal sanitizer checks. Both NRO variants built with
devkitA64 GCC 15.2. The smoke executable also
passed on Windows; that validates the test, not Autorun on Switch.

Known limits include precise resumable exceptions inside translated code,
remote thread context/suspend handling, AVX/XSTATE, and the base port's existing
multi-process and service limitations. Emulator faults without a precise guest
snapshot are noncontinuable. The dynarec's existing instruction-budget and
concurrent self-modifying-code limitations also remain. MMX values survive
emulator exits in private thread state, but full x87/MMX aliasing and MMX
exposure through Windows contexts are not implemented. Start with the supplied
console test before trying games.
