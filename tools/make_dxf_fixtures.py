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


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else Path("corpus/synthetic/dxf")
    out.mkdir(parents=True, exist_ok=True)
    (out / "plate-with-thumbnail.dxf").write_bytes(plate(True).encode("ascii"))
    (out / "plate-no-thumbnail.dxf").write_bytes(plate(False).encode("ascii"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
