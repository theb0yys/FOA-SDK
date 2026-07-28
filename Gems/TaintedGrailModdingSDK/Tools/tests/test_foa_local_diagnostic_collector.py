#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_local_diagnostic_collector.py"
SPEC = importlib.util.spec_from_file_location("foa_local_diagnostic_collector", MODULE_PATH)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = collector
SPEC.loader.exec_module(collector)


class FoALocalDiagnosticCollectorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="foa-local-diagnostic-collector-tests-"))
        self.workspace_path = self.temp_root / "workspace.tgworkspace.json"
        self.workspace_root = self.temp_root / "workspace"
        self.install_root = self.temp_root / "lawful-local-fixture" / "FoA"
        self.managed = self.install_root / "Tainted Grail_Data" / "Managed"
        self.plugins = self.install_root / "BepInEx" / "plugins"
        self.extracted = self.workspace_root / "Extracted"
        self.managed.mkdir(parents=True)
        self.plugins.mkdir(parents=True)
        self.extracted.mkdir(parents=True)
        (self.install_root / "Tainted Grail_Data" / "globalgamemanagers").write_bytes(b"synthetic-globalgamemanagers")
        (self.managed / "Assembly-CSharp.dll").write_bytes(b"synthetic-assembly-csharp")
        (self.managed / "UnityEngine.CoreModule.dll").write_bytes(b"synthetic-unity-core")
        self.workspace = {
            "SchemaVersion": 1,
            "WorkspaceId": "test.workspace",
            "DisplayName": "Test Workspace",
            "RootPath": "./workspace",
            "OutputPath": "./workspace/Build",
            "StagingPath": "./workspace/Staging",
            "DeploymentPath": "./workspace/Deployment",
            "ActiveGameProfileId": "foa.mono.test",
            "GameProfiles": [
                {
                    "ProfileId": "foa.mono.test",
                    "DisplayName": "FoA Mono Test",
                    "InstallPath": "./lawful-local-fixture/FoA",
                    "GameVersion": "1.23.401",
                    "Branch": "mono",
                    "RuntimeTarget": "Mono",
                    "UnityVersion": "6000.0.64f1",
                    "BepInExVersion": "5.4.23.3",
                    "ManagedAssembliesPath": "./lawful-local-fixture/FoA/Tainted Grail_Data/Managed",
                    "PluginPath": "./lawful-local-fixture/FoA/BepInEx/plugins",
                    "DiagnosticsPath": "./workspace/Diagnostics",
                    "ExtractedDataPath": "./workspace/Extracted",
                    "DlcScopes": ["base-game"],
                }
            ],
        }
        self.identifier_export = {
            "SchemaVersion": 1,
            "ProfileId": "foa.mono.test",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "PromoteAutomatically": False,
            "GrantsRuntimePermission": False,
            "Observations": [
                {
                    "ObservationId": "observation.test.item.native-ref",
                    "SubjectRef": "subject:foa:economy:item:test-ore",
                    "ClaimId": "native_ref_exact",
                    "Claim": "Native item GUID was observed in a bounded local identifier export.",
                    "Value": "00000000-0000-0000-0000-000000000101",
                    "Domain": "economy",
                    "RecordKind": "item",
                    "IdentityKind": "native",
                    "NativeRefExact": "00000000-0000-0000-0000-000000000101",
                    "DisplayName": "Test Ore",
                    "EvidenceKind": "native-identifier-observation",
                    "Confidence": "observed",
                    "Locator": "$.items[0].guid",
                    "RecordPath": "$.Observations[0]",
                    "PromoteAutomatically": False,
                    "GrantsRuntimePermission": False,
                }
            ],
        }
        self.write_workspace()
        self.write_identifier_export()

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def write_workspace(self) -> None:
        self.workspace_path.write_bytes(collector.pretty_json_bytes(self.workspace))

    def write_identifier_export(self) -> Path:
        path = self.extracted / collector.DEFAULT_IDENTIFIER_EXPORT_NAME
        path.write_bytes(collector.pretty_json_bytes(self.identifier_export))
        return path

    def test_collect_generates_sanitized_capture_with_identifier_observations(self) -> None:
        capture = collector.build_capture(self.workspace_path, captured_at="2026-07-28T00:00:00Z")
        text = json.dumps(capture, ensure_ascii=False)
        self.assertNotIn(str(self.temp_root), text)
        self.assertIn("$install", text)
        self.assertIn("$extracted", text)
        self.assertFalse(capture["PromoteAutomatically"])
        self.assertFalse(capture["GrantsRuntimePermission"])
        observation_ids = {value["ObservationId"] for value in capture["Observations"]}
        self.assertIn("observation.test.item.native-ref", observation_ids)
        self.assertTrue(any(value["ClaimId"] == "managed_assembly_fingerprint" for value in capture["Observations"]))

    def test_capture_verify_accepts_generated_output(self) -> None:
        capture = collector.build_capture(self.workspace_path, captured_at="2026-07-28T00:00:00Z")
        capture_path = self.temp_root / "capture.foa-local-capture.json"
        collector.write_capture(capture, capture_path)
        verified = collector.verify_capture(self.workspace_path, capture_path)
        self.assertEqual(verified["CaptureId"], capture["CaptureId"])

    def test_missing_install_path_is_rejected(self) -> None:
        shutil.rmtree(self.install_root)
        with self.assertRaisesRegex(collector.CollectorError, "install path"):
            collector.build_capture(self.workspace_path, captured_at="2026-07-28T00:00:00Z")

    def test_managed_path_outside_install_is_rejected(self) -> None:
        outside = self.temp_root / "outside" / "Managed"
        outside.mkdir(parents=True)
        self.workspace["GameProfiles"][0]["ManagedAssembliesPath"] = "./outside/Managed"
        self.write_workspace()
        with self.assertRaisesRegex(collector.CollectorError, "ManagedAssembliesPath"):
            collector.build_capture(self.workspace_path, captured_at="2026-07-28T00:00:00Z")

    def test_identifier_export_outside_extracted_data_is_rejected(self) -> None:
        outside = self.temp_root / "outside-export.json"
        outside.write_bytes(collector.pretty_json_bytes(self.identifier_export))
        with self.assertRaisesRegex(collector.CollectorError, "ExtractedDataPath"):
            collector.build_capture(
                self.workspace_path,
                identifier_exports=[outside],
                captured_at="2026-07-28T00:00:00Z",
            )

    def test_identifier_export_runtime_permission_escalation_is_rejected(self) -> None:
        self.identifier_export["Observations"][0]["GrantsRuntimePermission"] = True
        self.write_identifier_export()
        with self.assertRaisesRegex(collector.CollectorError, "GrantsRuntimePermission"):
            collector.build_capture(self.workspace_path, captured_at="2026-07-28T00:00:00Z")

    def test_export_locator_cannot_leak_absolute_paths(self) -> None:
        self.identifier_export["Observations"][0]["Locator"] = str(self.temp_root / "private" / "file.json")
        self.write_identifier_export()
        with self.assertRaisesRegex(collector.CollectorError, "Locator"):
            collector.build_capture(self.workspace_path, captured_at="2026-07-28T00:00:00Z")

    def test_cli_collect_and_verify_succeed(self) -> None:
        capture_path = self.temp_root / "cli-capture.json"
        self.assertEqual(
            collector.main(
                [
                    "collect",
                    "--workspace",
                    str(self.workspace_path),
                    "--output",
                    str(capture_path),
                    "--captured-at",
                    "2026-07-28T00:00:00Z",
                ]
            ),
            0,
        )
        self.assertEqual(
            collector.main(["verify", "--workspace", str(self.workspace_path), "--input", str(capture_path)]),
            0,
        )

    def test_fixture_command_generates_verifiable_capture(self) -> None:
        output = self.temp_root / "fixture"
        result = collector.generate_fixture(output)
        self.assertTrue(Path(result["workspace"]).is_file())
        self.assertTrue(Path(result["capture"]).is_file())
        self.assertGreater(result["observation_count"], 0)


if __name__ == "__main__":
    unittest.main()
