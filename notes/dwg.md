# DWG

Findings about AutoCAD DWG files. Samples are the Autodesk downloads listed in
`corpus/manifest.csv` under `vendor/dwg/` (local only, never committed). All observations were
made by reading the files with a short Python script (struct unpacking at the offsets below);
no AutoCAD or Autodesk SDK was used.

## version-string

**Claim:** A DWG file starts with six ASCII bytes naming its format version: `AC10` plus two
digits. Observed values: `AC1021` and `AC1024`.
**Status:** confirmed for AC1021 and AC1024
**Evidence:** all 17 files in `vendor/dwg/` (9 × AC1021, 8 × AC1024).
**Method:** printed bytes 0-5 of each file.
**Versions:** AC1021, AC1024.

Release names (general knowledge, not confirmed from files): AC1015 = AutoCAD 2000 format,
AC1018 = 2004, AC1021 = 2007, AC1024 = 2010, AC1027 = 2013, AC1032 = 2018. cadpreview reports
the AC code, which is what the file contains.

## image-seeker

**Claim:** The little-endian 32-bit value at offset 0x0D is the absolute file offset of the
preview block.
**Status:** confirmed
**Evidence:** all 17 files in `vendor/dwg/`. The value was 0x1C0 or 0x1F0 in AC1024 files and
between 0xD20 and 0xF20 in AC1021 files; in every file the bytes at that offset are the start
sentinel below.
**Method:** read u32 at 0x0D, compared the 16 bytes at that offset with the sentinel.
**Versions:** AC1021, AC1024.

## preview-block

**Claim:** The preview block is:

| Offset (from block start) | Size | Meaning |
|---|---|---|
| 0 | 16 | start sentinel `1F 25 6D 07 D4 36 28 28 9D 57 CA 3F 9D 44 10 2B` |
| 16 | 4 | overall size N of what follows, little-endian |
| 20 | 1 | entry count |
| 21 | 9 per entry | type (1 byte), absolute start (u32), size (u32) |
| 20 + N | 16 | end sentinel: the bitwise NOT of the start sentinel (`E0 DA 92 F8 …`) |

Entry types seen: **1** (always 80 bytes; not an image), **2** (a DIB that starts with a
40-byte BITMAPINFOHEADER, `28 00 00 00`). Files had 2 or 3 entries; the four
`visualization_-_*` files carry two type-2 DIBs (118 824 and 15 824 bytes).
**Status:** confirmed
**Evidence:** all 17 files in `vendor/dwg/`.
**Method:** parsed the table after the sentinel, checked that each entry lies inside the file,
that type-2 data starts with a BITMAPINFOHEADER, and that the end sentinel sits at 20 + N.
**Versions:** AC1021, AC1024.

**Hypotheses, not yet confirmed (no samples):** type 6 as PNG data in newer versions, and type 3
as WMF in older ones. cadpreview accepts a type-6 entry only when its bytes start with the PNG
signature, and never decodes WMF.

cadpreview picks the largest image entry. Pre-R13 files (no image seeker) are not covered.

## helper-protocol

**Claim:** not a format fact: this is cadpreview's own contract between the library and the
`cadpreview-dwg` helper, which decodes DWG with LibreDWG in a separate process.
**Status:** design (owner decision 2026-11-15: isolate LibreDWG, keep cairo away from GPL code)

- The library starts `cadpreview-dwg --max-memory-mb 2048 <file>` with the file's UTF-8 path,
  standard input from the null device, and only the two output pipes inherited
  (`src/platform/process_*.cpp`).
- The helper limits its own memory (Windows job object, POSIX `RLIMIT_AS`), refuses files over
  256 MiB or without the `AC10` prefix, and writes ASCII DXF to standard output:
  - TABLES: frozen or off layers, as `LAYER` entries with group 70 = 1;
  - BLOCKS: block definitions;
  - ENTITIES: model-space entities, then paper-space entities marked with group 67 = 1.
  Layers and blocks are named by object index (`L12`, `B40`). 3D content keeps its z values:
  LINE and 3DFACE carry groups 30-33; 3D polylines, polygon meshes and polyface meshes are
  written as DXF POLYLINE/VERTEX with the flags in [dxf.md](dxf.md#3d). Each edge of a solid,
  surface or subdivision mesh is written as a polyface mesh of two-vertex faces, which marks
  it as part of a body (see [acis.md](acis.md)); a body that yields no edges is written as a
  bare `3DSOLID` record so the library can report it. Values never contain line
  breaks; MTEXT is split into 250-byte group 3 chunks on UTF-8 boundaries
  (`helpers/dwg/dxf_writer.cpp`).
- Exit status: 0 drawing written; 2 usage; 3 not a DWG file or not decodable; 4 internal error;
  5 out of memory.
- The library kills the helper after 30 s or 128 MiB of output, and reads its output with the
  ordinary DXF reader and its limits. Any failure becomes a tier-4 failure with a fixed message;
  the helper's error output goes only to the debug log, because it can contain the path.
- The helper is looked up at `cp_options.dwg_helper_path`, else next to the library or in
  `../bin` relative to it. Without it, DWG tier 4 is reported as unavailable.

**Evidence:** `tests/unit/process_test.cpp`, `tests/api/dwg_helper_test.cpp` (with the stand-in
`tests/support/fake_helper.cpp`), `tests/unit/dxf_writer_test.cpp`; on the 17 vendor samples
the helper gave the same tier-4 results as the earlier in-process build (15 of 17), and 17 of
17 once 3D bodies were drawn (see [milestones.md](milestones.md#3d-previews)).
