# Overlay ports

Copies of ports from the pinned vcpkg baseline (external/vcpkg) with one change each.
Re-copy them and reapply the change when the baseline moves.

- `cairo`: adds `static-define-for-cpp.patch`. Upstream cairo's meson build defines
  `CAIRO_WIN32_STATIC_BUILD` for C sources only, so in a static Windows build the C++
  DirectWrite backend still marks its functions `dllexport`, and they appear in the export table
  of any DLL that links cairo (caught by the `abi.exports` test). The patch applies the define to
  C++ as well. It changes cairo's build script, not its source files.
