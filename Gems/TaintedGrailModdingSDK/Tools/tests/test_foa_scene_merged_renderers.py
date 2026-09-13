# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independent synthetic records exercise exact merged instance identities and framing."""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_merged_renderers as m
from foa_heightmap_importer import HeightmapImportError

GUID = '0123456789abcdef0123456789abcdef'
COLUMNS = (1., 0., 0., 0., 2., 0., 0., 0., -3., 12., 23., 34.)


def fixture():
    mesh = bytearray([0xA5] * 156)
    key = ('1' * 32 + '[Synthetic mesh]').encode()
    struct.pack_into('<H', mesh, 0, len(key))
    mesh[2:2 + len(key)] = key
    mesh[2 + len(key)] = 0
    definition = bytearray([0xCA] * 32)
    struct.pack_into('<iI', definition, 12, 3, 0)
    definition[20:22] = bytes((2, 1))
    struct.pack_into('<II', definition, 24, 0, 2)
    return {'lod_groups': [b'\xAB' * 44], 'meshes': [bytes(mesh)],
            'materials': [b'\x11' * 16, b'\x22' * 16], 'definitions': [bytes(definition)],
            'instances': [struct.pack('<12fII', *COLUMNS, 0, 0)]}


def payload(sections):
    return b''.join(struct.pack('<I', len(sections[name])) + b''.join(sections[name]) for name, _ in m.SECTIONS)


class MergedRendererTests(unittest.TestCase):
    def test_signed_source_guid_words_preserve_dotnet_byte_order(self):
        parts = (0x76543210, -0x10325477, 0x10325476, -0x67452302)
        value = dict(zip(('_guidPart' + str(i) for i in range(1, 5)), parts))
        self.assertEqual(m.serialized_guid(value), '76543210ab89efcd76543210fedcba98')
        for bad in ({}, {**value, 'name': 'guess'}, {**value, '_guidPart1': True},
                    {**value, '_guidPart1': 1 << 31}):
            with self.assertRaises(HeightmapImportError): m.serialized_guid(bad)

    def test_every_byte_survives_including_uninterpreted_fields_and_padding(self):
        raw = payload(fixture())
        source = m.MergedRenderers(GUID, raw)
        self.assertEqual(source.rebuild_unchanged(), raw)
        record = source.render_record(0)
        self.assertEqual(record['local_to_world_columns'], COLUMNS)
        self.assertEqual(record['definition']['filter_settings_bytes'], b'\xCA' * 12)
        self.assertEqual(record['lod_group_bytes'], b'\xAB' * 44)
        self.assertEqual(record['mesh_bytes'], fixture()['meshes'][0])

    def test_instance_identity_is_member_hash_and_ordinal_not_geometry_or_gameobject(self):
        sections = fixture()
        sections['instances'] *= 2
        source = m.MergedRenderers(GUID, payload(sections))
        self.assertNotEqual(source.identity('instances', 0), source.identity('instances', 1))
        self.assertEqual(source.instance(0), source.instance(1))
        changed = fixture()
        changed['lod_groups'][0] = b'\xAC' * 44
        other = m.MergedRenderers(GUID, payload(changed))
        self.assertNotEqual(source.identity('instances', 0), other.identity('instances', 0))
        self.assertEqual(set(source.identity('instances', 0)), {'member_guid', 'member_sha256', 'section', 'ordinal'})

    def test_material_order_and_duplicate_slots_are_preserved(self):
        sections = fixture()
        sections['materials'] = [b'\x22' * 16, b'\x11' * 16]
        source = m.MergedRenderers(GUID, payload(sections))
        definition = source.definition(0)
        self.assertEqual([r.guid for r in definition['materials']], ['2' * 32, '1' * 32])
        self.assertEqual((definition['lod_mask'], definition['light_probe_usage'], definition['transparent_mask']), (3, 2, 1))
        sections['materials'] = [b'\x11' * 16] * 2
        self.assertEqual(len(m.MergedRenderers(GUID, payload(sections)).definition(0)['materials']), 2)

    def test_invalid_definition_references_do_not_select_nearby_assets(self):
        for offset, values in ((16, (1,)), (24, (1, 2)), (24, (0xFFFFFFFF, 2))):
            sections = fixture()
            raw = bytearray(sections['definitions'][0])
            struct.pack_into('<' + 'I' * len(values), raw, offset, *values)
            sections['definitions'][0] = bytes(raw)
            with self.assertRaises(HeightmapImportError): m.MergedRenderers(GUID, payload(sections))

    def test_nonfinite_transform_or_invalid_instance_indices_fail(self):
        for columns, lod, definition in ((COLUMNS, 1, 0), (COLUMNS, 0, 1),
                ((float('nan'), *COLUMNS[1:]), 0, 0), ((float('inf'), *COLUMNS[1:]), 0, 0)):
            sections = fixture()
            sections['instances'][0] = struct.pack('<12fII', *columns, lod, definition)
            with self.assertRaises(HeightmapImportError): m.MergedRenderers(GUID, payload(sections))

    def test_fixed_string_missing_terminator_bad_encoding_or_wrong_key_fails(self):
        for mutation in ('terminator', 'length', 'utf8', 'guid'):
            sections = fixture()
            raw = bytearray(sections['meshes'][0])
            size = struct.unpack_from('<H', raw)[0]
            if mutation == 'terminator': raw[2 + size] = 1
            elif mutation == 'length': struct.pack_into('<H', raw, 0, 126)
            elif mutation == 'utf8': raw[2] = 255
            else: raw[2] = ord('x')
            sections['meshes'][0] = bytes(raw)
            with self.assertRaises(HeightmapImportError): m.MergedRenderers(GUID, payload(sections))

    def test_truncation_trailing_bytes_and_count_overflow_fail(self):
        raw = payload(fixture())
        for bad in (raw[:19], raw[:-1], raw + b'\0', b'\xff' * 4 + raw[4:]):
            with self.assertRaises(HeightmapImportError): m.MergedRenderers(GUID, bad)

    def test_budget_cancellation_and_record_index_controls(self):
        raw = payload(fixture())
        with patch.object(m, 'MAX_MEMBER_BYTES', 20), self.assertRaises(HeightmapImportError):
            m.MergedRenderers(GUID, raw)
        with self.assertRaises(HeightmapImportError): m.MergedRenderers(GUID, raw, lambda: True)
        source = m.MergedRenderers(GUID, raw)
        for index in (-1, 1, True, '0'):
            with self.assertRaises(HeightmapImportError): source.instance(index)
        with self.assertRaises(HeightmapImportError): source.record('GameObjects', 0)


if __name__ == '__main__':
    unittest.main()
