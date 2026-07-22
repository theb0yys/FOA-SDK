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
POWERSHELL_BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.ps1"
CMD_BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.cmd"
README = LAUNCHER_ROOT / "README.md"
QUICK_HOST = REPO_ROOT / "Installer" / "SuiteWizard" / "QuickHost" / "Source" / "quick_installer_host.py"
LAUNCHER_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "build-foa-sdk-installer-launcher.yml"
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

    def test_quick_host_has_normal_install_location_and_binary_receipt_export(self) -> None:
        host = QUICK_HOST.read_text(encoding="utf-8")
        self.assertIn("Install FOA-SDK", host)
        self.assertIn("Install location", host)
        self.assertIn("Browse…", host)
        self.assertIn("browse_install_location", host)
        self.assertIn("default_install_root()", host)
        self.assertIn("--target-root", host)
        self.assertIn("target_root", host)
        self.assertIn("create_quick_install_receipt", host)
        self.assertIn("utc_now()", host)
        self.assertIn("default_confirmed_by()", host)
        self.assertIn("controller.resolve_review()", host)
        self.assertIn("controller.set_acknowledgement", host)
        self.assertIn("export_quick_receipt", host)
        self.assertIn('target.open("xb")', host)
        self.assertIn("canonical_receipt_bytes", host)
        self.assertIn("Advanced review", host)
        self.assertNotIn("export_current_receipt", host)
        self.assertNotIn("confirmed_at_var", host)
        self.assertNotIn("confirmation_hash_vars", host)
        self.assertNotRegex(host, re.compile(r"\b(password|credential|secret)\b", re.IGNORECASE))

    def test_cmd_build_entrypoint_publishes_expected_exe_without_powershell_policy(self) -> None:
        build = CMD_BUILD.read_text(encoding="utf-8")
        self.assertIn("dotnet publish", build)
        self.assertIn("FOAInstallerLauncher.csproj", build)
        self.assertIn("/p:PublishSingleFile=true", build)
        self.assertIn("/p:AssemblyName=FOA-SDK-Installer", build)
        self.assertIn("FOA-SDK-Installer.exe", build)
        self.assertIn("-RuntimeIdentifier", build)
        self.assertNotIn("Set-ExecutionPolicy", build)
        self.assertNotIn("powershell", build.lower())

    def test_powershell_build_script_remains_available(self) -> None:
        build = POWERSHELL_BUILD.read_text(encoding="utf-8")
        self.assertIn("dotnet", build)
        self.assertIn("publish", build)
        self.assertIn("/p:PublishSingleFile=true", build)
        self.assertIn("FOA-SDK-Installer.exe", build)
        self.assertIn("--self-contained=false", build)
        self.assertIn("SelfContained", build)

    def test_launcher_artifact_workflow_builds_and_uploads_exe(self) -> None:
        workflow = LAUNCHER_WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("windows-latest", workflow)
        self.assertIn("build-foa-installer-launcher.cmd", workflow)
        self.assertIn("actions/setup-dotnet@v4", workflow)
        self.assertIn("actions/upload-artifact@v4", workflow)
        self.assertIn("FOA-SDK-Installer-win-x64", workflow)
        self.assertIn("FOA-SDK-Installer.exe", workflow)

    def test_readme_documents_cmd_build_artifact_and_quick_default(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("FOA-SDK-Installer.exe", readme)
        self.assertIn("quick installer", readme)
        self.assertIn("one-click", readme)
        self.assertIn("install location", readme.lower())
        self.assertIn("build-foa-installer-launcher.cmd", readme)
        self.assertIn("FOA-SDK-Installer-win-x64", readme)
        self.assertIn("--advanced-review", readme)
        self.assertIn("--smoke-test", readme)
        self.assertIn("capability-gated PackageEngine", readme)

    def test_discovery_bridge_registers_launcher_tests(self) -> None:
        bridge = DISCOVERY_BRIDGE.read_text(encoding="utf-8")
        self.assertIn("test_windows_installer_launcher.py", bridge)
        self.assertIn("WindowsInstallerLauncherTests", bridge)


if __name__ == "__main__":
    unittest.main()
