# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
from pathlib import Path
import struct
import sys
import unittest
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_scene_native_mesh_audit as a
from foa_scene_mesh_glb import encode_glb
from test_foa_scene_mesh_glb import fixture


def node(name, value=b'', children=()):
    return {'name':zlib.crc32(name.lower().encode()),'value':value,'children':list(children)}


def number(name, value):
    return node(name,value.to_bytes(4,'big'))


def fake_cache():
    blob,_=encode_glb(fixture());doc,read=a.glb_accessors(blob)
    attrs={k:read(v) for k,v in doc['meshes'][0]['primitives'][0]['attributes'].items()}
    channels={'POSITION0':attrs['POSITION'],'NORMAL0':attrs['NORMAL'],'TANGENT0':attrs['TANGENT'],
              'COLOR0':attrs['COLOR_0'],'UV0':[(u,a.f32(1-a.f32(1-v))) for u,v in attrs['TEXCOORD_0']],
              'UV1':[(u,a.f32(1-a.f32(1-v))) for u,v in attrs['TEXCOORD_1']]}
    slots=[];meshes=[]
    for i,primitive in enumerate(doc['meshes'][0]['primitives']):
        slots.append(node('element',children=[number('value1',10+i),node('value2',children=[number('StableId',10+i),node('DisplayName',('SourceSlot'+str(i)).encode())])]))
        streams=[]
        for name,rows in channels.items():
            v=node('BufferAssetView');v['fixture_rows']=copy.deepcopy(rows)
            streams.append(node('element',children=[node('Semantic',children=[node('m_name',name[:-1].encode()),number('m_index',int(name[-1]))]),v]))
        indices=node('IndexBufferAssetView');indices['fixture_rows']=[r[0] for r in read(primitive['indices'])]
        meshes.append(node('element',children=[number('MaterialSlotId',10+i),node('StreamBufferInfo',children=streams),indices]))
    class Cache:
        def __init__(self):
            self.root=node('model',children=[node('MaterialSlots',children=slots),node('LodAssets',children=[node('element')])])
            self.lod=node('lod',children=[node('Meshes',children=meshes)])
        def model(self,name):return self.root
        def resolve(self,value):return 'fixture'
        def read(self,value):return self.lod
        def view(self,value,indices=False):return value['fixture_rows']
    return blob,Cache()


def stream(cache, name):
    mesh=a.field(cache.lod,'Meshes')['children'][0]
    return next(a.field(item,'BufferAssetView')['fixture_rows'] for item in a.field(mesh,'StreamBufferInfo')['children']
                if a.field(a.field(item,'Semantic'),'m_name')['value'].decode()+str(a.integer(a.field(item,'Semantic'),'m_index'))==name)


class NativeMeshAuditTests(unittest.TestCase):
    def test_all_corner_channels_and_slots_pass_without_promoting_game_support(self):
        blob,cache=fake_cache();r=a.audit_projection(cache,'fixture.glb',blob)
        self.assertEqual(r['mapped_corner_channels_status'],'PASSED')
        self.assertEqual(r['material_slots'],2)
        self.assertEqual(r['triangles'],2)
        self.assertEqual(r['extra_normal_lane']['nonzero_values'],3)
        self.assertEqual(r['extra_normal_lane']['native_mapping'],'NOT_RUN')
        self.assertEqual(r['game_export'],'NOT_RUN')

    def test_reversed_or_dropped_triangles_fail(self):
        for indices in ([0,1,2],[]):
            blob,cache=fake_cache();mesh=a.field(cache.lod,'Meshes')['children'][0]
            a.field(mesh,'IndexBufferAssetView')['fixture_rows']=indices
            with self.assertRaises(a.HeightmapImportError):a.audit_projection(cache,'fixture.glb',blob)

    def test_uv_color_tangent_or_normal_changes_fail(self):
        for name in ('UV0','UV1','COLOR0','TANGENT0','NORMAL0'):
            blob,cache=fake_cache();rows=stream(cache,name);row=list(rows[0]);row[0]+=.1;rows[0]=tuple(row)
            with self.subTest(name=name),self.assertRaises(a.HeightmapImportError):a.audit_projection(cache,'fixture.glb',blob)

    def test_lost_tangent_sign_dimension_fails(self):
        blob,cache=fake_cache();rows=stream(cache,'TANGENT0');rows[:]=[r[:3] for r in rows]
        with self.assertRaises(a.HeightmapImportError):a.audit_projection(cache,'fixture.glb',blob)

    def test_source_material_slot_swap_fails(self):
        blob,cache=fake_cache();slots=a.field(cache.root,'MaterialSlots')['children']
        for i,slot in enumerate(slots):a.field(a.field(slot,'value2'),'DisplayName')['value']=('SourceSlot'+str(1-i)).encode()
        with self.assertRaises(a.HeightmapImportError):a.audit_projection(cache,'fixture.glb',blob)

    def test_cancellation_is_checked(self):
        blob,cache=fake_cache()
        with self.assertRaises(a.HeightmapImportError):a.audit_projection(cache,'fixture.glb',blob,lambda:True)

    def test_truncated_or_wrong_glb_framing_fails(self):
        blob,_=fake_cache()
        for bad in (blob[:20],b'bad'+blob[3:],blob[:-1]):
            with self.assertRaises(a.HeightmapImportError):a.glb_accessors(bad)

    def test_native_binary_requires_bounded_framing(self):
        for bad in (b'',b'\0\0\0\0\4',b'\0\0\0\0\3\x08',b'\0\0\0\0\3\0extra'):
            with self.assertRaises(a.HeightmapImportError):a.ObjectStream(bad)
        valid=b'\0\0\0\0\3\x08'+bytes(16)+b'\0\0'
        self.assertEqual(len(a.ObjectStream(valid).roots),1)

if __name__=='__main__':unittest.main()
