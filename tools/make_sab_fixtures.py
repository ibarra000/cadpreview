#!/usr/bin/env python3
"""Writes the synthetic SAB (binary ACIS) fixture in corpus/synthetic/sab, laid out as recorded
in notes/acis.md from inspecting sample files. It seeds the fuzz_sab harness. Rerun after
changing it and update corpus/manifest.csv.

    python tools/make_sab_fixtures.py corpus/synthetic/sab
"""

from __future__ import annotations

import math
import struct
import sys
from pathlib import Path

Vec3 = tuple[float, float, float]

INTEGER, DOUBLE, TEXT, TRUE, FALSE, POINTER = 0x04, 0x06, 0x07, 0x0A, 0x0B, 0x0C
IDENTIFIER, SUB_IDENTIFIER, RECORD_END, POSITION, VECTOR = 0x0D, 0x0E, 0x11, 0x13, 0x14


class Sab:
    """Builds a SAB stream record by record; records are numbered from 0 (the header)."""

    def __init__(self) -> None:
        self.data = bytearray(b"ACIS BinaryFile")
        self.data += struct.pack("<4i", 21500, 0, 1, 0)
        for text in ("cadpreview fixture", "ASM fixture", "2026-11-16"):
            self.text(TEXT, text)
        for number in (1.0, 1e-6, 1e-10):
            self.number(number)
        self.name("asmheader")
        self.pointer(-1)
        self.integer(-1)
        self.text(TEXT, "216")
        self.records = 0
        self.end()

    def text(self, tag: int, value: str) -> None:
        encoded = value.encode("ascii")
        self.data += bytes([tag, len(encoded)]) + encoded

    def number(self, value: float) -> None:
        self.data += bytes([DOUBLE]) + struct.pack("<d", value)

    def integer(self, value: int) -> None:
        self.data += bytes([INTEGER]) + struct.pack("<i", value)

    def pointer(self, value: int) -> None:
        self.data += bytes([POINTER]) + struct.pack("<i", value)

    def vector(self, tag: int, v: Vec3) -> None:
        self.data += bytes([tag]) + struct.pack("<3d", *v)

    def name(self, full: str) -> None:
        *subs, last = full.split("-")
        for sub in subs:
            self.text(SUB_IDENTIFIER, sub)
        self.text(IDENTIFIER, last)

    def end(self) -> int:
        self.data.append(RECORD_END)
        self.records += 1
        return self.records - 1

    def header(self, name: str) -> None:
        self.name(name)
        self.pointer(-1)
        self.integer(-1)
        self.pointer(-1)

    def point(self, p: Vec3) -> int:
        self.header("point")
        self.vector(POSITION, p)
        return self.end()

    def vertex(self, point: int) -> int:
        self.header("vertex")
        self.pointer(-1)
        self.integer(0)
        self.pointer(point)
        return self.end()

    def straight(self, root: Vec3, direction: Vec3) -> int:
        self.header("straight-curve")
        self.vector(POSITION, root)
        self.vector(VECTOR, direction)
        self.data += bytes([FALSE, FALSE])
        return self.end()

    def ellipse(self, center: Vec3, normal: Vec3, major: Vec3, ratio: float) -> int:
        self.header("ellipse-curve")
        self.vector(POSITION, center)
        self.vector(VECTOR, normal)
        self.vector(VECTOR, major)
        self.number(ratio)
        self.data += bytes([FALSE, FALSE])
        return self.end()

    def edge(self, start: int, t0: float, finish: int, t1: float, curve: int) -> int:
        self.header("edge")
        self.pointer(start)
        self.number(t0)
        self.pointer(finish)
        self.number(t1)
        self.pointer(-1)
        self.pointer(curve)
        self.data.append(FALSE)
        self.text(TEXT, "unknown")
        return self.end()

    def finish(self) -> bytes:
        for sub in ("End", "of", "ACIS"):
            self.text(SUB_IDENTIFIER, sub)
        self.text(IDENTIFIER, "data")  # observed without a record-end tag
        return bytes(self.data)


def box_with_hole() -> bytes:
    """A 10 x 10 x 4 box's twelve edges plus a circular edge on its top face."""
    sab = Sab()
    corners: list[Vec3] = [
        (float(x), float(y), float(z))
        for z in (0, 4)
        for (x, y) in ((0, 0), (10, 0), (10, 10), (0, 10))
    ]
    vertices = [sab.vertex(sab.point(c)) for c in corners]

    def segment(a: int, b: int) -> None:
        pa, pb = corners[a], corners[b]
        curve = sab.straight(pa, (pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]))
        sab.edge(vertices[a], 0, vertices[b], 1, curve)

    for i in range(4):
        segment(i, (i + 1) % 4)
        segment(i + 4, (i + 1) % 4 + 4)
        segment(i, i + 4)
    ring = sab.vertex(sab.point((7, 5, 4)))
    hole = sab.ellipse((5, 5, 4), (0, 0, 1), (2, 0, 0), 1.0)
    sab.edge(ring, 0, ring, 2 * math.pi, hole)
    return sab.finish()


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else Path("corpus/synthetic/sab")
    out.mkdir(parents=True, exist_ok=True)
    (out / "box-with-hole.sab").write_bytes(box_with_hole())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
