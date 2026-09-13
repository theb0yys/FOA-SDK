# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic array/structure packing, input completeness and layout corruption tests."""
import copy
import struct
import unittest
from test_foa_scene_shader_constants import fixtures
from foa_heightmap_importer import HeightmapImportError
from foa_scene_shader_constants import constant_buffers, pack_constants


def field(name, index, *, columns=4, kind=0, matrix=False, count=0):
    return dict(name=name, index=index, type=kind, rows=4 if matrix else 1,
                columns=columns, matrix=matrix, array_size=count)


def array_fixture():
    tree, program, params = fixtures()
    tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0]['progVertex']['m_CommonParameters']['m_ConstantBuffers'] = []
    params['groups'][1] = dict(name='Data', used_size=480, parameters=[
        field('lead', 0), field('vectors', 16, count=2), field('integers', 48, kind=1, count=2),
        field('matrices', 80, matrix=True, count=2)], structures=[dict(
            name='items', index=224, array_size=2, size=128, parameters=[
                field('pair', 0, columns=2), field('triple', 16, columns=3),
                field('factor', 28, columns=1), field('transform', 32, matrix=True),
                field('colours', 96, count=2)])])
    return tree, program, params


def explicit_values():
    return dict(lead=[1,2,3,4], vectors=[[5,6,7,8],[9,10,11,12]],
                integers=[[-1,2,-3,4],[5,-6,7,-8]],
                matrices=[[i+.25 for i in range(16)],[i+.5 for i in range(16)]],
                items=[dict(pair=[1,2], triple=[3,4,5], factor=6,
                            transform=[i+.75 for i in range(16)], colours=[[7,8,9,10],[11,12,13,14]]),
                       dict(pair=[-1,-2], triple=[-3,-4,-5], factor=-6,
                            transform=[-i-.75 for i in range(16)], colours=[[-7,-8,-9,-10],[-11,-12,-13,-14]])])


class ArrayConstantTests(unittest.TestCase):
    def test_exact_arrays_structure_stride_and_relative_offsets(self):
        data = pack_constants(constant_buffers(*array_fixture())['Data'], explicit_values())
        self.assertEqual(len(data), 480)
        self.assertEqual(struct.unpack_from('<4f', data, 32), (9,10,11,12))
        self.assertEqual(struct.unpack_from('<4i', data, 64), (5,-6,7,-8))
        self.assertEqual(struct.unpack_from('<16f', data, 144), tuple(i+.5 for i in range(16)))
        self.assertEqual(struct.unpack_from('<2f', data, 352), (-1,-2))
        self.assertEqual(struct.unpack_from('<4f', data, 368), (-3,-4,-5,-6))
        self.assertEqual(struct.unpack_from('<16f', data, 384), tuple(-i-.75 for i in range(16)))
        self.assertEqual(struct.unpack_from('<4f', data, 464), (-11,-12,-13,-14))
        self.assertEqual(data[208:224] + data[232:240] + data[360:368], bytes(32))

    def test_common_vector_and_matrix_arrays(self):
        tree, program, params = fixtures()
        common = tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0]['progVertex']['m_CommonParameters']['m_ConstantBuffers'][0]
        common['m_Size'] = 176
        common['m_VectorParams'][0].update(m_Dim=4, m_ArraySize=2)
        common['m_MatrixParams'][0].update(m_Index=32, m_ArraySize=2)
        params['groups'][1].update(used_size=176)
        params['groups'][1]['parameters'][0]['index'] = 160
        layout = constant_buffers(tree, program, params)['Data']
        values = dict(colour=[[1,2,3,4],[5,6,7,8]], matrix=[list(range(16)),list(range(16,32))], enabled=-5)
        raw = pack_constants(layout, values)
        self.assertEqual(struct.unpack_from('<4f', raw, 16), (5,6,7,8))
        self.assertEqual(struct.unpack_from('<16f', raw, 96), tuple(range(16,32)))
        self.assertEqual(struct.unpack_from('<i', raw, 160), (-5,))

    def test_duplicate_structure_and_conflict(self):
        tree, program, params = array_fixture()
        params['groups'].append(copy.deepcopy(params['groups'][1]))
        self.assertEqual(len(constant_buffers(tree, program, params)['Data']['structures']), 1)
        params['groups'][2]['structures'][0]['parameters'][0]['index'] = 8
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_array_shape_and_value_rejections(self):
        layout = constant_buffers(*array_fixture())['Data']
        for change in ([], [1,2], [[1,2,3,4]], [[1,2,3,4]]*3, [[1,2,3],[1,2,3]], [[1,2,3,4],[1,2,float('inf'),4]]):
            values = explicit_values(); values['vectors'] = change
            with self.subTest(value=change), self.assertRaises(HeightmapImportError): pack_constants(layout, values)
        for bad in (True, 2**31, -(2**31)-1, 2.5):
            values = explicit_values(); values['integers'][1][2] = bad
            with self.subTest(value=bad), self.assertRaises(HeightmapImportError): pack_constants(layout, values)

    def test_structure_value_rejections(self):
        layout = constant_buffers(*array_fixture())['Data']
        for change in ({}, [], [explicit_values()['items'][0]], [{},{}], [explicit_values()['items'][0],{}]):
            values = explicit_values(); values['items'] = change
            with self.subTest(value=change), self.assertRaises(HeightmapImportError): pack_constants(layout, values)
        values = explicit_values(); values['items'][1]['extra'] = 0
        with self.assertRaises(HeightmapImportError): pack_constants(layout, values)

    def test_structure_bounds_and_unsupported_shapes(self):
        for change in (dict(index=208), dict(index=228), dict(index=-16), dict(size=112), dict(size=129),
                       dict(size=65536), dict(size=True), dict(array_size=0), dict(array_size=True),
                       dict(array_size=4097), dict(array_size=3), dict(parameters=[]), dict(nested=[])):
            tree, program, params = array_fixture(); params['groups'][1]['structures'][0].update(change)
            # 208 is aligned and in bounds, but overlaps the matrices only after extending that field.
            if change == dict(index=208): params['groups'][1]['parameters'].append(field('end', 208))
            with self.subTest(change=change), self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_structure_member_bounds_and_overlap(self):
        for change in (dict(index=-4), dict(index=128), dict(index=16), dict(index=12), dict(index=4)):
            tree, program, params = array_fixture(); params['groups'][1]['structures'][0]['parameters'][0].update(change)
            # 4 is a valid float2 offset; extend it so that it overlaps the float3 at 16.
            if change == dict(index=4): params['groups'][1]['structures'][0]['parameters'][0].update(columns=4)
            with self.subTest(change=change), self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_array_layout_bounds_and_alignment(self):
        for change in (dict(array_size=-1), dict(array_size=True), dict(array_size=4097), dict(array_size=30),
                       dict(index=20), dict(columns=3), dict(type=2)):
            tree, program, params = array_fixture(); params['groups'][1]['parameters'][1].update(change)
            with self.subTest(change=change), self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_mutated_packed_structure_revalidated(self):
        layout = constant_buffers(*array_fixture())['Data']
        for change in (dict(bytes=4), dict(index=464), dict(size=16), dict(array_size=True), dict(fields={}), dict(extra=1)):
            bad = copy.deepcopy(layout); bad['structures']['items'].update(change)
            with self.subTest(change=change), self.assertRaises(HeightmapImportError): pack_constants(bad, explicit_values())
        bad = copy.deepcopy(layout); bad['structures']['items']['fields']['factor']['index'] = 500
        with self.assertRaises(HeightmapImportError): pack_constants(bad, explicit_values())

    def test_field_structure_name_collision(self):
        tree, program, params = array_fixture(); params['groups'][1]['structures'][0]['name'] = 'lead'
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_common_structure_shape_remains_unqualified(self):
        tree, program, params = fixtures()
        tree['m_ParsedForm']['m_SubShaders'][0]['m_Passes'][0]['progVertex']['m_CommonParameters']['m_ConstantBuffers'][0]['m_StructParams'] = [dict(name='unknown')]
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)

    def test_expanded_work_bound(self):
        tree, program, params = array_fixture()
        item = dict(name='many', index=0, array_size=4096, size=16,
                    parameters=[field('a', 0, columns=1), field('b', 4, columns=1)])
        params['groups'][1].update(used_size=65536, parameters=[], structures=[item])
        with self.assertRaises(HeightmapImportError): constant_buffers(tree, program, params)
        item['parameters'].pop()
        layout = constant_buffers(tree, program, params)['Data']
        raw = pack_constants(layout, {'many': [{'a': 3} for _ in range(4096)]})
        self.assertEqual(len(raw), 65536)
        self.assertEqual(struct.unpack_from('<f', raw, 65520), (3,))


if __name__ == '__main__': unittest.main()
