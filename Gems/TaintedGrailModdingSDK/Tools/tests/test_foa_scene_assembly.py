# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_assembly as a
from test_foa_scene_transform_capture import fixture as capture_fixture


def fixture():
    raw, cap = capture_fixture(); nodes = json.loads(raw)['nodes']; rows = []
    for i, node in enumerate(nodes):
        ref = lambda n: {'serialized_file': 'synthetic-scene', 'path_id': str(n+1)}
        rows.append({'transform': ref(i), 'owner': ref(i+100), 'parent': ref(node['parent']) if node['parent'] >= 0 else None,
                     'children': [ref(j) for j, child in enumerate(nodes) if child['parent'] == i],
                     'kind': node['kind'], 'name': 'Same name', 'active_self': i == 0, 'active_in_hierarchy': i == 0,
                     'position': list(node['p'].values()), 'rotation': list(node['q'].values()), 'scale': list(node['s'].values()),
                     'record_sha256': hashlib.sha256(str(i).encode()).hexdigest()})
    return b'\n'.join(a.encode(row) for row in rows), raw, a.encode(cap)


def plan():
    return a.prepare_scene(*fixture(), 'a'*64, 'hos', 'root')


def packet():
    return a.select_batch(plan(), [('synthetic-scene', '2')])


class Host:
    def __init__(self, fail=None):
        self.entities = {'unrelated': 'untouched'}; self.calls = []; self.next_id = 0; self.fail = fail
    def preflight(self, rows):
        self.calls.append('preflight')
        if self.fail == 'preflight': raise RuntimeError('conflict')
    def create(self, parent):
        self.next_id += 1; eid = self.next_id; self.entities[eid] = parent; self.calls.append(('create', eid)); return eid
    def configure(self, eid, row):
        if eid == self.fail: raise RuntimeError('binding failed')
    def verify(self, eid, row, parent):
        if self.fail == 'verify': raise RuntimeError('readback failed')
    def remove(self, eid):
        if self.fail == 'cleanup': raise RuntimeError('cleanup failed')
        del self.entities[eid]; self.calls.append(('remove', eid))


def finish(job):
    for _ in range(100):
        state = job.step(limit=1)
        if state in ('PASSED', 'FAILED', 'CANCELLED', 'CLEANUP_FAILED'): return state
    raise AssertionError('Assembly did not terminate')


class AssemblyTests(unittest.TestCase):
    def test_captured_bits_and_exact_ancestor_closure(self):
        data = plan(); raw = a.select_batch(data, [('synthetic-scene', '2')]); rows = a.validate_batch(raw)
        self.assertEqual(len(rows), 2); self.assertEqual(rows[1].parent, 0)
        self.assertEqual(rows[0].name, rows[1].name)
        self.assertFalse(rows[1].active_in_hierarchy)
        cap = json.loads(fixture()[2]); order = (0, 2, 1, 3)
        self.assertEqual(rows[1].host_world_bits, tuple(cap['rows'][1]['worldBits'][order[r]*4+order[c]] & 0xffffffff
                                                      for r in range(4) for c in range(4)))
        self.assertEqual(json.loads(raw)['source']['scene_nodes'], 2)
        one = a.select_batch(data, [('synthetic-scene', '1')]); self.assertEqual(len(a.validate_batch(one)), 1)
        self.assertEqual(json.loads(one)['source']['scene_nodes'], 2)

    def test_mutating_caller_plan_cannot_change_validated_job(self):
        data = plan(); host = Host(); job = a.AssemblyJob(a.encode(data), host)
        data['entities'][0]['name'] = 'altered'; self.assertEqual(job.rows[0].name, 'Same name')
        self.assertEqual(finish(job), 'PASSED'); self.assertEqual(host.entities, {'unrelated': 'untouched', 1: None, 2: 1})
        self.assertEqual(job.peak_step_entities, 1)

    def test_late_failure_rolls_back_every_created_entity(self):
        for fail in (1, 2, 'verify'):
            host = Host(fail); job = a.AssemblyJob(packet(), host)
            self.assertEqual(finish(job), 'FAILED'); self.assertEqual(host.entities, {'unrelated': 'untouched'})
            self.assertTrue(job.error); self.assertEqual(job.cleanup_errors, [])

    def test_cancellation_rolls_back_without_touching_existing_entities(self):
        host = Host(); stopped = [False]; job = a.AssemblyJob(packet(), host, lambda: stopped[0])
        self.assertEqual(job.step(limit=1), 'RUNNING'); stopped[0] = True
        self.assertEqual(finish(job), 'CANCELLED'); self.assertEqual(host.entities, {'unrelated': 'untouched'})
        self.assertEqual(job.step(), 'CANCELLED')

    def test_cleanup_failure_retains_ids_for_recovery(self):
        host = Host('cleanup'); stopped = [False]; job = a.AssemblyJob(packet(), host, lambda: stopped[0])
        job.step(limit=1); stopped[0] = True
        self.assertEqual(finish(job), 'CLEANUP_FAILED'); self.assertEqual(job.cleanup_errors[0][0], 1)
        self.assertIn(1, host.entities)

    def test_rejects_before_mutation_and_preflight_conflicts(self):
        data = json.loads(packet()); data['entities'][1]['binding']['source_parent'] = None
        host = Host()
        # Its active flag is already false; use a genuinely absent parent, not a valid second root.
        data['entities'][1]['binding']['source_parent'] = {'serialized_file': 'synthetic-scene', 'path_id': '999'}
        with self.assertRaises(a.HeightmapImportError): a.AssemblyJob(a.encode(data), host)
        self.assertEqual(host.calls, [])
        with self.assertRaises(RuntimeError): a.AssemblyJob(packet(), Host('preflight'))

    def test_identity_hierarchy_schema_and_bit_negatives(self):
        original = json.loads(packet()); bad = []
        def change(path, value):
            data = copy.deepcopy(original); target = data
            for key in path[:-1]: target = target[key]
            target[path[-1]] = value; bad.append(data)
        for path, value in [(['version'], True), (['version'], 2), (['profile'], 'unity-other'),
                (['source', 'map'], 'fake'), (['source', 'scene_nodes'], True),
                (['source', 'capture_sha256'], 'x'*64), (['entities', 1, 'active_in_hierarchy'], 1),
                (['entities', 0, 'active_in_hierarchy'], False), (['entities', 1, 'name'], 'bad\0name'),
                (['entities', 1, 'binding', 'identity', 'path_id'], '1'),
                (['entities', 1, 'binding', 'identity', 'gameobject_id'], '101'),
                (['entities', 1, 'binding', 'identity', 'bundle_sha256'], 'b'*64),
                (['entities', 1, 'binding', 'identity', 'path_id'], '9223372036854775808'),
                (['entities', 1, 'binding', 'identity', 'path_id'], 2),
                (['entities', 1, 'binding', 'identity', 'path_id'], '02'),
                (['entities', 1, 'binding', 'source_parent', 'path_id'], '2'),
                (['entities', 0, 'binding', 'source_world_bits', 0], True),
                (['entities', 0, 'binding', 'source_world_bits', 0], 0x7fc00000),
                (['entities', 0, 'binding', 'source_world_bits', 15], 0)]: change(path, value)
        for i, data in enumerate(bad):
            with self.subTest(case=i), self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(data))
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(packet().replace(b'"version":1', b'"version":1,"version":1'))
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(packet(), lambda: True)

    def test_duplicate_missing_and_over_capacity_selection_reject(self):
        data = plan()
        for selection in ([], [('synthetic-scene', '999')], [('synthetic-scene', '2')]*2):
            with self.assertRaises(a.HeightmapImportError): a.select_batch(data, selection)
        with patch.object(a, 'MAX_BATCH_NODES', 1), self.assertRaises(a.HeightmapImportError):
            a.select_batch(data, [('synthetic-scene', '2')])
        with patch.object(a, 'MAX_BATCH_BYTES', 10), self.assertRaises(a.HeightmapImportError): a.validate_batch(packet())

    def test_snapshot_capture_and_reciprocal_child_mismatches_reject(self):
        snap, raw, cap = fixture(); rows = [json.loads(line) for line in snap.splitlines()]
        variants = []
        for field, value in [('children', []), ('position', [100., 0., 0.]), ('kind', 'RectTransform')]:
            altered = copy.deepcopy(rows); altered[0][field] = value; variants.append(altered)
        altered = copy.deepcopy(rows); altered[0]['children'] *= 2; variants.append(altered)
        variants += [rows[:1], rows + rows[:1], list(reversed(rows))]
        for variant in variants:
            with self.assertRaises(a.HeightmapImportError): a.prepare_scene(b'\n'.join(map(a.encode, variant)), raw, cap, 'a'*64, 'hos', 'root')

    def test_version_one_remains_readable_and_unknown_versions_reject(self):
        old = plan()
        for row in old['entities']:
            row['binding']['schema_version'] = 1; del row['binding']['native_anchor_bits']
        self.assertEqual(len(a.validate_batch(a.encode(old))), 2)
        for version in (0, 3, True, '2'):
            bad = plan(); bad['entities'][0]['binding']['schema_version'] = version
            with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(bad))
        bad = plan(); bad['entities'][0]['binding']['schema_version'] = 1
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(bad))
        bad = plan(); del bad['entities'][0]['binding']['native_anchor_bits']
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(bad))

    def test_rounded_anchor_does_not_replace_original_source_matrix(self):
        import struct
        data = plan(); parent = data['entities'][0]['binding']; child = data['entities'][1]['binding']
        to_bits = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]
        # Values chosen to lose a bit when the native local coordinate is stored.
        parent['source_world_bits'][3] = to_bits(1024.)
        parent['native_anchor_bits'][0] = to_bits(1024.)
        child['source_world_bits'][3] = to_bits(204.62120056152344)
        expected = a.derive_anchor(tuple(child['source_world_bits'][i] for i in (3, 11, 7)), tuple(parent['native_anchor_bits']))
        child['native_anchor_bits'] = list(expected)
        rows = a.validate_batch(a.encode(data))
        self.assertNotEqual(rows[1].native_anchor_bits[0], rows[1].host_world_bits[3])
        self.assertEqual(rows[1].host_world_bits[3], to_bits(204.62120056152344))
        for value in ([True, 0, 0], [0, 0, 0], [0xffffffff]*3, expected[:2]):
            bad = copy.deepcopy(data); bad['entities'][1]['binding']['native_anchor_bits'] = list(value)
            with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(bad))

    def test_wide_source_child_lists_are_preserved_without_truncation(self):
        snap, raw, captured = fixture(); original = json.loads(snap.splitlines()[0])
        node = json.loads(raw)['nodes'][0]; cap = json.loads(captured); measured = cap['rows'][0]
        # A real campaign has child inventories larger than 64 KiB. Construct an
        # independent flat hierarchy with identity-local children for regression.
        import foa_scene_transform_capture as c
        from foa_scene_transforms import IDENTITY
        count = 1500
        root = copy.deepcopy(original)
        root['children'] = [{'serialized_file': 'synthetic-scene', 'path_id': str(i+2)} for i in range(count)]
        source_rows = [root]; nodes = [node]; captures = [measured]
        for i in range(count):
            row = copy.deepcopy(original); row['transform']['path_id'] = str(i+2); row['owner']['path_id'] = str(i+10001)
            row['children'] = []; row['parent'] = root['transform']; row['position'] = [0., 0., 0.]; row['scale'] = [1., 1., 1.]
            source_rows.append(row)
            child = {'parent': 0, 'kind': 'Transform', 'p': dict(zip('xyz', row['position'])),
                     'q': dict(zip('xyzw', row['rotation'])), 's': dict(zip('xyz', row['scale']))}
            nodes.append(child); measurement = copy.deepcopy(measured)
            for field in ('p', 'q', 's'): measurement[field] = child[field]
            measurement['local'] = list(IDENTITY); measurement['localBits'] = list(c.bits(IDENTITY))
            measurement['inputBits'] = measurement['assignedBits'] = list(c.bits([*row['position'], *row['rotation'], *row['scale']]))
            captures.append(measurement)
        self.assertGreater(len(a.encode(root)), 65536)
        inp = a.encode({'nodes': nodes}); cap['inputSha256'] = hashlib.sha256(inp).hexdigest(); cap['rows'] = captures
        scene = a.prepare_scene(b'\n'.join(map(a.encode, source_rows)), inp, a.encode(cap), 'a'*64, 'hos', 'root')
        self.assertEqual(len(scene['entities']), count+1)
        self.assertEqual(len(a.validate_batch(a.select_batch(scene, [('synthetic-scene', str(count+1))]))), 2)

    def test_time_budget_yields_and_invalid_budgets_do_not_mutate(self):
        times = iter([0., 1., 2., 3.]); host = Host(); job = a.AssemblyJob(packet(), host, clock=lambda: next(times))
        self.assertEqual(job.step(), 'RUNNING'); self.assertEqual(len(job.created), 1)
        for kwargs in ({'limit': 0}, {'limit': 33}, {'seconds': float('nan')}, {'seconds': 1.}):
            with self.assertRaises(a.HeightmapImportError): job.step(**kwargs)


if __name__ == '__main__': unittest.main()
