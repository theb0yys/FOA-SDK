# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic exact-field, signedness, binary32 and declaration checks."""
import copy
from pathlib import Path
import struct
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_directional_lights as directional
from foa_heightmap_importer import HeightmapImportError

PROFILE = dict(unity_version=directional.UNITY_VERSION, hdrp_runtime_sha256=directional.HDRP_RUNTIME_SHA256)


def vector(*values):
    return dict(zip('xyzw', values))


def record():
    return dict(positionRWS=vector(1., 2., 3.), lightLayers=0xffffffff,
                forward=vector(4., 5., 6.), cookieMode=2, cookieScaleOffset=vector(7., 8., 9., 10.),
                right=vector(11., 12., 13.), shadowIndex=-1, up=vector(14., 15., 16.), contactShadowIndex=-2,
                color=vector(17., 18., 19.), contactShadowMask=0x7fffffff, shadowTint=vector(20., 21., 22.),
                shadowDimmer=23., volumetricShadowDimmer=24., nonLightMappedOnly=1, minRoughness=25.,
                screenSpaceShadowIndex=255, shadowMaskSelector=vector(26., 27., 28., 29.),
                diffuseDimmer=30., specularDimmer=31., lightDimmer=32., volumetricLightDimmer=33.,
                penumbraTint=34., isRayTracedContactShadow=35., angularDiameter=36., distanceFromCamera=-37.)


class DirectionalLightTests(unittest.TestCase):
    def test_all_words_and_signed_integer_fields(self):
        words = [1., 2., 3., 0xffffffff, 4., 5., 6., 2, 7., 8., 9., 10., 11., 12., 13., 0xffffffff,
                 14., 15., 16., 0xfffffffe, 17., 18., 19., 0x7fffffff, 20., 21., 22., 23., 24., 1,
                 25., 255, 26., 27., 28., 29., 30., 31., 32., 33., 34., 35., 36., -37.]
        expected = b''.join(struct.pack('<f' if type(value) is float else '<I', value) for value in words)
        self.assertEqual(len(expected), 176)
        self.assertEqual(directional.pack_directional_lights([record()], PROFILE), expected)

    def test_order_and_source_are_preserved(self):
        first, second = record(), record(); second['color']['x'] = -0.0
        originals = copy.deepcopy([first, second])
        packed = directional.pack_directional_lights([first, second], PROFILE)
        self.assertEqual(packed[176:], directional.pack_directional_lights([second], PROFILE))
        self.assertEqual(struct.unpack_from('<I', packed, 176 + 80)[0], 0x80000000)
        self.assertEqual([first, second], originals)

    def test_no_missing_or_unknown_fields(self):
        for name in record():
            value = record(); del value[name]
            with self.subTest(name=name), self.assertRaises(HeightmapImportError):
                directional.pack_directional_lights([value], PROFILE)
        value = record(); value['invented'] = 0
        with self.assertRaises(HeightmapImportError): directional.pack_directional_lights([value], PROFILE)

    def test_vectors_require_exact_components(self):
        for value in ([1., 2., 3.], dict(r=1., g=2., b=3.), vector(1., 2.), vector(1., 2., 3., 4.)):
            data = record(); data['positionRWS'] = value
            with self.subTest(value=value), self.assertRaises(HeightmapImportError):
                directional.pack_directional_lights([data], PROFILE)

    def test_integers_never_go_through_float_conversion(self):
        for name, value in (('lightLayers', -1), ('lightLayers', 0x100000000), ('lightLayers', True),
                            ('cookieMode', 1.0), ('shadowIndex', -0x80000001), ('contactShadowMask', 0x80000000)):
            data = record(); data[name] = value
            with self.subTest(name=name, value=value), self.assertRaises(HeightmapImportError):
                directional.pack_directional_lights([data], PROFILE)

    def test_malformed_floats_are_not_clamped(self):
        for value in (1, True, '1', float('nan'), float('inf'), -float('inf'), 1e39):
            for name in ('angularDiameter', 'color'):
                data = record(); data[name] = vector(value, 1., 2.) if name == 'color' else value
                with self.subTest(name=name, value=value), self.assertRaises(HeightmapImportError):
                    directional.pack_directional_lights([data], PROFILE)

    def test_count_and_profile_bounds(self):
        for records in ([], (), {}, [None]):
            with self.subTest(records=records), self.assertRaises(HeightmapImportError):
                directional.pack_directional_lights(records, PROFILE)
        with mock.patch.object(directional, 'MAX_LIGHTS', 1), self.assertRaises(HeightmapImportError):
            directional.pack_directional_lights([record(), record()], PROFILE)
        for profile in ({}, {**PROFILE, 'unity_version': 'unknown'}, {**PROFILE, 'hdrp_runtime_sha256': '0'*64}):
            with self.assertRaises(HeightmapImportError): directional.pack_directional_lights([record()], profile)

    def test_shader_binding_requires_explicit_matching_declaration(self):
        declaration = dict(slot=3, type=2, stride=176)
        result = directional.directional_shader_buffer([record()], declaration, PROFILE)
        self.assertEqual(result, {**declaration, 'hex': directional.pack_directional_lights([record()], PROFILE).hex()})
        self.assertEqual(declaration, dict(slot=3, type=2, stride=176))
        for changed in ({}, {**declaration, 'slot': True}, {**declaration, 'slot': 128},
                        {**declaration, 'type': 4}, {**declaration, 'stride': 172}, {**declaration, 'extra': 0}):
            with self.subTest(declaration=changed), self.assertRaises(HeightmapImportError):
                directional.directional_shader_buffer([record()], changed, PROFILE)


if __name__ == '__main__':
    unittest.main()
