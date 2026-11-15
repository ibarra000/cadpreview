# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/). The version is set once, in `CMakeLists.txt`.

## [Unreleased]

## [0.2.0] - unreleased

### Added

- DWG: format and version detection, and tier 1 (embedded preview) read from the documented
  image-seeker layout, which was confirmed on 17 Autodesk sample files (AC1021, AC1024).
- DWG tier 4 through LibreDWG, available only in the opt-in GPL build (`CADPREVIEW_WITH_DWG`).
  Default builds mark the tier unavailable and add a warning to the preview.
- DXF (ASCII): tier 1 from THUMBNAILIMAGE, and tier 4 from the in-house reader.
- Tier 4 drawing with statically linked cairo and an embedded Noto Sans subset.
- In-house DIB decoder and libpng PNG decoding, fitted onto the canvas with the 5% margin.
- `CP_FORMAT_DWG`, `CP_FORMAT_DXF`; `CadFormat.Dwg`, `CadFormat.Dxf`.
- Fuzz harnesses for DXF, the DWG preview and image decoding, plus nightly fuzzing.
- `corpus.py add` and local-only `vendor-sample` corpus entries.

### Changed

- The registry can list tiers that a build leaves out; forcing one reports why.

## [0.1.0] - unreleased

### Added

- Public C API (`cadpreview.h`): `cp_render_file`, `cp_render_memory`, `cp_detect_format`,
  `cp_preview_png`, `cp_preview_info`, `cp_preview_free`, `cp_version`, `cp_reason_name`.
  Extensible structs with `struct_size`, reason codes, and a log callback.
- Strategy-ladder framework with tier forcing; every input currently yields a tier-5
  placeholder naming the format and the reason.
- Content-based detection of OLE compound files and ZIP archives (container only).
- `cadpreview` command-line tool printing one JSON line per file.
- `CadPreview` .NET wrapper (net10.0) and NuGet packaging for win-x64 and win-arm64.
- CMake package config (`find_package(cadpreview)`), presets, vcpkg manifest.
- Test suites (C++, C11 header, CLI, corpus tools, .NET), fuzz harness, corpus manifest and
  validator, CI on Windows x64/ARM64 and Linux.
