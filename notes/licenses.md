# License register

Every library, tool and published specification the project relies on, with its license and
the date the license was checked. Add a row before relying on anything new.

## Shipped in the binaries

| Component | Version | Used for | License | Source | Checked |
|---|---|---|---|---|---|
| libpng | 1.6.59 | Writing PNG files | libpng-2.0 | https://github.com/pnggroup/libpng | 2026-11-14 |
| zlib | 1.3.2 | Compression for libpng | Zlib | https://github.com/madler/zlib | 2026-11-14 |

Both are linked statically into `cadpreview.dll` / `libcadpreview.so`; their notices are in
`THIRD_PARTY_NOTICES.md`, which ships with the install tree and the NuGet package.

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

## Pending decisions (not yet used)

| Component | License | Note |
|---|---|---|
| LibreDWG | GPL-3.0-or-later | Linking makes the program GPLv3. Planned as the off-by-default `CADPREVIEW_WITH_DWG` option; needs maintainer approval before it is added. |
| OpenCascade | LGPL-2.1 with exception | Must be linked dynamically. Planned for tier 3. |
| cairo | LGPL-2.1 or MPL-1.1 | Planned for 2D vector drawing; link dynamically or use under MPL-1.1. |
| stb_image | MIT or public domain | Planned for thumbnail decoding. Its documentation says it is not hardened against malicious input; consider a hardened decoder for untrusted files. |
