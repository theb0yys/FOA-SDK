#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations

import importlib.util
import struct
import unittest
import zlib
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_thumbnail_texture_codecs.py"
SPEC = importlib.util.spec_from_file_location(
    "foa_thumbnail_texture_codecs",
    MODULE_PATH,
)
assert SPEC and SPEC.loader
codecs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(codecs)


def tga_2x2(*, rle: bool, top_origin: bool) -> bytes:
    header = struct.pack(
        "<BBBHHBHHHHBB",
        0,
        0,
        10 if rle else 2,
        0,
        0,
        0,
        0,
        0,
        2,
        2,
        24,
        0x20 if top_origin else 0,
    )
    pixels = [
        bytes((0, 0, 255)),
        bytes((0, 255, 0)),
        bytes((255, 0, 0)),
        bytes((255, 255, 255)),
    ]
    body = b"".join(pixels)
    return header + (bytes((3,)) + body if rle else body)


def dds(fourcc: bytes, block: bytes) -> bytes:
    header = bytearray(124)
    struct.pack_into("<7I", header, 0, 124, 0x0002100F, 4, 4, len(block), 0, 1)
    struct.pack_into("<II4s5I", header, 72, 32, 0x4, fourcc, 0, 0, 0, 0, 0)
    struct.pack_into("<5I", header, 104, 0x1000, 0, 0, 0, 0)
    return b"DDS " + bytes(header) + block


def decode_png(payload: bytes) -> tuple[int, int, bytes]:
    assert payload.startswith(codecs.PNG_SIGNATURE)
    offset = len(codecs.PNG_SIGNATURE)
    width = height = 0
    compressed = bytearray()
    while offset < len(payload):
        length = int.from_bytes(payload[offset : offset + 4], "big")
        kind = payload[offset + 4 : offset + 8]
        data = payload[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, *_ = struct.unpack(
                ">IIBBBBB",
                data,
            )
            assert depth == 8 and color_type == 6
        elif kind == b"IDAT":
            compressed.extend(data)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    rgba = bytearray()
    for row in range(height):
        start = row * (stride + 1)
        assert raw[start] == 0
        rgba.extend(raw[start + 1 : start + 1 + stride])
    return width, height, bytes(rgba)


class ThumbnailTextureCodecTests(unittest.TestCase):
    def test_png_encoder_is_deterministic(self) -> None:
        rgba = bytes((255, 0, 0, 255, 0, 255, 0, 128))
        first = codecs.encode_png_rgba(2, 1, rgba)
        second = codecs.encode_png_rgba(2, 1, rgba)
        self.assertEqual(first, second)
        self.assertEqual(decode_png(first), (2, 1, rgba))

    def test_tga_raw_and_rle_origins(self) -> None:
        width, height, rgba, source_format, fidelity = codecs.decode_tga(
            tga_2x2(rle=False, top_origin=True)
        )
        self.assertEqual((width, height), (2, 2))
        self.assertEqual(source_format, "tga-uncompressed-true-color-24")
        self.assertEqual(fidelity, "exact")
        self.assertEqual(rgba[:4], bytes((255, 0, 0, 255)))

        width, height, rgba, source_format, fidelity = codecs.decode_tga(
            tga_2x2(rle=True, top_origin=False)
        )
        self.assertEqual((width, height), (2, 2))
        self.assertEqual(source_format, "tga-rle-true-color-24")
        self.assertEqual(fidelity, "exact")
        self.assertEqual(rgba[:4], bytes((0, 0, 255, 255)))
        self.assertEqual(rgba[8:12], bytes((255, 0, 0, 255)))

    def test_dds_bc1_and_bc3(self) -> None:
        bc1 = struct.pack("<HHI", 0xF800, 0x07E0, 0)
        width, height, rgba, source_format, fidelity = codecs.decode_dds(
            dds(b"DXT1", bc1)
        )
        self.assertEqual((width, height), (4, 4))
        self.assertEqual(source_format, "dds-bc1")
        self.assertEqual(fidelity, "exact")
        self.assertEqual(rgba[:4], bytes((255, 0, 0, 255)))

        alpha = bytes((255, 0)) + (1).to_bytes(6, "little")
        color = struct.pack("<HHI", 0xF800, 0x07E0, 0)
        width, height, rgba, source_format, fidelity = codecs.decode_dds(
            dds(b"DXT5", alpha + color)
        )
        self.assertEqual(source_format, "dds-bc3")
        self.assertEqual(fidelity, "exact")
        self.assertEqual(rgba[3], 0)
        self.assertEqual(rgba[7], 255)

    def test_dds_cubemap_and_volume_are_unsupported(self) -> None:
        cubemap = bytearray(dds(b"DXT1", bytes(8)))
        struct.pack_into("<I", cubemap, 4 + 108, 0x00000200)
        with self.assertRaisesRegex(
            codecs.UnsupportedTextureError,
            "cubemaps and volume",
        ):
            codecs.decode_dds(bytes(cubemap))

        volume = bytearray(dds(b"DXT1", bytes(8)))
        struct.pack_into("<I", volume, 4 + 20, 2)
        struct.pack_into("<I", volume, 4 + 108, 0x00200000)
        with self.assertRaisesRegex(
            codecs.UnsupportedTextureError,
            "cubemaps and volume",
        ):
            codecs.decode_dds(bytes(volume))

    def test_dds_unknown_fourcc_is_explicitly_unsupported(self) -> None:
        with self.assertRaisesRegex(
            codecs.UnsupportedTextureError,
            "FourCC",
        ):
            codecs.decode_dds(dds(b"ZZZZ", bytes(16)))

    def test_bounds_and_truncation_fail_closed(self) -> None:
        with self.assertRaisesRegex(
            codecs.UnsupportedTextureError,
            "shorter|truncated",
        ):
            codecs.decode_tga(b"\x00" * 10)
        with self.assertRaisesRegex(
            codecs.UnsupportedTextureError,
            "truncated",
        ):
            codecs.decode_dds(b"DDS " + b"\x00" * 20)
        with self.assertRaisesRegex(
            codecs.UnsupportedTextureError,
            "exceed",
        ):
            codecs.validate_dimensions(9000, 1, "fixture")


if __name__ == "__main__":
    unittest.main()
