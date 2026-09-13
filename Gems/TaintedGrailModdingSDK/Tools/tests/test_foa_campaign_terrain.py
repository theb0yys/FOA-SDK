# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import json
import struct
import unittest
from foa_heightmap_importer import HeightmapImportError
from foa_scene_medusa import MedusaScene, MEMBERS, PROFILE
from foa_scene_mesh import SourceMesh
from foa_campaign_terrain import prepare, validate, compare_source, sha


class CampaignTerrainTests(unittest.TestCase):
    def setUp(self):
        self.ref = ('cab-'+'a'*32, 3)
        self.material = ('cab-'+'b'*32, -9)
        self.matrix = struct.pack('<12f', -2, 0, 0, .25, 3, 0, 0, 0, .5, 17, -4, 32)
        identity = struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
        renderers = [dict(lod_mask=1, instances=2, draws=(dict(mesh=self.ref, material=self.material, submesh=0),))]
        members = dict(zip(('Scene/'+n for n in MEMBERS), (self.matrix+identity+identity+self.matrix, bytes(98), struct.pack('<2I', 1, 0), bytes(8))))
        self.scene = MedusaScene('Scene', tuple(renderers), 2, 2, 2, members, PROFILE)
        self.mesh = SourceMesh({0: [(0., 0., 0.), (1., 2., 3.), (-1., 3., -2.)]}, [[(2, 0, 1)]],
                              dict(serialized_file=self.ref[0], path_id='3', record_sha256='c'*64), 0)
        self.meshes = {self.ref: self.mesh}
        self.raw = prepare(self.scene, self.meshes, 'hos', 'd'*64, 'e'*64)
        self.doc = json.loads(self.raw)

    def compare(self, doc=None):
        return compare_source(self.raw if doc is None else json.dumps(doc).encode(), self.scene, self.meshes, 'd'*64, 'e'*64)

    def test_original_vertices_indices_and_instance_order_survive(self):
        row = self.doc['groups'][0]; draw = row['rendering']['draw']
        self.assertEqual(bytes.fromhex(draw['indices']), struct.pack('<3I', 2, 0, 1))
        self.assertEqual(bytes.fromhex(draw['streams'][0]['hex']), b''.join(struct.pack('<3f', *v) for v in self.mesh.attributes[0]))
        self.assertEqual(row['source']['instance_indices'], [1, 0])
        self.assertEqual(bytes.fromhex(draw['stages'][0]['buffers'][0]['hex'])[48:], self.matrix)
        self.assertEqual(self.compare()['source_draws'], 2)

    def test_missing_original_mesh_fails_preparation(self):
        with self.assertRaises(HeightmapImportError): prepare(self.scene, {}, 'hos', 'd'*64, 'e'*64)

    def test_altered_vertex_with_rehashed_packet_fails_source_comparison(self):
        row=self.doc['groups'][0]; draw=row['rendering']['draw']; raw=bytearray.fromhex(draw['streams'][0]['hex'])
        struct.pack_into('<f', raw, 0, .25); draw['streams'][0]['hex']=raw.hex(); row['source']['vertices_sha256']=sha(raw)
        validate(json.dumps(self.doc).encode())
        with self.assertRaises(HeightmapImportError): self.compare(self.doc)

    def test_altered_triangle_with_rehashed_packet_fails_source_comparison(self):
        row=self.doc['groups'][0]; raw=struct.pack('<3I', 0, 2, 1); row['rendering']['draw']['indices']=raw.hex(); row['source']['indices_sha256']=sha(raw)
        with self.assertRaises(HeightmapImportError): self.compare(self.doc)

    def test_misplaced_section_with_rehashed_packet_fails_source_comparison(self):
        row=self.doc['groups'][0]; raw=bytearray.fromhex(row['rendering']['draw']['stages'][0]['buffers'][0]['hex'])
        struct.pack_into('<f', raw, 36, 7); row['rendering']['draw']['stages'][0]['buffers'][0]['hex']=raw.hex(); row['source']['matrices_sha256']=sha(raw)
        row['placement']['identity']['instances_sha256']=sha(struct.pack('<2I', 1, 0)+raw)
        validate(json.dumps(self.doc).encode())
        with self.assertRaises(HeightmapImportError): self.compare(self.doc)

    def test_missing_instance_with_recounted_packet_fails_source_comparison(self):
        row=self.doc['groups'][0]; raw=bytes.fromhex(row['rendering']['draw']['stages'][0]['buffers'][0]['hex'])[:48]
        row['rendering']['draw']['stages'][0]['buffers'][0]['hex']=raw.hex(); row['source']['matrices_sha256']=sha(raw)
        row['source']['instance_indices']=[1]; row['placement']['identity'].update(instance_count=1, instances_sha256=sha(struct.pack('<I', 1)+raw))
        row['rendering']['draw']['instance_count']=1; self.doc['source_draw_count']=1
        validate(json.dumps(self.doc).encode())
        with self.assertRaises(HeightmapImportError): self.compare(self.doc)

    def test_duplicate_group_and_range_gap_fail(self):
        for mode in ('duplicate', 'gap'):
            doc=copy.deepcopy(self.doc)
            if mode=='duplicate': doc['groups'].append(copy.deepcopy(doc['groups'][0]))
            else: doc['groups'][0]['placement']['identity']['first_instance']=1
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): validate(json.dumps(doc).encode())

    def test_source_material_submesh_or_fingerprint_substitution_fails(self):
        for kind in ('material', 'record', 'archive', 'renderer'):
            doc=copy.deepcopy(self.doc); row=doc['groups'][0]
            if kind=='material': row['source']['material'][1]=10
            elif kind=='record': row['source']['mesh_record_sha256']='f'*64
            elif kind=='archive': row['placement']['identity']['archive_sha256']='f'*64
            else: row['placement']['identity']['renderer_ordinal']=99
            with self.subTest(kind=kind), self.assertRaises(HeightmapImportError): self.compare(doc)

    def test_nonfinite_geometry_and_changed_byte_counts_fail(self):
        for kind in ('nan', 'count'):
            doc=copy.deepcopy(self.doc); row=doc['groups'][0]
            if kind=='nan':
                raw=bytearray.fromhex(row['rendering']['draw']['streams'][0]['hex']); struct.pack_into('<I',raw,0,0x7fc00000)
                row['rendering']['draw']['streams'][0]['hex']=raw.hex(); row['source']['vertices_sha256']=sha(raw)
            else: doc['vertex_bytes']+=4
            with self.subTest(kind=kind), self.assertRaises(HeightmapImportError): validate(json.dumps(doc).encode())

    def test_cancellation_prevents_preparation(self):
        with self.assertRaises(HeightmapImportError): prepare(self.scene, self.meshes, 'hos', 'd'*64, 'e'*64, lambda: True)


if __name__ == '__main__': unittest.main()
