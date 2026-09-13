# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Constant-buffer joins and explicit-value rejection tests using synthetic data."""
import copy
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_heightmap_importer import HeightmapImportError
from foa_scene_shader_constants import constant_buffers, pack_constants


def fixtures():
    common = {'m_ConstantBuffers': [{'m_NameIndex': 0, 'm_Size': 112, 'm_IsPartialCB': True,
               'm_VectorParams': [{'m_NameIndex': 1, 'm_Index': 0, 'm_ArraySize': 0, 'm_Type': 0, 'm_Dim': 3}],
               'm_MatrixParams': [{'m_NameIndex': 2, 'm_Index': 16, 'm_ArraySize': 0, 'm_Type': 0, 'm_RowCount': 4}],
               'm_StructParams': []}]}
    tree = {'m_ParsedForm': {'m_SubShaders': [{'m_Passes': [{'m_NameIndices': [('Data', 0), ('colour', 1), ('matrix', 2)],
            'progVertex': {'m_CommonParameters': common}}]}]}}
    field = {'name': 'enabled', 'type': 1, 'rows': 1, 'columns': 1, 'matrix': False, 'array_size': 0, 'index': 96}
    params = {'groups': [{'name': '', 'used_size': 0, 'parameters': [], 'structures': []},
                        {'name': 'Data', 'used_size': 112, 'parameters': [field], 'structures': []}]}
    program = {'subshader': 0, 'pass': 0, 'stage': 'progVertex'}
    return tree, program, params


class ConstantTests(unittest.TestCase):
    def test_common_variant_merge_and_exact_byte_locations(self):
        layout = constant_buffers(*fixtures())['Data']
        matrix = tuple(i+.25 for i in range(16))
        raw = pack_constants(layout, {'colour': (.125, .5, 2), 'matrix': matrix, 'enabled': -7})
        self.assertEqual(len(raw), 112)
        self.assertEqual(struct.unpack_from('<3f', raw), (.125, .5, 2))
        self.assertEqual(struct.unpack_from('<16f', raw, 16), matrix)
        self.assertEqual(struct.unpack_from('<i', raw, 96), (-7,))
        self.assertEqual(raw[12:16]+raw[80:96]+raw[100:], bytes(32))

    def test_duplicate_matching_field_is_allowed(self):
        tree, program, params = fixtures()
        original = constant_buffers(tree, program, params)['Data']['fields']['colour']
        params['groups'][1]['parameters'].append({k:v for k,v in original.items() if k != 'bytes'})
        self.assertEqual(len(constant_buffers(tree, program, params)['Data']['fields']), 3)

    def test_conflicting_duplicate_is_rejected(self):
        tree, program, params = fixtures()
        f = params['groups'][1]['parameters'][0]
        f.update(name='colour', type=0, columns=3)
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_size_disagreement_is_rejected(self):
        tree, program, params = fixtures(); params['groups'][1]['used_size'] = 128
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_missing_extra_and_wrong_shape_values(self):
        layout = constant_buffers(*fixtures())['Data']
        good = {'colour': (1,2,3), 'matrix': tuple(range(16)), 'enabled': 1}
        for bad in ({}, dict(good, extra=1), dict(good, colour=(1,2)), dict(good, enabled=True)):
            with self.subTest(bad=bad), self.assertRaises(HeightmapImportError): pack_constants(layout, bad)

    def test_unsupported_array_structure_type_and_matrix(self):
        for changed in ({'array_size': 1}, {'rows': 0}, {'rows': 2}, {'type': 2}, {'type': True}, {'matrix': True, 'rows': 3}):
            tree, program, params = fixtures(); params['groups'][1]['parameters'][0].update(changed)
            with self.subTest(changed=changed), self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)
        tree, program, params = fixtures(); params['groups'][1]['structures'] = [{'name': 'unknown'}]
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_unbound_global_is_not_silently_ignored(self):
        tree, program, params = fixtures(); params['groups'][0]['used_size'] = 16
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_bad_offsets_overlap_and_register_crossing(self):
        for changed in ({'index': 0}, {'index': 97}, {'index': 112}, {'index': 92, 'columns': 2}):
            tree, program, params = fixtures(); params['groups'][1]['parameters'][0].update(changed)
            with self.subTest(changed=changed), self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_nonfinite_and_integer_overflow_rejected(self):
        layout = constant_buffers(*fixtures())['Data']
        good = {'colour': (1,2,3), 'matrix': tuple(range(16)), 'enabled': 1}
        for bad in (dict(good, colour=(float('nan'),1,1)), dict(good, enabled=2**31), dict(good, colour=(1e100,0,0))):
            with self.subTest(bad=bad), self.assertRaises(HeightmapImportError): pack_constants(layout, bad)

    def test_mutated_layout_cannot_write_outside_buffer(self):
        layout = constant_buffers(*fixtures())['Data']
        values = {'colour': (1,2,3), 'matrix': tuple(range(16)), 'enabled': 1}
        for changed in ({'index': -4}, {'index': 112}, {'bytes': 3}, {'index': 0}, {'type': 3}):
            bad = copy.deepcopy(layout); bad['fields']['enabled'].update(changed)
            with self.subTest(changed=changed), self.assertRaises(HeightmapImportError): pack_constants(bad, values)


if __name__ == '__main__': unittest.main()
