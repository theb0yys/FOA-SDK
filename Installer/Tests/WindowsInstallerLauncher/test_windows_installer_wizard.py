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
OPTIONS = LAUNCHER_ROOT / "InstallerOptions.cs"
PAYLOAD = LAUNCHER_ROOT / "InstallerPayload.cs"
RUNNER = LAUNCHER_ROOT / "WindowsInstallerRunner.cs"
WIZARD = LAUNCHER_ROOT / "InstallerWizardForm.cs"
MANIFEST = LAUNCHER_ROOT / "app.manifest"
POWERSHELL_BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.ps1"
CMD_BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.cmd"
README = LAUNCHER_ROOT / "README.md"
DISCOVERY_BRIDGE = (
    REPO_ROOT
    / "Gems"
    / "TaintedGrailModdingSDK"
    / "Tools"
    / "tests"
    / "test_installer_windows_launcher.py"
)


class WindowsInstallerWizardTests(unittest.TestCase):
    def test_project_builds_self_contained_winforms_exe_with_optional_embedded_msi(self) -> None:
        root = ET.fromstring(PROJECT.read_text(encoding="utf-8"))
        values = {child.tag: (child.text or "") for group in root for child in group}
        self.assertEqual(values["OutputType"], "WinExe")
        self.assertEqual(values["TargetFramework"], "net8.0-windows")
        self.assertEqual(values["UseWindowsForms"], "true")
        self.assertEqual(values["AssemblyName"], "FOA-SDK-Installer")
        self.assertEqual(values["PublishSingleFile"], "true")
        project = PROJECT.read_text(encoding="utf-8")
        self.assertIn('LogicalName="FOA.SDK.Payload.msi"', project)
        self.assertIn('LogicalName="FOA.SDK.Payload.msi.sha256"', project)
        self.assertIn("ValidateEmbeddedInstallerPayload", project)

    def test_manifest_remains_per_user_and_never_forces_elevation(self) -> None:
        manifest = MANIFEST.read_text(encoding="utf-8")
        self.assertIn('requestedExecutionLevel level="asInvoker"', manifest)
        self.assertNotIn("requireAdministrator", manifest)
        self.assertNotIn("highestAvailable", manifest)
        self.assertNotIn('uiAccess="true"', manifest)

    def test_exe_runs_native_installer_wizard_without_python_or_receipts(self) -> None:
        program = PROGRAM.read_text(encoding="utf-8")
        self.assertIn("InstallerPayload.Resolve", program)
        self.assertIn("InstallerWizardForm", program)
        self.assertIn("WindowsInstallerRunner.RunAsync", program)
        self.assertNotRegex(program, re.compile(r"\bpython(w|3)?(?:\.exe)?\b", re.IGNORECASE))
        self.assertNotIn("SuiteWizard", program)
        self.assertNotIn("receipt", program.lower())
        self.assertNotIn("Arguments =", program)

    def test_payload_is_embedded_or_adjacent_and_sha256_verified(self) -> None:
        payload = PAYLOAD.read_text(encoding="utf-8")
        self.assertIn('EmbeddedMsiName = "FOA.SDK.Payload.msi"', payload)
        self.assertIn('EmbeddedChecksumName = "FOA.SDK.Payload.msi.sha256"', payload)
        self.assertIn('GetFiles("*.msi"', payload)
        self.assertIn("SHA256.HashData", payload)
        self.assertIn("canonical lowercase SHA-256", payload)
        self.assertIn("FileAttributes.ReparsePoint", payload)
        self.assertIn("FileShare.Read", payload)
        self.assertIn('"FOA-SDK-Payload.msi"', payload)
        self.assertIn("return new InstallerPayload(captured, expected, temporaryRoot)", payload)

    def test_runner_uses_argument_list_for_install_repair_and_uninstall(self) -> None:
        runner = RUNNER.read_text(encoding="utf-8")
        self.assertIn('FileName = "msiexec.exe"', runner)
        self.assertIn("startInfo.ArgumentList.Add", runner)
        self.assertIn('InstallerOperation.InstallOrUpgrade => "/i"', runner)
        self.assertIn('InstallerOperation.Repair => "/fa"', runner)
        self.assertIn('InstallerOperation.Uninstall => "/x"', runner)
        self.assertIn('startInfo.ArgumentList.Add("/qn")', runner)
        self.assertIn("INSTALL_ROOT=", runner)
        self.assertIn("FileAttributes.ReparsePoint", runner)
        self.assertNotIn('Verb = "runas"', runner)
        self.assertNotIn("Arguments =", runner)

    def test_wizard_exposes_lifecycle_and_external_workspace_boundary(self) -> None:
        wizard = WIZARD.read_text(encoding="utf-8")
        options = OPTIONS.read_text(encoding="utf-8")
        self.assertIn("Install or upgrade the complete FOA-SDK", wizard)
        self.assertIn("Repair the installed FOA-SDK", wizard)
        self.assertIn("Uninstall FOA-SDK", wizard)
        self.assertIn("External workspaces are never removed", wizard)
        self.assertIn("Reviewed MSI SHA-256", wizard)
        self.assertIn("--operation install|repair|uninstall", options)
        self.assertIn("--smoke-test", options)
        self.assertIn("--quiet", options)

    def test_build_entrypoints_embed_reviewed_msi_and_default_self_contained(self) -> None:
        cmd = CMD_BUILD.read_text(encoding="utf-8")
        ps1 = POWERSHELL_BUILD.read_text(encoding="utf-8")
        for contents in (cmd, ps1):
            self.assertIn("dotnet", contents)
            self.assertIn("publish", contents)
            self.assertIn("InstallerMsiPath", contents)
            self.assertIn("InstallerMsiChecksumPath", contents)
            self.assertIn("FOA-SDK-Installer.exe", contents)
            self.assertIn("PublishSingleFile=true", contents)
            self.assertIn("BaseIntermediateOutputPath", contents)
            self.assertIn("BaseOutputPath", contents)
        self.assertIn('set "SELF_CONTAINED=true"', cmd)
        self.assertIn("--self-contained=$(-not $FrameworkDependent)", ps1)
        self.assertNotIn("Set-ExecutionPolicy", cmd)

    def test_workflow_checksums_the_final_executable(self) -> None:
        workflow = (REPO_ROOT / ".github/workflows/tainted-grail-sdk-installer.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("FOA-SDK-Installer.exe.sha256", workflow)
        self.assertIn("Get-FileHash", workflow)
        self.assertIn("BaseIntermediateOutputPath", workflow)
        self.assertIn("BaseOutputPath", workflow)
        self.assertIn("Start-Process -FilePath $wizard -Wait -PassThru", workflow)
        self.assertIn("$install.ExitCode", workflow)
        self.assertIn("$repair.ExitCode", workflow)
        self.assertIn("$remove.ExitCode", workflow)

    def test_readme_documents_real_installation_not_review_receipts(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("FOA-SDK-Installer.exe", readme)
        self.assertIn("embeds one reviewed MSI", readme)
        self.assertIn("install/upgrade, repair, and uninstall", readme)
        self.assertIn("no Python", readme)
        self.assertNotIn("review evidence only", readme)
        self.assertNotIn("Suite Wizard receipt", readme)

    def test_discovery_bridge_registers_wizard_tests(self) -> None:
        bridge = DISCOVERY_BRIDGE.read_text(encoding="utf-8")
        self.assertIn("test_windows_installer_wizard.py", bridge)
        self.assertIn("WindowsInstallerWizardTests", bridge)


if __name__ == "__main__":
    unittest.main()
