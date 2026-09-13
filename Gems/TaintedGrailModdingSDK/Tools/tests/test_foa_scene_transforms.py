# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import math
from pathlib import Path
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_transforms as t
from foa_heightmap_importer import HeightmapImportError
from test_foa_scene_ownership import fixture, graph


def source_fixture():
    f = fixture()
    for obj in f.objects.values():
        obj.get_raw_data = lambda: b'Q'*64
        if obj.type.name == 'GameObject':obj.tree['m_IsActive'] = True
        if obj.type.name == 'Transform':
            obj.tree.update(m_LocalPosition=dict(x=0.,y=0.,z=0.),
                m_LocalRotation=dict(x=0.,y=0.,z=0.,w=1.),m_LocalScale=dict(x=1.,y=1.,z=1.))
    return f


class TransformTests(unittest.TestCase):
    def test_affine_basis_preserves_point_action_and_composition(self):
        root = t.trs((5,-7,11),(0,0,math.sqrt(.5),math.sqrt(.5)),(-2,3,.5))
        child = t.trs((.5,2,-4),(math.sin(.3),0,0,math.cos(.3)),(1,2,3))
        world = t.multiply(root,child);p=(3,4,9);expected=t.point(world,p)
        actual=t.point(t.host_matrix(world),(p[0],p[2],p[1]))
        for a,b in zip(actual,(expected[0],expected[2],expected[1])):self.assertAlmostEqual(a,b)
        self.assertEqual(t.host_matrix(world),t.multiply(t.host_matrix(root),t.host_matrix(child)))
        self.assertGreater(t.linear_properties(world)['max_column_cosine'],.1)
        self.assertTrue(t.linear_properties(world)['reflected'])

    def test_normal_inverse_preserves_orthogonality_under_shear_and_reflection(self):
        matrix=(2.,.5,.2,11., 0.,-3.,.7,-9., .4,0.,1.,3., 0.,0.,0.,1.)
        normal=t.point(t.normal_matrix(matrix),(0.,0.,1.))
        for tangent in ((matrix[0],matrix[4],matrix[8]),(matrix[1],matrix[5],matrix[9])):
            self.assertAlmostEqual(sum(a*b for a,b in zip(normal,tangent)),0.)
        self.assertTrue(t.linear_properties(matrix)['reflected'])
        with self.assertRaises(HeightmapImportError):t.normal_matrix(t.trs((0,0,0),(0,0,0,1),(0,1,1)))

    def test_zero_scale_is_retained_as_degenerate_not_repaired(self):
        m=t.trs((1,2,3),(0,0,0,1),(0,2,-3))
        self.assertTrue(t.linear_properties(m)['degenerate'])
        self.assertEqual(t.point(m,(99,2,3)),(1,6,-6))

    def test_no_quaternion_normalization(self):
        q=(0.,0.,.3,math.sqrt(.91)*(1.+1e-7));m=t.trs((0,0,0),q,(1,1,1))
        normalized=tuple(v/math.sqrt(sum(x*x for x in q)) for v in q)
        self.assertNotEqual(m,t.trs((0,0,0),normalized,(1,1,1)))
        self.assertEqual(m[1],-2*q[2]*q[3])
        with self.assertRaises(HeightmapImportError):t.trs((0,0,0),(0,0,0,2),(1,1,1))

    def test_bad_matrices_and_values_reject(self):
        for value in (None,[1]*15,list(t.IDENTITY[:15])+[2],list(t.IDENTITY[:3])+[float('nan')]+list(t.IDENTITY[4:])):
            with self.subTest(value=value),self.assertRaises(HeightmapImportError):t.affine(value)
        for value in ((True,2,3),(1,2,float('inf'))):
            with self.assertRaises(HeightmapImportError):t.trs(value,(0,0,0,1),(1,1,1))

    def test_snapshot_keeps_original_trs_identity_parent_and_active_state(self):
        f=source_fixture();f.objects[1].tree['m_IsActive']=False
        f.objects[2].tree['m_LocalScale']['x']=2.
        f.objects[4].tree['m_LocalPosition']['x']=3.
        original=copy.deepcopy(f.objects[4].tree)
        snapshot=t.SceneTransforms(graph(f));root,child=snapshot.rows.values()
        self.assertEqual(child.world[3],6.)
        self.assertEqual(child.position,(3,0,0))
        self.assertFalse(child.active_in_hierarchy);self.assertTrue(child.active_self)
        self.assertEqual(child.parent,root.key)
        self.assertEqual(child.record()['parent']['path_id'],'2')
        self.assertEqual(f.objects[4].tree,original)
        self.assertEqual(snapshot.report()['serialized_hierarchy'],'PASSED')
        self.assertEqual(snapshot.report()['native_projection'],'NOT_RUN')

    def test_cancel_budget_changed_record_and_invalid_fields_reject(self):
        f=source_fixture();g=graph(f)
        with self.assertRaises(HeightmapImportError):t.SceneTransforms(g,lambda:True)
        with mock.patch.object(t,'MAX_TOTAL_BYTES',1),self.assertRaises(HeightmapImportError):t.SceneTransforms(g)
        f.objects[2].get_raw_data=mock.Mock(side_effect=[b'Q'*64,b'R'*64])
        with self.assertRaisesRegex(HeightmapImportError,'source changed'):t.SceneTransforms(g)
        f=source_fixture();f.objects[1].tree['m_IsActive']=1
        with self.assertRaises(HeightmapImportError):t.SceneTransforms(graph(f))
        f=source_fixture();f.objects[4].tree['m_LocalPosition']['w']=1
        with self.assertRaises(HeightmapImportError):t.SceneTransforms(graph(f))


if __name__=='__main__':unittest.main()
