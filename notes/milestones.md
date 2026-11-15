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

## Milestone 2: DWG and DXF, embedded preview (tier 1) and 2D vector (tier 4)

Status: complete, awaiting sign-off. Version 0.2.0 (unreleased).

### Formats and versions covered

| Format | Versions checked on real files | Tier 1 | Tier 4 |
|---|---|---|---|
| DWG | AC1021 (2007-2009), AC1024 (2010-2012) | in-house, all builds | LibreDWG, GPL build only |
| DXF (ASCII) | none: no vendor DXF samples exist; synthetic fixtures only | in-house | in-house |
| DXF (binary) | detected only | no | no |

The DWG preview layout ([dwg.md](dwg.md#preview-block)) is confirmed on 17 Autodesk sample
drawings. DXF claims in [dxf.md](dxf.md) come from the published DXF Reference and stay
hypotheses until real DXF files are available.

### Results

Corpus run `corpus.py run --cli build/win-release-dwg/bin/cadpreview.exe --tier 0 --tier 1 --tier 4`,
17 DWG sample drawings (local-only, never committed):

| Format | Tier | Files | ok | Crashes | Timeouts | ok % | Median / max ms |
|---|---|---|---|---|---|---|---|
| DWG | auto (0) | 17 | 17 | 0 | 0 | 100% | |
| DWG | 1 forced | 17 | 17 | 0 | 0 | 100% | 10 / 20 |
| DWG | 4 forced (GPL build) | 17 | 15 | 0 | 0 | 88% | 17 / 370 |
| DWG | 4 forced (default build) | 17 | 0 | 0 | 0 | 0% (tier unavailable, by design) | |
| DXF | 1 and 4 | 2 synthetic | as expected | 0 | 0 | n/a (synthetic never counts) | |

All 17 tier-1 previews and a sample of the tier-4 images (floor plan, mechanical part,
title block) were opened and checked by eye; all are recognizable. The one DWF sample is detected as unknown,
as expected (DWF is out of scope).

Top failure reasons: DWG tier 4 `no visible entities` (2: `visualization_-_aerial.dwg` and
`visualization_-_sun_and_sky_demo.dwg`, whose model space holds only 3D solids and surfaces).
DWG tier 4 misses the 90% target by one file; auto mode is unaffected because tier 1 succeeds
on every file.

| Check | Result |
|---|---|
| Windows x64 release / ASan (local) | 171 / 166 tests pass |
| Windows x64 GPL build (local) | 171 tests pass |
| GCC 15 strict warnings (local syntax check, incl. LibreDWG adapter) | clean |
| clang-tidy 23 (default and GPL builds) / pre-commit | clean |
| Fuzz harnesses | 5 (render, C API, DXF, DWG preview, image decode); replay in CTest, 60 s each in CI, 20 min each nightly |

### Review findings

Independent C++ and security reviews of the milestone found four HIGH denial-of-service issues
on hostile DXF input. All are fixed, each with a regression test:
- a MINSERT array whose row × column count wrapped around 64 bits and skipped the limit;
- block expansion that was never charged when the blocks drew nothing (10^12 visits from a few
  KB of input);
- a work limit that counted objects rather than points and text bytes;
- an arc-angle loop that never ended for angles above about 1e16.

The converter now charges one work unit per record visited, per array copy, per output point
and per 16 bytes of text (4 million units by default).

Also fixed:
- Spline degree is capped at 25.
- DXF numbers must be finite.
- Extents near ±DBL_MAX are fitted without overflowing.
- Text that is huge, has a NaN rotation or an absurd width factor is skipped instead of breaking
  the whole render.
- DIB and PNG decoders check that the pixel data can exist before allocating the image.
- DXF group limit lowered to 16 million.
- Locale-independent upper-casing.
- LibreDWG adapter:
  - null checks on every LibreDWG pointer;
  - the 256 MiB cap applied before LibreDWG opens the file;
  - model space no longer translated twice;
  - LWPOLYLINE "closed" now read from bit 512 (bit 1 means "has extrusion");
  - split into functions under 50 lines.
- The DXF fuzzer now runs through the renderer, and CI fuzzing uses a 10 s per-input timeout.

Not fixed, and recorded here:
- LibreDWG still runs in-process with no time limit and has no fuzz harness, because it reads
  only from a path (see open question 2). Non-ASCII Windows paths may not reach it.
- Low-impact rendering gaps:
  - `%%nnn` codes;
  - extrusion direction (group 210, mirrored arcs);
  - text extents that ignore alignment;
  - an unknown MTEXT escape without `;`, which drops the rest of the string.

### Decisions and deviations from the spec

- **Vendor samples** (owner-approved exception to rule 5): Autodesk's published sample
  drawings are used locally from the gitignored `corpus/vendor/`; only their metadata is in
  `corpus/manifest.csv`. CI runs on synthetic fixtures only.
- **LibreDWG** (GPL-3.0-or-later) is linked only in the opt-in `CADPREVIEW_WITH_DWG` build;
  the default build and NuGet package stay Apache-2.0. It builds with MSVC, so no MinGW DLL
  was needed.
- **cairo is linked statically** under its MPL-1.1 option (milestone 1 proposed dynamic
  linking). MPL-1.1 is file-level copyleft: it covers cairo's own files, which are unmodified
  apart from one build-flag patch published in `ports/cairo/`. A maintainer should confirm
  this reading.
- **Thumbnails** are decoded with libpng and an in-house DIB decoder instead of stb_image
  (owner decision).
- **Text** is drawn with an embedded Noto Sans subset (OFL-1.1), so output does not depend
  on installed fonts. `%%c` (diameter) is drawn as Ø because the subset has no U+2300.
- **Tiers left out of a build** stay registered as unavailable: auto mode adds a warning,
  forcing one returns `tier_unavailable` with the reason.

### Open questions for a maintainer

1. **Version coverage**: the spec asks for at least three versions per format. The DWG
   samples cover only AC1021 and AC1024, and there are no vendor DXF files. Converting files
   with AutoCAD would break rule 2, so more samples must come from contributors' own
   drawings or openly licensed sets (for example AC1015, AC1018, AC1027, AC1032).
2. **LibreDWG risk**: it is C code with a history of CVEs, and it reads only from a file
   path (so `cp_render_memory` cannot use DWG tier 4). It runs in-process, serialized behind
   a mutex. Running it out of process would isolate crashes; worth deciding before the GPL
   build is offered to anyone.
3. **3D-only drawings**: tier 4 draws 2D entities only. Solids, surfaces and meshes need the
   tier 2/3 work from later milestones, or a wireframe fallback.
4. **Monochrome output**: all entities draw in one dark ink. Colour index 7 is black or white
   depending on the background, and many ACI colours are unreadable on white.
5. **cairo static linking under MPL-1.1** (see above), and whether the VC++ runtime should
   ship app-locally (still open from milestone 1).

### Hard rules that came close to being an issue

- Rule 5 (clean test files): the Autodesk samples needed the owner's explicit exception;
  they are never committed or redistributed.
- Rule 6 (licenses): LibreDWG's GPLv3 was flagged and approved as an opt-in build only.
- No vendor software was run; no SDKs, decompilers or leaked material were used. The DWG
  preview layout was learned by inspecting the sample files with a hex dump.
