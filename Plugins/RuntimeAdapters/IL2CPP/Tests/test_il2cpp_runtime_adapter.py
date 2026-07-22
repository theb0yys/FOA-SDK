# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import json, sys, unittest
from pathlib import Path
PACKAGE_ROOT=Path(__file__).resolve().parents[1]; TOOLS=PACKAGE_ROOT/'Tools'
if str(TOOLS) not in sys.path: sys.path.insert(0,str(TOOLS))
from _il2cpp_contract import *
from _il2cpp_gate import evaluate_execution_gate, validate_gate
from _il2cpp_result import build_result, validate_result

def fixture(name): return load_json(PACKAGE_ROOT/'Tests/Fixtures'/name)
class Il2CppRuntimeAdapterTests(unittest.TestCase):
    def test_package_and_exact_profile_validate(self):
        manifest=validate_package(); self.assertEqual(manifest['profile'],PROFILE); self.assertEqual(manifest['evidence_state'],'PackageInstallValidated')
    def test_manifest_has_distinct_bepinex6_and_interop_dependencies(self):
        manifest=validate_package(); value=json.dumps(manifest); self.assertIn('BepInEx.Unity.IL2CPP.dll',value); self.assertIn('Il2CppInterop.Runtime.dll',value); self.assertNotIn('BepInEx.dll',value)
    def test_interop_manifest_and_required_materials_validate(self):
        checked=validate_interop_manifest(fixture('interop-manifest.json')); self.assertEqual({r['role'] for r in checked['materials']},set(REQUIRED_INTEROP))
    def test_interop_manifest_rejects_missing_material(self):
        doc=fixture('interop-manifest.json'); doc['materials'].pop()
        with self.assertRaises(Il2CppAdapterError): validate_interop_manifest(doc)
    def test_build_plan_is_canonical_and_matches_fixture(self): self.assertEqual(build_plan(fixture('interop-manifest.json')),fixture('build-plan.json'))
    def test_execution_gate_is_non_authorizing_and_matches_fixture(self):
        checked=validate_gate(fixture('execution-gate.json')); self.assertTrue(checked['ready_for_external_executor_review']); self.assertFalse(checked['execution_authorized']); self.assertEqual(checked,fixture('execution-gate.json'))
    def test_execution_gate_rejects_mono_profile(self):
        profile=dict(PROFILE); profile.update(branch='mono',runtime_target='Mono')
        with self.assertRaises(Il2CppAdapterError): evaluate_execution_gate(binary_sha256='sha256:'+'a'*64,binary_bytes=1,profile=profile,interop_manifest=fixture('interop-manifest.json'),identity_evidence_ids=['evidence.identity'],parity_evidence_ids=['evidence.parity'],interop_evidence_ids=['evidence.interop'],confirmation_receipt_sha256='sha256:'+'b'*64,requested_by='operator.review',requested_at_utc='2026-07-22T08:00:00Z',expires_at_utc='2026-07-22T09:00:00Z')
    def test_execution_gate_rejects_cross_class_evidence_reuse(self):
        with self.assertRaises(Il2CppAdapterError): evaluate_execution_gate(binary_sha256='sha256:'+'a'*64,binary_bytes=1,profile=PROFILE,interop_manifest=fixture('interop-manifest.json'),identity_evidence_ids=['evidence.same'],parity_evidence_ids=['evidence.same'],interop_evidence_ids=['evidence.interop'],confirmation_receipt_sha256='sha256:'+'b'*64,requested_by='operator.review',requested_at_utc='2026-07-22T08:00:00Z',expires_at_utc='2026-07-22T09:00:00Z')
    def test_not_attempted_result_matches_fixture(self): self.assertEqual(validate_result(fixture('not-attempted-result.json'),fixture('execution-gate.json')),fixture('not-attempted-result.json'))
    def test_success_requires_all_il2cpp_observations(self):
        with self.assertRaises(Il2CppAdapterError): build_result(fixture('execution-gate.json'),outcome='succeeded',captured_at_utc='2026-07-22T08:20:00Z',started_at_utc='2026-07-22T08:10:00Z',completed_at_utc='2026-07-22T08:15:00Z',loader_seen=True,framework_seen=True,interop_runtime_seen=True,generated_interop_seen=False,startup_marker_seen=True,log_reference='logs/LogOutput.log',failures=[])
    def test_success_result_is_candidate_only(self):
        result=build_result(fixture('execution-gate.json'),outcome='succeeded',captured_at_utc='2026-07-22T08:20:00Z',started_at_utc='2026-07-22T08:10:00Z',completed_at_utc='2026-07-22T08:15:00Z',loader_seen=True,framework_seen=True,interop_runtime_seen=True,generated_interop_seen=True,startup_marker_seen=True,log_reference='logs/LogOutput.log',failures=[]); self.assertTrue(result['candidate_evidence_eligible']); self.assertFalse(result['candidate_evidence_promoted'])
    def test_unsafe_log_and_impossible_chronology_fail(self):
        with self.assertRaises(Il2CppAdapterError): build_result(fixture('execution-gate.json'),outcome='failed',captured_at_utc='2026-07-22T08:20:00Z',started_at_utc='2026-07-22T08:18:00Z',completed_at_utc='2026-07-22T08:17:00Z',loader_seen=True,framework_seen=False,interop_runtime_seen=False,generated_interop_seen=False,startup_marker_seen=False,log_reference='../private.log',failures=['load failed'])
if __name__=='__main__': unittest.main()
