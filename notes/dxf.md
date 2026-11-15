# DXF

DXF is documented publicly in Autodesk's *DXF Reference* (AutoCAD 2012 onward editions on
help.autodesk.com). The reader follows that document. **No real DXF files were available**:
the Autodesk sample page offers only DWG and DWF. Every claim below is therefore marked
"spec", and none is confirmed against AutoCAD-written files yet. The synthetic fixtures in
`corpus/synthetic/dxf/` (from `tools/make_dxf_fixtures.py`) test that the reader follows the
spec. They cannot show that AutoCAD writes files that way.

## ascii-groups

**Claim:** An ASCII DXF file is a sequence of groups. Each group is two lines: an integer group
code, then its value. Code 0 starts an object, and code 999 is a comment.
**Status:** spec (DXF Reference, "DXF Format" / "Group Codes in Numerical Order")
**Evidence:** synthetic fixtures; unit tests in `tests/unit/dxf_test.cpp`.
**Method:** implemented from the reference.

The tokenizer also accepts CRLF line ends, a UTF-8 byte-order mark and blank trailing lines,
which are common in files written by other tools. A line longer than 64 KiB is rejected.

## binary-sentinel

**Claim:** A binary DXF file starts with the 22 bytes `AutoCAD Binary DXF\r\n\x1A\0`.
**Status:** spec (DXF Reference, "Binary DXF Files")
**Evidence:** `tests/unit/dxf_test.cpp`.

cadpreview detects binary DXF but does not read it yet. Both tiers fail with "binary DXF is not
supported yet".

## sections

**Claim:** Data sits in sections (`0 SECTION`, `2 <name>` … `0 ENDSEC`). The ones used are:
- HEADER, where `$ACADVER` (group 9) is followed by its value in group 1;
- TABLES, for LAYER entries;
- BLOCKS, from `BLOCK` to `ENDBLK`, with the base point in groups 10/20;
- ENTITIES;
- THUMBNAILIMAGE.
**Status:** spec

## layers

**Claim:** A LAYER table entry is hidden when group 70 has bit 1 set (frozen) or when its colour
(group 62) is negative (off).
**Status:** spec

## thumbnail

**Claim:** THUMBNAILIMAGE holds group 90, the byte count, followed by group 310 lines. Each 310
line is hexadecimal data, and the concatenated bytes form a device-independent bitmap.
**Status:** spec. The DWG preview contains a DIB (see dwg.md#preview-block), which is
consistent with this.
**Method:** cadpreview concatenates the 310 values and decodes them as a DIB. Data that starts
with the PNG signature is decoded as PNG instead, a hedge that no sample has needed yet.

## bulge

**Claim:** In LWPOLYLINE (group 42) and in VERTEX, a vertex's bulge is the tangent of a quarter
of the included angle of the arc to the next vertex. Positive bulges run counter-clockwise, and
0 means a straight segment.
**Status:** spec; geometry checked in `tests/unit/geometry_test.cpp`.

## text-alignment

**Claim:** TEXT uses group 72 for horizontal justification (0 left, 1 centre, 2 right,
3 aligned, 4 middle, 5 fit) and group 73 for vertical (0 baseline, 1 bottom, 2 middle, 3 top).
When either is non-zero, the alignment point (11/21) is the anchor instead of 10/20.
**Status:** spec. Aligned and fit are drawn left-aligned.

## mtext

**Claim:** MTEXT text arrives as zero or more group 3 chunks followed by group 1. Group 71 is
the attachment point (1-9, top-left to bottom-right), group 50 or the direction vector 11/21
gives the rotation, and formatting uses backslash codes (`\P` newline, `\S…;` stacked text,
`\f…;`, `\H…;` and similar).
**Status:** spec. cadpreview removes formatting, breaks lines at `\P` and uses AutoCAD's default
line spacing of 5/3 of the height. It does not wrap to the reference width.

## text-codes

**Claim:** In TEXT, `%%c`, `%%d` and `%%p` stand for the diameter, degree and plus-minus signs,
`%%u`/`%%o` toggle underline/overline, `%%%` is a percent sign, and `\U+XXXX` is a Unicode
character.
**Status:** spec. The embedded font has no ⌀ (U+2300), so `%%c` is drawn as Ø (U+00D8).

## fixtures

`tools/make_dxf_fixtures.py corpus/synthetic/dxf` regenerates both fixtures byte for byte.
Change it and the manifest rows together.
