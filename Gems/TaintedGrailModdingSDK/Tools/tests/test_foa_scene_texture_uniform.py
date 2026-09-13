# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic source/consumer cases for qualified texture uniform packing."""
from copy import deepcopy
from dataclasses import replace
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_heightmap_importer import HeightmapImportError
import foa_scene_material_upload as numeric
import foa_scene_texture_uniform as t

KEY = '1' * 32
REF = ('cab-' + '2' * 32, 27)


def resource():
    tree = dict(m_TextureDimension=2, m_ColorSpace=0, m_Width=8, m_Height=4, m_MipCount=4, m_TextureFormat=3)
    return t.source_resource(REF, tree, bundle_sha256='3' * 64, record_sha256='4' * 64, asset_path='assets/synthetic.asset')


def binding(reference=None):
    default = {'m_DefaultName': 'black', 'm_TexDim': 2}
    prop = {'type': 'Texture', 'flags': 0, 'saved': True,
            'declaration': {'m_Name': '_HeightMap', 'm_Type': 4, 'm_Flags': 0, 'm_Attributes': [], 'm_DefTexture': default},
            'texture': {'reference': reference, 'saved_reference': reference, 'default': default,
                        'value_origin': 'material' if reference else 'shader-default-unresolved', 'scale': [1, 1], 'offset': [0, 0]}}
    return {'schema': 'foa-source-material-binding', 'version': 1, 'source_binding': 'PASSED', 'shader_reference': ['cab-' + '5' * 32, 19],
            'properties': {'_Value': {'type': 'Float', 'flags': 0, 'saved': True, 'value_origin': 'material', 'value': .25}, '_HeightMap': prop}}


def field(name, offset, columns):
    return dict(name=name, type=0, rows=1, columns=columns, matrix=False, array_size=0, index=offset, bytes=columns * 4)


def layout():
    return {'size': 32, 'fields': {'_Value': field('_Value', 0, 1), '_HeightMap_TexelSize': field('_HeightMap_TexelSize', 16, 4)}}


def measurement(key, operation, shape, value, dimensions):
    return dict(key=key, operation=operation, width=shape[0], height=shape[1], mip_count=shape[2], format=shape[3],
                hex=struct.pack('<4f', *value).hex(), dimensions_hex=struct.pack('<4f', *dimensions).hex())


def receipt(request):
    result = dict(schema='foa-texture-uniform-receipt', version=1, profile=t.PROFILE, color_space=request['color_space'],
                  mip_limit=0, streaming=False, input_sha256=t.digest(request), status='PASSED', api='Direct3D11', threading='Direct',
                  resources=[], controls=[])
    for operation in t.CONTROL_OPERATIONS:
        control = operation in ('non-square', 'non-square-st')
        result['controls'].append(measurement('control', operation, (7, 3, 1, 4) if control else (0, 0, 0, 0),
                                             (1/7, 1/3, 7, 3) if control else (1, 1, 1, 1), (7, 3, 1, 1) if control else (4, 4, 1, 1)))
    for r in request['resources']:
        default = r['kind'] == 'black-default'
        result['resources'].append(measurement(r['key'], 'material', tuple(r[k] for k in ('width', 'height', 'mip_count', 'format')),
                                              (1, 1, 1, 1) if default else (.125, .25, 8, 4), (4, 4, 1, 1) if default else (8, 4, 4, 1)))
    return result


class TextureUniformTests(unittest.TestCase):
    def setup_data(self):
        resources = [t.black_default(), resource()]
        request = t.uniform_request(resources, color_space='Linear', mip_limit=0, streaming=False)
        return request, t.read_uniforms(request, receipt(request))

    def pack(self, b, uploads, **options):
        source = numeric.material_request(KEY, b)
        measured = numeric.MaterialUpload(KEY, source['source_sha256'], 'Linear', (('_Value', struct.pack('<f', .25)),))
        args = dict(texture_resources={REF: resource()}, texture_uploads=uploads, color_space='Linear', mip_limit=0,
                    streaming=False, explicit_values={})
        args.update(options)
        target = args.pop('layout', layout())
        return t.pack_textured_material_constants(target, KEY, b, measured, **args)

    def test_null_uniform_is_not_derived_from_fallback_resource_dimensions(self):
        request, uploads = self.setup_data(); default = uploads[request['resources'][0]['key']]
        self.assertEqual(struct.unpack('<4f', default.dimensions), (4, 4, 1, 1))
        data = self.pack(binding(), uploads)
        self.assertEqual(data[16:], struct.pack('<4f', 1, 1, 1, 1))
        self.assertNotEqual(data[16:], struct.pack('<4f', .25, .25, 4, 4))

    def test_assigned_source_bytes_and_numeric_padding_are_preserved(self):
        _, uploads = self.setup_data(); b = binding(REF); before = deepcopy(b)
        data = self.pack(b, uploads)
        self.assertEqual(data, struct.pack('<f', .25) + bytes(12) + struct.pack('<4f', .125, .25, 8, 4))
        self.assertEqual(b, before)

    def test_descriptor_and_request_are_deterministic(self):
        request, _ = self.setup_data()
        self.assertEqual(t.canonical(request), t.canonical(t.uniform_request(deepcopy(request['resources']), color_space='Linear', mip_limit=0, streaming=False)))

    def test_stale_record_bundle_or_view_cannot_reuse_upload(self):
        _, uploads = self.setup_data()
        for name, value in (('record_sha256', '6'*64), ('bundle_sha256', '7'*64), ('width', 16), ('asset_path', 'assets/other.asset')):
            r = resource(); r[name] = value; r['key'] = t.digest({k: v for k, v in r.items() if k != 'key'})
            with self.subTest(name=name), self.assertRaises(HeightmapImportError): self.pack(binding(REF), uploads, texture_resources={REF: r})

    def test_changed_reference_missing_resource_and_source_mismatch_reject(self):
        _, uploads = self.setup_data()
        for b, resources in ((binding((REF[0], 28)), {REF: resource()}), (binding(REF), {}), (binding((REF[0], 28)), {(REF[0], 28): resource()})):
            with self.assertRaises(HeightmapImportError): self.pack(b, uploads, texture_resources=resources)

    def test_flags_declaration_defaults_and_override_origin_require_new_proof(self):
        _, uploads = self.setup_data()
        for mode in range(7):
            b = binding(); p = b['properties']['_HeightMap']
            if mode == 0: p['flags'] = 64
            if mode == 1: p['declaration']['m_Type'] = 3
            if mode == 2: p['declaration']['m_Attributes'] = ['NoScaleOffset']
            if mode == 3: p['texture']['default']['m_DefaultName'] = 'white'
            if mode == 4: p['texture']['default']['m_TexDim'] = 5
            if mode == 5: p['texture']['value_origin'] = 'shader-owned'
            if mode == 6: b['properties']['_HeightMap_TexelSize'] = {'type': 'Vector'}
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): self.pack(b, uploads)

    def test_streaming_mip_limit_unknown_space_and_reused_color_space_reject(self):
        _, uploads = self.setup_data()
        for options in (dict(streaming=True), dict(mip_limit=1), dict(mip_limit=False), dict(color_space='unknown'), dict(color_space='Gamma')):
            with self.subTest(options=options), self.assertRaises(HeightmapImportError): self.pack(binding(), uploads, **options)

    def test_explicit_override_and_incomplete_uniform_shape_reject(self):
        _, uploads = self.setup_data()
        with self.assertRaises(HeightmapImportError): self.pack(binding(), uploads, explicit_values={'_HeightMap_TexelSize': (1, 1, 1, 1)})
        for change in (dict(columns=3, bytes=12), dict(type=1), dict(array_size=1)):
            target = layout(); target['fields']['_HeightMap_TexelSize'].update(change)
            with self.assertRaises(HeightmapImportError): self.pack(binding(), uploads, layout=target)

    def test_layout_without_texture_uniform_retains_numeric_contract(self):
        _, uploads = self.setup_data(); target = {'size': 16, 'fields': {'_Value': field('_Value', 0, 1)}}
        self.assertEqual(self.pack(binding(), {}, layout=target, texture_resources={}), struct.pack('<f', .25) + bytes(12))

    def test_request_bounds_duplicates_versions_and_hashes_reject(self):
        request, _ = self.setup_data()
        for resources in ([], [t.black_default()]*65, [resource(), resource()]):
            with self.assertRaises(HeightmapImportError): t.uniform_request(resources, color_space='Linear', mip_limit=0, streaming=False)
        for field_name, value in (('version', True), ('version', 2), ('profile', 'other'), ('extra', 0)):
            bad = deepcopy(request); bad[field_name] = value
            with self.assertRaises(HeightmapImportError): t.read_uniforms(bad, receipt(bad))
        stale = resource(); stale['record_sha256'] = '6'*64
        with self.assertRaises(HeightmapImportError): t.uniform_request([stale], color_space='Linear', mip_limit=0, streaming=False)

    def test_unknown_texture_shapes_formats_and_default_asset_fields_reject(self):
        for name, value in (('width', True), ('height', 0), ('mip_count', 5), ('format', 10), ('reference', [REF[0], 0])):
            r = resource(); r[name] = value; r['key'] = t.digest({k: v for k, v in r.items() if k != 'key'})
            with self.subTest(name=name), self.assertRaises(HeightmapImportError): t.uniform_request([r], color_space='Linear', mip_limit=0, streaming=False)
        r = t.black_default(); r['width'] = 4; r['key'] = t.digest({k: v for k, v in r.items() if k != 'key'})
        with self.assertRaises(HeightmapImportError): t.uniform_request([r], color_space='Linear', mip_limit=0, streaming=False)

    def test_incomplete_or_changed_calibration_cannot_qualify_receipt(self):
        request, _ = self.setup_data()
        for mode in range(4):
            bad = receipt(request)
            if mode == 0: bad['controls'].pop()
            if mode == 1: bad['controls'].reverse()
            if mode == 2: bad['controls'][0]['hex'] = struct.pack('<4f', .25, .25, 4, 4).hex()
            if mode == 3: bad['controls'][3]['dimensions_hex'] = struct.pack('<4f', 3, 7, 1, 1).hex()
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): t.read_uniforms(request, bad)

    def test_failed_mismatched_host_or_input_receipts_reject(self):
        request, _ = self.setup_data()
        for field_name, value in (('status', 'PARTIAL'), ('input_sha256', '0'*64), ('api', 'Direct3D12'), ('threading', 'MultiThreaded'), ('color_space', 'Gamma'), ('version', True), ('mip_limit', 1), ('streaming', True)):
            bad = receipt(request); bad[field_name] = value
            with self.subTest(field_name=field_name), self.assertRaises(HeightmapImportError): t.read_uniforms(request, bad)

    def test_resource_identity_count_and_gpu_view_mismatches_reject(self):
        request, _ = self.setup_data()
        for mode in range(6):
            bad = receipt(request)
            if mode == 0: bad['resources'].reverse()
            if mode == 1: bad['resources'].pop()
            if mode == 2: bad['resources'][1]['width'] = 16
            if mode == 3: bad['resources'][1]['dimensions_hex'] = struct.pack('<4f', 4, 2, 3, 1).hex()
            if mode == 4: bad['resources'][1]['hex'] = struct.pack('<4f', .125, -.25, 8, 4).hex()
            if mode == 5: bad['resources'][0]['hex'] = struct.pack('<4f', .25, .25, 4, 4).hex()
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): t.read_uniforms(request, bad)

    def test_nonfinite_wrong_size_and_invalid_hex_reject(self):
        request, _ = self.setup_data()
        for value in ('', 'AB'*16, '0'*31, 'z'*32, '0000807f'*4):
            bad = receipt(request); bad['resources'][0]['hex'] = value
            with self.assertRaises(HeightmapImportError): t.read_uniforms(request, bad)

    def test_cached_upload_revalidates_type_bytes_and_dimensions(self):
        request, uploads = self.setup_data(); key = request['resources'][0]['key']
        for change in (dict(value=bytes(16)), dict(value=b'bad'), dict(dimensions=bytes(16)), dict(resource_sha256='0'*64), dict(value=bytearray(16))):
            altered = dict(uploads); altered[key] = replace(uploads[key], **change)
            with self.subTest(change=change), self.assertRaises(HeightmapImportError): self.pack(binding(), altered)

    def test_missing_texture_property_and_measurement_reject(self):
        _, uploads = self.setup_data(); b = binding(); del b['properties']['_HeightMap']
        with self.assertRaises(HeightmapImportError): self.pack(b, uploads)
        with self.assertRaises(HeightmapImportError): self.pack(binding(), {})


if __name__ == '__main__':
    unittest.main()
