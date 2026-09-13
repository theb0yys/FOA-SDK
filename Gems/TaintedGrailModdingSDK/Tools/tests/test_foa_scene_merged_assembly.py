# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Exact archive identities, source bits and native transaction failure contracts."""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_assembly as a
import foa_scene_merged_assembly as m
from foa_scene_merged_renderers import MergedRenderers
from foa_scene_render_assembly import prepare_render_batch, validate_render_batch, placement_digest
from test_foa_scene_merged_renderers import fixture, payload, GUID
from test_foa_scene_assembly import Host, finish


def source():
    sections = fixture()
    # Identical geometry/pose are distinct instances; preserve tiny values and signed zero.
    raw = bytearray(sections['instances'][0]); struct.pack_into('<II', raw, 4, 0x80000000, 1)
    sections['instances'] = [bytes(raw)] * 3
    return MergedRenderers(GUID, payload(sections))


def packet(): return m.prepare_batch(source(), [2, 0, 1], 'a'*64, 'hos')


class MergedAssemblyTests(unittest.TestCase):
    def test_exact_original_bits_order_and_distinct_instance_identities(self):
        rows = a.validate_batch(packet()); doc = a.decode(packet(), a.MAX_BATCH_BYTES)
        self.assertEqual([r.key[-1] for r in rows], [2, 0, 1])
        self.assertEqual(len(set(r.key for r in rows)), 3)
        self.assertEqual(len(set(placement_digest(r) for r in rows)), 3)
        for row, item in zip(rows, doc['entities']):
            original = source().record('instances', row.key[-1])
            self.assertEqual(bytes.fromhex(item['record_hex']), original)
            self.assertEqual(item['binding']['identity']['record_sha256'], hashlib.sha256(original).hexdigest())
            columns = struct.unpack('<12I', original[:48]); matrix = item['binding']['source_world_bits']
            for c in range(4):
                for r in range(3): self.assertEqual(matrix[r*4+c], columns[c*3+r])
            self.assertEqual(matrix[4], 0x80000000); self.assertEqual(matrix[8], 1)
            self.assertEqual(row.parent, -1); self.assertTrue(row.active_in_hierarchy)
            self.assertIsNone(item['binding']['source_parent']); self.assertNotIn('gameobject_id', item['binding']['identity'])

    def test_bad_source_identity_count_matrix_and_unknown_fields_reject(self):
        original = a.decode(packet(), a.MAX_BATCH_BYTES)
        cases = [(['version'], True), (['version'], 2), (['profile'], 'other'), (['source', 'map'], 'unknown'),
                 (['source', 'instance_count'], True), (['source', 'instance_count'], 1000001),
                 (['source', 'member_guid'], 'A'*32), (['source', 'archive_sha256'], 'z'*64),
                 (['entities', 0, 'binding', 'identity', 'instance_ordinal'], True),
                 (['entities', 0, 'binding', 'identity', 'instance_ordinal'], 3),
                 (['entities', 0, 'binding', 'identity', 'member_sha256'], 'a'*64),
                 (['entities', 0, 'binding', 'identity', 'record_sha256'], 'f'*64),
                 (['entities', 0, 'binding', 'source_parent'], {'path_id': '1'}),
                 (['entities', 0, 'binding', 'source_world_bits', 1], 0x80000000),
                 (['entities', 0, 'binding', 'source_local_bits', 1], True),
                 (['entities', 0, 'binding', 'schema_version'], 2),
                 (['entities', 0, 'record_hex'], 'x'*112)]
        for path, value in cases:
            doc = copy.deepcopy(original); target = doc
            for k in path[:-1]: target = target[k]
            target[path[-1]] = value
            with self.subTest(path=path), self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(doc))
        doc = copy.deepcopy(original); doc['entities'][0]['binding']['identity']['gameobject_id'] = '1'
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(doc))
        doc = copy.deepcopy(original); doc['entities'][1] = doc['entities'][0]
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(doc))
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(packet().replace(b'"version":1', b'"version":1,"version":1'))

    def test_nonfinite_record_cannot_be_authorized_by_a_matching_hash(self):
        doc = a.decode(packet(), a.MAX_BATCH_BYTES); item = doc['entities'][0]
        raw = bytearray.fromhex(item['record_hex']); struct.pack_into('<I', raw, 0, 0x7f800000)
        item['record_hex'] = raw.hex(); item['binding']['identity']['record_sha256'] = hashlib.sha256(raw).hexdigest()
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(a.encode(doc))

    def test_selection_bounds_cancellation_and_member_changes_reject(self):
        for selected in ([], [0, 0], [-1], [3], [True], list(range(4097))):
            with self.assertRaises(a.HeightmapImportError): m.prepare_batch(source(), selected, 'a'*64, 'hos')
        with self.assertRaises(a.HeightmapImportError): m.prepare_batch(source(), [0], 'a'*64, 'hos', lambda: True)
        with self.assertRaises(a.HeightmapImportError): a.validate_batch(packet(), lambda: True)
        changed = source(); changed.payload += b'x'
        with self.assertRaises(a.HeightmapImportError): m.prepare_batch(changed, [0], 'a'*64, 'hos')
        raw = packet()
        with patch.object(a, 'MAX_BATCH_BYTES', len(raw)-1), self.assertRaises(a.HeightmapImportError): a.validate_batch(raw)

    def test_native_transaction_creates_roots_and_rolls_back_late_failure(self):
        host = Host(); job = a.AssemblyJob(packet(), host)
        self.assertEqual(finish(job), 'PASSED'); self.assertEqual(host.entities, {'unrelated':'untouched', 1:None, 2:None, 3:None})
        for fail in (2, 'verify'):
            host = Host(fail); job = a.AssemblyJob(packet(), host)
            self.assertEqual(finish(job), 'FAILED'); self.assertEqual(host.entities, {'unrelated':'untouched'})
        host = Host(); stop = [False]; job = a.AssemblyJob(packet(), host, lambda: stop[0])
        job.step(limit=1); stop[0] = True
        self.assertEqual(finish(job), 'CANCELLED'); self.assertEqual(host.entities, {'unrelated':'untouched'})

    def test_render_packet_accepts_exact_merged_placements_and_rejects_stale_records(self):
        raw = packet(); rows = a.validate_batch(raw)
        binding = {'version':1, 'draw':{}, 'matrices':[]}
        rendering = prepare_render_batch(raw, [(placement_digest(row), binding) for row in rows])
        self.assertEqual(validate_render_batch(raw, rendering).draw_count, 3)
        with self.assertRaises(a.HeightmapImportError): validate_render_batch(raw+b' ', rendering)
        with self.assertRaises(a.HeightmapImportError): prepare_render_batch(raw, [('f'*64, binding)])
        job = a.AssemblyJob(raw, Host()); binding['draw']['changed'] = True
        self.assertEqual(finish(job), 'PASSED')


if __name__ == '__main__': unittest.main()
