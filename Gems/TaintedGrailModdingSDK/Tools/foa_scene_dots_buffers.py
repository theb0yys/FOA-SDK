# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Explicit DOTS instance streams for the qualified source shader profile.

The caller supplies exact property bytes and visibility. This does not calculate
inverses, probes, LODs, material defaults, controller state or runtime visibility.
"""
import struct
import foa_scene_assembly as a

PROFILE = {
    'unity_version': '6000.0.64f1',
    'entities_graphics_sha256': '136a70fcbf38274eb1662420f41faed52e88b3895b6ec5253d7245cfa24360a6',
    'awaken_ecs_sha256': 'b74a5bf79e93a75b11c5b70ace44d85e5a88650168077c6503dd10f4d2abd6f6',
}
MAX_INSTANCES = 4096
MAX_BYTES = 16 * 1024 * 1024


def _profile(profile):
    a.require(type(profile) is dict and profile == PROFILE, 'Unqualified instance-buffer profile.')


def _count(value):
    a.require(type(value) is int and 0 < value <= MAX_INSTANCES, 'Invalid explicit GPU slot count.')


def pack_matrix_columns(matrices):
    """Pack source-space row-major float bits as four stored float3 columns.

    Source matrices are explicit, including the inverse when a shader needs it.
    Host-space matrices must first be converted by their owning placement adapter.
    """
    a.require(type(matrices) is list, 'Expected ordered source matrices.')
    _count(len(matrices))
    output = bytearray()
    for matrix in matrices:
        a.matrix(matrix)
        a.require(matrix[12:] == [0, 0, 0, 0x3f800000], 'Unrepresentable source matrix bottom row.')
        output.extend(struct.pack('<12I', *(matrix[r*4+c] for c in range(4) for r in range(3))))
    return bytes(output)


def pack_streams(properties, slot_count, profile):
    """Pack ordered per-instance property streams and return name-to-metadata words.

    Property order and GPU slot capacity are explicit; Unity IDs are not invented.
    Original ECS chunks reserve Capacity, not Count. The caller must preserve
    those gaps and supply the matching visible-slot mapping; source ordinals
    cannot be used as GPU slot indices without a separately qualified mapping.
    Each property has only name, stride and data. No missing property is synthesized.
    The 64-byte shared zero allocation and 16-byte stream alignment follow the
    inspected original allocator. Metadata's upper bit selects per-instance data.
    """
    _profile(profile); _count(slot_count)
    a.require(type(properties) is list and 0 < len(properties) <= 256, 'Invalid property stream count.')
    rows, names, size = [], set(), 64
    for prop in properties:
        a.keys(prop, {'name', 'stride', 'data'})
        name = a.text(prop['name'], 256); stride = prop['stride']; data = prop['data']
        a.require(name not in names, 'Duplicate instance property.')
        a.require(type(stride) is int and 4 <= stride <= 2048 and stride % 4 == 0, 'Invalid property stride.')
        a.require(type(data) is bytes and len(data) == stride*slot_count, 'Incomplete property stream.')
        names.add(name); padded = (len(data)+15)//16*16
        a.require(size+padded <= MAX_BYTES, 'Instance data exceeds its buffer bound.')
        rows.append((name, size, data, padded)); size += padded
    output = bytearray(size); metadata = {}
    for name, offset, data, padded in rows:
        output[offset:offset+len(data)] = data
        metadata[name] = 0x80000000 | offset
    return bytes(output), metadata


def visibility_buffers(indices, slot_count, profile, *, indirect, strip_upper_byte):
    """Encode explicit visible indices for the qualified original shader branch.

    Upper-byte stripping preserves each supplied raw word and sets flag 32. This
    flag is a consumer operation, not an inferred LOD choice. The indirect stream
    begins at word zero, selected by the metadata high bit. Direct mode still
    returns the supplied index bytes for its declared but unused raw resource.
    """
    _profile(profile); _count(slot_count)
    a.require(type(indirect) is bool and type(strip_upper_byte) is bool, 'Visibility modes must be explicit booleans.')
    a.require(type(indices) is list and 0 < len(indices) <= (MAX_INSTANCES if indirect else 256),
              'Invalid explicit visible count.')
    for word in indices:
        a.require(type(word) is int and 0 <= word <= 0xffffffff, 'Invalid visible index word.')
        a.require((word & 0x00ffffff if strip_upper_byte else word) < slot_count, 'Visible instance is absent.')
    constants = bytearray(4096)
    if indirect:
        struct.pack_into('<I', constants, 4, 0x80000000)
    else:
        for i, word in enumerate(indices): struct.pack_into('<I', constants, i*16, word)
    struct.pack_into('<I', constants, 12, 32 if strip_upper_byte else 0)
    return bytes(constants), struct.pack('<'+'I'*len(indices), *indices)
