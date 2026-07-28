#!/usr/bin/env python3

import importlib.machinery
import importlib.util
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


REPOSITORY = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPOSITORY / "tools" / "reconstruct-lattice-rule"
LOADER = importlib.machinery.SourceFileLoader("reconstruct_lattice_rule", str(SCRIPT))
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
assert SPEC is not None and SPEC.loader is not None
reconstruct = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = reconstruct
SPEC.loader.exec_module(reconstruct)


PENTOMINO = frozenset({(0, 0), (0, 1), (0, 2), (1, 1), (1, 2)})
CHILDREN = (
    frozenset({(0, 3), (0, 4), (0, 5), (1, 4), (1, 5)}),
    frozenset({(0, 0), (0, 1), (0, 2), (1, 0), (1, 1)}),
    frozenset({(1, 2), (1, 3), (2, 2), (2, 3), (3, 2)}),
    frozenset({(2, 4), (2, 5), (3, 3), (3, 4), (3, 5)}),
)


def write_bmp(path: pathlib.Path) -> None:
    width, height = 112, 64
    background = (255, 255, 255)
    pixels = [background] * (width * height)

    def paint(cells, origin, colour) -> None:
        pitch = 8
        for cell_x, cell_y in cells:
            for y in range(origin[1] + pitch * cell_y, origin[1] + pitch * (cell_y + 1)):
                for x in range(
                    origin[0] + pitch * cell_x,
                    origin[0] + pitch * (cell_x + 1),
                ):
                    pixels[y * width + x] = colour

    paint(PENTOMINO, (4, 12), (20, 60, 180))
    colours = ((20, 60, 180), (220, 80, 30), (40, 150, 70), (160, 50, 170))
    for cells, colour in zip(CHILDREN, colours, strict=True):
        paint(cells, (60, 8), colour)

    stride = ((width * 24 + 31) // 32) * 4
    body = bytearray()
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for red, green, blue in pixels[y * width : (y + 1) * width]:
            row.extend((blue, green, red))
        row.extend(b"\0" * (stride - len(row)))
        body.extend(row)
    offset = 14 + 40
    header = struct.pack("<2sIHHI", b"BM", offset + len(body), 0, 0, offset)
    dib = struct.pack(
        "<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(body), 0, 0, 0, 0
    )
    path.write_bytes(header + dib + body)


class ReconstructionTest(unittest.TestCase):
    def test_flat_rule_image_produces_stable_aper_definition(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            image = root / "rule.bmp"
            definition = root / "rule.aper"
            write_bmp(image)
            command = [
                str(SCRIPT),
                "--id",
                "fixture",
                "--name",
                "Synthetic fixture",
                "--cells",
                "5",
                str(image),
            ]
            first = subprocess.run(command, check=True, capture_output=True).stdout
            second = subprocess.run(command, check=True, capture_output=True).stdout
            self.assertEqual(first, second)
            self.assertIn(b"inflation 2\n", first)
            self.assertIn(b"polygon 0 0 1 0 1 1 2 1 2 3 0 3\n", first)
            self.assertEqual(first.count(b"child tile "), 4)
            definition.write_bytes(first)

            aper = REPOSITORY / "aper"
            if aper.is_file() and aper.stat().st_mode & 0o111:
                canonical = subprocess.run(
                    [str(aper), "--file", str(definition), "--definition"],
                    check=True,
                    capture_output=True,
                ).stdout
                repeated = subprocess.run(
                    [str(aper), "--file", "-", "--definition"],
                    check=True,
                    input=canonical,
                    capture_output=True,
                ).stdout
                self.assertEqual(canonical, repeated)

    def test_boundary_rejects_a_hole(self) -> None:
        ring = frozenset(
            (x, y)
            for x in range(3)
            for y in range(3)
            if (x, y) != (1, 1)
        )
        with self.assertRaises(ValueError):
            reconstruct.boundary(ring)


if __name__ == "__main__":
    unittest.main()
