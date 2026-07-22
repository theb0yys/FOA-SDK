# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
LAUNCHER_ROOT = REPO_ROOT / "Installer" / "Launcher" / "Windows"
PROJECT = LAUNCHER_ROOT / "FOAInstallerLauncher.csproj"
PROGRAM = LAUNCHER_ROOT / "Program.cs"
MANIFEST = LAUNCHER_ROOT / "app.manifest"
BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.ps1"
README = LAUNCHER_ROOT / "README.md"
QUICK_HOST = REPO_ROOT / "Installer" / "SuiteWizard" / "QuickHost" / "Source" / "quick_installer_host.py"
DISCOVERY_BRIDGE = REPO_ROOT / "Gems" / "TaintedGrailModdingSDK" / "Tools" / "tests" / "test_installer_windows_launcher.py"


class WindowsInstallerLauncherTests(unittest.TestCase):
    def test_launcher_project_builds_user_facing_exe_name(self) -> None:
        root = ET.fromstring(PROJECT.read_text(encoding="utf-8"))
        values = {child.tag: (child.text or "") for group in root for child in group}
        self.assertEqual(values["OutputType"], "WinExe")
        self.assertEqual(values["TargetFramework"], "net8.0-windows")
        self.assertEqual(values["UseWindowsForms"], "true")
        self.assertEqual(values["AssemblyName"], "FOA-SDK-Installer")
        self.assertEqual(values["ApplicationManifest"], "app.manifest")

    def test_launcher_manifest_is_user_mode_not_elevation_prompt(self) -> None:
        manifest = MANIFEST.read_text(encoding="utf-8")
        self.assertIn('requestedExecutionLevel level="asInvoker"', manifest)
        self.assertNotIn("requireAdministrator", manifest)
        self.assertNotIn("highestAvailable", manifest)
        self.assertNotIn("uiAccess=\"true\"", manifest)

    def test_launcher_default_is_quick_installer_not_receipt_review(self) -> None:
        program = PROGRAM.read_text(encoding="utf-8")
        self.assertIn("quick_installer_host.py", program)
        self.assertIn("--advanced-review", program)
        self.assertIn("suite_wizard_receipt_host.py", program)
        self.assertIn("--installer-root", program)
        self.assertIn("--receipt-root", program)
        self.assertIn("--smoke-test", program)
        self.assertIn("FOA_SDK_PYTHON", program)
        self.assertIn("UseShellExecute = false", program)
        self.assertNotIn("Verb = \"runas\"", program)
        self.assertNotIn("ShellExecuteW", program)
        self.assertNotIn("package_engine.py", program)
        self.assertNotIn("coordinate_lifecycle", program)
        self.assertNotIn("publish_state_record", program)
        self.assertNotIn("ElevationHelper", program)
        self.assertNotRegex(program, re.compile(r"\b(password|credential|secret)\b", re.IGNORECASE))

    def test_quick_host_auto_handles_internal_review_receipt_details(self) -> None:
        host = QUICK_HOST.read_text(encoding="utf-8")
        self.assertIn("Install FOA-SDK", host)
        self.assertIn("create_quick_install_receipt", host)
        self.assertIn("utc_now()", host)
        self.assertIn("default_confirmed_by()", host)
        self.assertIn("controller.resolve_review()", host)
        self.assertIn("controller.set_acknowledgement", host)
        self.assertIn("export_current_receipt", host)
        self.assertIn("Advanced review", host)
        self.assertNotIn("confirmed_at_var", host)
        self.assertNotIn("confirmation_hash_vars", host)
        self.assertNotRegex(host, re.compile(r"\b(password|credential|secret)\b", re.IGNORECASE))

    def test_build_script_publishes_expected_exe(self) -> None:
        build = BUILD.read_text(encoding="utf-8")
        self.assertIn("dotnet", build)
        self.assertIn("publish", build)
        self.assertIn("/p:PublishSingleFile=true", build)
        self.assertIn("FOA-SDK-Installer.exe", build)
        self.assertIn("--self-contained=false", build)
        self.assertIn("SelfContained", build)

    def test_readme_documents_quick_default_and_advanced_review_escape_hatch(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("FOA-SDK-Installer.exe", readme)
        self.assertIn("quick installer", readme)
        self.assertIn("one-click", readme)
        self.assertIn("--advanced-review", readme)
        self.assertIn("--smoke-test", readme)
        self.assertIn("capability-gated PackageEngine", readme)

    def test_discovery_bridge_registers_launcher_tests(self) -> None:
        bridge = DISCOVERY_BRIDGE.read_text(encoding="utf-8")
        self.assertIn("test_windows_installer_launcher.py", bridge)
        self.assertIn("WindowsInstallerLauncherTests", bridge)


if __name__ == "__main__":
    unittest.main()
