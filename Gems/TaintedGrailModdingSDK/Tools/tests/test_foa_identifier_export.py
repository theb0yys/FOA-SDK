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
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_identifier_export.py"
SPEC = importlib.util.spec_from_file_location("foa_identifier_export", MODULE_PATH)
assert SPEC and SPEC.loader
export = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = export
SPEC.loader.exec_module(export)


class FoAIdentifierExportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="foa-identifier-export-tests-"))
        self.workspace_root = self.temp_root / "workspace"
        self.extracted = self.workspace_root / "Extracted"
        self.extracted.mkdir(parents=True)
        self.workspace_path = self.temp_root / "workspace.tgworkspace.json"
        self.export_path = self.extracted / export.DEFAULT_EXPORT_NAME
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
        self.document = {
            "SchemaVersion": 1,
            "DocumentKind": export.DOCUMENT_KIND,
            "ExportId": "export.foa.test.identifiers",
            "ProfileId": "foa.mono.test",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "ToolName": "FoA Test Identifier Exporter",
            "ToolVersion": "1.0.0",
            "CapturedAt": "2026-07-28T00:00:00Z",
            "PromoteAutomatically": False,
            "GrantsRuntimePermission": False,
            "Observations": [
                {
                    "ObservationId": "observation.test.item.native-ref",
                    "SubjectRef": "subject:foa:economy:item:test-ore",
                    "ClaimId": "native_ref_exact",
                    "Claim": "Native item GUID was observed in a sanitized identifier export.",
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
                    "Claim": "Template addressable key was observed in a sanitized identifier export.",
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
        self.write_documents()

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def write_documents(self) -> None:
        self.workspace_path.write_bytes(export.pretty_json_bytes(self.workspace))
        self.export_path.write_bytes(export.pretty_json_bytes(self.document))

    def test_verify_accepts_profile_bound_export(self) -> None:
        normalized = export.load_and_validate_export(self.export_path, self.workspace_path)
        self.assertEqual(normalized["ExportId"], "export.foa.test.identifiers")
        self.assertEqual(len(normalized["Observations"]), 2)
        self.assertEqual(normalized["Observations"][0]["ObservationId"], "observation.test.item.native-ref")
        self.assertFalse(normalized["PromoteAutomatically"])
        self.assertFalse(normalized["GrantsRuntimePermission"])

    def test_normalization_sorts_observations(self) -> None:
        self.document["Observations"] = list(reversed(self.document["Observations"]))
        self.write_documents()
        normalized = export.load_and_validate_export(self.export_path, self.workspace_path)
        ids = [item["ObservationId"] for item in normalized["Observations"]]
        self.assertEqual(ids, sorted(ids))

    def test_profile_mismatch_is_rejected(self) -> None:
        self.document["GameVersion"] = "1.23.999"
        self.write_documents()
        with self.assertRaisesRegex(export.ExportError, "exact active workspace profile"):
            export.load_and_validate_export(self.export_path, self.workspace_path)

    def test_runtime_permission_escalation_is_rejected(self) -> None:
        self.document["Observations"][0]["GrantsRuntimePermission"] = True
        self.write_documents()
        with self.assertRaisesRegex(export.ExportError, "GrantsRuntimePermission must be false"):
            export.load_and_validate_export(self.export_path, self.workspace_path)

    def test_millisecond_capture_time_is_rejected(self) -> None:
        self.document["CapturedAt"] = "2026-07-28T00:00:00.123Z"
        self.write_documents()
        with self.assertRaisesRegex(export.ExportError, "whole-second UTC"):
            export.load_and_validate_export(self.export_path, self.workspace_path)

    def test_absolute_path_leakage_is_rejected(self) -> None:
        self.document["Observations"][1]["Value"] = "C:\\Games\\FoA\\Tainted Grail_Data\\foo"
        self.write_documents()
        with self.assertRaisesRegex(export.ExportError, "absolute or private path"):
            export.load_and_validate_export(self.export_path, self.workspace_path)

    def test_duplicate_native_refs_are_rejected(self) -> None:
        duplicate = copy.deepcopy(self.document["Observations"][0])
        duplicate["ObservationId"] = "observation.test.item.duplicate-native-ref"
        duplicate["SubjectRef"] = "subject:foa:economy:item:test-ore-duplicate"
        self.document["Observations"].append(duplicate)
        self.write_documents()
        with self.assertRaisesRegex(export.ExportError, "Duplicate NativeRefExact"):
            export.load_and_validate_export(self.export_path, self.workspace_path)

    def test_synthetic_record_requires_owner_pack_and_no_native_ref(self) -> None:
        self.document["Observations"][0]["IdentityKind"] = "synthetic"
        self.document["Observations"][0]["OwnerPackId"] = "owner.pack"
        self.write_documents()
        with self.assertRaisesRegex(export.ExportError, "no NativeRefExact"):
            export.load_and_validate_export(self.export_path, self.workspace_path)

    def test_export_must_remain_inside_extracted_data_path(self) -> None:
        outside = self.workspace_root / "outside-identifiers.json"
        outside.write_bytes(export.pretty_json_bytes(self.document))
        with self.assertRaisesRegex(export.ExportError, "inside ExtractedDataPath"):
            export.load_and_validate_export(outside, self.workspace_path)

    def test_cli_normalize_and_verify_succeed(self) -> None:
        normalized = self.extracted / "normalized.foa-identifiers.json"
        self.assertEqual(
            export.main(
                [
                    "normalize",
                    "--workspace",
                    str(self.workspace_path),
                    "--input",
                    str(self.export_path),
                    "--output",
                    str(normalized),
                ]
            ),
            0,
        )
        self.assertEqual(export.main(["verify", "--workspace", str(self.workspace_path), "--input", str(normalized)]), 0)

    def test_fixture_command_generates_verifiable_export(self) -> None:
        output = self.temp_root / "fixture"
        manifest = export.generate_fixture(output)
        self.assertEqual(manifest["ToolId"], export.TOOL_ID)
        self.assertEqual(manifest["ObservationCount"], 3)
        fixture_workspace = output / "workspace.tgworkspace.json"
        fixture_export = output / "workspace" / "Extracted" / export.DEFAULT_EXPORT_NAME
        self.assertEqual(export.main(["verify", "--workspace", str(fixture_workspace), "--input", str(fixture_export)]), 0)


if __name__ == "__main__":
    unittest.main()
