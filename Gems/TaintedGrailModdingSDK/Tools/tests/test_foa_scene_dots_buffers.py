# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independent byte expectations and rejection checks for explicit DOTS streams."""
import copy
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_dots_buffers as d
from foa_heightmap_importer import HeightmapImportError


class DotsBufferTests(unittest.TestCase):
    def test_column_words_preserve_signed_zero_and_subnormal(self):
        words=[1,2,3,4,5,6,7,8,9,10,0x80000000,12,0,0,0,0x3f800000]
        self.assertEqual(struct.unpack('<12I',d.pack_matrix_columns([words])),
                         (1,5,9,2,6,10,3,7,0x80000000,4,8,12))

    def test_stream_offsets_padding_and_explicit_order(self):
        properties=[dict(name='z',stride=12,data=bytes(range(36))),dict(name='a',stride=4,data=b'Q'*12)]
        before=copy.deepcopy(properties);data,metadata=d.pack_streams(properties,3,d.PROFILE)
        self.assertEqual(metadata,{'z':0x80000040,'a':0x80000070})
        self.assertEqual(data,bytes(64)+bytes(range(36))+bytes(12)+b'Q'*12+bytes(4))
        self.assertEqual(properties,before)

    def test_direct_visibility_uses_uint4_stride(self):
        constants,raw=d.visibility_buffers([2,0,1],3,d.PROFILE,indirect=False,strip_upper_byte=False)
        expected=bytearray(4096);struct.pack_into('<I',expected,0,2);struct.pack_into('<I',expected,32,1)
        self.assertEqual(constants,expected);self.assertEqual(raw,struct.pack('<3I',2,0,1))

    def test_indirect_visibility_and_upper_byte_are_explicit(self):
        words=[0xff000002,0x12000000]
        constants,raw=d.visibility_buffers(words,3,d.PROFILE,indirect=True,strip_upper_byte=True)
        self.assertEqual(constants,struct.pack('<4I',0,0x80000000,0,32)+bytes(4080))
        self.assertEqual(raw,struct.pack('<2I',*words))
        with self.assertRaises(HeightmapImportError):d.visibility_buffers(words,3,d.PROFILE,indirect=True,strip_upper_byte=False)

    def test_explicit_capacity_gaps_are_preserved(self):
        # Active slots 0 and 3 are separated by explicitly supplied unused bytes.
        values=struct.pack('<4I',0x3f800000,0x12345678,0xabcdef12,0x40000000)
        data,metadata=d.pack_streams([dict(name='x',stride=4,data=values)],4,d.PROFILE)
        self.assertEqual(data[64:],values);self.assertEqual(metadata['x'],0x80000040)
        constants,raw=d.visibility_buffers([3,0],4,d.PROFILE,indirect=True,strip_upper_byte=False)
        self.assertEqual(raw,struct.pack('<2I',3,0))
        with self.assertRaises(HeightmapImportError):
            d.visibility_buffers([3,0],2,d.PROFILE,indirect=True,strip_upper_byte=False)

    def test_bad_profile_and_count_rejected(self):
        prop=[dict(name='x',stride=4,data=bytes(4))]
        for profile in ({},dict(d.PROFILE,unity_version='other'),dict(d.PROFILE,extra=1)):
            with self.assertRaises(HeightmapImportError):d.pack_streams(prop,1,profile)
        for count in (0,-1,True,4097):
            with self.assertRaises(HeightmapImportError):d.pack_streams(prop,count,d.PROFILE)

    def test_missing_duplicate_or_malformed_stream_rejected(self):
        prop=dict(name='x',stride=4,data=bytes(4))
        for properties in ([],[prop,prop],[None],[dict(prop,extra=0)],[dict(prop,data=bytes(3))],
                           [dict(prop,data=bytearray(4))],[dict(prop,stride=True)],[dict(prop,stride=3)]):
            with self.subTest(properties=properties),self.assertRaises(HeightmapImportError):d.pack_streams(properties,1,d.PROFILE)
        with patch.object(d,'MAX_BYTES',79),self.assertRaises(HeightmapImportError):d.pack_streams([prop],1,d.PROFILE)

    def test_visibility_bounds_and_modes_rejected(self):
        for indices in ([],[3],[-1],[True],[0x100000000]):
            with self.assertRaises(HeightmapImportError):d.visibility_buffers(indices,3,d.PROFILE,indirect=False,strip_upper_byte=False)
        with self.assertRaises(HeightmapImportError):d.visibility_buffers([0]*257,1,d.PROFILE,indirect=False,strip_upper_byte=False)
        with self.assertRaises(HeightmapImportError):d.visibility_buffers([0],1,d.PROFILE,indirect=1,strip_upper_byte=False)
        self.assertEqual(len(d.visibility_buffers([0]*4096,1,d.PROFILE,indirect=True,strip_upper_byte=False)[1]),16384)

    def test_lossy_or_nonfinite_matrix_rejected(self):
        matrix=[0]*16;matrix[15]=0x3f800000
        for index,value in ((0,0x7f800000),(0,0x7fc00000),(0,True),(12,0x80000000),(15,0)):
            bad=matrix.copy();bad[index]=value
            with self.assertRaises(HeightmapImportError):d.pack_matrix_columns([bad])
        for matrices in ([],[matrix[:-1]],(matrix,)):
            with self.assertRaises(HeightmapImportError):d.pack_matrix_columns(matrices)


if __name__=='__main__':unittest.main()
