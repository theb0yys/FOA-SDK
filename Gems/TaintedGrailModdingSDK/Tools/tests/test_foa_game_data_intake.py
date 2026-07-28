#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import copy
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_game_data_intake.py"
SPEC = importlib.util.spec_from_file_location("foa_game_data_intake", MODULE_PATH)
assert SPEC and SPEC.loader
intake = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = intake
SPEC.loader.exec_module(intake)


class FoAGameDataIntakeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="foa-game-data-intake-tests-"))
        self.workspace_path = self.temp_root / "workspace.tgworkspace.json"
        self.capture_path = self.temp_root / "capture.foa-local-capture.json"
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
        self.capture = {
            "SchemaVersion": 1,
            "CaptureId": "capture.foa.test",
            "Title": "Synthetic local item capture",
            "SourceKind": intake.SOURCE_KIND,
            "ProfileId": "foa.mono.test",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "ToolName": "FoA Test Diagnostic Capture",
            "ToolVersion": "1.0.0",
            "CapturedAt": "2026-07-28T00:00:00Z",
            "Locator": "capture.foa-local-capture.json",
            "PromoteAutomatically": False,
            "GrantsRuntimePermission": False,
            "Observations": [
                {
                    "ObservationId": "observation.test.item.native-ref",
                    "SubjectRef": "subject:foa:economy:item:test-ore",
                    "ClaimId": "native_ref_exact",
                    "Claim": "Native item GUID was observed in a sanitized local capture.",
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
                },
                {
                    "ObservationId": "observation.test.template.addressable",
                    "SubjectRef": "subject:foa:population:template:test-bandit",
                    "ClaimId": "addressable_key",
                    "Claim": "Template addressable key was observed in a sanitized local capture.",
                    "Value": "Characters/Templates/TestBandit",
                    "Domain": "population",
                    "RecordKind": "template",
                    "IdentityKind": "source_scoped",
                    "DisplayName": "Test Bandit Template",
                    "EvidenceKind": "addressable-observation",
                    "Confidence": "observed",
                    "Locator": "$.templates[0].addressableKey",
                    "RecordPath": "$.Observations[1]",
                    "PromoteAutomatically": False,
                    "GrantsRuntimePermission": False,
                },
            ],
        }
        self.write_inputs()

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def write_inputs(self) -> None:
        self.workspace_path.write_bytes(intake.pretty_json_bytes(self.workspace))
        self.capture_path.write_bytes(intake.pretty_json_bytes(self.capture))

    def capture_output(self, name: str = "out") -> Path:
        output = self.temp_root / name
        documents = intake.build_documents(
            self.workspace_path,
            self.capture_path,
            imported_at="2026-07-28T00:00:01Z",
        )
        intake.write_documents(documents, output)
        return output

    def test_capture_generates_source_evidence_and_catalog_candidates(self) -> None:
        output = self.capture_output()
        manifest = intake.verify_output(output)
        self.assertEqual(manifest["ToolId"], intake.IMPORTER_ID)
        source_id = manifest["SourceId"]
        evidence = json.loads((output / intake.EVIDENCE_DOCUMENT_PATH.format(source_id=source_id)).read_text())
        candidates = json.loads((output / intake.CATALOG_CANDIDATE_PATH.format(source_id=source_id)).read_text())
        self.assertEqual(len(evidence["Evidence"]), 2)
        self.assertEqual(len(candidates["Records"]), 2)
        self.assertFalse(candidates["PromotionAllowed"])
        self.assertFalse(candidates["RuntimePermissionGranted"])
        native_record = next(record for record in candidates["Records"] if record["RecordKind"] == "item")
        self.assertEqual(native_record["NativeRefExact"], "00000000-0000-0000-0000-000000000101")
        self.assertEqual(native_record["ForbiddenUsages"], [intake.RESERVED_FORBIDDEN_USAGE])

    def test_profile_mismatch_is_rejected(self) -> None:
        self.capture["GameVersion"] = "1.23.999"
        self.write_inputs()
        with self.assertRaisesRegex(intake.IntakeError, "exact active workspace profile"):
            intake.build_documents(self.workspace_path, self.capture_path, imported_at="2026-07-28T00:00:01Z")

    def test_runtime_permission_escalation_is_rejected(self) -> None:
        self.capture["Observations"][0]["GrantsRuntimePermission"] = True
        self.write_inputs()
        with self.assertRaisesRegex(intake.IntakeError, "GrantsRuntimePermission must be false"):
            intake.build_documents(self.workspace_path, self.capture_path, imported_at="2026-07-28T00:00:01Z")

    def test_millisecond_capture_time_is_rejected(self) -> None:
        self.capture["CapturedAt"] = "2026-07-28T00:00:00.123Z"
        self.write_inputs()
        with self.assertRaisesRegex(intake.IntakeError, "whole-second UTC"):
            intake.build_documents(self.workspace_path, self.capture_path, imported_at="2026-07-28T00:00:01Z")

    def test_duplicate_native_refs_create_blocking_candidate_issue(self) -> None:
        duplicate = copy.deepcopy(self.capture["Observations"][0])
        duplicate["ObservationId"] = "observation.test.item.duplicate-native-ref"
        duplicate["SubjectRef"] = "subject:foa:economy:item:test-ore-duplicate"
        self.capture["Observations"].append(duplicate)
        self.write_inputs()
        output = self.capture_output()
        source_id = intake.verify_output(output)["SourceId"]
        candidates = json.loads((output / intake.CATALOG_CANDIDATE_PATH.format(source_id=source_id)).read_text())
        self.assertEqual(len(candidates["Issues"]), 1)
        self.assertEqual(candidates["Issues"][0]["Code"], "catalog-candidate.duplicate-native-ref")

    def test_synthetic_records_require_pack_ownership_and_no_native_ref(self) -> None:
        self.capture["Observations"][0]["IdentityKind"] = "synthetic"
        self.capture["Observations"][0]["OwnerPackId"] = "owner.pack"
        self.write_inputs()
        with self.assertRaisesRegex(intake.IntakeError, "cannot assign a native ref"):
            intake.build_documents(self.workspace_path, self.capture_path, imported_at="2026-07-28T00:00:01Z")

    def test_tampered_payload_fails_verification(self) -> None:
        output = self.capture_output()
        source_id = intake.verify_output(output)["SourceId"]
        candidate_path = output / intake.CATALOG_CANDIDATE_PATH.format(source_id=source_id)
        candidates = json.loads(candidate_path.read_text())
        candidates["RuntimePermissionGranted"] = True
        candidate_path.write_bytes(intake.pretty_json_bytes(candidates))
        with self.assertRaisesRegex(intake.IntakeError, "mismatch"):
            intake.verify_output(output)

    def test_fixture_command_generates_verifiable_project_owned_output(self) -> None:
        output = self.temp_root / "fixture"
        manifest = intake.generate_fixture(output)
        self.assertEqual(manifest["ToolId"], intake.IMPORTER_ID)
        self.assertEqual(intake.main(["verify", "--output", str(output)]), 0)

    def test_cli_capture_and_verify_succeed(self) -> None:
        output = self.temp_root / "cli"
        self.assertEqual(
            intake.main(
                [
                    "capture",
                    "--workspace",
                    str(self.workspace_path),
                    "--input",
                    str(self.capture_path),
                    "--output",
                    str(output),
                    "--imported-at",
                    "2026-07-28T00:00:01Z",
                ]
            ),
            0,
        )
        self.assertEqual(intake.main(["verify", "--output", str(output)]), 0)


if __name__ == "__main__":
    unittest.main()
