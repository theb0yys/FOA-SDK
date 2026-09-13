# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Inverse placement, stale-binding rejection and unchanged member-byte checks."""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_assembly as a
import foa_scene_merged_candidate as c
import foa_scene_merged_assembly as m
from foa_scene_merged_renderers import MergedRenderers, Section
from test_foa_scene_merged_assembly import source


def inputs():
    member = source()
    packet = m.prepare_batch(member, [2, 0, 1], 'a'*64, 'hos')
    doc = a.decode(packet, a.MAX_BATCH_BYTES)
    rows = a.validate_batch(packet)
    edits = [dict(identity=copy.deepcopy(item['binding']['identity']), host_world_bits=list(row.host_world_bits))
             for item, row in zip(doc['entities'], rows)]
    return member, packet, edits


def run(member, packet, edits, **kwargs):
    return c.matrix_candidate(member, packet, edits, 'a'*64, 'hos', **kwargs)


class MergedCandidateTests(unittest.TestCase):
    def test_noop_preserves_every_original_byte_and_float_bit(self):
        member, packet, edits = inputs(); before = copy.deepcopy(edits)
        result, report = run(member, packet, edits)
        self.assertEqual(result, member.payload)
        self.assertEqual(report['candidate_sha256'], member.sha256)
        self.assertEqual(report['changed_instances'], [])
        self.assertEqual(report['game_export'], 'BLOCKED')
        self.assertEqual(edits, before)

    def test_move_rotation_scale_and_shear_have_exact_inverse(self):
        member, packet, edits = inputs()
        wanted = [2., .5, 0., 127., 0., 0., -3., -29., .25, 4., 0., 93., 0., 0., 0., 1.]
        edits[0]['host_world_bits'] = list(struct.unpack('<16I', struct.pack('<16f', *wanted)))
        result, report = run(member, packet, edits[:1])
        decoded = MergedRenderers(member.guid, result)
        self.assertEqual(decoded.instance(2)['local_to_world_columns'], (2., .25, 0., 0., 0., -3., .5, 4., 0., 127., 93., -29.))
        offset = member.sections['instances'].offset+2*56
        self.assertEqual(result[:offset], member.payload[:offset])
        self.assertEqual(result[offset+48:], member.payload[offset+48:])
        self.assertEqual(report['changed_instances'][0]['instance_ordinal'], 2)
        self.assertEqual(decoded.record('instances', 2)[48:], member.record('instances', 2)[48:])

    def test_reverse_native_translation_and_signed_zero_without_rounding(self):
        member, packet, edits = inputs()
        edits[0]['host_world_bits'][3] = 1
        edits[0]['host_world_bits'][7] = 0x80000000
        edits[0]['host_world_bits'][11] = 0x7f7fffff
        result, _ = run(member, packet, edits[:1])
        raw = MergedRenderers(member.guid, result).record('instances', 2)
        self.assertEqual(struct.unpack_from('<3I', raw, 36), (1, 0x7f7fffff, 0x80000000))

    def test_selection_order_does_not_change_candidate_bytes(self):
        member, packet, edits = inputs()
        for i, edit in enumerate(edits): edit['host_world_bits'][3] = 0x3f800000+i
        first, _ = run(member, packet, edits)
        second, _ = run(member, packet, list(reversed(edits)))
        self.assertEqual(first, second)
        self.assertEqual(member.sha256, hashlib.sha256(member.payload).hexdigest())

    def test_wrong_archive_map_member_or_record_rejected(self):
        member, packet, edits = inputs()
        for field, value in [('archive_sha256', 'b'*64), ('member_guid', 'b'*32),
                             ('member_sha256', 'b'*64), ('record_sha256', 'b'*64), ('instance_ordinal', 7)]:
            bad = copy.deepcopy(edits); bad[0]['identity'][field] = value
            with self.subTest(field=field), self.assertRaises(a.HeightmapImportError): run(member, packet, bad)
        with self.assertRaises(a.HeightmapImportError): c.matrix_candidate(member, packet, edits, 'b'*64, 'hos')
        with self.assertRaises(a.HeightmapImportError): c.matrix_candidate(member, packet, edits, 'a'*64, 'sarras')
        member.payload += b'x'
        with self.assertRaises(a.HeightmapImportError): run(member, packet, edits)

    def test_stale_self_consistent_placement_packet_cannot_replace_source(self):
        member, packet, edits = inputs(); doc = a.decode(packet, a.MAX_BATCH_BYTES)
        item = doc['entities'][0]; raw = bytearray.fromhex(item['record_hex']); raw[36] ^= 1
        item['record_hex'] = raw.hex(); item['binding']['identity']['record_sha256'] = hashlib.sha256(raw).hexdigest()
        item['binding']['source_local_bits'] = item['binding']['source_world_bits'] = m.expand_bits(bytes(raw))
        a.validate_batch(a.encode(doc))
        with self.assertRaises(a.HeightmapImportError): run(member, a.encode(doc), edits)

    def test_malformed_duplicate_and_unselected_edits_rejected(self):
        member, packet, edits = inputs()
        for bad in ([], (), [edits[0]]*2, [None], [dict(**edits[0], ignored=1)]):
            with self.subTest(bad=bad), self.assertRaises(a.HeightmapImportError): run(member, packet, bad)
        small = m.prepare_batch(member, [0], 'a'*64, 'hos')
        with self.assertRaises(a.HeightmapImportError): run(member, small, edits[:1])
        edits[0]['identity']['instance_ordinal'] = True
        with self.assertRaises(a.HeightmapImportError): run(member, packet, edits)

    def test_nonfinite_perspective_and_lossy_bottom_row_rejected(self):
        member, packet, edits = inputs()
        for index, bits in ((0,0x7f800000),(0,0x7fc00000),(12,1),(12,0x80000000),(15,0), (0,True),(0,-1),(0,0x100000000)):
            bad = copy.deepcopy(edits); bad[0]['host_world_bits'][index] = bits
            with self.subTest(index=index,bits=bits), self.assertRaises(a.HeightmapImportError): run(member, packet, bad)
        for value in ([], [0]*15, tuple(edits[0]['host_world_bits'])):
            bad = copy.deepcopy(edits); bad[0]['host_world_bits'] = value
            with self.assertRaises(a.HeightmapImportError): run(member, packet, bad)

    def test_cached_offsets_are_not_write_authority(self):
        member, packet, edits = inputs(); edits[0]['host_world_bits'][3] = 0x3f800000
        expected, _ = run(member, packet, edits)
        member.sections['instances'] = Section(3, 56, 0)
        actual, _ = run(member, packet, edits)
        self.assertEqual(actual, expected)

    def test_bounds_and_mid_operation_cancellation_preserve_inputs(self):
        member, packet, edits = inputs(); original = member.payload; before = copy.deepcopy(edits)
        with patch.object(a, 'MAX_BATCH_BYTES', len(packet)-1), self.assertRaises(a.HeightmapImportError): run(member, packet, edits)
        count = [0]
        def observe(): count[0] += 1; return False
        run(member, packet, edits, cancelled=observe); limit = count[0]//2; count[0] = 0
        def cancel(): count[0] += 1; return count[0] >= limit
        with self.assertRaises(a.HeightmapImportError): run(member, packet, edits, cancelled=cancel)
        self.assertEqual(member.payload, original); self.assertEqual(edits, before)


if __name__ == '__main__': unittest.main()
