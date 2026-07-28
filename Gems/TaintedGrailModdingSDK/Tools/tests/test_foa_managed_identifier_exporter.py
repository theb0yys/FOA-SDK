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
MODULE_PATH = TOOLS_ROOT / "foa_managed_identifier_exporter.py"
SPEC = importlib.util.spec_from_file_location("foa_managed_identifier_exporter", MODULE_PATH)
assert SPEC and SPEC.loader
exporter = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = exporter
SPEC.loader.exec_module(exporter)


class FoAManagedIdentifierExporterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="foa-managed-exporter-tests-"))
        self.workspace_root = self.temp_root / "workspace"
        self.install_root = self.temp_root / "FoA"
        self.managed = self.install_root / "Tainted Grail_Data" / "Managed"
        self.extracted = self.workspace_root / "Extracted"
        self.managed.mkdir(parents=True)
        self.extracted.mkdir(parents=True)
        (self.managed / "Assembly-CSharp.dll").write_bytes(
            b"\x00Game.Inventory.InventoryService\x00"
            b"\x00Game.Crafting.RecipeDatabase\x00"
            b"\x00C:\\Users\\Private\\DoNotLeak\x00"
        )
        self.workspace_path = self.temp_root / "workspace.tgworkspace.json"
        self.workspace = {
            "SchemaVersion": 1,
            "WorkspaceId": "test.workspace",
            "DisplayName": "Test Workspace",
            "RootPath": str(self.workspace_root.relative_to(self.temp_root)),
            "OutputPath": str((self.workspace_root / "Build").relative_to(self.temp_root)),
            "StagingPath": str((self.workspace_root / "Staging").relative_to(self.temp_root)),
            "DeploymentPath": str((self.workspace_root / "Deployment").relative_to(self.temp_root)),
            "ActiveGameProfileId": "foa.mono.test",
            "GameProfiles": [
                {
                    "ProfileId": "foa.mono.test",
                    "DisplayName": "FoA Mono Test",
                    "InstallPath": str(self.install_root.relative_to(self.temp_root)),
                    "GameVersion": "1.23.401",
                    "Branch": "mono",
                    "RuntimeTarget": "Mono",
                    "UnityVersion": "6000.0.64f1",
                    "BepInExVersion": "5.4.23.3",
                    "ManagedAssembliesPath": str(self.managed.relative_to(self.temp_root)),
                    "PluginPath": "FoA/BepInEx/plugins",
                    "DiagnosticsPath": str((self.workspace_root / "Diagnostics").relative_to(self.temp_root)),
                    "ExtractedDataPath": str(self.extracted.relative_to(self.temp_root)),
                    "DlcScopes": ["base-game"],
                }
            ],
        }
        self.write_workspace()

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def write_workspace(self) -> None:
        self.workspace_path.write_bytes(exporter.pretty_json_bytes(self.workspace))

    def write_seed(self, name: str = exporter.DEFAULT_SEED_NAME, **updates: object) -> Path:
        seed = {
            "SchemaVersion": 1,
            "ProfileId": "foa.mono.test",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "PromoteAutomatically": False,
            "GrantsRuntimePermission": False,
            "TemplateKeys": [
                {
                    "Value": "Characters/Templates/TestBandit",
                    "SubjectRef": "subject:foa:population:template:test-bandit",
                    "DisplayName": "Test Bandit Template",
                }
            ],
            "RecipeKeys": [
                {
                    "Value": "Crafting/Recipes/TestIngot",
                    "SubjectRef": "subject:foa:economy:recipe:test-ingot",
                    "DisplayName": "Test Ingot Recipe",
                }
            ],
        }
        seed.update(updates)
        path = self.extracted / name
        path.write_bytes(exporter.pretty_json_bytes(seed))
        return path

    def test_export_creates_contract_and_managed_type_observations(self) -> None:
        document = exporter.build_export(
            self.workspace_path,
            captured_at="2026-07-28T00:00:00Z",
            include_assembly_strings=True,
        )
        values = {entry["Value"] for entry in document["Observations"]}
        self.assertIn("Game.Inventory.InventoryService", values)
        self.assertIn("Game.Crafting.RecipeDatabase", values)
        self.assertFalse(document["PromoteAutomatically"])
        self.assertFalse(document["GrantsRuntimePermission"])
        self.assertNotIn("C:\\Users", json.dumps(document))

    def test_export_reads_seed_template_and_recipe_keys(self) -> None:
        self.write_seed()
        document = exporter.build_export(
            self.workspace_path,
            captured_at="2026-07-28T00:00:00Z",
            include_assembly_strings=False,
        )
        claims = {entry["ClaimId"] for entry in document["Observations"]}
        self.assertIn("template_key", claims)
        self.assertIn("recipe_key", claims)
        template = next(entry for entry in document["Observations"] if entry["ClaimId"] == "template_key")
        self.assertEqual(template["Domain"], "population")
        self.assertEqual(template["RecordKind"], "template")

    def test_missing_install_is_rejected(self) -> None:
        shutil.rmtree(self.install_root)
        with self.assertRaisesRegex(exporter.ManagedExportError, "install path"):
            exporter.build_export(self.workspace_path, captured_at="2026-07-28T00:00:00Z")

    def test_managed_path_must_remain_inside_install(self) -> None:
        outside = self.temp_root / "outside" / "Managed"
        outside.mkdir(parents=True)
        self.workspace["GameProfiles"][0]["ManagedAssembliesPath"] = str(outside.relative_to(self.temp_root))
        self.write_workspace()
        with self.assertRaisesRegex(exporter.ManagedExportError, "ManagedAssembliesPath"):
            exporter.build_export(self.workspace_path, captured_at="2026-07-28T00:00:00Z")

    def test_seed_path_must_remain_inside_extracted_data(self) -> None:
        seed = self.temp_root / "outside-seed.json"
        seed.write_bytes(exporter.pretty_json_bytes({"SchemaVersion": 1}))
        with self.assertRaisesRegex(exporter.ManagedExportError, "ExtractedDataPath"):
            exporter.build_export(
                self.workspace_path,
                seed_paths=[seed],
                captured_at="2026-07-28T00:00:00Z",
                include_assembly_strings=False,
            )

    def test_seed_runtime_permission_escalation_is_rejected(self) -> None:
        seed = self.write_seed(GrantsRuntimePermission=True)
        with self.assertRaisesRegex(exporter.ManagedExportError, "GrantsRuntimePermission"):
            exporter.build_export(
                self.workspace_path,
                seed_paths=[seed],
                captured_at="2026-07-28T00:00:00Z",
                include_assembly_strings=False,
            )

    def test_private_path_leak_from_seed_is_rejected(self) -> None:
        seed = self.write_seed(
            TemplateKeys=[
                {
                    "Value": "C:\\Users\\Private\\Template",
                    "SubjectRef": "subject:foa:population:template:bad",
                }
            ]
        )
        with self.assertRaisesRegex(exporter.ManagedExportError, "absolute or private path"):
            exporter.build_export(
                self.workspace_path,
                seed_paths=[seed],
                captured_at="2026-07-28T00:00:00Z",
                include_assembly_strings=False,
            )

    def test_empty_observation_set_is_rejected(self) -> None:
        (self.managed / "Assembly-CSharp.dll").write_bytes(b"no useful strings")
        with self.assertRaisesRegex(exporter.ManagedExportError, "No managed identifier observations"):
            exporter.build_export(
                self.workspace_path,
                captured_at="2026-07-28T00:00:00Z",
                include_assembly_strings=True,
            )

    def test_cli_export_and_verify_succeed(self) -> None:
        output = self.extracted / exporter.DEFAULT_OUTPUT_NAME
        self.assertEqual(
            exporter.main(
                [
                    "export",
                    "--workspace",
                    str(self.workspace_path),
                    "--output",
                    str(output),
                    "--captured-at",
                    "2026-07-28T00:00:00Z",
                ]
            ),
            0,
        )
        self.assertEqual(
            exporter.main(["verify", "--input", str(output), "--workspace", str(self.workspace_path)]),
            0,
        )

    def test_fixture_command_generates_verifiable_export(self) -> None:
        output = self.temp_root / "fixture"
        manifest = exporter.generate_fixture(output)
        self.assertGreaterEqual(manifest["ObservationCount"], 3)
        export_path = output / "workspace" / "Extracted" / exporter.DEFAULT_OUTPUT_NAME
        workspace_path = output / "workspace.tgworkspace.json"
        self.assertEqual(
            exporter.main(["verify", "--input", str(export_path), "--workspace", str(workspace_path)]),
            0,
        )


if __name__ == "__main__":
    unittest.main()
