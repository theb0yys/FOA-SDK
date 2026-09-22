# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import struct
import unittest
from types import SimpleNamespace

from foa_heightmap_importer import HeightmapImportError
from foa_scene_medusa import MedusaScene, matrix_bits, manager_renderers, MANAGER_TYPE, MEMBERS, PROFILE


class MedusaTests(unittest.TestCase):
    def setUp(self):
        self.ref = ('cab-'+'a'*32, -7)
        self.renderers = ({'draws': (), 'lod_mask': 2, 'instances': 1},
                          {'draws': ({'mesh': self.ref, 'material': ('cab-'+'b'*32, 9), 'submesh': 2},),
                           'lod_mask': 1, 'instances': 2})
        self.matrix = struct.pack('<12f', -2, 0, 0, .25, 3, 0, 0, 0, .5, 17, -4, 32)
        identity = struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
        self.members = dict(zip(('Scene/'+n for n in MEMBERS),
                               (self.matrix+identity+identity+self.matrix, bytes(98),
                                struct.pack('<3I', 1, 1, 0), bytes(8))))

    def scene(self, **changes):
        args = dict(scene_name='Scene', renderers=self.renderers, transform_count=2,
                    renderer_count=3, uv_count=2, members=self.members, profile=PROFILE)
        args.update(changes)
        return MedusaScene(**args)

    def test_original_matrix_and_inverse_are_distinct_exact_records(self):
        result = self.scene()
        self.assertEqual(result.matrix_record(0), self.matrix)
        self.assertEqual(result.matrix_record(1, inverse=True), self.matrix)
        self.assertNotEqual(result.matrix_record(0, inverse=True), self.matrix)
        values = struct.unpack('<16f', struct.pack('<16I', *matrix_bits(self.matrix)))
        self.assertEqual(values, (-2, .25, 0, 17, 0, 3, 0, -4, 0, 0, .5, 32, 0, 0, 0, 1))

    def test_empty_lod_rows_still_consume_their_indices(self):
        result = list(self.scene().draws(lod=0))
        self.assertEqual([r['instance_ordinal'] for r in result], [1, 0])
        self.assertEqual([r['renderer_ordinal'] for r in result], [1, 1])
        self.assertEqual(list(self.scene().draws(lod=1)), [])

    def test_negative_zero_is_preserved(self):
        words = list(struct.unpack('<12I', self.matrix)); words[1] = 0x80000000
        self.assertEqual(matrix_bits(struct.pack('<12I', *words))[4], 0x80000000)

    def test_missing_extra_truncated_and_trailing_members_reject(self):
        for name in self.members:
            for mutate in ('missing', 'truncated', 'trailing'):
                with self.subTest(name=name, mutate=mutate):
                    data = dict(self.members)
                    if mutate == 'missing': del data[name]
                    elif mutate == 'truncated': data[name] = data[name][:-1]
                    else: data[name] += b'\0'
                    with self.assertRaises(HeightmapImportError): self.scene(members=data)
        with self.assertRaises(HeightmapImportError): self.scene(members={**self.members, 'Scene/unclaimed': b''})

    def test_absent_duplicate_and_unclaimed_indices_reject(self):
        for indices in ((1, 2, 0), (1, 0, 0)):
            with self.subTest(indices=indices), self.assertRaises(HeightmapImportError):
                self.scene(members={**self.members, 'Scene/renderers.medusa': struct.pack('<3I', *indices)})
        with self.assertRaises(HeightmapImportError):
            self.scene(renderer_count=4, members={**self.members, 'Scene/renderers.medusa': bytes(16)})

    def test_nonfinite_matrix_in_either_array_rejects(self):
        for offset in (0, 96):
            raw = bytearray(self.members['Scene/matrices.medusa']); struct.pack_into('<I', raw, offset, 0x7f800000)
            with self.subTest(offset=offset), self.assertRaises(HeightmapImportError):
                self.scene(members={**self.members, 'Scene/matrices.medusa': bytes(raw)})

    def test_profile_scene_and_count_bounds_reject(self):
        for changes in ({'profile': {}}, {'scene_name': '../Scene'}, {'scene_name': 'Other'},
                        {'transform_count': True}, {'transform_count': 100001}, {'uv_count': 3}):
            with self.subTest(changes=changes), self.assertRaises(HeightmapImportError): self.scene(**changes)
        for index in (-1, True, 2):
            with self.subTest(index=index), self.assertRaises(HeightmapImportError): self.scene().matrix_record(index)

    def test_cancellation_is_observed(self):
        with self.assertRaises(HeightmapImportError): self.scene(cancelled=lambda: True)

    def test_constructor_does_not_retain_mutable_input_draws(self):
        result = self.scene(); self.renderers[1]['draws'][0]['submesh'] = 99
        self.assertEqual(list(result.draws(lod=0))[0]['submesh'], 2)

    def test_manager_uses_exact_script_and_external_reference(self):
        asset = SimpleNamespace(name='cab-'+'a'*32, externals=[SimpleNamespace(path='archive:/cab-'+'b'*32)])
        tree = dict(m_GameObject={}, m_Enabled=1, m_Script={}, m_Name='', _transformsCount=2,
                    _allRenderersCount=3, _allUvDistributionsCount=2,
                    _renderers=[dict(lodMask=1, instancesCount=2,
                        renderData=[dict(mesh=dict(m_FileID=0, m_PathID=-7),
                                         material=dict(m_FileID=1, m_PathID=9), subMeshIndex=2)])])
        actual = manager_renderers(asset, tree, MANAGER_TYPE)
        self.assertEqual(actual[0]['draws'][0]['mesh'], self.ref)
        self.assertEqual(actual[0]['draws'][0]['material'], ('cab-'+'b'*32, 9))
        with self.assertRaises(HeightmapImportError): manager_renderers(asset, tree, ('Guess', '', 'MedusaRendererManager'))
        bad = copy.deepcopy(tree); bad['_renderers'][0]['renderData'][0]['mesh']['m_FileID'] = 2
        with self.assertRaises(HeightmapImportError): manager_renderers(asset, bad, MANAGER_TYPE)


if __name__ == '__main__': unittest.main()
