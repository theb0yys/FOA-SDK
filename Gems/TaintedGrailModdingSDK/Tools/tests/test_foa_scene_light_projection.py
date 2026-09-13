# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic projection compatibility, preservation and rejection tests."""
import copy
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_lighting as lighting
from foa_heightmap_importer import HeightmapImportError
from test_foa_scene_lighting import PROFILE, native_values, fixture


def additional():
    return dict(m_Version=12, m_EnableSpotReflector=0, m_LuxAtDistance=2.5,
                m_LightUnit=1, m_Intensity=999.0)


class LightProjectionTests(unittest.TestCase):
    def test_migration_keeps_source_and_intensity(self):
        source, hd = native_values(), additional()
        before = copy.deepcopy((source, hd))
        result = lighting.project_light_values(source, hd, PROFILE)
        self.assertEqual(result['values']['m_Intensity'], 7.5)
        self.assertEqual(result['values']['m_LightUnit'], 1)
        self.assertEqual(result['values']['m_LuxAtDistance'], 2.5)
        self.assertIs(result['values']['m_EnableSpotReflector'], False)
        self.assertEqual(set(result['changed_fields']), {'m_LightUnit', 'm_LuxAtDistance', 'm_EnableSpotReflector'})
        self.assertEqual((result['source_version'], result['projected_version']), (12, 13))
        self.assertEqual((source, hd), before)
        result['values']['m_Color']['r'] = 10.0
        self.assertEqual((source, hd), before)

    def test_version_13_does_not_reapply_obsolete_fields(self):
        hd = additional(); hd['m_Version'] = 13
        result = lighting.project_light_values(native_values(), hd, PROFILE)
        self.assertEqual(result['values'], native_values())
        self.assertEqual(result['changed_fields'], [])
        self.assertEqual(result['migration'], 'NOT_APPLICABLE')

    def test_no_companion_is_not_invented(self):
        result = lighting.project_light_values(native_values(), None, PROFILE)
        self.assertIsNone(result['projected_version'])
        self.assertEqual(result['values'], native_values())

    def test_unknown_shapes_and_versions_reject(self):
        for shape in (0, 1, 3, 4, 5, 6, 7, 255):
            source = native_values(); source['m_Type'] = shape
            with self.subTest(shape=shape), self.assertRaisesRegex(HeightmapImportError, 'Point-light'):
                lighting.project_light_values(source, additional(), PROFILE)
        for version in (None, True, 12.0, 11, 14):
            hd = additional(); hd['m_Version'] = version
            with self.subTest(version=version), self.assertRaises(HeightmapImportError):
                lighting.project_light_values(native_values(), hd, PROFILE)

    def test_incomplete_and_lossy_migration_inputs_reject(self):
        for field in ('m_EnableSpotReflector', 'm_LuxAtDistance', 'm_LightUnit'):
            hd = additional(); del hd[field]
            with self.subTest(missing=field), self.assertRaises(HeightmapImportError):
                lighting.project_light_values(native_values(), hd, PROFILE)
        for field, value in (('m_EnableSpotReflector', True), ('m_EnableSpotReflector', 2),
                             ('m_LightUnit', True), ('m_LightUnit', 5), ('m_LightUnit', -1),
                             ('m_LuxAtDistance', float('nan')), ('m_LuxAtDistance', float('inf')),
                             ('m_LuxAtDistance', 0.0), ('m_LuxAtDistance', -1.0),
                             ('m_LuxAtDistance', 0.1), ('m_LuxAtDistance', 1e39)):
            hd = additional(); hd[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(HeightmapImportError):
                lighting.project_light_values(native_values(), hd, PROFILE)

    def test_profile_and_companion_type_reject(self):
        for profile in ({}, {**PROFILE, 'unity_version': 'other'}):
            with self.assertRaises(HeightmapImportError):
                lighting.project_light_values(native_values(), additional(), profile)
        with self.assertRaises(HeightmapImportError):
            lighting.project_light_values(native_values(), [], PROFILE)

    def test_default_capture_v1_stays_unchanged(self):
        _, graph, scripts = fixture()
        packet = lighting.capture_scene_lights(graph, scripts, PROFILE)
        self.assertEqual(packet['version'], 1)
        self.assertNotIn('projected_values_status', packet)
        self.assertNotIn('value_projection', packet['lights'][0])

    def test_opt_in_projection_preserves_the_pre_migration_blocker(self):
        f, graph, scripts = fixture()
        f.objects[4].tree.update(additional())
        packet = lighting.capture_scene_lights(graph, scripts, PROFILE, include_projection=True)
        row, = packet['lights']
        self.assertEqual(packet['version'], 2)
        self.assertEqual(packet['projected_values_status'], 'PASSED')
        self.assertEqual(row['stored_values_status'], 'BLOCKED')
        self.assertEqual(row['migration_required'], 12)
        self.assertEqual(row['value_projection']['values']['m_Intensity'], 7.5)
        self.assertEqual(row['additional']['tree']['m_Version'], 12)
        self.assertEqual(row['gpu_light_data'], 'NOT_RUN')
        self.assertEqual(packet['rendering'], 'NOT_RUN')
        self.assertEqual(packet['game_export'], 'NOT_RUN')

    def test_unqualified_migration_is_preserved_in_v2(self):
        f, graph, scripts = fixture(); f.objects[4].tree['m_Version'] = 14
        packet = lighting.capture_scene_lights(graph, scripts, PROFILE, include_projection=True)
        row, = packet['lights']
        self.assertEqual(row['projected_values_status'], 'BLOCKED')
        self.assertEqual(packet['projected_values_status'], 'PARTIAL')
        self.assertIsNone(row['value_projection'])
        self.assertEqual(row['additional']['tree']['m_Version'], 14)


if __name__ == '__main__':
    unittest.main()
