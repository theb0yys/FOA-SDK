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

    def test_launcher_default_is_explicit_review_host(self) -> None:
        program = PROGRAM.read_text(encoding="utf-8")
        self.assertIn("suite_wizard_receipt_host.py", program)
        self.assertNotIn("quick_installer_host.py", program)
        self.assertIn("ArgumentList.Add(paths.ReviewHost.FullName)", program)
        self.assertIn("ArgumentList.Add(\"--installer-root\")", program)
        self.assertNotIn("Arguments =", program)
        self.assertNotIn("private static string Quote", program)
        self.assertIn("Win32Exception", program)
        self.assertIn("--advanced-review", program)
        self.assertNotIn("--receipt-root", program)
        self.assertIn("--smoke-test", program)
        self.assertIn("FOA_SDK_PYTHON", program)
        self.assertIn("UseShellExecute = false", program)
        self.assertNotIn("Verb = \"runas\"", program)
        self.assertNotIn("ShellExecuteW", program)
        self.assertNotIn("package_engine.py", program)
        self.assertNotIn("coordinate_lifecycle", program)
        self.assertNotIn("publish_state_record", program)
        self.assertNotRegex(program, re.compile(r"\b(password|credential|secret)\b", re.IGNORECASE))

    def test_quick_host_is_only_a_reviewed_host_compatibility_alias(self) -> None:
        host = QUICK_HOST.read_text(encoding="utf-8")
        compile(host, str(QUICK_HOST), "exec")
        self.assertIn("reviewed_main", host)
        self.assertIn("suite_wizard_receipt_host", host)
        self.assertNotIn("set_acknowledgement", host)
        self.assertNotIn("default_confirmed_by", host)
        self.assertNotIn("utc_now", host)
        self.assertNotIn("export_quick_receipt", host)
        self.assertNotIn("target.open", host)
        self.assertNotIn("Install FOA-SDK", host)

    def test_cmd_build_entrypoint_publishes_expected_exe_without_script_policy_dependency(self) -> None:
        build = CMD_BUILD.read_text(encoding="utf-8")
        self.assertIn("dotnet publish", build)
        self.assertIn("FOAInstallerLauncher.csproj", build)
        self.assertIn("/p:PublishSingleFile=true", build)
        self.assertIn("/p:AssemblyName=FOA-SDK-Installer", build)
        self.assertIn("FOA-SDK-Installer.exe", build)
        self.assertIn("-RuntimeIdentifier", build)
        self.assertNotIn("Set-ExecutionPolicy", build)
        self.assertNotIn("powershell.exe", build.lower())

    def test_powershell_build_script_remains_available(self) -> None:
        build = POWERSHELL_BUILD.read_text(encoding="utf-8")
        self.assertIn("dotnet", build)
        self.assertIn("publish", build)
        self.assertIn("/p:PublishSingleFile=true", build)
        self.assertIn("FOA-SDK-Installer.exe", build)
        self.assertIn("--self-contained=false", build)
        self.assertIn("SelfContained", build)

    def test_readme_documents_review_not_install(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("FOA-SDK-Installer.exe", readme)
        self.assertIn("explicit acknowledgement", readme)
        self.assertIn("review evidence only", readme)
        self.assertIn("does not copy payloads", readme)
        self.assertIn("build-foa-installer-launcher.cmd", readme)
        self.assertIn("--advanced-review", readme)
        self.assertIn("--smoke-test", readme)
        self.assertNotIn("one-click", readme)
        self.assertNotIn("handles the internal acknowledgement set automatically", readme)

    def test_discovery_bridge_registers_launcher_tests(self) -> None:
        bridge = DISCOVERY_BRIDGE.read_text(encoding="utf-8")
        self.assertIn("test_windows_installer_launcher.py", bridge)
        self.assertIn("WindowsInstallerLauncherTests", bridge)


if __name__ == "__main__":
    unittest.main()
