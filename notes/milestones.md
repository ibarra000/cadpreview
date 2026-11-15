# Milestone reports

## Milestone 1: package skeleton, API, CLI, CI, placeholder, corpus manifest

Status: complete, awaiting sign-off. Version 0.1.0 (unreleased).

### Formats and versions covered

None yet, by design. Every input goes through detection and the strategy ladder and ends in a
tier-5 placeholder. Detection recognises only the published OLE compound file and ZIP
signatures (see [containers.md](containers.md)) and reports them as "unknown".

### Results

| Check | Result |
|---|---|
| Corpus run (`corpus.py run --tier 0 --tier 1 --tier 5`) | 15 renders of 5 synthetic files: 0 crashes, 0 timeouts, all placeholders with the expected reason |
| Success rate per format | n/a: no real corpus files yet; synthetic files never count |
| Windows x64 (MSVC 14.51, local) | 105 tests pass (C++ unit and API, C11 header, export list, install + find_package consumer, installed CLI, CLI, corpus tools, fuzz replay, .NET) |
| Windows x64 AddressSanitizer (local) | 100 tests pass |
| GCC 15 strict warnings (local syntax check) | clean |
| clang-tidy 23 / pre-commit hooks | clean |
| NuGet | packs win-x64 (+ win-arm64 in CI); fresh console app restores it and renders, including a RID-specific publish |
| CI (GitHub Actions run 43732597915) | all 12 jobs green: Windows x64 + ASan, Windows ARM64, Linux GCC / Clang / ASan+UBSan / TSan, coverage, clang-tidy, cppcheck, libFuzzer (2 harnesses), pre-commit, NuGet pack + smoke on x64 and ARM64 |
| Line coverage (gcovr, src + cli) | 91.6% lines, 99.2% functions, 66.5% branches (gate: 80% lines) |

Most common failure reasons: `unknown_format` (8), `placeholder_requested` (4),
`empty_input` (3).

### Decisions and deviations from the spec

- **.NET 10** instead of .NET 8 (owner's decision; .NET 8 went out of support on 2026-11-10).
- **`struct cp_preview_info` has no typedef**: in C the spec's function and type names
  collide. Both names are kept, as with POSIX `struct stat` and `stat()`.
- **API additions**: `reason_code` + `cp_reason_name()`, explicit `reserved` fields, output
  structs report the bytes filled, and unknown trailing input fields must be zero.
- **Managed assembly is `CadPreview.Net.dll`**: `CadPreview.dll` and the native
  `cadpreview.dll` collide on Windows' case-insensitive file system.
- **libpng** (simplified API) writes PNGs instead of stb_image_write, which crashes on
  allocation failure.
- **GoogleTest** instead of Catch2, and **pytest** for the Python suites, per the
  contributors' language rules.
- **CLI**: exit 2 for usage errors including an unwritable output; exit 1 only when not even
  a placeholder can be built. One input per invocation; the corpus tool loops.
- `CADPREVIEW_WITH_DWG` / `CADPREVIEW_WITH_OCCT` arrive with the milestones that use them.

### Open questions for a maintainer

1. VC++ runtime: ship the redistributable DLLs app-locally in the NuGet package, or keep
   documenting it as a prerequisite?
2. Git LFS quota on a public repository versus a separate corpus download.
3. Thumbnail decoding for milestone 2: stb_image (spec) is documented as not hardened
   against malicious input; consider libpng/libjpeg-turbo or Wuffs instead.
4. Windows long paths (260+ characters) in host processes that are not long-path aware, and
   the race between checking and opening a file (a swapped-in FIFO could block on POSIX):
   both need platform file APIs; planned with the first real parser.

### Hard rules that came close to being an issue

None. Only published specifications ([MS-CFB], APPNOTE) and synthetic fixtures were used;
no vendor files, software, SDKs or leaked material were touched. FluentAssertions was not
adopted because its current license is commercial.

### Needed for milestone 2 (DWG/DXF)

- Sign-off on milestone 1.
- DWG and DXF sample files from at least three AutoCAD versions, with their origin.
- A decision on LibreDWG (GPLv3; would be a separate, off-by-default DLL, built with MinGW if
  MSVC cannot build it).
- Agreement to link cairo (LGPL-2.1 / MPL-1.1) dynamically.
