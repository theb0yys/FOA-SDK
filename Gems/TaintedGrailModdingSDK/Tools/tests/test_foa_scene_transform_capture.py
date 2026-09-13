# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import hashlib
import json
import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import foa_scene_transform_capture as c
import foa_scene_transforms as t
from foa_heightmap_importer import HeightmapImportError


def fixture():
    nodes=[];rows=[]
    for parent,p,q,s in [(-1,(1.,-2.,3.),(0.,0.,0.,1.),(-2.,3.,1.)),
                         (0,(2.,0.,1.),(0.,0.,math.sin(.3),math.cos(.3)),(1.,2.,3.))]:
        p,q,s=(tuple(map(c.f32,v)) for v in (p,q,s))
        local=tuple(map(c.f32,t.trs(p,q,s)))
        world=tuple(map(c.f32,t.multiply(rows[parent]['world'],local))) if parent>=0 else local
        fields={'p':dict(zip('xyz',p)),'q':dict(zip('xyzw',q)),'s':dict(zip('xyz',s))}
        nodes.append(dict(fields,parent=parent,kind='Transform'))
        rows.append(dict(fields,worldBits=list(c.bits(world)),localBits=list(c.bits(local)),world=list(world),local=list(local),inputBits=list(c.bits((*p,*q,*s))),assignedBits=list(c.bits((*p,*q,*s)))))
    raw=json.dumps({'nodes':nodes}).encode()
    return raw,{'schemaVersion':3,'status':'PASSED','unityVersion':'6000.0.64f1','inputSha256':hashlib.sha256(raw).hexdigest(),'rectTransforms':0,'rows':rows}


class CaptureTests(unittest.TestCase):
    def test_preserves_full_matrix_shear_reflection_and_source_binding(self):
        raw,cap=fixture();report=c.audit_capture(raw,cap)
        self.assertEqual(report['transforms'],2)
        self.assertEqual(report['source_world_matrices'][1],tuple(cap['rows'][1]['world']))
        self.assertTrue(t.linear_properties(report['host_world_matrices'][1])['reflected'])
        self.assertGreater(t.linear_properties(report['host_world_matrices'][1])['max_column_cosine'],.1)
        self.assertEqual(report['native_scene_projection'],'NOT_RUN')

    def test_matrix_signed_zero_bits_survive_json_canonicalization(self):
        raw,cap=fixture();cap['rows'][0]['localBits'][1]=cap['rows'][0]['worldBits'][1]=-(1<<31)
        result=c.audit_capture(raw,cap)
        self.assertEqual(c.bits(result['source_world_matrices'][0])[1],-(1<<31))

    def test_one_ulp_rotation_change_is_rejected(self):
        raw,cap=fixture();cap['rows'][1]['assignedBits'][6]+=1
        with self.assertRaisesRegex(HeightmapImportError,'changed source TRS'):c.audit_capture(raw,cap)

    def test_signed_zero_is_accounted_without_changing_source_input(self):
        raw,cap=fixture();doc=json.loads(raw);doc['nodes'][0]['p']['y']=-0.
        raw=json.dumps(doc).encode();cap['inputSha256']=hashlib.sha256(raw).hexdigest()
        cap['rows'][0]['inputBits'][1]=-(1<<31);cap['rows'][0]['assignedBits'][1]=0
        cap['rows'][0]['p']['y']=0.;cap['rows'][0]['local'][7]=cap['rows'][0]['world'][7]=0.
        cap['rows'][1]['world'][7]+=2.
        for row in cap['rows']:
            row['localBits']=list(c.bits(row['local']));row['worldBits']=list(c.bits(row['world']))
        self.assertEqual(c.audit_capture(raw,cap)['signed_zero_canonicalizations'],1)
        self.assertEqual(json.loads(raw)['nodes'][0]['p']['y'],0.)

    def test_wrong_hierarchy_matrix_and_root_translation_reject(self):
        for index,offset in ((0,3),(1,7)):
            raw,cap=fixture();cap['rows'][index]['world'][offset]+=.5;cap['rows'][index]['worldBits']=list(c.bits(cap['rows'][index]['world']))
            with self.assertRaises(HeightmapImportError):c.audit_capture(raw,cap)

    def test_stale_missing_extra_and_future_capture_reject(self):
        raw,cap=fixture()
        for field,value in [('inputSha256','0'*64),('rows',cap['rows'][:1]),('rows',cap['rows']+[cap['rows'][0]]),('schemaVersion',4),('schemaVersion',True),('unityVersion','unknown')]:
            bad=copy.deepcopy(cap);bad[field]=value
            with self.subTest(field=field),self.assertRaises(HeightmapImportError):c.audit_capture(raw,bad)

    def test_malformed_order_nonfinite_and_cancel_reject(self):
        raw,cap=fixture()
        with self.assertRaises(HeightmapImportError):c.audit_capture(raw,cap,lambda:True)
        with self.assertRaises(HeightmapImportError):c.audit_capture(b'{"nodes":[],"nodes":[]}',cap)
        cap['rows'][0]['world'][0]=float('nan')
        with self.assertRaises(HeightmapImportError):c.audit_capture(raw,cap)
        raw,cap=fixture();doc=json.loads(raw);doc['nodes'][0]['parent']=0;raw=json.dumps(doc).encode();cap['inputSha256']=hashlib.sha256(raw).hexdigest()
        with self.assertRaises(HeightmapImportError):c.audit_capture(raw,cap)

    def test_serialized_columns_preserve_every_entry_and_reject_perspective(self):
        value={'c'+str(i):dict(zip('xyzw',[t.IDENTITY[r*4+i] for r in range(4)])) for i in range(4)}
        value['c1']['x']=.25;value['c3']['y']=-7
        matrix=c.column_matrix(value);self.assertEqual(matrix[1],.25);self.assertEqual(matrix[7],-7)
        value['c0']['w']=.1
        with self.assertRaises(HeightmapImportError):c.column_matrix(value)


if __name__=='__main__':unittest.main()
