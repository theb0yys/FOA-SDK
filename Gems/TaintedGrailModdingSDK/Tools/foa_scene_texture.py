# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Preserve bounded PC Texture2D mip payloads for the SDK native image builder.

No decoding/recompression, resampling, gamma correction, normal unpacking or row
flip. Original records remain return authority. The private .foatexture source is a GPU-data projection, not a canonical scene
or game format. Version 1 remains a single 2D surface; version 2 is explicitly
a 2D array, including a one-layer array. Array bytes are mip-major, then layer,
with tightly packed rows/blocks. Reading source Texture2DArray records is not
qualified by this packet extension. Sampling and binding need separate proof.
"""
from dataclasses import dataclass
import hashlib
import importlib.metadata
import math
import struct

import foa_heightmap_importer as h
from foa_scene_asset_binding import embedded_tree

MAX_BYTES = 128 * 1024 * 1024
MAX_DIMENSION = 8192
MAX_LAYERS = 256
MAGIC = b'FOATEX01'
HEADER = struct.Struct('<8s6I32s')
ARRAY_HEADER = struct.Struct('<8s7I32s')
# Verified TextureFormat values; BC4 is 26, BC5 is 27 and is not yet supported.
FORMATS = {1: ('Alpha8', 1), 3: ('RGB24', 3), 4: ('RGBA32', 4), 10: ('BC1', 8),
           12: ('BC3', 16), 26: ('BC4', 8)}


def require(condition, message):
    if not condition:
        raise h.HeightmapImportError(message)


def integer(value, low, high, label):
    require(type(value) is int and low <= value <= high, 'Invalid texture ' + label + '.')
    return value


@dataclass(frozen=True)
class Mip:
    width: int
    height: int
    offset: int
    size: int


@dataclass(frozen=True)
class Texture:
    width: int
    height: int
    mip_count: int
    format: int
    color_space: int
    payload: bytes

    def mips(self):
        return mip_layout(self.width, self.height, self.mip_count, self.format)


@dataclass(frozen=True)
class TextureArray(Texture):
    """Explicit array view; each Mip.size is one layer, offset is the first layer."""
    layer_count: int = 1

    def mips(self):
        integer(self.layer_count, 1, MAX_LAYERS, 'array layer count')
        mips = mip_layout(self.width, self.height, self.mip_count, self.format)
        source_size = (mips[-1].offset + mips[-1].size) * self.layer_count
        native_size = sum(m.width*m.height*4 if self.format == 3 else m.size for m in mips) * self.layer_count
        require(source_size <= MAX_BYTES and native_size <= MAX_BYTES, 'Texture array payload exceeds its bound.')
        return tuple(Mip(m.width, m.height, m.offset*self.layer_count, m.size) for m in mips)


def layer_count(texture):
    require(type(texture) in (Texture, TextureArray), 'Unqualified texture projection kind.')
    return texture.layer_count if type(texture) is TextureArray else 1


def mip_layout(width, height, count, fmt):
    integer(width, 1, MAX_DIMENSION, 'width')
    integer(height, 1, MAX_DIMENSION, 'height')
    integer(count, 1, max(width, height).bit_length(), 'mip count')
    require(type(fmt) is int and fmt in FORMATS, 'Unsupported source texture format.')
    offset, native_bytes, mips = 0, 0, []
    for _ in range(count):
        size = width * height * FORMATS[fmt][1] if fmt in (1, 3, 4) else ((width+3)//4)*((height+3)//4)*FORMATS[fmt][1]
        native_bytes += width * height * 4 if fmt == 3 else size
        require(offset + size <= MAX_BYTES and native_bytes <= MAX_BYTES, 'Texture mip payload exceeds its bound.')
        mips.append(Mip(width, height, offset, size))
        offset += size
        width, height = max(1, width//2), max(1, height//2)
    return tuple(mips)


def validate(texture):
    layers = layer_count(texture)
    mips = texture.mips()
    integer(texture.color_space, 0, 1, 'data colour space')
    require(texture.format != 26 or texture.color_space == 0, 'BC4 has no sRGB native format.')
    require(type(texture.payload) is bytes and len(texture.payload) == mips[-1].offset + mips[-1].size * layers,
            'Texture payload disagrees with its complete mip layout.')
    return texture


def from_tree(tree, payload):
    require(isinstance(tree, dict), 'Invalid texture tree.')
    require(type(tree.get('m_TextureDimension')) is int and tree['m_TextureDimension'] == 2 and
            type(tree.get('m_ImageCount')) is int and tree['m_ImageCount'] == 1,
            'Only a single Texture2D surface is qualified.')
    require(tree.get('m_IsPreProcessed') is False, 'Preprocessed texture layout is unqualified.')
    require(tree.get('m_Depth', 1) == 1 and not tree.get('m_PlatformBlob'), 'Unqualified texture depth or platform layout.')
    integer(tree.get('m_CompleteImageSize'), 1, MAX_BYTES, 'complete byte count')
    require(tree['m_CompleteImageSize'] == len(payload), 'Texture complete size disagrees with payload.')
    sampler = tree.get('m_TextureSettings')
    require(isinstance(sampler, dict) and set(sampler) == {'m_FilterMode','m_Aniso','m_MipBias','m_WrapU','m_WrapV','m_WrapW'},
            'Unqualified texture sampler schema.')
    integer(sampler['m_FilterMode'], 0, 2, 'filter mode')
    integer(sampler['m_Aniso'], 0, 16, 'anisotropy')
    for key in ('m_WrapU','m_WrapV','m_WrapW'):
        integer(sampler[key], 0, 3, 'wrap mode')
    require(type(sampler['m_MipBias']) in (float, int) and math.isfinite(sampler['m_MipBias']), 'Invalid texture mip bias.')
    return validate(Texture(tree.get('m_Width'), tree.get('m_Height'), tree.get('m_MipCount'),
                            tree.get('m_TextureFormat'), tree.get('m_ColorSpace'), payload))



def shader_default_2d(name, *, unity_version, rendering_color_space):
    """Project the five measured built-in 2D defaults for the exact source profile.

    Caller must have resolved a shader-declared default, not an absent required
    asset or a runtime global. The rendering color space is explicit evidence;
    it is not inferred from a material, image record or current host settings.
    This preserves the measured RGBA8 payload, 4x4 extent and sole mip. It does
    not bind a sampler, infer array globals or change the v1 texture packet.
    """
    require(unity_version == '6000.0.64f1', 'Unqualified shader-default Unity version.')
    require(rendering_color_space in ('Gamma', 'Linear'), 'Explicit source rendering color space required.')
    pixels = {'white': (255, 255, 255, 255), 'black': (0, 0, 0, 0),
              'bump': (127, 127, 255, 255), 'linearGrey': (127, 127, 127, 127),
              'grey': (127, 127, 127, 127)}
    require(type(name) is str and name in pixels, 'Unqualified shader-declared 2D default.')
    srgb = rendering_color_space == 'Linear' and name in ('white', 'black', 'grey')
    return validate(Texture(4, 4, 1, 4, int(srgb), bytes(pixels[name]) * 16))


def read_texture(obj, members, cancelled=lambda: False):
    """Read only exact embedded PC records and explicitly addressed bundle streams."""
    h.check_cancelled(cancelled)
    require(importlib.metadata.version('UnityPy') == '1.24.2', 'Unqualified source texture reader version.')
    require(obj.type.name == 'Texture2D' and obj.assets_file.target_platform == 19 and
            obj.assets_file.reader.endian == '<', 'Unqualified source texture type, platform or byte order.')
    integer(getattr(obj, 'byte_size', None), 1, 64 * 1024 * 1024, 'record byte count')
    raw = obj.get_raw_data()
    require(len(raw) == obj.byte_size, 'Source texture record size changed.')
    tree = embedded_tree(obj)
    stream = tree.get('m_StreamData')
    inline = tree.get('image data', tree.get('m_ImageData', b''))
    require(isinstance(inline, (bytes, bytearray, list)) and len(inline) <= MAX_BYTES, 'Invalid inline texture payload.')
    if stream and stream.get('path'):
        name = str(obj.assets_file.name)
        require(stream['path'] == 'archive:/' + name + '/' + name + '.resS' and not inline,
                'Texture stream is not the explicit same-bundle member or has conflicting inline bytes.')
        reader = members.get(name + '.resS')
        require(reader is not None, 'Explicit texture stream member is absent.')
        offset = integer(stream.get('offset'), 0, reader.Length, 'stream offset')
        size = integer(stream.get('size'), 1, MAX_BYTES, 'stream size')
        require(offset+size <= reader.Length, 'Texture stream extends beyond its member.')
        position = reader.Position
        try:
            reader.Position = offset
            payload = reader.read_bytes(size)
        finally:
            reader.Position = position
        require(len(payload) == size, 'Truncated texture stream read.')
    else:
        require(not stream or (stream.get('size') == 0 and stream.get('offset') == 0), 'Unaddressed texture stream data.')
        payload = bytes(inline)
    texture = from_tree(tree, payload)
    h.check_cancelled(cancelled)
    require(obj.get_raw_data() == raw, 'Source texture record changed while reading.')
    return texture, {'record_sha256': hashlib.sha256(raw).hexdigest(),
                     'payload_sha256': hashlib.sha256(payload).hexdigest(),
                     'sampler': dict(tree['m_TextureSettings']),
                     'lightmap_format': tree.get('m_LightmapFormat')}


def encode(texture):
    validate(texture)
    if type(texture) is TextureArray:
        prefix = struct.pack('<8s7I', MAGIC, 2, texture.width, texture.height,
                             texture.mip_count, texture.format, texture.color_space, texture.layer_count)
    else:
        # Version-1 producers and saved bytes remain unchanged.
        prefix = struct.pack('<8s6I', MAGIC, 1, texture.width, texture.height,
                             texture.mip_count, texture.format, texture.color_space)
    digest = hashlib.sha256(prefix + texture.payload).digest()
    return prefix + digest + texture.payload


def decode(blob):
    require(type(blob) is bytes and HEADER.size <= len(blob) <= MAX_BYTES + ARRAY_HEADER.size,
            'Invalid source texture projection size.')
    magic, version = struct.unpack_from('<8sI', blob)
    require(magic == MAGIC and version in (1, 2), 'Unsupported source texture projection version.')
    header = HEADER if version == 1 else ARRAY_HEADER
    require(header.size <= len(blob) <= MAX_BYTES + header.size, 'Invalid source texture projection size.')
    fields = header.unpack_from(blob)
    width, height, count, fmt, color = fields[2:7]
    payload = blob[header.size:]
    texture = (Texture(width, height, count, fmt, color, payload) if version == 1 else
               TextureArray(width, height, count, fmt, color, payload, fields[7]))
    validate(texture)
    require(hashlib.sha256(blob[:header.size-32] + payload).digest() == fields[-1],
            'Source texture projection hash mismatch.')
    return texture
