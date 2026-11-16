# cadpreview

cadpreview is an open-source, read-only library that turns a CAD file into a single 2D PNG
preview. It is written in C++20 behind a small, stable C API, with a thin .NET wrapper and a
thin command-line tool on top. Windows (MSVC) is the primary platform; the code also builds and
is tested on Linux.

> **Status: milestone 2.** Reads DWG and DXF files. Other formats produce a neutral placeholder
> that names the detected format and the reason. See [Roadmap](#roadmap).

## Supported formats

| Format | Tier 1 (embedded preview) | Tier 4 (vector line drawing) |
|---|---|---|
| DWG (R13 and later; checked on AC1021, AC1024) | yes | with the separate `cadpreview-dwg` helper (LibreDWG, GPL); solids and surfaces as wireframes |
| DXF (ASCII) | yes (THUMBNAILIMAGE) | yes; 3D meshes as wireframes, ACIS solids counted only |
| DXF (binary) | detected, not read yet | detected, not read yet |

The library, CLI and `CadPreview` NuGet package are Apache-2.0 and contain no GPL code. DWG
vector rendering uses LibreDWG, which is GPL-3.0, so it runs in a separate program,
`cadpreview-dwg`. The library starts that program when it is installed and reads the drawing
from its output. Build it with `CADPREVIEW_BUILD_DWG_HELPER=ON` (presets `win-release-dwg`,
`linux-debug-dwg`); see [helpers/dwg/README.md](helpers/dwg/README.md). Without the helper,
DWG files still get their embedded preview, and tier 4 is reported as unavailable with a
warning.

## How it renders

For each file, cadpreview tries the simplest method that works and stops at the first one
that yields an image:

| Tier | Method |
|---|---|
| 1 | Preview image embedded in the file |
| 2 | Cached display mesh, rendered by a software rasterizer |
| 3 | B-rep geometry from the modeling kernel stream, rebuilt and tessellated |
| 4 | Vector entities, with 3D bodies as an isometric wireframe (DWG/DXF only) |
| 5 | Placeholder with the format name and a short reason |

Formats are detected from file content, never from the extension. Unreadable, unsupported,
encrypted or corrupt files never fail: they produce a placeholder, and the result says why.

## Command line

```sh
cadpreview part.sldprt -o part.png [--size 512] [--tier 0-5] [--verbose]
```

It writes the PNG and prints one JSON line:

```json
{"input":"part.sldprt","output":"part.png","format":"unknown","version":null,"tier":5,"ok":false,"render_ms":1.2,"reason":"unknown_format","reason_detail":"unrecognized OLE compound file","warnings":[]}
```

The exit status is 0 whenever a PNG was written, including placeholders. It is 2 for usage
errors (bad arguments, or an output path that cannot be written) and 1 only if memory runs out.

## C API

```c
#include <cadpreview/cadpreview.h>

cp_options opts = {0};
opts.struct_size = sizeof opts;
opts.size = 512;

cp_preview* preview = NULL;
if (cp_render_file("part.sldprt", &opts, &preview) == CP_OK) {
    const void* png; size_t len;
    cp_preview_png(preview, &png, &len);          /* write png/len to disk */
    struct cp_preview_info info = {0};
    info.struct_size = sizeof info;
    cp_preview_info(preview, &info);              /* info.ok, info.tier, info.reason ... */
    cp_preview_free(preview);
}
```

See [include/cadpreview/cadpreview.h](include/cadpreview/cadpreview.h) for the full contract.
In short, every function is thread-safe, and the library keeps no global state and never
prints. Only plain C types cross the DLL boundary. Error statuses mean an invalid argument or
out of memory; anything wrong with the file yields a placeholder. `struct cp_preview_info` has
no typedef because its name is taken by the function, as with POSIX `struct stat`.

C++ projects can use the installed CMake package:

```cmake
find_package(cadpreview CONFIG REQUIRED)
target_link_libraries(app PRIVATE cadpreview::cadpreview)
```

## .NET

```csharp
using CadPreview;

using var preview = Previewer.Render("part.sldprt", new RenderOptions { Size = 512 });
preview.Save("part.png");
```

The `CadPreview` NuGet package (net10.0) ships the native library for win-x64 and win-arm64,
with the C++ runtime linked in statically, so no Visual C++ redistributable is needed.
The managed assembly is `CadPreview.Net.dll`, so it cannot collide with the native
`cadpreview.dll` on case-insensitive file systems.

## Building

Prerequisites: Visual Studio 2022 17.14+ or 2026 with the C++ workload (CMake 3.31+ and Ninja
are included), Git, Python 3.11+, and the .NET 10 SDK. On Linux: GCC 13+ or Clang 18+,
CMake 3.31+, Ninja, and the vcpkg prerequisites (curl, zip, unzip, tar, pkg-config).

```sh
git clone --recurse-submodules https://github.com/ibarra000/cadpreview
cd cadpreview
python -m venv .venv
.venv/Scripts/pip install -r requirements-dev.txt     # .venv/bin/pip on Linux
```

Then, from a "Developer PowerShell for VS" (or any shell on Linux):

```sh
cmake --workflow --preset win-release    # configure, build, run every test suite
cmake --workflow --preset win-asan       # the same under AddressSanitizer
cmake --install build/win-release --prefix build/win-release/stage
```

Linux presets: `linux-debug`, `linux-asan` (ASan + UBSan), `linux-tsan`, `linux-coverage`,
and `linux-fuzz` (Clang + libFuzzer). Dependencies come from vcpkg, pinned by the
`external/vcpkg` submodule and `vcpkg.json`.

To pack and try the NuGet package locally:

```sh
python tools/nuget.py pack --stage win-x64=build/win-release/stage --version-suffix local
python tools/nuget.py smoke --version 0.2.0-local
```

## Repository layout

| Path | Contents |
|---|---|
| `include/cadpreview/` | Public C header |
| `src/` | Library: `api/` (C boundary), `containers/`, `formats/`, `tiers/`, `render/` |
| `cli/` | Command-line tool |
| `bindings/dotnet/` | .NET wrapper and its tests |
| `tests/` | C++, C, CLI and packaging tests |
| `fuzz/` | Fuzz harnesses |
| `corpus/` | Test files and their provenance manifest |
| `notes/` | Format findings, license register, milestone reports |
| `helpers/dwg/` | `cadpreview-dwg`, the separate GPL program that decodes DWG with LibreDWG |
| `tools/` | Corpus runner and NuGet packing scripts |

## Roadmap

1. Package skeleton, public API, CLI, CI, placeholder output, corpus manifest *(0.1)*
2. DWG/DXF: embedded preview, then vector rendering *(0.2)*
3. Will read SolidWorks files: embedded preview
4. Will read Inventor and Fusion 360 files: embedded preview
5. Will read Siemens NX files: embedded preview
6. Will read PTC Creo files: embedded preview, told apart from NX by content
7. Cached display meshes for the 3D formats
8. B-rep geometry through OpenCascade (Parasolid, ACIS/ShapeManager, then Creo)

## Clean-room policy

Formats are learned only by inspecting sample files and published specifications. The project
does not use vendor SDKs, run vendor software in development or tests, decompile vendor
programs, or touch leaked material. Encrypted or password-protected content is skipped, never
broken. Test files are self-made by licensed contributors, openly licensed, or synthetic, and
each one's origin is recorded. See [notes/README.md](notes/README.md).

## Trademarks

AutoCAD, Inventor and Fusion 360 are trademarks of Autodesk, Inc. SolidWorks and CATIA are
trademarks of Dassault Systèmes. Siemens NX and Parasolid are trademarks of Siemens. Creo is
a trademark of PTC Inc. These names are used only to say which files cadpreview reads. This
project is not affiliated with, endorsed by, or sponsored by any of these companies.

## License

Apache License 2.0; see [LICENSE](LICENSE) and [NOTICE](NOTICE). Third-party components are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
