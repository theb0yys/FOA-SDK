# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_assembly as a
import foa_scene_render_assembly as r
from test_foa_scene_assembly import Host, finish, packet


def draw(version=1, ordinal=0):
    value = {'version': version, 'draw': {'sort_key': ordinal}, 'matrices': []}
    if version == 2: value['vectors'] = []
    return value


def group(count=2):
    return {'version': 3, 'draws': [draw(1 if i % 2 == 0 else 2, i) for i in range(count)]}


def render_packet(raw=None):
    raw = raw if raw is not None else packet(); rows = a.validate_batch(raw)
    return r.prepare_render_batch(raw, [(r.placement_digest(rows[0]), group()), (r.placement_digest(rows[1]), draw(2))])


class RenderAssemblyTests(unittest.TestCase):
    def test_exact_source_identity_with_identical_labels_and_immutable_result(self):
        raw = packet(); rows = a.validate_batch(raw); self.assertEqual(rows[0].name, rows[1].name)
        payload = render_packet(raw); validated = r.validate_render_batch(raw, payload)
        self.assertEqual(validated.draw_count, 3)
        self.assertNotEqual(r.placement_digest(rows[0]), r.placement_digest(rows[1]))
        self.assertEqual(json.loads(validated.bindings[r.placement_digest(rows[0])]), group())
        with self.assertRaises(TypeError): validated.bindings[r.placement_digest(rows[0])] = 'changed'
        changed = json.loads(payload); changed['renderers'][0]['binding']['draws'][0]['draw']['sort_key'] = 9
        self.assertEqual(json.loads(validated.bindings[r.placement_digest(rows[0])]), group())

    def test_caller_mutation_cannot_change_prepared_packet(self):
        raw = packet(); binding = group(); source = r.placement_digest(a.validate_batch(raw)[0])
        payload = r.prepare_render_batch(raw, [(source, binding)]); binding['draws'].clear()
        self.assertEqual(r.validate_render_batch(raw, payload).draw_count, 2)

    def test_stale_hierarchy_bytes_and_cross_source_placement_reject(self):
        raw = packet(); payload = render_packet(raw)
        with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw+b' ', payload)
        for field, value in [('bundle_sha256', 'b'*64), ('record_sha256', 'b'*64), ('gameobject_id', '999')]:
            changed = json.loads(raw); changed['entities'][0]['binding']['identity'][field] = value
            stale = json.loads(payload)
            import hashlib
            stale['hierarchy_sha256'] = hashlib.sha256(a.encode(changed)).hexdigest()
            with self.subTest(field=field), self.assertRaises(a.HeightmapImportError):
                r.validate_render_batch(a.encode(changed), a.encode(stale))

    def test_duplicate_unknown_and_label_based_bindings_reject(self):
        raw = packet(); payload = json.loads(render_packet(raw))
        for key in ('unknown', 'Same name', 'b'*64):
            changed = copy.deepcopy(payload); changed['renderers'][0]['placement_sha256'] = key
            with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, a.encode(changed))
        payload['renderers'][1] = payload['renderers'][0]
        with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, a.encode(payload))

    def test_closed_versions_and_duplicate_json_fields(self):
        raw = packet(); payload = render_packet(raw)
        for version in (0, 2, True, '1'):
            changed = json.loads(payload); changed['version'] = version
            with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, a.encode(changed))
        for target in ('top', 'entry'):
            changed = json.loads(payload); (changed if target == 'top' else changed['renderers'][0])['extra'] = 1
            with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, a.encode(changed))
        with self.assertRaises(a.HeightmapImportError):
            r.validate_render_batch(raw, payload.replace(b'"schema":', b'"version":1,"schema":', 1))

    def test_native_binding_versions_and_group_order_reject_before_host(self):
        raw = packet(); source = r.placement_digest(a.validate_batch(raw)[0]); bad = []
        for version in (0, 4, True, '1'): bad.append({'version': version})
        bad += [{'version': 3, 'draws': []}, {'version': 3, 'draws': [group()]}, group(129)]
        for value in (0, -1, True, 0x100000000):
            member = group(); member['draws'][1]['draw']['sort_key'] = value; bad.append(member)
        member = group(); member['draws'][0]['version'] = True; bad.append(member)
        for value in bad:
            with self.subTest(value=str(value)[:80]), self.assertRaises(a.HeightmapImportError):
                r.prepare_render_batch(raw, [(source, value)])

    def test_byte_entity_and_draw_budgets(self):
        raw = packet(); payload = render_packet(raw)
        with patch.object(r, 'MAX_RENDER_BYTES', len(payload)-1), self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, payload)
        with patch.object(r, 'MAX_DRAW_BYTES', 1), self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, payload)
        with patch.object(r, 'MAX_DRAWS', 2), self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, payload)
        value = json.loads(payload); value['renderers'] = []
        with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, a.encode(value))
        value['renderers'] = json.loads(payload)['renderers']*2
        with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(raw, a.encode(value))

    def test_preparation_cancelled_without_native_mutation(self):
        with self.assertRaises(a.HeightmapImportError): r.validate_render_batch(packet(), render_packet(), lambda: True)
        with self.assertRaises(a.HeightmapImportError):
            r.prepare_render_batch(packet(), [(r.placement_digest(a.validate_batch(packet())[0]), draw())], lambda: True)

    def test_old_hierarchy_only_job_and_packet_remain_unchanged(self):
        raw = packet(); job = a.AssemblyJob(raw, Host())
        self.assertEqual(finish(job), 'PASSED'); self.assertEqual(json.loads(raw)['version'], 1)
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(render_packet(raw))

    def test_transaction_waits_and_yields_before_success(self):
        host = Host(); ready = set(); seen = []
        def check(eid, row): seen.append(eid); return eid in ready
        job = a.AssemblyJob(packet(), host, ready=check)
        self.assertEqual(job.step(), 'WAITING'); self.assertEqual(len(job.created), 2); self.assertEqual(seen, [1])
        for _ in range(3): self.assertEqual(job.step(), 'WAITING')
        self.assertEqual(len(seen), 4); ready.add(1)
        self.assertEqual(job.step(), 'WAITING'); ready.add(2)
        self.assertEqual(job.step(), 'PASSED'); self.assertEqual(host.entities, {'unrelated': 'untouched', 1: None, 2: 1})

    def test_late_render_failure_rolls_back_all_created_objects(self):
        host = Host()
        def ready(eid, row):
            if eid == 2: raise RuntimeError('Shader resource rejected')
            return True
        job = a.AssemblyJob(packet(), host, ready=ready)
        self.assertEqual(finish(job), 'FAILED'); self.assertEqual(host.entities, {'unrelated': 'untouched'})
        self.assertIn('Shader resource rejected', job.error)

    def test_timeout_rolls_back_and_cannot_be_reported_as_success(self):
        now = [0.]; host = Host(); job = a.AssemblyJob(packet(), host, clock=lambda: now[0], ready=lambda *_: False, ready_timeout=1.)
        self.assertEqual(job.step(), 'WAITING'); now[0] = 1.
        self.assertEqual(finish(job), 'FAILED'); self.assertIn('timed out', job.error)
        self.assertEqual(host.entities, {'unrelated': 'untouched'}); self.assertEqual(job.step(), 'FAILED')

    def test_cancellation_during_loading_rolls_back(self):
        stopped = [False]; host = Host(); job = a.AssemblyJob(packet(), host, lambda: stopped[0], ready=lambda *_: False)
        self.assertEqual(job.step(), 'WAITING'); stopped[0] = True
        self.assertEqual(finish(job), 'CANCELLED'); self.assertEqual(host.entities, {'unrelated': 'untouched'})

    def test_readiness_and_rollback_each_obey_operation_limits(self):
        host = Host(); now = [0.]; job = a.AssemblyJob(packet(), host, clock=lambda: now[0], ready=lambda *_: False)
        self.assertEqual(job.step(limit=1), 'RUNNING'); self.assertEqual(job.step(limit=1), 'RUNNING')
        self.assertEqual(job.step(limit=1), 'WAITING'); now[0] = 121.
        self.assertEqual(job.step(limit=1), 'ROLLING_BACK'); self.assertEqual(len(host.entities), 2)
        self.assertEqual(job.step(limit=1), 'ROLLING_BACK'); self.assertEqual(len(host.entities), 1)
        self.assertEqual(job.step(limit=1), 'FAILED'); self.assertEqual(job.peak_step_entities, 1)

    def test_cleanup_failure_after_render_failure_retains_recovery_ids(self):
        host = Host('cleanup'); job = a.AssemblyJob(packet(), host, ready=lambda *_: 'FAILED')
        self.assertEqual(finish(job), 'CLEANUP_FAILED'); self.assertEqual({eid for eid, _ in job.cleanup_errors}, {1, 2})
        self.assertEqual(host.entities['unrelated'], 'untouched')

    def test_invalid_readiness_configuration_does_not_call_host(self):
        for options in ({'ready': True}, {'ready_timeout': True}, {'ready_timeout': 0}, {'ready_timeout': 121}, {'ready_timeout': float('nan')}):
            host = Host()
            with self.assertRaises(a.HeightmapImportError): a.AssemblyJob(packet(), host, **options)
            self.assertEqual(host.calls, [])


if __name__ == '__main__': unittest.main()
