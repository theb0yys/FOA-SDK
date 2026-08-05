#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Dependency-free bounded DDS/TGA thumbnail codecs for FOA-SDK."""
from __future__ import annotations

import binascii
import struct
import zlib

MAX_IMAGE_DIMENSION = 8192
MAX_IMAGE_PIXELS = 16 * 1024 * 1024
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class TextureCodecError(RuntimeError):
    """Raised when a texture payload is malformed."""


class UnsupportedTextureError(TextureCodecError):
    """Raised when a valid texture uses an out-of-cohort encoding."""


def validate_dimensions(width: int, height: int, label: str) -> None:
    if width <= 0 or height <= 0:
        raise UnsupportedTextureError(f"{label} dimensions must be positive.")
    if width > MAX_IMAGE_DIMENSION or height > MAX_IMAGE_DIMENSION:
        raise UnsupportedTextureError(
            f"{label} dimensions exceed {MAX_IMAGE_DIMENSION} pixels."
        )
    if width * height > MAX_IMAGE_PIXELS:
        raise UnsupportedTextureError(
            f"{label} pixel count exceeds {MAX_IMAGE_PIXELS}."
        )


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    checksum = binascii.crc32(kind)
    checksum = binascii.crc32(payload, checksum) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", checksum)
    )


def encode_png_rgba(width: int, height: int, rgba: bytes) -> bytes:
    """Encode a deterministic, non-interlaced 8-bit RGBA PNG."""

    validate_dimensions(width, height, "PNG")
    expected = width * height * 4
    if len(rgba) != expected:
        raise TextureCodecError(
            f"RGBA payload size mismatch: expected {expected}, received {len(rgba)}."
        )
    stride = width * 4
    scanlines = bytearray()
    for row in range(height):
        scanlines.append(0)
        start = row * stride
        scanlines.extend(rgba[start : start + stride])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", zlib.compress(bytes(scanlines), level=9))
        + _png_chunk(b"IEND", b"")
    )


def _tga_pixel(
    pixel: bytes,
    pixel_depth: int,
    grayscale: bool,
    alpha_bits: int,
) -> tuple[int, int, int, int]:
    if grayscale:
        if pixel_depth == 8:
            value = pixel[0]
            return value, value, value, 255
        if pixel_depth == 16:
            value, alpha = pixel
            return value, value, value, alpha
        raise UnsupportedTextureError(
            f"TGA grayscale pixel depth {pixel_depth} is unsupported."
        )
    if pixel_depth == 16:
        packed = int.from_bytes(pixel, "little")
        blue = (packed & 0x1F) * 255 // 31
        green = ((packed >> 5) & 0x1F) * 255 // 31
        red = ((packed >> 10) & 0x1F) * 255 // 31
        alpha = 255 if alpha_bits == 0 or (packed & 0x8000) else 0
        return red, green, blue, alpha
    if pixel_depth == 24:
        blue, green, red = pixel
        return red, green, blue, 255
    if pixel_depth == 32:
        blue, green, red, alpha = pixel
        return red, green, blue, alpha
    raise UnsupportedTextureError(
        f"TGA true-color pixel depth {pixel_depth} is unsupported."
    )


def decode_tga(payload: bytes) -> tuple[int, int, bytes, str, str]:
    """Decode bounded raw/RLE true-color or grayscale TGA."""

    if len(payload) < 18:
        raise UnsupportedTextureError(
            "TGA payload is shorter than its 18-byte header."
        )
    (
        id_length,
        color_map_type,
        image_type,
        _color_map_first,
        _color_map_length,
        _color_map_depth,
        _x_origin,
        _y_origin,
        width,
        height,
        pixel_depth,
        descriptor,
    ) = struct.unpack_from("<BBBHHBHHHHBB", payload, 0)
    if color_map_type != 0:
        raise UnsupportedTextureError(
            "Color-mapped TGA images are outside the Alpha cohort."
        )
    if image_type not in {2, 3, 10, 11}:
        raise UnsupportedTextureError(
            f"TGA image type {image_type} is unsupported."
        )
    validate_dimensions(width, height, "TGA")
    grayscale = image_type in {3, 11}
    rle = image_type in {10, 11}
    bytes_per_pixel = (pixel_depth + 7) // 8
    if grayscale and pixel_depth not in {8, 16}:
        raise UnsupportedTextureError(
            f"TGA grayscale pixel depth {pixel_depth} is unsupported."
        )
    if not grayscale and pixel_depth not in {16, 24, 32}:
        raise UnsupportedTextureError(
            f"TGA true-color pixel depth {pixel_depth} is unsupported."
        )

    offset = 18 + id_length
    if offset > len(payload):
        raise UnsupportedTextureError("TGA image ID exceeds the payload.")
    pixel_count = width * height
    source_pixels: list[bytes] = []
    if rle:
        while len(source_pixels) < pixel_count:
            if offset >= len(payload):
                raise UnsupportedTextureError(
                    "TGA RLE stream ended before all pixels were decoded."
                )
            packet = payload[offset]
            offset += 1
            count = (packet & 0x7F) + 1
            if len(source_pixels) + count > pixel_count:
                raise UnsupportedTextureError(
                    "TGA RLE packet exceeds the declared pixel count."
                )
            if packet & 0x80:
                end = offset + bytes_per_pixel
                if end > len(payload):
                    raise UnsupportedTextureError(
                        "TGA RLE run pixel is truncated."
                    )
                pixel = payload[offset:end]
                offset = end
                source_pixels.extend([pixel] * count)
            else:
                end = offset + count * bytes_per_pixel
                if end > len(payload):
                    raise UnsupportedTextureError(
                        "TGA raw RLE packet is truncated."
                    )
                for index in range(count):
                    start = offset + index * bytes_per_pixel
                    source_pixels.append(
                        payload[start : start + bytes_per_pixel]
                    )
                offset = end
    else:
        end = offset + pixel_count * bytes_per_pixel
        if end > len(payload):
            raise UnsupportedTextureError("TGA pixel payload is truncated.")
        source_pixels = [
            payload[
                offset + index * bytes_per_pixel :
                offset + (index + 1) * bytes_per_pixel
            ]
            for index in range(pixel_count)
        ]

    top_origin = bool(descriptor & 0x20)
    right_origin = bool(descriptor & 0x10)
    alpha_bits = descriptor & 0x0F
    rgba = bytearray(pixel_count * 4)
    for source_index, pixel in enumerate(source_pixels):
        storage_y, storage_x = divmod(source_index, width)
        x = width - 1 - storage_x if right_origin else storage_x
        y = storage_y if top_origin else height - 1 - storage_y
        red, green, blue, alpha = _tga_pixel(
            pixel, pixel_depth, grayscale, alpha_bits
        )
        target = (y * width + x) * 4
        rgba[target : target + 4] = bytes((red, green, blue, alpha))

    mode = "rle" if rle else "uncompressed"
    family = "grayscale" if grayscale else "true-color"
    return (
        width,
        height,
        bytes(rgba),
        f"tga-{mode}-{family}-{pixel_depth}",
        "exact",
    )


def _color_565(value: int) -> tuple[int, int, int]:
    return (
        ((value >> 11) & 0x1F) * 255 // 31,
        ((value >> 5) & 0x3F) * 255 // 63,
        (value & 0x1F) * 255 // 31,
    )


def _bc1_colors(
    block: bytes,
    force_four_color: bool,
) -> tuple[list[tuple[int, int, int, int]], int]:
    if len(block) < 8:
        raise UnsupportedTextureError("BC color block is truncated.")
    color0, color1, indices = struct.unpack_from("<HHI", block, 0)
    r0, g0, b0 = _color_565(color0)
    r1, g1, b1 = _color_565(color1)
    colors: list[tuple[int, int, int, int]] = [
        (r0, g0, b0, 255),
        (r1, g1, b1, 255),
    ]
    if color0 > color1 or force_four_color:
        colors.extend(
            [
                (
                    (2 * r0 + r1) // 3,
                    (2 * g0 + g1) // 3,
                    (2 * b0 + b1) // 3,
                    255,
                ),
                (
                    (r0 + 2 * r1) // 3,
                    (g0 + 2 * g1) // 3,
                    (b0 + 2 * b1) // 3,
                    255,
                ),
            ]
        )
    else:
        colors.extend(
            [
                (
                    (r0 + r1) // 2,
                    (g0 + g1) // 2,
                    (b0 + b1) // 2,
                    255,
                ),
                (0, 0, 0, 0),
            ]
        )
    return colors, indices


def _bc_alpha(block: bytes) -> list[int]:
    if len(block) < 8:
        raise UnsupportedTextureError("BC alpha block is truncated.")
    alpha0, alpha1 = block[0], block[1]
    palette = [alpha0, alpha1]
    if alpha0 > alpha1:
        palette.extend(
            ((7 - index) * alpha0 + index * alpha1) // 7
            for index in range(1, 7)
        )
    else:
        palette.extend(
            ((5 - index) * alpha0 + index * alpha1) // 5
            for index in range(1, 5)
        )
        palette.extend((0, 255))
    bits = int.from_bytes(block[2:8], "little")
    return [palette[(bits >> (3 * index)) & 0x7] for index in range(16)]


def _bc_block(
    block: bytes,
    format_name: str,
) -> list[tuple[int, int, int, int]]:
    if format_name == "BC1":
        colors, indices = _bc1_colors(block, False)
        return [
            colors[(indices >> (2 * index)) & 0x3]
            for index in range(16)
        ]
    if format_name == "BC2":
        if len(block) < 16:
            raise UnsupportedTextureError("BC2 block is truncated.")
        alpha_bits = int.from_bytes(block[:8], "little")
        colors, indices = _bc1_colors(block[8:16], True)
        result = []
        for index in range(16):
            red, green, blue, _ = colors[
                (indices >> (2 * index)) & 0x3
            ]
            alpha = ((alpha_bits >> (4 * index)) & 0xF) * 17
            result.append((red, green, blue, alpha))
        return result
    if format_name == "BC3":
        if len(block) < 16:
            raise UnsupportedTextureError("BC3 block is truncated.")
        alpha_values = _bc_alpha(block[:8])
        colors, indices = _bc1_colors(block[8:16], True)
        return [
            (
                *colors[(indices >> (2 * index)) & 0x3][:3],
                alpha_values[index],
            )
            for index in range(16)
        ]
    if format_name == "BC4":
        values = _bc_alpha(block[:8])
        return [(value, value, value, 255) for value in values]
    if format_name == "BC5":
        if len(block) < 16:
            raise UnsupportedTextureError("BC5 block is truncated.")
        red_values = _bc_alpha(block[:8])
        green_values = _bc_alpha(block[8:16])
        return [
            (red_values[index], green_values[index], 0, 255)
            for index in range(16)
        ]
    raise UnsupportedTextureError(
        f"Unsupported BC format: {format_name}"
    )


def _decode_blocks(
    data: bytes,
    width: int,
    height: int,
    format_name: str,
) -> tuple[bytes, str]:
    block_size = 8 if format_name in {"BC1", "BC4"} else 16
    blocks_x = (width + 3) // 4
    blocks_y = (height + 3) // 4
    required = blocks_x * blocks_y * block_size
    if len(data) < required:
        raise UnsupportedTextureError(
            f"{format_name} payload is truncated: "
            f"expected {required} bytes, received {len(data)}."
        )
    rgba = bytearray(width * height * 4)
    offset = 0
    for block_y in range(blocks_y):
        for block_x in range(blocks_x):
            pixels = _bc_block(
                data[offset : offset + block_size],
                format_name,
            )
            offset += block_size
            for local_y in range(4):
                y = block_y * 4 + local_y
                if y >= height:
                    continue
                for local_x in range(4):
                    x = block_x * 4 + local_x
                    if x >= width:
                        continue
                    target = (y * width + x) * 4
                    rgba[target : target + 4] = bytes(
                        pixels[local_y * 4 + local_x]
                    )
    return bytes(rgba), ("partial" if format_name == "BC5" else "exact")


def _mask_channel(value: int, mask: int, default: int) -> int:
    if mask == 0:
        return default
    shift = (mask & -mask).bit_length() - 1
    maximum = mask >> shift
    raw = (value & mask) >> shift
    return (
        (raw * 255 + maximum // 2) // maximum
        if maximum
        else default
    )


def _decode_uncompressed(
    data: bytes,
    width: int,
    height: int,
    bits_per_pixel: int,
    row_pitch: int,
    red_mask: int,
    green_mask: int,
    blue_mask: int,
    alpha_mask: int,
) -> bytes:
    if bits_per_pixel not in {8, 16, 24, 32}:
        raise UnsupportedTextureError(
            f"DDS RGB bit depth {bits_per_pixel} is unsupported."
        )
    bytes_per_pixel = (bits_per_pixel + 7) // 8
    minimum_pitch = width * bytes_per_pixel
    pitch = max(row_pitch, minimum_pitch)
    required = pitch * height
    if len(data) < required:
        raise UnsupportedTextureError(
            "DDS uncompressed payload is truncated: "
            f"expected {required} bytes, received {len(data)}."
        )
    rgba = bytearray(width * height * 4)
    for y in range(height):
        row = data[y * pitch : y * pitch + minimum_pitch]
        for x in range(width):
            start = x * bytes_per_pixel
            value = int.from_bytes(
                row[start : start + bytes_per_pixel],
                "little",
            )
            fallback = value & 0xFF if bits_per_pixel == 8 else 0
            red = _mask_channel(value, red_mask, fallback)
            green = _mask_channel(value, green_mask, red)
            blue = _mask_channel(value, blue_mask, red)
            alpha = _mask_channel(value, alpha_mask, 255)
            target = (y * width + x) * 4
            rgba[target : target + 4] = bytes(
                (red, green, blue, alpha)
            )
    return bytes(rgba)


def decode_dds(payload: bytes) -> tuple[int, int, bytes, str, str]:
    """Decode the first mip of common legacy and DX10 DDS formats."""

    if len(payload) < 128 or payload[:4] != b"DDS ":
        raise UnsupportedTextureError(
            "DDS magic/header is missing or truncated."
        )
    header = payload[4:128]
    (
        size,
        flags,
        height,
        width,
        pitch_or_linear,
        _depth,
        _mip_count,
    ) = struct.unpack_from("<7I", header, 0)
    if size != 124:
        raise UnsupportedTextureError(
            f"DDS header size {size} is unsupported."
        )
    validate_dimensions(width, height, "DDS")
    pixel_format_offset = 72
    pixel_format_size, pixel_format_flags = struct.unpack_from(
        "<II",
        header,
        pixel_format_offset,
    )
    if pixel_format_size != 32:
        raise UnsupportedTextureError(
            f"DDS pixel-format size {pixel_format_size} is unsupported."
        )
    fourcc = header[
        pixel_format_offset + 8 :
        pixel_format_offset + 12
    ]
    (
        rgb_bits,
        red_mask,
        green_mask,
        blue_mask,
        alpha_mask,
    ) = struct.unpack_from(
        "<5I",
        header,
        pixel_format_offset + 12,
    )

    DDPF_FOURCC = 0x4
    DDPF_RGB = 0x40
    DDPF_LUMINANCE = 0x20000
    DDSD_PITCH = 0x8

    data_offset = 128
    format_name = ""
    if pixel_format_flags & DDPF_FOURCC:
        legacy_formats = {
            b"DXT1": "BC1",
            b"DXT3": "BC2",
            b"DXT5": "BC3",
            b"ATI1": "BC4",
            b"BC4U": "BC4",
            b"ATI2": "BC5",
            b"BC5U": "BC5",
        }
        if fourcc == b"DX10":
            if len(payload) < 148:
                raise UnsupportedTextureError(
                    "DDS DX10 header is truncated."
                )
            (
                dxgi_format,
                resource_dimension,
                _misc,
                array_size,
                _misc2,
            ) = struct.unpack_from("<5I", payload, 128)
            data_offset = 148
            if resource_dimension != 3 or array_size != 1:
                raise UnsupportedTextureError(
                    "DDS arrays, cubemaps, and non-2D resources "
                    "are outside the Alpha cohort."
                )
            dxgi_formats = {
                28: "RGBA8",
                29: "RGBA8",
                61: "R8",
                71: "BC1",
                72: "BC1",
                74: "BC2",
                75: "BC2",
                77: "BC3",
                78: "BC3",
                80: "BC4",
                83: "BC5",
                87: "BGRA8",
                91: "BGRA8",
            }
            format_name = dxgi_formats.get(dxgi_format, "")
            if not format_name:
                raise UnsupportedTextureError(
                    f"DDS DXGI format {dxgi_format} is outside "
                    "the bounded decoder cohort."
                )
        else:
            format_name = legacy_formats.get(fourcc, "")
            if not format_name:
                printable = fourcc.decode(
                    "latin-1",
                    errors="replace",
                )
                raise UnsupportedTextureError(
                    f"DDS FourCC {printable!r} is outside "
                    "the bounded decoder cohort."
                )
    elif pixel_format_flags & (DDPF_RGB | DDPF_LUMINANCE):
        format_name = "MASKED"
    else:
        raise UnsupportedTextureError(
            "DDS pixel format is neither RGB/luminance "
            "nor supported FourCC."
        )

    data = payload[data_offset:]
    if format_name in {"BC1", "BC2", "BC3", "BC4", "BC5"}:
        rgba, fidelity = _decode_blocks(
            data,
            width,
            height,
            format_name,
        )
        return (
            width,
            height,
            rgba,
            f"dds-{format_name.lower()}",
            fidelity,
        )
    if format_name == "RGBA8":
        rgba = _decode_uncompressed(
            data,
            width,
            height,
            32,
            width * 4,
            0x000000FF,
            0x0000FF00,
            0x00FF0000,
            0xFF000000,
        )
        return width, height, rgba, "dds-rgba8", "exact"
    if format_name == "BGRA8":
        rgba = _decode_uncompressed(
            data,
            width,
            height,
            32,
            width * 4,
            0x00FF0000,
            0x0000FF00,
            0x000000FF,
            0xFF000000,
        )
        return width, height, rgba, "dds-bgra8", "exact"
    if format_name == "R8":
        rgba = _decode_uncompressed(
            data,
            width,
            height,
            8,
            width,
            0xFF,
            0xFF,
            0xFF,
            0,
        )
        return width, height, rgba, "dds-r8", "exact"

    row_pitch = (
        pitch_or_linear
        if flags & DDSD_PITCH
        else width * ((rgb_bits + 7) // 8)
    )
    rgba = _decode_uncompressed(
        data,
        width,
        height,
        rgb_bits,
        row_pitch,
        red_mask,
        green_mask,
        blue_mask,
        alpha_mask,
    )
    return (
        width,
        height,
        rgba,
        f"dds-masked-rgb{rgb_bits}",
        "exact",
    )
