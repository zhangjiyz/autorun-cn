# AutoRunNX Biko3 source changes

This directory vendors D7VK Sarek v1.13.0 from
`pythonlover02/dxvk-sarek` at commit
`37f397e142b977a343e920dbc4c7bf7ed2c63a81`.
The original authors' notices and license remain in `LICENSE` and the
included dependency directories. This is a modified source version, not an
unmodified upstream release.

AutoRunNX changes relative to that commit:

- `src/ddraw/d3d7/d3d7_device.cpp`: opt-in hooks for the verified Biko3 code layout, with an
  opaque sort plan cache, conservative far-wall skip, and discarded triangle
  visibility block skip. `WINE_NX_D7VK_BIKO3_PATCHES=1` enables them; image
  base and original instructions are checked before patching. The executable
  stays unchanged on disk.
- `src/util/log/log.h`: use Wine's i386 `__cdecl` calling convention for
  `__wine_dbg_output`.
- `cross-i686-wine-nx.txt`: local cross-compilation configuration.

The upstream Git metadata, CI files, and dependency test directories are
omitted from this vendored source tree. No built DLLs or device logs are
included.

See `../../docs/biko3-optimizations.md` for build steps, device evidence,
and remaining validation limits.
