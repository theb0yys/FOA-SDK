# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Source-bound merged renderer records, retaining every original payload byte.

The inspected source member consists of five little-endian counted POD arrays.
Ordinal identities belong to that member; they are never inferred GameObject IDs.
This decoder does not establish native layout execution, LOD policy or game export.
"""
from dataclasses import dataclass
import hashlib
import math
import struct
import uuid

import foa_heightmap_importer as h
from foa_scene_asset_binding import SourceAssetReference

MAX_MEMBER_BYTES = 64 * 1024 * 1024
MAX_SECTION_RECORDS = 1000000
SECTIONS = (('lod_groups', 44), ('meshes', 156), ('materials', 16),
            ('definitions', 32), ('instances', 56))


def serialized_guid(value):
    fields = tuple('_guidPart' + str(i) for i in range(1, 5))
    if (not isinstance(value, dict) or set(value) != set(fields)
            or any(type(value[k]) is not int or not -(1 << 31) <= value[k] < (1 << 31) for k in fields)):
        raise h.HeightmapImportError('Merged renderer GUID needs four exact signed source words.')
    return uuid.UUID(bytes_le=struct.pack('<4i', *(value[k] for k in fields))).hex


@dataclass(frozen=True)
class Section:
    count: int
    stride: int
    offset: int


class MergedRenderers:
    """Decode exact references and transforms while retaining unsupported fields."""
    def __init__(self, member_guid, payload, cancelled=lambda: False):
        SourceAssetReference.runtime_key('Material', member_guid)  # Strict main GUID grammar.
        if len(member_guid) != 32 or not isinstance(payload, bytes) or not 20 <= len(payload) <= MAX_MEMBER_BYTES:
            raise h.HeightmapImportError('Invalid merged renderer member identity or byte count.')
        self.guid, self.payload, self.cancelled = member_guid, payload, cancelled
        self.sha256 = hashlib.sha256(payload).hexdigest()
        self.sections, cursor = {}, 0
        for name, stride in SECTIONS:
            h.check_cancelled(cancelled)
            if cursor + 4 > len(payload):
                raise h.HeightmapImportError('Merged renderer section header is truncated.')
            count = struct.unpack_from('<I', payload, cursor)[0]
            cursor += 4
            if count > MAX_SECTION_RECORDS or cursor + count * stride > len(payload):
                raise h.HeightmapImportError('Merged renderer section exceeds its source bounds.')
            self.sections[name] = Section(count, stride, cursor)
            cursor += count * stride
        if cursor != len(payload):
            raise h.HeightmapImportError('Unclaimed merged renderer member bytes.')
        self.meshes = tuple(self._mesh(i) for i in range(self.sections['meshes'].count))
        self.materials = tuple(SourceAssetReference.runtime_key('Material',
            uuid.UUID(bytes_le=self.record('materials', i)).hex)
            for i in range(self.sections['materials'].count))
        for i in range(self.sections['definitions'].count):
            self.definition(i)
        for i in range(self.sections['instances'].count):
            self.instance(i)

    def record(self, section, index):
        h.check_cancelled(self.cancelled)
        shape = self.sections.get(section)
        if shape is None or type(index) is not int or not 0 <= index < shape.count:
            raise h.HeightmapImportError('Merged renderer record index is outside its source section.')
        start = shape.offset + index * shape.stride
        return self.payload[start:start + shape.stride]

    def identity(self, section, index):
        self.record(section, index)
        return {'member_guid': self.guid, 'member_sha256': self.sha256,
                'section': section, 'ordinal': index}

    def _mesh(self, index):
        raw = self.record('meshes', index)
        size = struct.unpack_from('<H', raw)[0]
        if not 0 < size <= 125 or raw[2 + size] != 0:
            raise h.HeightmapImportError('Invalid merged renderer fixed-string length or terminator.')
        try:
            key = raw[2:2 + size].decode('utf-8')
        except UnicodeDecodeError as error:
            raise h.HeightmapImportError('Invalid merged renderer asset key encoding.') from error
        return SourceAssetReference.runtime_key('Mesh', key)

    def definition(self, index):
        raw = self.record('definitions', index)
        mesh = struct.unpack_from('<I', raw, 16)[0]
        start, length = struct.unpack_from('<II', raw, 24)
        if mesh >= len(self.meshes) or start + length > len(self.materials):
            raise h.HeightmapImportError('Merged renderer definition references an absent asset.')
        return {'mesh_index': mesh, 'material_start': start, 'material_count': length,
                'mesh': self.meshes[mesh], 'materials': self.materials[start:start + length],
                'lod_mask': struct.unpack_from('<i', raw, 12)[0],
                'light_probe_usage': raw[20], 'transparent_mask': raw[21],
                'filter_settings_bytes': raw[:12], 'source_bytes': raw}

    def instance(self, index):
        raw = self.record('instances', index)
        columns = struct.unpack_from('<12f', raw)
        lod, definition = struct.unpack_from('<II', raw, 48)
        if (not all(math.isfinite(v) for v in columns) or lod >= self.sections['lod_groups'].count
                or definition >= self.sections['definitions'].count):
            raise h.HeightmapImportError('Merged renderer instance has invalid transform or references.')
        return {'local_to_world_columns': columns, 'lod_group_index': lod,
                'definition_index': definition, 'source_bytes': raw}

    def render_record(self, index):
        """Retain source matrix columns, render fields and exact ordered material slots."""
        instance = self.instance(index)
        definition = self.definition(instance['definition_index'])
        return {**instance, 'identity': self.identity('instances', index),
                'definition': definition,
                'lod_group_bytes': self.record('lod_groups', instance['lod_group_index']),
                'mesh_bytes': self.record('meshes', definition['mesh_index'])}

    def rebuild_unchanged(self):
        """Reassemble all framed records including their padding and uninterpreted fields."""
        chunks = []
        for name, _ in SECTIONS:
            chunks.append(struct.pack('<I', self.sections[name].count))
            chunks.extend(self.record(name, i) for i in range(self.sections[name].count))
        return b''.join(chunks)
