# ACIS (SAB) in DWG

DWG stores solids, regions, bodies and surfaces as ACIS modeler data inside the entity. The
DWG helper reads only the edges of that data, to draw a wireframe in tier 4
(`helpers/dwg/sab_reader.cpp`). Everything below was learned by **inspecting the bytes of the
local Autodesk sample drawings** (the hard rule to work only from files). No ACIS documentation, SDK
or vendor software was used. Spatial's published save-file documentation was not consulted,
so its terms did not need checking; if it is consulted later, check its terms first and
record them here. Claims are limited to what the samples show, and the reader understands
nothing beyond them.

Samples: `corpus/vendor/dwg/visualization_-_aerial.dwg` (5 3DSOLIDs, 1 PLANESURFACE) and
`visualization_-_sun_and_sky_demo.dwg` (15 3DSOLIDs), both AC1024. The streams were dumped
with a throwaway LibreDWG program and decoded with a throwaway Python script. Neither is kept;
both are described here.

## storage

**Claim:** LibreDWG exposes the modeler data of 3DSOLID, REGION, BODY and the surface entities
through shared fields: `version`, `acis_data` and `sab_size`. Version 2 data is a SAB stream
of `sab_size` bytes starting with `ACIS BinaryFile`.
**Status:** confirmed (every body in both samples is version 2 with that header).
**Evidence:** the two samples above; 21 bodies.

**Version 1** (SAT text, used by older releases) is stored with a character cipher. LibreDWG
deciphers it, but by the hard rule "skip encrypted data rather than break it" cadpreview
does not use it. Such bodies are skipped and reported as undrawn. None of the samples has version 1.

LibreDWG's `dwg_convert_SAB_to_SAT1` failed on the samples, so SAB is read directly.

## sab-layout

**Claim:**
- The stream is the 15 bytes `ACIS BinaryFile`, then four untagged little-endian int32 values
  (21500, 0, 2, 24 in the samples), then tagged values.
- Each tagged value is one tag byte followed by its data:

| Tag | Data |
|---|---|
| 0x04 | int32 |
| 0x06 | double |
| 0x07 | string: 1 length byte, then the bytes |
| 0x0A, 0x0B | flag, no data (true, false) |
| 0x0C | pointer: int32 record index, -1 for none |
| 0x0D | record name: 1 length byte, then the bytes |
| 0x0E | name prefix: as 0x0D; prefixes and the name join with `-` (`ellipse` + `curve`) |
| 0x11 | end of record |
| 0x13 | position: 3 doubles |
| 0x14 | vector: 3 doubles |

- Records are numbered from 0. Record 0 is `asmheader`, which also holds the header strings
  and doubles that precede its name.
- The stream ends with a record named `End-of-ACIS-data`, which has **no** 0x11 tag.

**Status:** confirmed. These 11 tags are the only ones in the samples, and walking the stream
with them ends exactly at the last byte of all 13 distinct streams dumped.
**Evidence:** tag counts over all streams: 0x0C 10767, 0x04 2630, 0x0D 2193, 0x11 2180,
0x0B 1649, 0x0E 1401, 0x06 671, 0x07 539, 0x13 504, 0x14 469, 0x0A 304.
**Method:** hex dump of the smallest stream (1076 bytes), then a decoder checked against every
stream. Any other tag makes the reader reject the body.

## records

**Claim:** For the records used, the values after the first three (an attribute pointer, an
integer and a pointer, common to every entity record) are:

| Record | Values used |
|---|---|
| `point` | position |
| `vertex` | (pointer, integer), then a pointer to its `point` |
| `edge` | start vertex, start parameter (double), end vertex, end parameter, coedge, curve |
| `straight-curve` | root position, direction vector |
| `ellipse-curve` | centre, unit normal, major axis (its length is the radius), radius ratio |
| `sphere-surface` | centre, radius, two axis vectors |

A point on an ellipse at parameter t is centre + major·cos t + (normal × major)·ratio·sin t.
Edge parameters lie on the curve, or on the negated parameter for edges that run against the
curve. The samples have no field that says which, so the reader uses the variant whose ends
meet the vertices, and draws the chord if neither does.
**Status:** confirmed for boxes, extrusions with arcs, cylinders (cone surfaces with circular
edges) and one sphere. For example, every straight edge's vertices lie on its line, and
sampled ellipse edges end on their vertices.
**Evidence:** both samples. Their wireframes match the embedded thumbnails' content: the
aerial ground plane with two boxes, a tower and a sphere, and the sun-and-sky room with
beams.

Only `straight-curve` and `ellipse-curve` occur. Other curves would be drawn as chords. A body
with no edges (the sphere) is drawn as three great circles of each sphere surface. A body
transform record was not seen, and every body's transform pointer is -1. Nothing applies
transforms, so a body that had one would be drawn untransformed.

## limits

The reader rejects a body when:
- the data is over 64 MiB;
- it has over 1 million records or 2 million output points;
- a name is over 256 bytes;
- a value is truncated or a number is not finite.

Records are kept as 8-byte entries. Only the five record types it reads (`point`, `vertex`,
`edge`, `ellipse-curve` and `sphere-surface`) keep values: at most 9 each, in one shared pool
capped at 2 million values (64 MiB). Every pointer is range-checked and type-checked before
use. Ellipses and spheres are sampled with 32 segments per turn. Polylines whose computed
points overflow to infinity are dropped. In the helper, any failure while reading a body,
including running out of memory, loses only that body.
It is fuzzed by `fuzz/fuzz_sab.cpp`, seeded by `corpus/synthetic/sab/box-with-hole.sab`
(written by `tools/make_sab_fixtures.py`).
