# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/). The version is set once, in `CMakeLists.txt`.

## [Unreleased]

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
