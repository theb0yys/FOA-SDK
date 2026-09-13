# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Compiled declaration and versioned read-only buffer binding corruption tests."""
import copy
import hashlib
import struct
import unittest
from test_foa_scene_native_shader import pair, state, structural_program
from foa_scene_native_shader import encode
from foa_scene_shader_buffers import buffer_declarations
from foa_heightmap_importer import HeightmapImportError


def program(words, vertex=False):
    code = bytearray(structural_program(vertex))
    original = len(code)
    data = struct.pack('<'+'I'*len(words), *words)
    struct.pack_into('<I', code, 24, original+len(data))
    struct.pack_into('<I', code, original-12, 8+len(data))
    struct.pack_into('<I', code, original-4, 2+len(words))
    return bytes(code)+data


def buffered_pair():
    stages = pair()
    stages[1]['code'] = program([0x30000a1,0x107000,2,0x40000a2,0x107000,5,176])
    stages[1]['buffers'] = [dict(slot=2,type=4,stride=4),dict(slot=5,type=2,stride=176)]
    return stages


class ShaderBufferTests(unittest.TestCase):
    def test_exact_raw_structured_and_custom_data(self):
        words = [53,5,0x30000a1,0x107000,99,0x40000a2,0x107000,5,176,0x30000a1,0x107000,2]
        self.assertEqual(buffer_declarations(program(words),'fragment'),
                         [dict(slot=2,type=4,stride=4),dict(slot=5,type=2,stride=176)])

    def test_short_or_extended_declarations_reject(self):
        for words in ([0x20000a1,0x107000],[0x30000a2,0x107000,0],
                      [0x830000a1,0x107000,0],[0x30008a1,0x107000,0],
                      [0x30000a1,0x107001,0],[0x30000a1,0x807000,0],
                      [0x30000a1,0x107000,128]):
            with self.subTest(words=words), self.assertRaises(HeightmapImportError): buffer_declarations(program(words),'fragment')

    def test_stride_bounds_and_duplicate_declarations(self):
        for stride in (0,3,6,2052,0xffffffff):
            with self.subTest(stride=stride), self.assertRaises(HeightmapImportError):
                buffer_declarations(program([0x40000a2,0x107000,5,stride]),'fragment')
        with self.assertRaises(HeightmapImportError): buffer_declarations(program([0x30000a1,0x107000,5,0x40000a2,0x107000,5,16]),'fragment')

    def test_instruction_extent_and_custom_data_bounds(self):
        for words in ([0],[0x7000001],[53],[53,1],[53,99], [0x1000001,0]):
            with self.subTest(words=words), self.assertRaises(HeightmapImportError): buffer_declarations(program(words),'fragment')

    def test_non_buffer_instructions_are_skipped(self):
        self.assertEqual(buffer_declarations(program([0x100003e]),'fragment'),[])
        self.assertEqual(buffer_declarations(program([]),'fragment'),[])

    def test_v2_packet_keeps_source_program_and_exact_declarations(self):
        stages = buffered_pair(); before=copy.deepcopy(stages)
        data = encode(stages,state=state(),draw_list='auxgeom')
        self.assertEqual(stages,before)
        self.assertEqual(struct.unpack_from('<4I',data,8),(2,1,len(data)-56,0))
        self.assertEqual(data[24:56],hashlib.sha256(data[:24]+data[56:]).digest())
        self.assertTrue(data.endswith(struct.pack('<7I',2,2,4,4,5,2,176)))
        self.assertIn(stages[1]['code'],data)

    def test_old_packet_bytes_remain_identical(self):
        stages=pair();data=encode(stages,state=state(),draw_list='auxgeom')
        body=struct.pack('<I',7)+b'auxgeom'+struct.pack('<12I',*state().values())
        for stage in stages:
            code=stage['code'];size=stage['constant_buffers'][0]['size']
            body+=struct.pack('<I',len(code))+code+struct.pack('<5I',1,0,size,0,0)
        header=b'FOASHD01'+struct.pack('<4I',1,1,len(body),0)
        self.assertEqual(data,header+hashlib.sha256(header+body).digest()+body)

    def test_missing_extra_wrong_stride_or_type_reject(self):
        for mutation in (lambda s:s[1].pop('buffers'),lambda s:s[1].update(buffers=[]),
                         lambda s:s[1]['buffers'].append(dict(slot=6,type=2,stride=4)),
                         lambda s:s[1]['buffers'][1].update(stride=16),
                         lambda s:s[1]['buffers'][0].update(type=2),
                         lambda s:s[1]['buffers'][0].update(type=True),
                         lambda s:s[1]['buffers'][1].update(stride=True),
                         lambda s:s[1]['buffers'][0].update(stride=8),
                         lambda s:s[1]['buffers'][0].update(extra=0),
                         lambda s:s[1]['buffers'][1].update(type=3)):
            stages=buffered_pair();mutation(stages)
            with self.subTest(mutation=mutation), self.assertRaises(HeightmapImportError): encode(stages,state=state(),draw_list='auxgeom')

    def test_texture_buffer_and_duplicate_buffer_registers_reject(self):
        for mutation in (lambda s:s[1]['images'].append(dict(slot=2,type=3)),
                         lambda s:s[1]['buffers'].append(dict(slot=2,type=4,stride=4)),
                         lambda s:s[1]['buffers'][0].update(slot=128)):
            stages=buffered_pair();mutation(stages)
            with self.assertRaises(HeightmapImportError): encode(stages,state=state(),draw_list='auxgeom')

    def test_register_namespaces_and_stages_remain_distinct(self):
        stages=buffered_pair();stages[1]['constant_buffers']=[dict(slot=2,size=16)]
        stages[1]['samplers']=[dict(slot=2)]
        stages[0]['images']=[dict(slot=2,type=3)]
        self.assertTrue(encode(stages,state=state(),draw_list='auxgeom'))


if __name__ == '__main__': unittest.main()
