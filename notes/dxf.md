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

## 3d

**Claim:** 3D content is stored as follows.
- Points carry z in the group code 20 above x (30 for 10, 31 for 11, and so on).
- POLYLINE group 70 bits: 1 closed (the M direction for meshes), 8 3D polyline, 16 polygon
  mesh, 32 mesh closed in the N direction, 64 polyface mesh.
- A polygon mesh lists M × N vertices (groups 71 and 72) row by row.
- A polyface mesh lists its coordinate VERTEX records (group 70 bits 64 and 128) first, then
  face records (bit 128 only) whose groups 71-74 hold 1-based vertex indices. A negative
  index hides the edge that starts at that vertex. Two indices make a single edge.
- 3DSOLID, REGION, BODY and the *SURFACE entities hold ACIS data; MESH holds a subdivision
  mesh.
**Status:** spec (DXF Reference, "POLYLINE", "VERTEX", "3DFACE", "3DSOLID").
**Evidence:** `tests/unit/dxf3d_test.cpp`; `corpus/synthetic/dxf/box-3d.dxf`.

How tier 4 uses them (`src/formats/dxf/entities.cpp`, `src/render/vector/projection.cpp`):
- Lines and faces without depth stay 2D. With depth they become 3D wires, as do 3D polylines.
- Mesh rows, columns and visible face edges become wires marked as body edges.
- ACIS bodies and MESH entities in DXF are not read; they are counted, so a drawing holding
  only them fails with "the drawing holds only 3D solids or surfaces (N)", and a drawing that
  has other content gets a warning.
- Consecutive polyface edges that continue where the previous edge ended are joined into one
  wire. The DWG helper writes each solid edge as a polyface with one two-vertex face per
  segment, so an edge costs one wire.
- Every VERTEX of a POLYLINE counts toward the work limit, even when nothing is drawn.
- Block transforms are 2D: they move x and y and keep z. Z scale (group 43), insertion z and
  extrusion directions (group 210) are ignored, so mirrored or z-scaled inserts are drawn as if
  unscaled.
- The view is the top view unless body edges have a depth over 1% of the drawing's plan size;
  then it is the south-east isometric view, with 2D content placed at z = 0. Lines alone never
  tilt the view, because 2D drawings often carry stray z values (the `tablet` sample has 3002
  lines at z = -0.1).
- In the top view wires are drawn in file order, so 2D drawings render byte for byte as before.
- Drawings with over 50,000 arcs stay in the top view, since the isometric view turns each arc
  into a polyline.
- Models drawn only with 3D lines, 3D faces or 3D polylines (no meshes or solids) stay in the
  top view. This is deliberate, so stray z values cannot tilt a 2D drawing.

## fixtures

`tools/make_dxf_fixtures.py corpus/synthetic/dxf` regenerates the fixtures byte for byte.
Change it and the manifest rows together.
