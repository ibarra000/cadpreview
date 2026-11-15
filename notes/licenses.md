# License register

Every library, tool and published specification the project relies on, with its license and
the date the license was checked. Add a row before relying on anything new.

## Shipped in the binaries

| Component | Version | Used for | License | Source | Checked |
|---|---|---|---|---|---|
| libpng | 1.6.59 | Writing PNG files | libpng-2.0 | https://github.com/pnggroup/libpng | 2026-11-14 |
| zlib | 1.3.2 | Compression for libpng | Zlib | https://github.com/madler/zlib | 2026-11-14 |
| cairo | 1.18.6 | Tier-4 vector drawing | MPL-1.1 (elected from LGPL-2.1-only OR MPL-1.1) | https://www.cairographics.org | 2026-11-15 |
| pixman | 0.46.4 | Rasterizer under cairo | MIT AND BSD-2-Clause | https://gitlab.freedesktop.org/pixman | 2026-11-15 |
| FreeType | 2.14.3 | Text rendering under cairo | FTL (elected from FTL OR GPL-2.0-or-later) | https://freetype.org | 2026-11-15 |
| Expat | 2.9.0 | cairo dependency | MIT (vcpkg: MIT AND CC0-1.0) | https://libexpat.github.io | 2026-11-15 |
| PThreads4W | 3.0.0 | Threads for cairo/pixman on Windows | Apache-2.0 | https://sourceforge.net/projects/pthreads4w | 2026-11-15 |
| dirent | 1.26 | cairo dependency on Windows | MIT | https://github.com/tronkko/dirent | 2026-11-15 |
| Noto Sans (Latin subset) | from notofonts main, 2026-11-15 | Embedded font for tier-4 text | OFL-1.1 (no reserved font name) | https://github.com/notofonts/latin-greek-cyrillic | 2026-11-15 |

All of these are linked statically into `cadpreview.dll` / `libcadpreview.so`. Their notices are
generated into `THIRD_PARTY_NOTICES.md` by `tools/third_party_notices.py`. That file ships with
the install tree and the NuGet package.

**FreeType credit (FTL):** portions of this software are copyright © 2006-2026 The FreeType
Project (www.freetype.org). All rights reserved.

The font subset was made with fonttools (MIT) using
`pyftsubset NotoSans-Regular.ttf --unicodes=U+0020-007E,U+00A0-00FF,U+00B0,U+00B1,U+00D8,U+2205,U+2300,U+2013,U+2014,U+2018-201D,U+2022,U+2026 --layout-features='' --no-hinting --desubroutinize`.
The source font's SHA-256 is 478c558e…3823.

### GPL build only (`CADPREVIEW_WITH_DWG=ON`)

| Component | Version | Used for | License | Checked |
|---|---|---|---|---|
| LibreDWG | 0.13.4 | DWG tier 4 | GPL-3.0-or-later | 2026-11-15 |
| jsmn | 2019-04-27 | LibreDWG dependency | MIT | 2026-11-15 |

Linking LibreDWG makes that build GPL-3.0-or-later as a whole. The default build and the NuGet
package do not include it. The GPL build installs `LIBREDWG-COPYING` and `JSMN-COPYING`.

The native library links the Microsoft C++ runtime dynamically (/MD), so the Visual C++
2015-2026 redistributable must be present on Windows. Whether the NuGet package should carry
the runtime DLLs app-locally is an open question for a maintainer.

## Build, test and development only (not distributed)

| Component | Version | Used for | License | Checked |
|---|---|---|---|---|
| vcpkg | 2026.10.16 | C++ dependency manager | MIT | 2026-11-14 |
| GoogleTest | 1.18.0 | C++ tests | BSD-3-Clause | 2026-11-14 |
| xunit.v3 | 4.0.3 | .NET tests | Apache-2.0 | 2026-11-14 |
| coverlet.MTP | 10.1.0 | .NET coverage | MIT | 2026-11-14 |
| pytest | 9.2.0 | Python tests | MIT | 2026-11-14 |
| pre-commit | 4.7.0 | Hook runner | MIT | 2026-11-14 |
| pre-commit-hooks | 6.0.0 | Hygiene hooks | MIT | 2026-11-14 |
| clang-format (wheel) | 23.1.5 | C/C++ formatting | Apache-2.0 WITH LLVM-exception | 2026-11-14 |
| clang-tidy (wheel) | 23.1.4 | C/C++ static analysis | Apache-2.0 WITH LLVM-exception | 2026-11-14 |
| cppcheck | distro package | C/C++ static analysis (CI) | GPL-3.0-or-later (tool only) | 2026-11-14 |
| gcovr | 8.8 | C++ coverage report | BSD-3-Clause | 2026-11-14 |
| black | 26.10.1 | Python formatting | MIT | 2026-11-14 |
| isort | 9.0.2 | Python import order | MIT | 2026-11-14 |
| ruff | 0.17.5 | Python linting | MIT | 2026-11-14 |
| mypy | 2.4.1 | Python type checking | MIT | 2026-11-14 |
| bandit | 1.9.4 | Python security lint | Apache-2.0 | 2026-11-14 |
| GitHub Actions (actions/*) | pinned by SHA | CI | MIT | 2026-11-14 |

FluentAssertions was considered for .NET tests and not used: from version 8 it is released
under a commercial license, which does not fit an Apache-2.0 project's contributors and users.

## Published specifications

| Specification | Used for | Terms | Source | Checked |
|---|---|---|---|---|
| [MS-CFB] Compound File Binary File Format | OLE container signature | Microsoft Open Specifications (documentation published for implementers; patent promise) | https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-cfb/ | 2026-11-14 |
| PKWARE APPNOTE.TXT 6.3.10 | ZIP local header signature | PKWARE notice in the document; the format may be implemented freely | https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT | 2026-11-14 |
| Autodesk DXF Reference | DXF reader (group codes, sections, entities) | Publicly published by Autodesk for DXF interoperability; used for facts only, nothing copied | https://help.autodesk.com (AutoCAD DXF Reference) | 2026-11-15 |
| Microsoft BITMAPINFOHEADER / BITMAPFILEHEADER documentation | DIB decoder | Microsoft documentation published for implementers | https://learn.microsoft.com/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader | 2026-11-15 |

## Pending decisions (not yet used)

| Component | License | Note |
|---|---|---|
| OpenCascade | LGPL-2.1 with exception | Must be linked dynamically. Planned for tier 3. |

stb_image was replaced by libpng plus an in-house DIB decoder (owner decision, 2026-11-15).
