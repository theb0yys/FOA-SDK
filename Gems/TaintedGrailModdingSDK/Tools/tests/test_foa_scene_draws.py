# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_heightmap_importer import HeightmapImportError
from foa_scene_asset_binding import SourceAssetReference
from foa_scene_draws import drake_draw_slots, project_drake_draw_geometry, project_source_submesh_geometry
from test_foa_scene_mesh_glb import fixture, decode

PROFILE = {'unity_version': '6000.0.64f1'}
MATERIALS = [SourceAssetReference('Material', str(i) * 32, None) for i in (1, 2, 3)]


class DrawSlotTests(unittest.TestCase):
    def test_fewer_materials_leave_source_submeshes_undrawn(self):
        rows = drake_draw_slots(3, MATERIALS[:1], **PROFILE)
        self.assertEqual([(r.material_ordinal, r.effective_submesh) for r in rows], [(0, 0)])
        self.assertEqual(drake_draw_slots(3, [], **PROFILE), ())

    def test_overflow_draws_repeat_last_submesh_without_dropping_materials(self):
        for count, expected in ((1, [0, 0, 0]), (2, [0, 1, 1]), (3, [0, 1, 2])):
            rows = drake_draw_slots(count, MATERIALS, **PROFILE)
            self.assertEqual([r.effective_submesh for r in rows], expected)
            self.assertEqual([r.requested_submesh for r in rows], [0, 1, 2])
            self.assertEqual([r.material for r in rows], MATERIALS)

    def test_duplicate_material_identities_remain_distinct_draws(self):
        rows = drake_draw_slots(1, [MATERIALS[0]] * 3, **PROFILE)
        self.assertEqual(len(rows), 3)
        self.assertEqual([r.material_ordinal for r in rows], [0, 1, 2])

    def test_unknown_profile_and_ordinal_overflow_reject(self):
        with self.assertRaises(HeightmapImportError):
            drake_draw_slots(1, MATERIALS, unity_version='unknown')
        with self.assertRaises(HeightmapImportError):
            drake_draw_slots(1, [MATERIALS[0]] * 129, **PROFILE)
        self.assertEqual(drake_draw_slots(1, [MATERIALS[0]] * 128, **PROFILE)[-1].requested_submesh, 127)

    def test_invalid_counts_and_untyped_references_reject(self):
        for count in (True, 0, -1, 4097, 1.0, None):
            with self.subTest(count=count), self.assertRaises(HeightmapImportError):
                drake_draw_slots(count, MATERIALS, **PROFILE)
        for materials in ([None], [MATERIALS[0].key], [SourceAssetReference('Mesh', '1' * 32, None)], iter(MATERIALS)):
            with self.assertRaises(HeightmapImportError):
                drake_draw_slots(1, materials, **PROFILE)

    def test_geometry_projection_preserves_repeated_oriented_triangles(self):
        mesh = fixture(); original = copy.deepcopy(mesh)
        data, report = project_drake_draw_geometry(mesh, MATERIALS, **PROFILE)
        doc, _, read = decode(data)
        primitives = doc['meshes'][0]['primitives']
        self.assertEqual([read(p['indices']) for p in primitives], [[(0,), (2,), (1,)], [(2,), (0,), (1,)], [(2,), (0,), (1,)]])
        self.assertEqual([p['material'] for p in primitives], [0, 1, 2])
        self.assertEqual(report['source_submesh_count'], 2)
        self.assertEqual(report['triangles'], 3)
        self.assertEqual(mesh, original)
        self.assertEqual(report['native_draw_order'], 'NOT_RUN')
        self.assertEqual(report['source_material_mapping'], 'NOT_RUN')
        self.assertEqual(report['game_export'], 'NOT_RUN')
        self.assertTrue(report['source_record_required_for_return'])

    def test_geometry_projection_removes_only_undrawn_primitives(self):
        data, report = project_drake_draw_geometry(fixture(), MATERIALS[:1], **PROFILE)
        self.assertEqual(len(decode(data)[0]['meshes'][0]['primitives']), 1)
        self.assertEqual(report['undrawn_source_submeshes'], [1])
        with self.assertRaises(HeightmapImportError):
            project_drake_draw_geometry(fixture(), [], **PROFILE)
        with self.assertRaises(HeightmapImportError):
            project_drake_draw_geometry(fixture(), MATERIALS, cancelled=lambda: True, **PROFILE)

    def test_isolated_submesh_retains_source_geometry_and_identity(self):
        mesh = fixture(); original = copy.deepcopy(mesh)
        data, report = project_source_submesh_geometry(mesh, 1)
        doc, _, read = decode(data)
        primitives = doc['meshes'][0]['primitives']
        self.assertEqual(len(primitives), 1)
        self.assertEqual(read(primitives[0]['indices']), [(2,), (0,), (1,)])
        self.assertEqual(doc['meshes'][0]['extras']['source_binding'], mesh.binding)
        self.assertEqual(report['source_submesh'], 1)
        self.assertEqual(report['source_submesh_count'], 2)
        self.assertEqual(report['native_draw_order'], 'NOT_RUN')
        self.assertEqual(mesh, original)
        for index in (-1, 2, True, 0.0):
            with self.assertRaises(HeightmapImportError):
                project_source_submesh_geometry(mesh, index)
        with self.assertRaises(HeightmapImportError):
            project_source_submesh_geometry(mesh, 0, lambda: True)


if __name__ == '__main__':
    unittest.main()
