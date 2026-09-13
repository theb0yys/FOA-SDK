#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Auditor negative tests use invented report bytes; they are not native Unity evidence."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import verify_foa_heightmap_native_probe as probe


class NativeProbeAuditorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='foa-m6-auditor-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.root.joinpath('source.u16le').write_bytes(probe.source_bytes())
        expected = [probe.sample(x, 32-z) / 65535 for z in range(33) for x in range(33)]
        raw = struct.pack('<1089f', *expected)
        actual = struct.unpack('<1089f', raw)
        error = max(abs(a-e) for a, e in zip(actual, expected))
        base = {'schema': 'foa.m6.native-heightmap-qualification', 'schemaVersion': 1,
                'status': 'PASSED', 'error': '', 'unityVersion': '6000.0.64f1',
                'sourceSha256': probe.sha(probe.source_bytes()), 'width': 33, 'height': 33,
                'sampleSpacingMetres': 1, 'minimumHeightMetres': -2, 'maximumHeightMetres': 6,
                'normalizedTolerance': probe.TOLERANCE, 'rejectionChecks': 4,
                'terrainGuid': 'a'*32, 'prefabGuid': 'b'*32, 'repeatedBundleBytesEqual': True,
                'observations': [], 'artifacts': [], **{key: False for key in probe.AUTHORITY}}
        observations = []
        for stage in probe.STAGES:
            self.root.joinpath(stage + '.f32le').write_bytes(raw)
            observations.append({'stage': stage, 'decodedSha256': probe.sha(raw), 'comparedSamples': 1089,
                                 'collisionProbes': 0 if stage == 'asset-reopened' else 5,
                                 'maxNormalizedError': error, 'maxElevationErrorMetres': error*8,
                                 'maxCollisionErrorMetres': 0,
                                 'cornerHeights': [actual[0], actual[32], actual[1056], actual[1088]],
                                 'size': {'x': 32, 'y': 8, 'z': 32}, 'origin': {'x': 0, 'y': -2, 'z': 0}})
        for folder in ('build-a', 'build-b'):
            self.root.joinpath(folder).mkdir()
            for name in ('foa-m6-terrain', 'foa-m6-terrain.manifest', folder, folder + '.manifest'):
                data = b'unit-test invented artifact; not a Unity bundle'
                relative = folder + '/' + name
                self.root.joinpath(relative).write_bytes(data)
                base['artifacts'].append({'path': relative, 'bytes': len(data), 'sha256': probe.sha(data)})
        self.initial = copy.deepcopy(base)
        self.initial['observations'] = observations[:4]
        self.reopened = copy.deepcopy(base)
        self.reopened['observations'] = observations[4:]
        self.save()

    def save(self):
        self.root.joinpath('result.json').write_text(json.dumps(self.initial), encoding='utf-8')
        self.root.joinpath('reopen.json').write_text(json.dumps(self.reopened), encoding='utf-8')

    def reject(self):
        self.save()
        with self.assertRaises((ValueError, OSError)):
            probe.verify(self.root)

    def test_complete_report_inventory_is_accepted(self):
        result = probe.verify(self.root)
        self.assertEqual(result['samples_per_stage'], 1089)
        self.assertEqual(result['runtime_signoff'], 'NOT_RUN')
        self.assertEqual(result['m2_isolation_qualification'], 'NOT_RUN')

    def test_rotated_readback_rejected_even_with_matching_hash(self):
        values = struct.unpack('<1089f', self.root.joinpath('created.f32le').read_bytes())
        changed = struct.pack('<1089f', *reversed(values))
        self.root.joinpath('created.f32le').write_bytes(changed)
        self.initial['observations'][0]['decodedSha256'] = probe.sha(changed)
        self.reject()

    def test_truncated_readback_rejected(self):
        self.root.joinpath('bundle-a.f32le').write_bytes(b'bad')
        self.reject()

    def test_nonfinite_readback_rejected(self):
        changed = struct.pack('<1089f', *([float('nan')] * 1089))
        self.root.joinpath('created.f32le').write_bytes(changed)
        self.initial['observations'][0]['decodedSha256'] = probe.sha(changed)
        self.reject()

    def test_source_tampering_rejected(self):
        self.root.joinpath('source.u16le').write_bytes(bytes(2178))
        self.reject()

    def test_all_authority_fields_fail_closed(self):
        for key in probe.AUTHORITY:
            with self.subTest(key=key):
                self.initial[key] = True
                self.reject()
                self.initial[key] = False
        del self.reopened[probe.AUTHORITY[0]]
        self.reject()

    def test_missing_or_duplicate_stage_rejected(self):
        self.initial['observations'][1] = copy.deepcopy(self.initial['observations'][0])
        self.reject()

    def test_no_fresh_process_report_is_not_a_pass(self):
        self.root.joinpath('reopen.json').unlink()
        with self.assertRaises(OSError):
            probe.verify(self.root)

    def test_bundle_drift_rejected(self):
        self.root.joinpath('build-b/foa-m6-terrain').write_bytes(b'changed')
        self.reject()

    def test_duplicate_inventory_rejected(self):
        self.initial['artifacts'][1] = self.initial['artifacts'][0]
        self.reject()

    def test_traversal_rejected(self):
        for path in ('../outside', '/outside', 'C:/outside', 'build-a\\outside', 'build-a//outside'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                probe.read(self.root, path, 100)

    def test_larger_tolerance_rejected(self):
        self.initial['normalizedTolerance'] = 1
        self.reject()

    def test_wrong_version_or_editor_rejected(self):
        self.initial['schemaVersion'] = 2
        self.reject()
        self.initial['schemaVersion'] = 1
        self.initial['unityVersion'] = '6000.0.41f1'
        self.reject()

    def test_missing_collisions_rejected(self):
        self.reopened['observations'][0]['collisionProbes'] = 0
        self.reject()

    def test_extent_and_comparison_summary_rejected(self):
        self.initial['observations'][0]['size']['z'] = 33
        self.reject()
        self.initial['observations'][0]['size']['z'] = 32
        self.initial['observations'][0]['comparedSamples'] = 0
        self.reject()

    def test_repeatability_claim_checked_against_bytes(self):
        self.initial['repeatedBundleBytesEqual'] = False
        self.reject()

    def test_duplicate_json_key_rejected(self):
        path = self.root.joinpath('result.json')
        path.write_text(path.read_text(encoding='utf-8').replace('"schemaVersion": 1', '"schemaVersion": 2, "schemaVersion": 1'), encoding='utf-8')
        with self.assertRaises(ValueError):
            probe.verify(self.root)

    def test_report_size_bounded(self):
        self.initial['padding'] = 'x' * 65536
        self.reject()

    def test_hard_link_rejected(self):
        self.root.joinpath('linked').hardlink_to(self.root.joinpath('source.u16le'))
        self.reject()


class CoreBindingAuditorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='foa-m6-binding-auditor-')
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / 'terrain-input.bin'
        self.document = {'schema': 'foa.terrain-heightmap', 'schema_version': 1,
                         'tiles': [{'sha256': 'sha256:' + probe.sha(probe.source_bytes()), 'byte_size': 2178}],
                         'authority': {key: False for key in ('runtime_use_allowed', 'deployment_allowed',
                             'publication_allowed', 'packaging_allowed', 'game_write_allowed', 'evidence_promotion_allowed')}}
        self.encode()

    def encode(self):
        document = json.dumps(self.document).encode('utf-8')
        samples = probe.source_bytes()
        self.data = (b'FOAHM001' + struct.pack('<II', len(document), len(samples)) +
                     probe.sha(document).encode('ascii') + probe.sha(samples).encode('ascii') + document + samples)
        self.input_fp, self.doc_fp = 'sha256:' + probe.sha(self.data), 'sha256:' + probe.sha(document)
        self.path.write_bytes(self.data)
        self.initial = {'sourceKind': 'core-terrain-handoff', 'inputFingerprint': self.input_fp,
                        'sourceDocumentFingerprint': self.doc_fp, 'coreInputRejectionChecks': 12}
        self.reopened = copy.deepcopy(self.initial)

    def verify(self):
        probe.verify_core_binding(self.initial, self.reopened, self.path, self.input_fp, self.doc_fp)

    def test_matching_core_binding_passes(self):
        self.verify()

    def test_core_claim_without_independent_input_is_rejected(self):
        with self.assertRaises(ValueError):
            probe.verify_core_binding(self.initial, self.reopened, None, None, None)

    def test_missing_negative_checks_or_changed_reopen_binding_is_rejected(self):
        for key, value in [('coreInputRejectionChecks', 0), ('inputFingerprint', 'sha256:' + '0'*64),
                           ('sourceDocumentFingerprint', 'sha256:' + '0'*64), ('sourceKind', 'sdk-fixed')]:
            with self.subTest(key=key):
                self.reopened[key] = value
                with self.assertRaises(ValueError): self.verify()
                self.reopened = copy.deepcopy(self.initial)

    def test_input_drift_truncation_and_trailing_bytes_are_rejected(self):
        for data in (self.data[:-1], self.data + b'x', self.data[:-1] + bytes([self.data[-1] ^ 1])):
            self.path.write_bytes(data)
            with self.assertRaises(ValueError): self.verify()

    def test_forged_packet_lengths_and_version_are_rejected_even_if_rehashed(self):
        for offset in (7, 8, 12):
            changed = bytearray(self.data); changed[offset] ^= 1
            self.path.write_bytes(changed); self.input_fp = 'sha256:' + probe.sha(changed)
            with self.assertRaises(ValueError): self.verify()

    def test_canonical_authority_and_sample_binding_cannot_be_promoted(self):
        self.document['authority']['runtime_use_allowed'] = True
        self.encode()
        with self.assertRaises(ValueError): self.verify()
        self.document['authority']['runtime_use_allowed'] = False
        self.document['tiles'][0]['sha256'] = 'sha256:' + '0'*64
        self.encode()
        with self.assertRaises(ValueError): self.verify()

    def test_fixed_fixture_cannot_claim_a_core_binding(self):
        for initial in ({}, {'sourceKind': 'sdk-fixed'}, {'sourceKind': 'sdk-fixed', 'inputFingerprint': self.input_fp}):
            with self.assertRaises(ValueError):
                probe.verify_core_binding(initial, initial, self.path, self.input_fp, self.doc_fp)

    def test_oversized_core_file_is_rejected_before_parse(self):
        self.path.write_bytes(bytes(144 + 65536 + 2179))
        with self.assertRaises(ValueError): self.verify()


if __name__ == '__main__':
    unittest.main()
