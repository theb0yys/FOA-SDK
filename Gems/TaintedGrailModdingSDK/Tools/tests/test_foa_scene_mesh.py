# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic mesh layout/quantization/bounds tests; no proprietary fixtures."""
import copy
import io
from pathlib import Path
import struct
import sys
from types import SimpleNamespace as NS
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_scene_mesh as m
from foa_heightmap_importer import HeightmapImportError


def channel(stream=0, offset=0, fmt=0, dim=0):
    return {"stream":stream,"offset":offset,"format":fmt,"dimension":dim}


def packed(values, bits=8, floating=False, start=0., extent=1.):
    data=bytearray((len(values)*bits+7)//8)
    for i,value in enumerate(values):
        for bit in range(bits):
            if value & (1 << bit):
                dest=i*bits+bit;data[dest//8] |= 1 << (dest%8)
    result={"m_NumItems":len(values),"m_BitSize":bits,"m_Data":bytes(data)}
    if floating: result.update(m_Start=start,m_Range=extent)
    return result


def compressed():
    values={name:packed([],floating=True) for name in ["m_Vertices","m_UV","m_Normals","m_Tangents","m_FloatColors"]}
    values.update({name:packed([]) for name in ["m_NormalSigns","m_TangentSigns","m_Triangles","m_Weights","m_BoneIndices"]})
    values["m_Vertices"]=packed([0,0,0,255,0,0,0,255,0],floating=True)
    values["m_Triangles"]=packed([0,1,2],2)
    values["m_UVInfo"]=0
    return {"m_CompressedMesh":values}


class MeshLayoutTests(unittest.TestCase):
    def test_float_positions_keep_bits_and_stream_alignment(self):
        points=[(-0.,1.25,-4.),(2.5,-3.25,6.),(10.,-12.,.125)]
        payload=struct.pack('<9f',*(v for p in points for v in p))+b'\0'*12
        normals=[(1.,0.,0.,0.),(0.,1.,0.,0.),(0.,0.,1.,0.)]
        payload+=struct.pack('<12e',*(v for p in normals for v in p))
        table=[channel(dim=3),channel(1,0,1,52)]
        decoded=m.vertex_channels({'m_VertexCount':3,'m_Channels':table},payload)
        self.assertEqual(struct.pack('<9f',*(v for p in decoded[0] for v in p)),payload[:36])
        self.assertEqual(decoded[1],normals)

    def test_normalized_color_and_signed_extremes(self):
        table=[channel(dim=3),channel(0,12,3,3),channel(0,15,2,4)]
        payload=struct.pack('<3f3b4B',1,2,3,-128,0,127,0,128,254,255)
        decoded=m.vertex_channels({'m_VertexCount':1,'m_Channels':table},payload)
        self.assertEqual(decoded[1],[(-1.,0.,1.)])
        self.assertEqual(decoded[2],[(0.,128/255,254/255,1.)])

    def test_overlaps_truncation_and_bad_channels_rejected(self):
        table=[channel(dim=3),channel(0,8,0,3)]
        with self.assertRaisesRegex(HeightmapImportError,'Overlapping'):
            m.vertex_channels({'m_VertexCount':1,'m_Channels':table},bytes(24))
        for table,payload in [([channel(dim=3)],bytes(11)),([channel(fmt=99,dim=3)],bytes(12)),
                              ([channel(stream=8,dim=3)],bytes(12)),([channel(dim=5)],bytes(20)),
                              ([channel(dim=3),channel(offset=40,dim=3)],bytes(24))]:
            with self.subTest(table=table),self.assertRaises(HeightmapImportError):
                m.vertex_channels({'m_VertexCount':1,'m_Channels':table},payload)

    def test_nonfinite_and_counts_fail(self):
        for count,payload in [(0,bytes(12)),(m.MAX_VERTICES+1,bytes(12)),(1,struct.pack('<3f',0,float('nan'),0))]:
            with self.subTest(count=count),self.assertRaises(HeightmapImportError):
                m.vertex_channels({'m_VertexCount':count,'m_Channels':[channel(dim=3)]},payload)
        with self.assertRaises(HeightmapImportError):
            m.vertex_channels({'m_VertexCount':1,'m_Channels':[channel(dim=3)]},bytes(12),lambda:True)

    def test_all_integer_formats(self):
        for fmt,(kind,width) in enumerate(m.FORMATS):
            if fmt < 6:continue
            table=[channel(dim=3),channel(offset=12,fmt=fmt,dim=1)]
            raw=struct.pack('<3f'+kind,1,2,3,7)
            self.assertEqual(m.vertex_channels({'m_VertexCount':1,'m_Channels':table},raw)[1],[(7,)])

    def test_submeshes_preserve_base_vertex_and_winding(self):
        submeshes=[{'topology':0,'firstByte':2,'indexCount':3,'baseVertex':2},
                   {'topology':0,'firstByte':8,'indexCount':3,'baseVertex':0}]
        self.assertEqual(m.triangles(submeshes,[99,0,1,2,4,3,2],5,0),[[(2,3,4)],[(4,3,2)]])

    def test_invalid_triangles_never_become_synthetic_faces(self):
        base={'topology':0,'firstByte':0,'indexCount':3,'baseVertex':0}
        for field,value in [('topology',1),('firstByte',1),('indexCount',2),('baseVertex',1)]:
            s={**base,field:value}
            with self.subTest(field=field),self.assertRaises(HeightmapImportError):m.triangles([s],[0,1,2],3,0)
        with self.assertRaises(HeightmapImportError):m.triangles([base],[0,1,3],3,0)

    def test_packed_values_cross_byte_boundaries(self):
        for bits in [1,3,7,13,24,32]:
            values=[0,(1 << bits)-1,1,(1 << bits)//2]
            self.assertEqual(m.packed_values(packed(values,bits)),values)
        self.assertEqual(m.packed_values(packed([0,3,7],3,True,-2.,7.),True),[-2.,1.,5.])

    def test_packed_truncation_and_bad_quantization_fail(self):
        bad=packed([1,2,3],13);bad['m_Data']=bad['m_Data'][:-1]
        with self.assertRaises(HeightmapImportError):m.packed_values(bad)
        bad=packed([1],3,True);bad['m_Range']=float('inf')
        with self.assertRaises(HeightmapImportError):m.packed_values(bad,True)
        bad=packed([1]);bad['m_NumItems']=m.MAX_INDICES*4+1
        with self.assertRaises(HeightmapImportError):m.packed_values(bad)

    def test_compressed_vertices_and_indices(self):
        attrs,indices=m.compressed_channels(compressed())
        self.assertEqual(attrs[0],[(0,0,0),(1,0,0),(0,1,0)])
        self.assertEqual(indices,[0,1,2])
        self.assertEqual(set(attrs),{0})

    def test_three_packed_uv_sets_use_cumulative_offsets(self):
        tree=compressed();c=tree['m_CompressedMesh'];c['m_UVInfo']=0x555
        c['m_UV']=packed(list(range(18)),5,True,0.,31.)
        attrs,_=m.compressed_channels(tree)
        self.assertEqual(attrs[4],[(0,1),(2,3),(4,5)])
        self.assertEqual(attrs[5],[(6,7),(8,9),(10,11)])
        self.assertEqual(attrs[6],[(12,13),(14,15),(16,17)])

    def test_packed_normal_and_tangent_signs(self):
        tree=compressed();c=tree['m_CompressedMesh']
        c['m_Normals']=packed([0]*6,8,True);c['m_NormalSigns']=packed([0,1,0],1)
        c['m_Tangents']=packed([0]*6,8,True);c['m_TangentSigns']=packed([0,0,1,1,0,1],1)
        attrs,_=m.compressed_channels(tree)
        self.assertEqual(attrs[1],[(0,0,-1),(0,0,1),(0,0,-1)])
        self.assertEqual(attrs[2],[(0,0,-1,-1),(0,0,1,1),(0,0,-1,1)])

    def test_packed_channels_cannot_be_short_or_silently_ignored(self):
        tree=compressed();tree['m_CompressedMesh']['m_NormalSigns']=packed([1],1)
        with self.assertRaises(HeightmapImportError):m.compressed_channels(tree)
        tree=compressed();tree['m_CompressedMesh']['m_Weights']=packed([1])
        with self.assertRaises(HeightmapImportError):m.compressed_channels(tree)

    def test_explicit_stream_bounds_and_reader_position_preserved(self):
        class Reader:
            def __init__(self):self.data=io.BytesIO(b'prefixpayload');self.Length=13
            @property
            def Position(self):return self.data.tell()
            @Position.setter
            def Position(self,value):self.data.seek(value)
            def read_bytes(self,size):return self.data.read(size)
        f='CAB-'+'1'*32;obj=NS(assets_file=NS(name=f));resource=Reader();resource.Position=2
        tree={'m_StreamData':{'path':'archive:/'+f+'/'+f+'.resS','offset':6,'size':7}}
        payload,proof=m.stream_payload(obj,tree,{f+'.resS':resource})
        self.assertEqual(payload,b'payload');self.assertEqual(resource.Position,2)
        self.assertEqual(proof['offset'],6)
        tree['m_StreamData']['size']=8
        with self.assertRaises(HeightmapImportError):m.stream_payload(obj,tree,{f+'.resS':resource})
        tree['m_StreamData']['path']='../other.resS'
        with self.assertRaises(HeightmapImportError):m.stream_payload(obj,tree,{f+'.resS':resource})

class PackedCancellationTests(unittest.TestCase):
    def test_packed_decode_checks_cancel_before_allocating_values(self):
        packet={"m_NumItems":8192,"m_BitSize":1,"m_Data":bytes(1024)}
        with self.assertRaises(HeightmapImportError):
            m.packed_values(packet, cancelled=lambda: True)

    def test_packed_channel_bound_is_checked_before_decoding(self):
        packet={"m_NumItems":8192,"m_BitSize":1,"m_Data":bytes(1024)}
        with self.assertRaises(HeightmapImportError):
            m.packed_values(packet, max_items=4)


if __name__=='__main__':unittest.main()
