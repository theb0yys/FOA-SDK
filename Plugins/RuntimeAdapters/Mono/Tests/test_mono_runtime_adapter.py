from __future__ import annotations

import copy
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

PACKAGE_ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = PACKAGE_ROOT / "Tools" / "mono_runtime_adapter.py"
SPEC = importlib.util.spec_from_file_location("foa_mono_runtime_adapter_tests_target", TOOL_PATH)
assert SPEC and SPEC.loader
adapter = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = adapter
SPEC.loader.exec_module(adapter)


class MonoRuntimeAdapterTests(unittest.TestCase):
    def _gate(self, **overrides: object) -> dict[str, object]:
        arguments: dict[str, object] = {
            "binary_sha256": "sha256:" + "a" * 64,
            "binary_bytes": 4096,
            "profile": adapter.PROFILE,
            "identity_evidence_ids": ["evidence.adapter.mono.identity"],
            "parity_evidence_ids": ["evidence.adapter.mono.parity"],
            "confirmation_receipt_sha256": "sha256:" + "b" * 64,
            "requested_by": "operator.foa",
            "requested_at_utc": "2026-07-22T06:00:00Z",
            "expires_at_utc": "2026-07-22T07:00:00Z",
            "package_root": PACKAGE_ROOT,
        }
        arguments.update(overrides)
        return adapter.evaluate_execution_gate(**arguments)

    def test_manifest_is_exact_mono_only_package(self) -> None:
        manifest = adapter.validate_package(PACKAGE_ROOT)
        self.assertEqual(manifest["adapter_id"], "adapter.foa.mono")
        self.assertEqual(manifest["profile"], adapter.PROFILE)
        self.assertEqual(manifest["profile"]["runtime_target"], "Mono")
        self.assertNotIn("IL2CPP", json.dumps(manifest["dependencies"]))
        self.assertTrue(all(value is False for value in manifest["authority"].values()))

    def test_project_source_is_report_only_and_dependency_bound(self) -> None:
        project = (PACKAGE_ROOT / "Source/FOA.SDK.RuntimeAdapter.Mono.csproj").read_text(
            encoding="utf-8"
        )
        source = (PACKAGE_ROOT / "Source/MonoRuntimeAdapterPlugin.cs").read_text(
            encoding="utf-8"
        )
        self.assertIn("<TargetFramework>netstandard2.1</TargetFramework>", project)
        self.assertIn("<Deterministic>true</Deterministic>", project)
        self.assertIn("BepInEx.dll", project)
        self.assertIn("UnityEngine.CoreModule.dll", project)
        self.assertIn("[BepInPlugin(", source)
        self.assertIn("[BepInDependency(FrameworkGuid, FrameworkVersion)]", source)
        self.assertIn(adapter.STARTUP_MARKER, source)
        for prohibited in ("Harmony", "System.IO", "Process.", "File.", "Directory."):
            self.assertNotIn(prohibited, source)

    def test_build_plan_is_byte_stable_and_matches_golden(self) -> None:
        first = adapter.build_plan(PACKAGE_ROOT)
        second = adapter.build_plan(PACKAGE_ROOT)
        self.assertEqual(adapter.canonical_json(first), adapter.canonical_json(second))
        golden = (PACKAGE_ROOT / "Tests/Fixtures/build-plan.json").read_bytes()
        self.assertEqual(adapter.canonical_json(first), golden)
        self.assertFalse(first["steps"][0]["execution_allowed"])
        self.assertTrue(all(value is False for value in first["authority"].values()))

    def test_exact_gate_is_eligible_but_grants_no_authority(self) -> None:
        gate = self._gate()
        self.assertTrue(gate["ready_for_external_executor_review"])
        self.assertFalse(gate["execution_authorized"])
        self.assertEqual(gate["reasons"], [])
        self.assertEqual(adapter.validate_gate(gate, PACKAGE_ROOT), gate)
        self.assertTrue(all(value is False for value in gate["authority"].values()))

    def test_il2cpp_profile_is_refused(self) -> None:
        profile = dict(adapter.PROFILE)
        profile.update(
            {
                "profile_id": "foa-1.23.401-il2cpp-bepinex6-tf-0.1.36",
                "branch": "il2cpp",
                "runtime_target": "IL2CPP",
                "bepinex_version": "6.0.0-be.735",
                "framework_version": "0.1.36",
                "evidence_state": "PackageInstallValidated",
            }
        )
        with self.assertRaisesRegex(adapter.MonoAdapterError, "exact Mono profile"):
            self._gate(profile=profile)

    def test_missing_or_overlapping_evidence_is_refused(self) -> None:
        with self.assertRaisesRegex(adapter.MonoAdapterError, "between 1 and 64"):
            self._gate(identity_evidence_ids=[], parity_evidence_ids=[])

        with self.assertRaisesRegex(adapter.MonoAdapterError, "multiple evidence classes"):
            self._gate(
                identity_evidence_ids=["evidence.adapter.shared"],
                parity_evidence_ids=["evidence.adapter.shared"],
            )

    def test_not_attempted_fixture_is_canonical(self) -> None:
        fixture = json.loads(
            (PACKAGE_ROOT / "Tests/Fixtures/not-attempted-result.json").read_text(
                encoding="utf-8"
            )
        )
        checked = adapter.validate_result(
            fixture["result"], fixture["gate"], PACKAGE_ROOT
        )
        self.assertEqual(checked["outcome"], "not_attempted")
        self.assertFalse(checked["candidate_evidence_eligible"])

    def test_succeeded_result_requires_all_startup_observations(self) -> None:
        gate = self._gate()
        result = adapter.build_result(
            gate,
            outcome="succeeded",
            started_at_utc="2026-07-22T06:10:00Z",
            completed_at_utc="2026-07-22T06:12:00Z",
            captured_at_utc="2026-07-22T06:13:00Z",
            loader_seen=True,
            framework_seen=True,
            startup_marker_seen=True,
            log_reference="evidence/mono/load-log.sha256",
            failures=[],
            package_root=PACKAGE_ROOT,
        )
        self.assertTrue(result["candidate_evidence_eligible"])
        self.assertFalse(result["candidate_evidence_promoted"])
        self.assertEqual(adapter.validate_result(result, gate, PACKAGE_ROOT), result)

        with self.assertRaisesRegex(
            adapter.MonoAdapterError, "all exact startup observations"
        ):
            adapter.build_result(
                gate,
                outcome="succeeded",
                started_at_utc="2026-07-22T06:10:00Z",
                completed_at_utc="2026-07-22T06:12:00Z",
                captured_at_utc="2026-07-22T06:13:00Z",
                loader_seen=True,
                framework_seen=False,
                startup_marker_seen=True,
                log_reference="evidence/mono/load-log.sha256",
                failures=[],
                package_root=PACKAGE_ROOT,
            )

    def test_unsafe_log_reference_and_impossible_chronology_fail(self) -> None:
        gate = self._gate()
        with self.assertRaisesRegex(adapter.MonoAdapterError, "safe relative"):
            adapter.build_result(
                gate,
                outcome="failed",
                started_at_utc="2026-07-22T06:10:00Z",
                completed_at_utc="2026-07-22T06:12:00Z",
                captured_at_utc="2026-07-22T06:13:00Z",
                loader_seen=True,
                framework_seen=False,
                startup_marker_seen=False,
                log_reference="C:/Users/private/LogOutput.log",
                failures=["framework-not-observed"],
                package_root=PACKAGE_ROOT,
            )
        with self.assertRaisesRegex(adapter.MonoAdapterError, "chronology"):
            adapter.build_result(
                gate,
                outcome="failed",
                started_at_utc="2026-07-22T06:20:00Z",
                completed_at_utc="2026-07-22T06:12:00Z",
                captured_at_utc="2026-07-22T06:13:00Z",
                loader_seen=True,
                framework_seen=False,
                startup_marker_seen=False,
                log_reference="evidence/mono/failure.sha256",
                failures=["startup-marker-not-observed"],
                package_root=PACKAGE_ROOT,
            )

    def test_tampered_result_and_authority_escalation_fail(self) -> None:
        gate = self._gate()
        result = adapter.build_result(
            gate,
            outcome="succeeded",
            started_at_utc="2026-07-22T06:10:00Z",
            completed_at_utc="2026-07-22T06:12:00Z",
            captured_at_utc="2026-07-22T06:13:00Z",
            loader_seen=True,
            framework_seen=True,
            startup_marker_seen=True,
            log_reference="evidence/mono/load-log.sha256",
            failures=[],
            package_root=PACKAGE_ROOT,
        )
        tampered = copy.deepcopy(result)
        tampered["authority"]["runtime_execution"] = True
        with self.assertRaises(adapter.MonoAdapterError):
            adapter.validate_result(tampered, gate, PACKAGE_ROOT)

        tampered = copy.deepcopy(result)
        tampered["candidate_evidence_promoted"] = True
        with self.assertRaises(adapter.MonoAdapterError):
            adapter.validate_result(tampered, gate, PACKAGE_ROOT)

    def test_source_fingerprint_and_binary_payload_tampering_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "Mono"
            shutil.copytree(PACKAGE_ROOT, copied)
            source = copied / "Source/MonoRuntimeAdapterPlugin.cs"
            source.write_text(source.read_text(encoding="utf-8") + "\n// altered\n", encoding="utf-8")
            with self.assertRaisesRegex(adapter.MonoAdapterError, "fingerprint mismatch"):
                adapter.validate_package(copied)

        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "Mono"
            shutil.copytree(PACKAGE_ROOT, copied)
            (copied / "adapter.dll").write_bytes(b"binary")
            with self.assertRaisesRegex(adapter.MonoAdapterError, "binary payload"):
                adapter.validate_package(copied)

    def test_tool_has_no_executor_or_network_surface(self) -> None:
        source = TOOL_PATH.read_text(encoding="utf-8")
        for prohibited in (
            "import subprocess",
            "from subprocess",
            "import socket",
            "import requests",
            "urllib.request",
            "os.system",
            "Popen(",
            "run(",
        ):
            self.assertNotIn(prohibited, source)


if __name__ == "__main__":
    unittest.main()
