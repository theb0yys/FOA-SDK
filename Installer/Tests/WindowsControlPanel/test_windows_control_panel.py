# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import json
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
CONTROL_PANEL = REPO_ROOT / "Installer" / "ControlPanel" / "Windows"
PROJECT = CONTROL_PANEL / "FOAControlPanel.csproj"
MANIFEST = CONTROL_PANEL / "app.manifest"
PROGRAM = CONTROL_PANEL / "Program.cs"
CORE = CONTROL_PANEL / "ProviderManifest.cs"
PROVIDER = CONTROL_PANEL / "Providers" / "foa.provider.json"
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "tainted-grail-sdk-installer.yml"
PACKAGING = REPO_ROOT / "Installer" / "Packaging" / "Windows" / "CMakeLists.txt"


class WindowsControlPanelTests(unittest.TestCase):
    def test_project_is_self_contained_accessible_winforms_front_door(self) -> None:
        root = ET.fromstring(PROJECT.read_text(encoding="utf-8"))
        values = {child.tag: (child.text or "") for group in root for child in group}
        self.assertEqual(values["OutputType"], "WinExe")
        self.assertEqual(values["TargetFramework"], "net8.0-windows")
        self.assertEqual(values["UseWindowsForms"], "true")
        self.assertEqual(values["AssemblyName"], "FOA-SDK-ControlPanel")
        self.assertEqual(values["PublishSingleFile"], "true")
        self.assertEqual(values["ApplicationHighDpiMode"], "PerMonitorV2")
        self.assertIn("$(MSBuildProjectDirectory)/obj/**", values["DefaultItemExcludes"])
        manifest = MANIFEST.read_text(encoding="utf-8")
        self.assertIn('requestedExecutionLevel level="asInvoker"', manifest)
        self.assertNotIn("requireAdministrator", manifest)

    def test_provider_manifest_is_read_only_manifest_first_contract(self) -> None:
        provider = json.loads(PROVIDER.read_text(encoding="utf-8"))
        self.assertEqual(provider["schema"], "foa.sdk.provider_manifest.v1")
        self.assertEqual(provider["schema_version"], 1)
        self.assertEqual(provider["provider_id"], "game.foa")
        self.assertEqual(provider["discovery"]["mode"], "user-selected-path")
        self.assertFalse(provider["discovery"]["machine_scan_allowed"])
        self.assertFalse(provider["discovery"]["network_allowed"])
        self.assertEqual(
            {route["route_id"] for route in provider["runtime_routes"]},
            {"mono-bepinex5", "il2cpp-bepinex6"},
        )
        for key in (
            "conversion_execution_allowed",
            "deployment_execution_allowed",
            "game_launch_allowed",
            "save_access_allowed",
        ):
            self.assertFalse(provider[key])

    def test_core_has_versioned_profile_migration_and_redacted_evidence(self) -> None:
        core = CORE.read_text(encoding="utf-8")
        self.assertIn('ProfileSchema = "foa.sdk.setup_profile.v1"', core)
        self.assertIn('"foa.sdk.support_report.v1"', core)
        self.assertIn("ReadLegacyProfile", core)
        self.assertIn("AtomicWriteJson", core)
        self.assertIn("RedactPath", core)
        self.assertIn("RequirePathWithoutExistingReparsePoint", core)
        self.assertIn('"machine_scan_allowed": false', PROVIDER.read_text(encoding="utf-8").lower())
        self.assertNotIn("Directory.EnumerateFiles", core)
        self.assertNotIn("SearchOption.AllDirectories", core)
        self.assertIn('route = mono == il2cpp ? "unknown"', core)
        self.assertIn("indicated, not runtime verified", core)

    def test_ui_is_keyboard_accessible_and_progressively_disclosed(self) -> None:
        program = PROGRAM.read_text(encoding="utf-8")
        for label in ("Home", "Setup", "Compatibility", "Diagnostics"):
            self.assertIn(f'NewTab("{label}"', program)
        self.assertIn("AccessibleName", program)
        self.assertIn("AccessibleDescription", program)
        self.assertIn("Keys.Control | Keys.S", program)
        self.assertIn("Keys.F5", program)
        self.assertIn("ValidateSmokeContract", program)
        self.assertIn('"Review setup", "Save changes", "Check again", "Open FOA-SDK", "Close"', program)
        self.assertIn("Export redacted report", program)
        self.assertIn("Open FOA-SDK", program)
        self.assertIn("never scans the whole PC", program)
        self.assertIn("does not write to the selected game path", program)

    def test_packaging_and_reviewed_workflow_include_exact_control_panel(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        packaging = PACKAGING.read_text(encoding="utf-8")
        self.assertIn("Build and verify installed FOA-SDK Control Panel", workflow)
        self.assertIn("Installer/ControlPanel/Windows/FOAControlPanel.csproj", workflow)
        self.assertIn("FOA-SDK-ControlPanel.exe self-test failed before inventory review.", workflow)
        self.assertIn("FOA-SDK-ControlPanel.exe staged self-test failed.", workflow)
        self.assertIn("Start-Process -FilePath $source", workflow)
        self.assertIn("Start-Process -FilePath $stagedControlPanel", workflow)
        self.assertIn("-Wait -PassThru -WindowStyle Hidden", workflow)
        self.assertLess(
            workflow.index("Build and verify installed FOA-SDK Control Panel"),
            workflow.index("Generate notices and third-party package inventory"),
        )
        self.assertIn("FOA-SDK-ControlPanel.exe", packaging)
        self.assertIn('"FOA-SDK Control Panel"', packaging)


if __name__ == "__main__":
    unittest.main()
