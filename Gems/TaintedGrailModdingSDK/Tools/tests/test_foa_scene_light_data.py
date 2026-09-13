# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independent ABI words, full-field preservation and malformed-input rejection."""
import copy
from pathlib import Path
import struct
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_light_data as lights
from foa_heightmap_importer import HeightmapImportError

PROFILE = dict(unity_version=lights.UNITY_VERSION, hdrp_runtime_sha256=lights.HDRP_RUNTIME_SHA256)


def vec(*values):
    return dict(zip('xyzw', values))


def record():
    return dict(positionRWS=vec(1., 2., 3.), lightLayers=0xfedcba98,
                lightDimmer=5., volumetricLightDimmer=6., angleScale=7., angleOffset=8.,
                forward=vec(9., 10., 11.), iesCut=12., lightType=13, right=vec(14., 15., 16.),
                penumbraTint=17., range=18., cookieMode=19, shadowIndex=-20, up=vec(21., 22., 23.),
                rangeAttenuationScale=24., color=vec(25., 26., 27.), rangeAttenuationBias=28.,
                cookieScaleOffset=vec(29., 30., 31., 32.), shadowTint=vec(33., 34., 35.),
                shadowDimmer=36., volumetricShadowDimmer=37., nonLightMappedOnly=38, minRoughness=39.,
                screenSpaceShadowIndex=-40, shadowMaskSelector=vec(41., 42., 43., 44.),
                size=vec(45., 46., 47., 48.), contactShadowMask=-49, diffuseDimmer=50., specularDimmer=51.,
                __unused__=52., padding=vec(53., 54.), isRayTracedContactShadow=55., boxLightSafeExtent=56.)


class LightDataTests(unittest.TestCase):
    def test_independent_word_order_and_signedness(self):
        words = [struct.pack('<f', float(n)) for n in range(1, 57)]
        for index, value in ((3, 0xfedcba98), (12, 13), (18, 19), (19, -20), (37, 38), (39, -40), (48, -49)):
            words[index] = struct.pack('<I', value & 0xffffffff)
        self.assertEqual(lights.pack_light_data([record()], PROFILE), b''.join(words))
        self.assertEqual(len(b''.join(words)), 224)

    def test_order_padding_signed_zero_and_subnormal_are_preserved(self):
        first, second = record(), record()
        second['padding'] = vec(-0., struct.unpack('<f', bytes.fromhex('01000000'))[0])
        original = copy.deepcopy([first, second]); payload = lights.pack_light_data(original, PROFILE)
        self.assertEqual(payload[224:], lights.pack_light_data([second], PROFILE))
        self.assertEqual(payload[432:440], bytes.fromhex('0000008001000000'))
        self.assertEqual(original, [first, second])
        self.assertEqual(struct.unpack_from('<f', payload, 204)[0], 52.)

    def test_every_field_required_and_unknown_fields_rejected(self):
        for name in record():
            value = record(); del value[name]
            with self.subTest(name=name), self.assertRaises(HeightmapImportError): lights.pack_light_data([value], PROFILE)
        value = record(); value['invented'] = 0.
        with self.assertRaises(HeightmapImportError): lights.pack_light_data([value], PROFILE)

    def test_vector_dimensions_and_axes_are_exact(self):
        for field in ('padding', 'color', 'size'):
            for value in (None, [], [1., 2.], {'r': 1., 'g': 2.}, {}, {'x': 1., 'y': 2., 'z': 3., 'w': 4., 'extra': 5.}):
                data = record(); data[field] = value
                with self.subTest(field=field, value=value), self.assertRaises(HeightmapImportError): lights.pack_light_data([data], PROFILE)

    def test_floats_do_not_accept_bools_integers_or_nonfinite_values(self):
        for value in (True, 1, '1', float('nan'), float('inf'), -float('inf'), 1e39):
            for field in ('range', 'padding'):
                data = record(); data[field] = vec(value, 0.) if field == 'padding' else value
                with self.subTest(field=field, value=value), self.assertRaises(HeightmapImportError): lights.pack_light_data([data], PROFILE)

    def test_integer_ranges_and_types(self):
        for field, value in (('lightLayers', -1), ('lightLayers', 2**32), ('lightLayers', True),
                             ('shadowIndex', 1.), ('cookieMode', 2**31), ('lightType', -2**31-1)):
            data = record(); data[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(HeightmapImportError): lights.pack_light_data([data], PROFILE)

    def test_count_and_profile_reject_before_record_work(self):
        for value in ([], (), {}, [None], [record()] * 4097):
            with self.subTest(kind=type(value).__name__), self.assertRaises(HeightmapImportError): lights.pack_light_data(value, PROFILE)
        with mock.patch.object(lights.struct, 'pack_into') as pack:
            with self.assertRaises(HeightmapImportError): lights.pack_light_data([record()] * 4097, PROFILE)
            pack.assert_not_called()
        for profile in ({}, {**PROFILE, 'unity_version': 'unknown'}, {**PROFILE, 'hdrp_runtime_sha256': '0'*64}, {**PROFILE, 'extra': 1}):
            with self.assertRaises(HeightmapImportError): lights.pack_light_data([record()], profile)

    def test_matching_resource_declaration_and_maximum_count(self):
        declaration = dict(slot=127, type=2, stride=224)
        output = lights.light_data_shader_buffer([record()], declaration, PROFILE)
        self.assertEqual(output, {**declaration, 'hex': lights.pack_light_data([record()], PROFILE).hex()})
        self.assertEqual(len(lights.pack_light_data([record()] * 4096, PROFILE)), 917504)
        for bad in ({}, {**declaration, 'slot': True}, {**declaration, 'slot': 128}, {**declaration, 'slot': -1},
                    {**declaration, 'type': 4}, {**declaration, 'stride': 176}, {**declaration, 'extra': 0}):
            with self.subTest(declaration=bad), self.assertRaises(HeightmapImportError): lights.light_data_shader_buffer([record()], bad, PROFILE)


    def test_explicit_nonfinite_bits_preserve_payload_and_reject_ambiguous_tags(self):
        values = record()
        values['size']['z'] = {'$foa_float32_bits': '4523c17f'}
        values['range'] = {'$foa_float32_bits': '000080ff'}
        values['iesCut'] = {'$foa_float32_bits': '0000807f'}
        packed = lights.pack_light_data([values], PROFILE)
        self.assertEqual(packed[184:188], bytes.fromhex('4523c17f'))
        self.assertEqual(packed[68:72], bytes.fromhex('000080ff'))
        self.assertEqual(packed[44:48], bytes.fromhex('0000807f'))
        for tag in ({}, {'$foa_float32_bits': '0000803f'}, {'$foa_float32_bits': '0000807F'},
                    {'$foa_float32_bits': '0000807f', 'extra': 0}, {'$foa_float32_bits': '0000807'},
                    {'$foa_float32_bits': 0}, {'$foa_float32_bits': 'zzzzzzzz'}):
            values['size']['z'] = tag
            with self.subTest(tag=tag), self.assertRaises(HeightmapImportError):
                lights.pack_light_data([values], PROFILE)


if __name__ == '__main__':
    unittest.main()
