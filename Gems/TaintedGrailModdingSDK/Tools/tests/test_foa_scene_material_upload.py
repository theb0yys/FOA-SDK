# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Source-bound material upload tests; fixtures contain only synthetic data."""
from copy import deepcopy
from dataclasses import replace
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_material_upload as u
from foa_heightmap_importer import HeightmapImportError

KEY = '1' * 32


def binding():
    props = {}
    for name, kind, value in (('_Color', 'Color', dict(zip('rgba', (.25, .5, 2., .375)))),
                              ('_Float', 'Float', -0.), ('_Vector', 'Vector', dict(zip('rgba', (-2., 1.5, .125, -.25))))):
        props[name] = {'type': kind, 'flags': 0, 'saved': True, 'value_origin': 'material', 'value': value}
    props['_Texture'] = {'type': 'Texture', 'texture': {'scale': (-2., 1.5), 'offset': (.125, -.25)}}
    return {'schema': 'foa-source-material-binding', 'version': 1, 'source_binding': 'PASSED',
            'shader_reference': ['cab-' + '2'*32, -123], 'properties': props}


def field(name, offset, columns=4, kind=0):
    return {'name': name, 'type': kind, 'rows': 1, 'columns': columns, 'matrix': False,
            'array_size': 0, 'index': offset, 'bytes': columns * 4}


def layout():
    values = [field('_Color', 0), field('_Float', 16, 1), field('_Vector', 32),
              field('_Texture_ST', 48), field('_Controller', 64, 1)]
    return {'size': 80, 'fields': {f['name']: f for f in values}}


def receipt(request):
    result = {'schema': 'foa-material-upload-receipt', 'version': 1, 'profile': u.PROFILE,
              'color_space': request['color_space'], 'input_sha256': u.digest(request), 'status': 'PASSED',
              'api': 'Direct3D11', 'threading': 'Direct', 'shader_count': 1, 'materials': []}
    for row in request['materials']:
        properties = [{'name': p['name'], 'hex': p['hex']} for p in row['properties']]
        # Independent synthetic native measurement: the first float differs from
        # both a raw .25 and the tested CPU Color.linear implementation.
        properties[0]['hex'] = '7263503d9a2d5b3e8d0893400000c03e'
        result['materials'].append({'key': row['key'], 'source_sha256': row['source_sha256'], 'properties': properties})
    return result


class MaterialUploadTests(unittest.TestCase):
    def setup_data(self):
        b = binding(); request = u.upload_request([(KEY, b)], color_space='Linear')
        measured = u.read_uploads(request, receipt(request))[KEY]
        return b, request, measured

    def test_input_bytes_identity_and_signed_zero(self):
        b, request, _ = self.setup_data()
        row = request['materials'][0]
        self.assertEqual(row['properties'][1]['hex'], '00000080')
        self.assertEqual(row['properties'][0]['hex'], '0000803e0000003f000000400000c03e')
        self.assertEqual(request, u.upload_request([(KEY, deepcopy(b))], color_space='Linear'))

    def test_exact_upload_bits_st_and_explicit_global(self):
        b, _, measured = self.setup_data(); before = deepcopy(b)
        raw = u.pack_material_constants(layout(), KEY, b, measured, color_space='Linear', explicit_values={'_Controller': 7.5})
        self.assertEqual(raw[:16].hex(), '7263503d9a2d5b3e8d0893400000c03e')
        self.assertEqual(raw[16:20].hex(), '00000080')
        self.assertEqual(raw[48:64], struct.pack('<4f', -2., 1.5, .125, -.25))
        self.assertEqual(raw[64:68], struct.pack('<f', 7.5))
        self.assertEqual(raw[20:32] + raw[68:80], bytes(24))
        self.assertEqual(b, before)

    def test_float3_uniform_uses_exact_measured_prefix(self):
        b, _, measured = self.setup_data()
        raw = u.pack_material_constants({'size': 16, 'fields': {'_Color': field('_Color', 0, 3)}}, KEY, b, measured,
                                        color_space='Linear', explicit_values={})
        self.assertEqual(raw[:12].hex(), '7263503d9a2d5b3e8d089340')
        self.assertEqual(raw[12:], bytes(4))

    def test_stale_material_value_shader_or_source_key_rejects(self):
        b, _, measured = self.setup_data()
        for change in ('value', 'shader', 'key'):
            altered = deepcopy(b); key = KEY
            if change == 'value': altered['properties']['_Color']['value']['r'] = .5
            if change == 'shader': altered['shader_reference'][1] += 1
            if change == 'key': key = '3' * 32
            with self.subTest(change=change), self.assertRaises(HeightmapImportError):
                u.pack_material_constants(layout(), key, altered, measured, color_space='Linear', explicit_values={'_Controller': 0})

    def test_color_space_cannot_be_reused(self):
        b, request, measured = self.setup_data()
        with self.assertRaises(HeightmapImportError):
            u.pack_material_constants(layout(), KEY, b, measured, color_space='Gamma', explicit_values={'_Controller': 0})
        request['color_space'] = 'Gamma'
        self.assertNotEqual(u.digest(request), receipt(u.upload_request([(KEY, b)], color_space='Linear'))['input_sha256'])

    def test_missing_extra_and_overridden_constants_reject(self):
        b, _, measured = self.setup_data()
        for explicit in ({}, {'_Controller': 0, 'extra': 0}, {'_Controller': 0, '_Color': (0,0,0,1)}, {'_Controller': 0, '_Texture_ST': (1,1,0,0)}):
            with self.subTest(explicit=explicit), self.assertRaises(HeightmapImportError):
                u.pack_material_constants(layout(), KEY, b, measured, color_space='Linear', explicit_values=explicit)

    def test_uniform_integer_array_and_width_mismatch_reject(self):
        b, _, measured = self.setup_data()
        for name, columns, kind, array in (('_Color', 4, 1, 0), ('_Float', 2, 0, 0), ('_Color', 4, 0, 1)):
            f = field(name, 0, columns, kind); f['array_size'] = array
            with self.subTest(name=name, columns=columns, kind=kind, array=array), self.assertRaises(HeightmapImportError):
                u.pack_material_constants({'size': 16, 'fields': {name: f}}, KEY, b, measured, color_space='Linear', explicit_values={})

    def test_numeric_defaults_are_not_treated_as_saved_uploads(self):
        b = binding(); b['properties']['_Color'].update(saved=False, value_origin='shader-default')
        with self.assertRaises(HeightmapImportError): u.upload_request([(KEY, b)], color_space='Linear')

    def test_unknown_flags_and_types_reject(self):
        for kind, flags in (('Color', 32), ('Color', 512), ('Vector', 16), ('Float', True), ('Int', 0)):
            b = binding(); b['properties']['_Color'].update(type=kind, flags=flags)
            with self.subTest(kind=kind, flags=flags), self.assertRaises(HeightmapImportError): u.upload_request([(KEY, b)], color_space='Linear')

    def test_nonfinite_input_and_unqualified_space_reject(self):
        for value in (float('nan'), float('inf'), 1e100):
            b = binding(); b['properties']['_Float']['value'] = value
            with self.assertRaises(HeightmapImportError): u.upload_request([(KEY, b)], color_space='Linear')
        with self.assertRaises(HeightmapImportError): u.upload_request([(KEY, binding())], color_space='unknown')

    def test_missing_duplicate_and_excessive_materials_reject(self):
        for materials in ([], [(KEY, binding())]*2, [(KEY, binding())]*1025):
            with self.assertRaises(HeightmapImportError): u.upload_request(materials, color_space='Linear')

    def test_wrong_profile_status_request_bytes_and_threading_reject(self):
        _, request, _ = self.setup_data()
        for key, value in (('profile', 'other'), ('status', 'PARTIAL'), ('input_sha256', '0'*64),
                           ('threading', 'MultiThreaded'), ('api', 'Direct3D12'), ('color_space', 'Gamma'), ('version', True)):
            bad = receipt(request); bad[key] = value
            with self.subTest(key=key), self.assertRaises(HeightmapImportError): u.read_uploads(request, bad)

    def test_changed_order_identity_count_and_names_reject(self):
        _, request, _ = self.setup_data()
        for mode in range(6):
            bad = receipt(request); row = bad['materials'][0]
            if mode == 0: row['key'] = '2'*32
            if mode == 1: row['source_sha256'] = '0'*64
            if mode == 2: row['properties'].pop()
            if mode == 3: row['properties'].reverse()
            if mode == 4: row['properties'][0]['name'] = '_Other'
            if mode == 5: bad['materials'].append(deepcopy(row))
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): u.read_uploads(request, bad)

    def test_malformed_nonfinite_and_wrong_width_bytes_reject(self):
        _, request, _ = self.setup_data()
        for value in ('', 'AB'*16, '0'*31, 'g'*32, '0000807f'*4, '0100c07f'*4):
            bad = receipt(request); bad['materials'][0]['properties'][0]['hex'] = value
            with self.subTest(value=value), self.assertRaises(HeightmapImportError): u.read_uploads(request, bad)

    def test_immutable_upload_shape_is_revalidated(self):
        b, _, measured = self.setup_data()
        for values in (measured.values[:-1], measured.values + measured.values[:1], (('_Color', b'bad'),) + measured.values[1:]):
            with self.assertRaises(HeightmapImportError):
                u.pack_material_constants(layout(), KEY, b, replace(measured, values=values), color_space='Linear', explicit_values={'_Controller': 0})

    def test_request_schema_and_property_names_reject(self):
        _, request, _ = self.setup_data()
        for mode in range(4):
            bad = deepcopy(request)
            if mode == 0: bad['version'] = True
            if mode == 1: bad['materials'][0]['properties'][1]['name'] = '_Color'
            if mode == 2: bad['materials'][0]['properties'][0]['hex'] = '0000807f'*4
            if mode == 3: bad['materials'][0]['properties'][0]['extra'] = 0
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): u.read_uploads(bad, receipt(bad))

    def test_texture_st_change_does_not_require_unrelated_color_measurement(self):
        b, _, measured = self.setup_data(); b['properties']['_Texture']['texture']['offset'] = (.75, -.5)
        raw = u.pack_material_constants(layout(), KEY, b, measured, color_space='Linear', explicit_values={'_Controller': 0})
        self.assertEqual(raw[48:64], struct.pack('<4f', -2, 1.5, .75, -.5))


if __name__ == '__main__':
    unittest.main()
