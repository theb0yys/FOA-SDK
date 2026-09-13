# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic lighting ownership and migrated-field tests, with no game fixtures."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_lighting as lighting
from foa_heightmap_importer import HeightmapImportError
from foa_scene_component_audit import ComponentScriptAudit
import test_foa_scene_ownership as ownership


PROFILE = {'unity_version': lighting.UNITY_VERSION, 'hdrp_runtime_sha256': lighting.HDRP_RUNTIME_SHA256}


def native_values():
    return dict(m_Enabled=1, m_Type=2, m_Color=dict(r=0.25, g=0.5, b=1.0, a=1.0),
                m_Intensity=7.5, m_Range=10.0, m_SpotAngle=30.0, m_InnerSpotAngle=20.0,
                m_LightUnit=4, m_LuxAtDistance=1.0, m_EnableSpotReflector=True,
                m_ColorTemperature=6000.0, m_UseColorTemperature=False)


def fixture(extra=()):
    f = ownership.file()
    f.unity_version = lighting.UNITY_VERSION
    ownership.entity(f, 1, 2, components=[3, 4, *extra])
    ownership.record(f, 3, 'Light', {'m_GameObject': ownership.pointer(1), **native_values()})
    for ident in (4, *extra):
        ownership.record(f, ident, 'MonoBehaviour', {'m_GameObject': ownership.pointer(1),
                         'm_Script': ownership.pointer(9), 'm_Version': 13, 'm_Intensity': 99.0})
    ownership.record(f, 9, 'MonoScript', dict(zip(('m_AssemblyName', 'm_Namespace', 'm_ClassName'), lighting.HD_LIGHT)))
    for obj in f.objects.values():
        raw = json.dumps(obj.tree, sort_keys=True).encode()
        obj.byte_size = len(raw)
        obj.get_raw_data = mock.Mock(return_value=raw)
    g = ownership.graph(f)
    scripts = ComponentScriptAudit(tuple(f.objects.values()), lambda name: ())
    return f, g, scripts


class LightSourceTests(unittest.TestCase):
    def test_migrated_values_come_from_native_light(self):
        source = native_values()
        result = lighting.stored_light_values(source, {'m_Version': 13, 'm_Intensity': 0.0, 'm_LightUnit': 0})
        self.assertEqual(result['m_Intensity'], 7.5)
        self.assertEqual(result['m_LightUnit'], 4)
        result['m_Color']['r'] = 5.0
        self.assertEqual(source['m_Color']['r'], 0.25)

    def test_missing_native_field_never_falls_back(self):
        for key in lighting.NATIVE_FIELDS:
            value = native_values(); del value[key]
            with self.subTest(field=key), self.assertRaises(HeightmapImportError):
                lighting.stored_light_values(value, {'m_Version': 13, **native_values()})

    def test_migration_version_must_be_qualified(self):
        for version in (None, 12, 14, True, 13.0):
            with self.subTest(version=version), self.assertRaises(HeightmapImportError):
                lighting.stored_light_values(native_values(), {'m_Version': version})

    def test_invalid_values_reject_instead_of_clamping(self):
        for field, value in (('m_Intensity', float('nan')), ('m_Range', float('inf')),
                             ('m_Type', True), ('m_LightUnit', -1), ('m_Enabled', 2),
                             ('m_EnableSpotReflector', 1), ('m_Color', {'r': 1.0})):
            data = native_values(); data[field] = value
            with self.subTest(field=field), self.assertRaises(HeightmapImportError):
                lighting.stored_light_values(data)

    def test_unclamped_finite_source_value_is_retained(self):
        data = native_values(); data['m_Intensity'] = -2.0
        self.assertEqual(lighting.stored_light_values(data)['m_Intensity'], -2.0)

    def test_light_companion_owner_and_originals_survive(self):
        f, graph, scripts = fixture()
        before = {key: obj.get_raw_data() for key, obj in f.objects.items()}
        result = lighting.capture_scene_lights(graph, scripts, PROFILE)
        row, = result['lights']
        self.assertEqual(row['stored_values']['m_Intensity'], 7.5)
        self.assertEqual(row['additional']['tree']['m_Intensity'], 99.0)
        self.assertEqual(row['entity']['identity']['path_id'], '1')
        for key in ('entity', 'transform', 'light', 'additional'):
            record = row[key]; raw = bytes.fromhex(record['raw_hex'])
            self.assertEqual(record['raw_sha256'], hashlib.sha256(raw).hexdigest())
            self.assertEqual(raw, before[int(record['identity']['path_id'])])
        self.assertEqual(result['game_export'], 'NOT_RUN')
        self.assertEqual(row['gpu_light_data'], 'NOT_RUN')
        self.assertEqual(before, {key: obj.get_raw_data() for key, obj in f.objects.items()})

    def test_similarly_named_script_is_not_an_hdrp_companion(self):
        f, graph, scripts = fixture()
        f.objects[9].tree['m_AssemblyName'] = 'Synthetic.Unrelated'
        row, = lighting.capture_scene_lights(graph, scripts, PROFILE)['lights']
        self.assertIsNone(row['additional'])
        self.assertEqual(row['hdrp_companion'], 'NOT_RUN')
        self.assertEqual(len(row['controllers']), 1)

    def test_companion_cannot_bind_through_display_name(self):
        f, graph, scripts = fixture()
        f.objects[4].tree['m_GameObject'] = ownership.pointer(42)
        with self.assertRaisesRegex(HeightmapImportError, 'owner changed'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_changed_source_component_list_rejects(self):
        f, graph, scripts = fixture()
        f.objects[1].tree['m_Component'].pop()
        with self.assertRaisesRegex(HeightmapImportError, 'component list changed'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_malformed_component_entry_is_not_silently_discarded(self):
        f, graph, scripts = fixture(); f.objects[1].tree['m_Component'].append(None)
        with self.assertRaisesRegex(HeightmapImportError, 'component list is malformed'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_ambiguous_companions_reject(self):
        _, graph, scripts = fixture(extra=(5,))
        with self.assertRaisesRegex(HeightmapImportError, 'Multiple HDRP'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_changed_source_script_reference_rejects(self):
        f, graph, scripts = fixture()
        f.objects[4].tree['m_Script'] = ownership.pointer(10)
        with self.assertRaisesRegex(HeightmapImportError, 'script changed'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_unresolved_companion_script_rejects(self):
        f, graph, scripts = fixture()
        f.objects[9].type.name = 'Texture2D'
        with self.assertRaisesRegex(HeightmapImportError, 'could not be resolved'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_source_profile_and_mid_read_changes_reject(self):
        f, graph, scripts = fixture(); f.unity_version = 'unqualified'
        with self.assertRaisesRegex(HeightmapImportError, 'Unity version'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)
        f, graph, scripts = fixture(); obj = f.objects[3]; raw = obj.get_raw_data()
        obj.get_raw_data.side_effect = [raw, raw[:-1] + b'!']
        with self.assertRaisesRegex(HeightmapImportError, 'changed during capture'):
            lighting.capture_scene_lights(graph, scripts, PROFILE)

    def test_stripped_marker_requires_separate_exact_profile(self):
        f, graph, scripts = fixture(); f.unity_version = '0.0.0'
        row, = lighting.capture_scene_lights(graph, scripts, PROFILE)['lights']
        self.assertEqual(row['light']['stored_unity_version'], '0.0.0')
        for profile in ({}, {**PROFILE, 'unity_version': 'unknown'},
                        {**PROFILE, 'hdrp_runtime_sha256': '0' * 64}):
            with self.subTest(profile=profile), self.assertRaisesRegex(HeightmapImportError, 'exact independently'):
                lighting.capture_scene_lights(graph, scripts, profile)

    def test_unqualified_migration_preserves_record_with_explicit_blocker(self):
        f, graph, scripts = fixture(); f.objects[4].tree['m_Version'] = 12
        result = lighting.capture_scene_lights(graph, scripts, PROFILE)
        row, = result['lights']
        self.assertEqual(result['source_capture'], 'PASSED')
        self.assertEqual(result['stored_values_status'], 'PARTIAL')
        self.assertEqual(row['stored_values_status'], 'BLOCKED')
        self.assertEqual(row['migration_required'], 12)
        self.assertIsNone(row['stored_values'])
        self.assertEqual(row['additional']['tree']['m_Version'], 12)

    def test_nonfinite_unknown_controller_data_has_an_explicit_view_tag(self):
        f, graph, scripts = fixture(); f.objects[4].tree['UnknownControllerSentinel'] = float('nan')
        result = lighting.capture_scene_lights(graph, scripts, PROFILE)
        json.dumps(result, allow_nan=False)
        row, = result['lights']
        self.assertIn('$foa_float64_bits', row['additional']['tree']['UnknownControllerSentinel'])
        self.assertEqual(bytes.fromhex(row['additional']['raw_hex']), f.objects[4].get_raw_data())

    def test_bounds_and_cancellation_reject(self):
        for limit in ('MAX_LIGHTS', 'MAX_RECORD_BYTES', 'MAX_CAPTURE_BYTES'):
            _, graph, scripts = fixture()
            with self.subTest(limit=limit), mock.patch.object(lighting, limit, 0), self.assertRaises(HeightmapImportError):
                lighting.capture_scene_lights(graph, scripts, PROFILE)
        _, graph, scripts = fixture()
        with self.assertRaises(HeightmapImportError):
            lighting.capture_scene_lights(graph, scripts, PROFILE, cancelled=lambda: True)


if __name__ == '__main__':
    unittest.main()
