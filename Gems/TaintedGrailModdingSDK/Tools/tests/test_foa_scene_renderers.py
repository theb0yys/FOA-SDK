# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic direct-renderer joins; no proprietary source fixtures."""
import copy
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_assembly as a
import foa_scene_render_assembly as rendering
import foa_scene_renderers as r
from foa_scene_component_audit import ComponentScriptAudit
from test_foa_scene_ownership import file, record, entity, pointer, graph


def asset(guid='a', sub=''):
    return {'m_AssetGUID': guid*32, 'm_SubObjectName': sub, 'm_SubObjectType': ''}


def fixture():
    f = file(); entity(f, 1, 2, children=[4], components=[5])
    entity(f, 3, 4, parent=2, components=[6, 8]); entity(f, 20, 21, components=[22])
    record(f, 100, 'MonoScript', dict(m_AssemblyName='Awaken.ECS', m_Namespace='Awaken.ECS.DrakeRenderer.Authoring', m_ClassName='DrakeLodGroup'))
    record(f, 101, 'MonoScript', dict(m_AssemblyName='Awaken.ECS', m_Namespace='Awaken.ECS.DrakeRenderer.Authoring', m_ClassName='DrakeMeshRenderer'))
    record(f, 102, 'MonoScript', dict(m_AssemblyName='Different', m_Namespace='Awaken.ECS.DrakeRenderer.Authoring', m_ClassName='DrakeMeshRenderer'))
    record(f, 5, 'MonoBehaviour', dict(m_GameObject=pointer(1), m_Script=pointer(100), children=[pointer(6), pointer(8)]))
    for ident in (6, 8, 22):
        record(f, ident, 'MonoBehaviour', dict(m_GameObject=pointer(3 if ident != 22 else 20),
            m_Script=pointer(101 if ident != 22 else 102), parentGroup=pointer(5), lodMask=3,
            meshReference=asset('b', 'Exact.Subobject'), materialReferences=[asset('c')]*(2 if ident == 6 else 1)))
    for obj in f.objects.values():
        obj.get_raw_data = lambda obj=obj: a.encode(obj.tree)
        obj.byte_size = len(obj.get_raw_data())
    return f


def inputs(f):
    g = graph(f); items = []; mat = [1065353216 if i % 5 == 0 else 0 for i in range(16)]
    for transform in (2, 4, 21):
        original = (f.name.lower(), transform); links = g.transforms[original]
        binding = {'schema_version': 1, 'profile': a.PROFILE,
            'identity': {'bundle_sha256': 'a'*64, 'serialized_file': f.name.lower(), 'path_id': str(transform),
                         'gameobject_id': str(links.owner[1]), 'record_sha256': hashlib.sha256(f.objects[transform].get_raw_data()).hexdigest()},
            'source_parent': {'serialized_file': f.name.lower(), 'path_id': str(links.parent[1])} if links.parent else None,
            'source_local_bits': mat, 'source_world_bits': mat, 'local_bounds': None}
        items.append(dict(name='Same name', active_self=True, active_in_hierarchy=True, kind='Transform', binding=binding))
    plan = {'schema': 'foa.source-hierarchy', 'version': 1, 'profile': a.PROFILE,
        'source': dict(map='hos', role='root', bundle_sha256='a'*64, serialized_file=f.name.lower(), snapshot_sha256='1'*64,
                       unity_input_sha256='2'*64, capture_sha256='3'*64, scene_nodes=3), 'entities': items}
    scripts = ComponentScriptAudit(f.objects.values(), lambda _: (_ for _ in ()).throw(AssertionError('Unexpected dependency')))
    return a.encode(plan), g, scripts


def prepare(f=None, **kwargs):
    return r.bind_direct_renderers(*inputs(f if f else fixture()), 'a'*64, **kwargs)


def native(count, start=0):
    draws = [{'version': 1, 'draw': {'sort_key': i}, 'matrices': []} for i in range(start, start+count)]
    return draws[0] if count == 1 else {'version': 3, 'draws': draws}


class DirectRendererTests(unittest.TestCase):
    def test_exact_owner_mesh_repeated_material_and_lod_links(self):
        scene = prepare(); self.assertEqual(len(scene.renderers), 2); self.assertEqual(len(scene.groups), 1)
        first, second = scene.renderers
        self.assertEqual(first.owner[1], '3'); self.assertEqual(first.transform[1], '4')
        self.assertEqual(first.parent_group, scene.groups[0].component); self.assertEqual(first.lod_mask, 3)
        self.assertEqual(first.mesh.key, 'b'*32+'[Exact.Subobject]')
        self.assertEqual(first.materials[0], first.materials[1]); self.assertEqual(len(second.materials), 1)
        self.assertEqual(scene.groups[0].owner[1], '1')
        self.assertEqual(scene.groups[0].children, (first.component, second.component))
        self.assertNotIn('22', [v.component[1] for v in scene.renderers])

    def test_native_import_combines_same_owner_and_keeps_all_ancestors(self):
        scene = prepare(); first, second = scene.renderers
        hierarchy, render = r.prepare_draw_import(scene, [(first, native(2)), (second, native(1, 2))])
        rows = a.validate_batch(hierarchy); self.assertEqual([row.key[1] for row in rows], ['2', '4'])
        checked = rendering.validate_render_batch(hierarchy, render)
        self.assertEqual(checked.draw_count, 3); self.assertEqual(len(checked.bindings), 1)
        group = json.loads(next(iter(checked.bindings.values())))
        self.assertEqual([d['draw']['sort_key'] for d in group['draws']], [0, 1, 2])

    def test_single_renderer_version_is_preserved_and_parent_group_not_implicitly_drawn(self):
        scene = prepare(); raw, render = r.prepare_draw_import(scene, [(scene.renderers[1], native(1))])
        checked = rendering.validate_render_batch(raw, render)
        self.assertEqual(json.loads(next(iter(checked.bindings.values())))['version'], 1)
        self.assertEqual(checked.draw_count, 1)

    def test_prepared_data_does_not_follow_mutations(self):
        f = fixture(); scene = prepare(f); f.objects[6].tree['materialReferences'].clear()
        self.assertEqual(len(scene.renderers[0].materials), 2)
        value = native(2); before = r.prepare_draw_import(scene, [(scene.renderers[0], value)])
        value['draws'].clear(); self.assertEqual(rendering.validate_render_batch(*before).draw_count, 2)

    def test_stale_bundle_transform_owner_parent_and_partial_hierarchy_reject(self):
        for kind in ('bundle', 'record', 'owner', 'parent', 'partial'):
            args = list(inputs(fixture())); doc = json.loads(args[0])
            if kind == 'bundle': doc['source']['bundle_sha256'] = 'f'*64
            elif kind == 'record': doc['entities'][1]['binding']['identity']['record_sha256'] = 'f'*64
            elif kind == 'owner': doc['entities'][1]['binding']['identity']['gameobject_id'] = '999'
            elif kind == 'parent': doc['entities'][1]['binding']['source_parent'] = None
            else: doc['entities'].pop()
            args[0] = a.encode(doc)
            with self.subTest(kind=kind), self.assertRaises(a.HeightmapImportError): r.bind_direct_renderers(*args, 'a'*64)

    def test_script_failure_is_not_silently_an_unmapped_renderer(self):
        args = list(inputs(fixture())); args[2].bindings[(file().name.lower(), 101)] = None
        with self.assertRaises(a.HeightmapImportError): r.bind_direct_renderers(*args, 'a'*64)

    def test_reciprocal_group_failures_reject(self):
        for kind in ('duplicate', 'missing', 'wrong-type', 'unclaimed', 'wrong-parent'):
            f = fixture()
            if kind == 'duplicate': f.objects[5].tree['children'].append(pointer(6))
            elif kind == 'missing': f.objects[5].tree['children'].append(pointer(999))
            elif kind == 'wrong-type': f.objects[5].tree['children'].append(pointer(22))
            elif kind == 'unclaimed': f.objects[5].tree['children'].pop()
            else: f.objects[6].tree['parentGroup'] = pointer(8)
            for obj in f.objects.values(): obj.byte_size = len(obj.get_raw_data())
            with self.subTest(kind=kind), self.assertRaises(a.HeightmapImportError): prepare(f)

    def test_exact_null_parent_allowed_but_malformed_null_rejects(self):
        f = fixture(); f.objects[5].tree['children'] = []; f.objects[6].tree['parentGroup'] = pointer(0); f.objects[8].tree['parentGroup'] = pointer(0)
        for obj in f.objects.values(): obj.byte_size = len(obj.get_raw_data())
        self.assertTrue(all(row.parent_group is None for row in prepare(f).renderers))
        for parent in ({'m_FileID': False, 'm_PathID': 0}, {'m_FileID': 1, 'm_PathID': 0}, {'m_PathID': 0}):
            f.objects[6].tree['parentGroup'] = parent; f.objects[6].byte_size = len(f.objects[6].get_raw_data())
            with self.assertRaises(a.HeightmapImportError): prepare(f)

    def test_invalid_asset_selector_lod_mask_and_material_count_reject(self):
        for kind in ('guid', 'subobject-type', 'fields', 'lod-mask', 'materials'):
            f = fixture(); tree = f.objects[6].tree
            if kind == 'guid': tree['meshReference']['m_AssetGUID'] = 'Display label'
            elif kind == 'subobject-type': tree['meshReference']['m_SubObjectType'] = 'Inferred'
            elif kind == 'fields': tree['meshReference']['other'] = 1
            elif kind == 'lod-mask': tree['lodMask'] = True
            else: tree['materialReferences'] = [asset('c')]*129
            f.objects[6].byte_size = len(f.objects[6].get_raw_data())
            with self.subTest(kind=kind), self.assertRaises(a.HeightmapImportError): prepare(f)

    def test_decode_inventory_edge_and_record_budgets(self):
        for name, limit in (('MAX_DECODE_BYTES', 1), ('MAX_RENDERERS', 1), ('MAX_GROUP_EDGES', 1), ('MAX_RECORD_BYTES', 1)):
            with patch.object(r, name, limit), self.assertRaises(a.HeightmapImportError): prepare()

    def test_cancelled_source_and_native_preparation(self):
        with self.assertRaises(a.HeightmapImportError): prepare(cancelled=lambda: True)
        scene = prepare()
        with self.assertRaises(a.HeightmapImportError): r.prepare_draw_import(scene, [(scene.renderers[1], native(1))], lambda: True)

    def test_stale_and_duplicate_native_renderer_selections_reject(self):
        scene = prepare(); row = scene.renderers[1]
        for selected in ([], [(replace(row, record_sha256='f'*64), native(1))], [(row, native(1)), (row, native(1, 1))], [('Same name', native(1))]):
            with self.assertRaises(a.HeightmapImportError): r.prepare_draw_import(scene, selected)

    def test_missing_ordinal_and_non_increasing_order_cannot_silently_collapse_draws(self):
        scene = prepare(); first, second = scene.renderers
        for selected in ([(first, native(1))], [(first, native(2)), (second, native(1))]):
            with self.assertRaises(a.HeightmapImportError): r.prepare_draw_import(scene, selected)

    def test_native_capacity_rejects_without_truncating(self):
        scene = prepare()
        with patch.object(r, 'MAX_DRAWS', 1), self.assertRaises(a.HeightmapImportError):
            r.prepare_draw_import(scene, [(scene.renderers[0], native(2))])

    def test_non_renderer_scene_is_valid_but_has_no_implicit_draws(self):
        f = fixture(); f.objects[101].tree['m_AssemblyName'] = 'Different'; f.objects[100].tree['m_AssemblyName'] = 'Different'
        scene = prepare(f); self.assertEqual(scene.renderers, ()); self.assertEqual(scene.groups, ())


if __name__ == '__main__': unittest.main()
