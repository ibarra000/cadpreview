#!/usr/bin/env python3
"""Writes the synthetic DXF fixtures in corpus/synthetic/dxf, built from the group codes in
Autodesk's published DXF Reference. Rerun after changing it and update corpus/manifest.csv.

    python tools/make_dxf_fixtures.py corpus/synthetic/dxf
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

Pairs = list[tuple[int, object]]


def pairs_to_text(pairs: Pairs) -> str:
    return "".join(f"{code:>3}\r\n{value}\r\n" for code, value in pairs)


def section(name: str, body: Pairs) -> Pairs:
    return [(0, "SECTION"), (2, name), *body, (0, "ENDSEC")]


def line(x1: float, y1: float, x2: float, y2: float) -> Pairs:
    return [(0, "LINE"), (8, "0"), (10, x1), (20, y1), (11, x2), (21, y2)]


def circle(x: float, y: float, r: float) -> Pairs:
    return [(0, "CIRCLE"), (8, "0"), (10, x), (20, y), (40, r)]


def dib_thumbnail(width: int, height: int) -> bytes:
    """24-bit bottom-up DIB: white background with a dark rectangle outline."""
    row_size = (width * 3 + 3) // 4 * 4
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            edge = x in (8, width - 9) or y in (8, height - 9)
            inside = 8 <= x <= width - 9 and 8 <= y <= height - 9
            row += b"\x30\x30\x30" if edge and inside else b"\xff\xff\xff"
        rows.append(bytes(row) + b"\0" * (row_size - len(row)))
    header = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, 0, 0, 0, 0, 0)
    return header + b"".join(rows)


def plate(with_thumbnail: bool) -> str:
    entities: Pairs = [
        (0, "LWPOLYLINE"), (8, "0"), (90, 4), (70, 1),
        (10, 0), (20, 0), (10, 120), (20, 0), (42, 0.0),
        (10, 120), (20, 60), (42, 0.4142135623), (10, 0), (20, 60),
    ]  # fmt: skip
    entities += circle(20, 30, 8) + circle(100, 30, 8)
    entities += [(0, "ARC"), (8, "0"), (10, 60), (20, 30), (40, 15), (50, 30), (51, 150)]
    entities += line(45, 30, 75, 30)
    entities += [(0, "TEXT"), (8, "0"), (10, 0), (20, -12), (40, 5), (1, "PLATE %%c8 HOLES")]
    entities += [
        (0, "INSERT"),
        (8, "0"),
        (2, "BOLT"),
        (10, 60),
        (20, 48),
        (70, 3),
        (71, 1),
        (44, 10),
    ]
    blocks: Pairs = [
        (0, "BLOCK"), (8, "0"), (2, "BOLT"), (70, 0), (10, 0), (20, 0),
        *circle(0, 0, 2), (0, "ENDBLK"),
    ]  # fmt: skip
    header: Pairs = [(9, "$ACADVER"), (1, "AC1032")]
    text = pairs_to_text(
        section("HEADER", header) + section("BLOCKS", blocks) + section("ENTITIES", entities)
    )
    if with_thumbnail:
        hexdata = dib_thumbnail(64, 40).hex().upper()
        chunks: Pairs = [(310, hexdata[i : i + 254]) for i in range(0, len(hexdata), 254)]
        text += pairs_to_text(section("THUMBNAILIMAGE", [(90, len(hexdata) // 2), *chunks]))
    return text + pairs_to_text([(0, "EOF")])


def box_3d() -> str:
    """A 3D polyface-mesh box with a 3D polyline handle: 3D content for tier 4."""
    corners = [(x, y, z) for z in (0, 30) for (x, y) in ((0, 0), (60, 0), (60, 40), (0, 40))]
    faces = [(1, 2, 3, 4), (5, 6, 7, 8), (1, 2, 6, 5), (2, 3, 7, 6), (3, 4, 8, 7), (4, 1, 5, 8)]
    entities: Pairs = [(0, "POLYLINE"), (8, "0"), (66, 1), (70, 64), (71, 8), (72, 6)]
    for x, y, z in corners:
        entities += [(0, "VERTEX"), (8, "0"), (10, x), (20, y), (30, z), (70, 192)]
    for face in faces:
        entities += [(0, "VERTEX"), (8, "0"), (10, 0), (20, 0), (30, 0), (70, 128)]
        entities += [(71 + i, index) for i, index in enumerate(face)]
    entities += [(0, "SEQEND"), (8, "0")]
    entities += [(0, "POLYLINE"), (8, "0"), (66, 1), (70, 8)]
    for x, y, z in ((20, 20, 30), (20, 20, 45), (40, 20, 45), (40, 20, 30)):
        entities += [(0, "VERTEX"), (8, "0"), (10, x), (20, y), (30, z), (70, 32)]
    entities += [(0, "SEQEND"), (8, "0")]
    header: Pairs = [(9, "$ACADVER"), (1, "AC1032")]
    return pairs_to_text(section("HEADER", header) + section("ENTITIES", entities) + [(0, "EOF")])


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else Path("corpus/synthetic/dxf")
    out.mkdir(parents=True, exist_ok=True)
    (out / "plate-with-thumbnail.dxf").write_bytes(plate(True).encode("ascii"))
    (out / "plate-no-thumbnail.dxf").write_bytes(plate(False).encode("ascii"))
    (out / "box-3d.dxf").write_bytes(box_3d().encode("ascii"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
