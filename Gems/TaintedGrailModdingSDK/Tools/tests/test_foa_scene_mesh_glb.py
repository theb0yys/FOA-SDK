# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import json
from pathlib import Path
import struct
import sys
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from foa_scene_mesh import SourceMesh
import foa_scene_mesh_glb as g
from foa_heightmap_importer import HeightmapImportError


def fixture():
    return SourceMesh({0:[(1.,2.,3.),(4.,5.,6.),(2.,8.,10.)],
        1:[(0.,2.,0.,.25)]*3,2:[(3.,0.,0.,-1.)]*3,3:[(0.,.25,.5,1.)]*3,
        4:[(.125,.25),(.75,.875),(1.,0.)],8:[(.1,.2),(.3,.4),(.5,.6)]},
        [[(0,1,2)],[(2,1,0)]],{'serialized_file':'synthetic','path_id':'-9007199254740993','record_sha256':'1'*64},0)


def decode(blob):
    magic,version,length=struct.unpack_from('<3I',blob)
    assert (magic,version,length)==(0x46546c67,2,len(blob))
    size,kind=struct.unpack_from('<2I',blob,12);assert kind==0x4e4f534a
    doc=json.loads(blob[20:20+size]);bsize,btype=struct.unpack_from('<2I',blob,20+size);assert btype==0x004e4942
    data=blob[28+size:];assert len(data)==bsize
    def read(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        code='f' if a['componentType']==5126 else 'I'
        values=list(struct.iter_unpack('<'+str(dims)+code,data[v['byteOffset']:v['byteOffset']+v['byteLength']]))
        assert len(values)==a['count']
        return values
    return doc,data,read


class GlbTests(unittest.TestCase):
    def test_geometry_axes_and_triangle_orientation(self):
        mesh=fixture();blob,report=g.encode_glb(mesh);doc,_,read=decode(blob)
        first,second=doc['meshes'][0]['primitives'];a=first['attributes']
        self.assertEqual(read(a['POSITION']),[(1.,3.,2.),(4.,6.,5.),(2.,10.,8.)])
        self.assertEqual(read(first['indices']),[(0,),(2,),(1,)])
        self.assertEqual(read(second['indices']),[(2,),(0,),(1,)])
        self.assertEqual(report['triangles'],2)
        matrix=doc['nodes'][0]['matrix']
        world=[tuple(sum(matrix[c*4+r]*p[c] for c in range(3)) for r in range(3)) for p in read(a['POSITION'])]
        self.assertEqual(world,[(-1.,2.,3.),(-4.,5.,6.),(-2.,8.,10.)])
        self.assertEqual([p['material'] for p in doc['meshes'][0]['primitives']],[0,1])

    def test_original_normal_extra_lane_and_sparse_uv_ordinals_are_retained(self):
        blob,report=g.encode_glb(fixture());doc,_,read=decode(blob);a=doc['meshes'][0]['primitives'][0]['attributes']
        self.assertEqual(read(a['_SOURCE_NORMAL_W']),[(.25,)]*3)
        self.assertEqual(read(a['NORMAL']),[(0.,0.,1.)]*3)
        self.assertEqual(read(a['TANGENT']),[(1.,0.,0.,-1.)]*3)
        self.assertEqual(read(a['TEXCOORD_0']),[(.125,.75),(.75,.125),(1.,1.)])
        mapping=[r for r in report['channel_mapping'] if r['attribute'].startswith('TEXCOORD')]
        self.assertEqual([(r['source_channel'],r['attribute']) for r in mapping],[(4,'TEXCOORD_0'),(8,'TEXCOORD_1')])
        self.assertNotIn('TEXCOORD_2',a)

    def test_projection_cannot_promote_material_or_game_support(self):
        mesh=fixture();old=copy.deepcopy(mesh);blob,report=g.encode_glb(mesh);doc,_,_=decode(blob)
        self.assertEqual(mesh,old)
        self.assertEqual(report['source_material_mapping'],'NOT_RUN')
        self.assertEqual(report['game_export'],'NOT_RUN')
        self.assertEqual(doc['meshes'][0]['extras']['source_binding'],old.binding)
        self.assertTrue(all(m['extras']['source_material_mapping']=='NOT_RUN' for m in doc['materials']))

    def test_missing_channels_are_not_generated(self):
        mesh=fixture();mesh.attributes={0:mesh.attributes[0]}
        doc,_,_=decode(g.encode_glb(mesh)[0])
        self.assertEqual(set(doc['meshes'][0]['primitives'][0]['attributes']),{'POSITION'})

    def test_unqualified_values_and_shapes_fail(self):
        for semantic,row in [(0,(float('nan'),0,0)),(1,(0,0,0,0)),(2,(1,0,0,0)),(3,(2,0,0,1)),(4,(1,2,3))]:
            mesh=fixture();mesh.attributes[semantic]=[row]*3
            with self.subTest(semantic=semantic),self.assertRaises(HeightmapImportError):g.encode_glb(mesh)
        mesh=fixture();mesh.submeshes=[[]]
        with self.assertRaises(HeightmapImportError):g.encode_glb(mesh)

    def test_limits_and_cancellation_fail(self):
        with mock.patch.object(g,'MAX_BYTES',8),self.assertRaises(HeightmapImportError):g.encode_glb(fixture())
        with self.assertRaises(HeightmapImportError):g.encode_glb(fixture(),lambda:True)
        mesh=fixture();mesh.attributes[12]=[(0.,)*4]*3
        with self.assertRaises(HeightmapImportError):g.encode_glb(mesh)

if __name__=='__main__':unittest.main()
