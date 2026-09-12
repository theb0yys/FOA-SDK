# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Structural packet tests; actual shader compilation/rendering needs native cases."""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_native_shader as native
from foa_heightmap_importer import HeightmapImportError


def structural_program(vertex, semantic=None):
    name = (semantic or ('POSITION' if vertex else 'SV_Target')).encode()+b'\0'
    name += b'\0'*(-len(name) % 4)
    signature = struct.pack('<II5IBBH', 1, 8, 32, 0, 0 if vertex else 64, 3, 0, 7 if vertex else 15, 7, 0)+name
    chunks = [(b'ISGN' if vertex else b'OSGN', signature),
              (b'SHEX', struct.pack('<II', (0x10000 if vertex else 0) | 0x50, 2))]
    body = b''.join(tag+struct.pack('<I', len(data))+data for tag, data in chunks)
    offset = 40
    return b'DXBC'+bytes(16)+struct.pack('<5I', 1, 40+len(body), 2, offset, offset+8+len(signature))+body


def pair():
    return [{'stage': stage, 'code': structural_program(stage == 'vertex'),
             'constant_buffers': [{'slot': 0, 'size': size}], 'images': [], 'samplers': []}
            for stage, size in (('vertex', 64), ('fragment', 16))]


def state():
    return dict(zip(native.STATE_FIELDS, (0, 0, 0, 7, 0, 1, 0, 0, 1, 0, 0, 15)))


class NativeShaderPacketTests(unittest.TestCase):
    def test_exact_programs_and_stage_local_slots_survive(self):
        stages = pair(); original = copy.deepcopy(stages)
        data = native.encode(stages, state=state(), draw_list='auxgeom')
        self.assertEqual(stages, original)
        self.assertEqual(data[:8], b'FOASHD01')
        self.assertEqual(struct.unpack_from('<4I', data, 8), (1, 1, len(data)-56, 0))
        self.assertEqual(data[24:56], hashlib.sha256(data[:24]+data[56:]).digest())
        self.assertIn(stages[0]['code'], data); self.assertIn(stages[1]['code'], data)
        self.assertEqual(native.program_contract(stages[0]['code'], 'vertex')['input_channels'][0]['components'], 3)
        self.assertEqual(native.program_contract(stages[1]['code'], 'fragment')['color_outputs'], [4])

    def test_no_implicit_render_policy(self):
        for mutation in (lambda v: v.pop('depth_write'), lambda v: v.update(depth_func=True),
                         lambda v: v.update(cull=3), lambda v: v.update(unknown=1)):
            values = state(); mutation(values)
            with self.assertRaises(HeightmapImportError): native.encode(pair(), state=values, draw_list='auxgeom')

    def test_wrong_stage_and_truncated_code_reject(self):
        for mutation in (lambda s: s[0].update(code=s[1]['code']), lambda s: s[1].update(code=s[1]['code'][:-1]),
                         lambda s: s.reverse()):
            stages = pair(); mutation(stages)
            with self.assertRaises(HeightmapImportError): native.encode(stages, state=state(), draw_list='auxgeom')

    def test_duplicate_or_unqualified_resources_reject(self):
        for mutation in (lambda s: s[0]['constant_buffers'].append({'slot': 0, 'size': 16}),
                         lambda s: s[0]['constant_buffers'][0].update(size=17),
                         lambda s: s[1]['images'].append({'slot': 128, 'type': 3}),
                         lambda s: s[1]['samplers'].append({'slot': True}),
                         lambda s: s[0].update(structured_buffers=[])):
            stages = pair(); mutation(stages)
            with self.assertRaises(HeightmapImportError): native.encode(stages, state=state(), draw_list='auxgeom')

    def test_unknown_profile_and_invalid_draw_list_reject(self):
        with self.assertRaises(HeightmapImportError): native.encode(pair(), state=state(), draw_list='auxgeom', profile='future')
        for name in ('', '../source', 'a\0b', 'a'*65, '\u00e9'):
            with self.assertRaises(HeightmapImportError): native.encode(pair(), state=state(), draw_list=name)

    def test_signature_chunk_overlap_and_version_reject(self):
        original = structural_program(True)
        for offset, value in ((36, 40), (20, 2), (24, len(original)+4)):
            changed = bytearray(original); struct.pack_into('<I', changed, offset, value)
            with self.assertRaises(HeightmapImportError): native.program_contract(bytes(changed), 'vertex')
        changed = bytearray(original); struct.pack_into('<I', changed, len(changed)-8, 0x10051)
        with self.assertRaises(HeightmapImportError): native.program_contract(bytes(changed), 'vertex')

    def test_exact_selector_rejects_boolean_and_unknown_stage_before_reading(self):
        baseline = {'subshader': 0, 'pass': 0, 'stage': 'progVertex', 'tier': 3, 'variant': 0}
        for key, value in (('subshader', True), ('variant', -1), ('tier', 65536), ('stage', 'progGeometry')):
            selector = {**baseline, key: value}
            with self.assertRaises(HeightmapImportError): native.source_stage(None, b'', selector)

    def test_non_ascii_signature_rejects_with_domain_error(self):
        code = structural_program(True).replace(b'POSITION', b'POSITIO\xff')
        with self.assertRaises(HeightmapImportError): native.program_contract(code, 'vertex')


if __name__ == '__main__':
    unittest.main()
