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
TOOL_PROFILE = LAUNCHER_ROOT / "ToolSetupProfile.cs"
TOOL_WIZARD = LAUNCHER_ROOT / "ToolSetupWizardForm.cs"
MANIFEST = LAUNCHER_ROOT / "app.manifest"
POWERSHELL_BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.ps1"
CMD_BUILD = LAUNCHER_ROOT / "build-foa-installer-launcher.cmd"
README = LAUNCHER_ROOT / "README.md"
FUNCTIONAL_READINESS_SCRIPT = (
    REPO_ROOT
    / "Installer"
    / "Tests"
    / "WindowsFunctionalReadiness"
    / "Invoke-FoaWindowsFunctionalReadiness.ps1"
)
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

    def test_exe_opens_native_installer_directly_without_express_or_python_front_doors(self) -> None:
        program = PROGRAM.read_text(encoding="utf-8")
        self.assertIn("InstallerPayload.Resolve", program)
        self.assertIn("InstallerWizardForm", program)
        self.assertIn("Application.Run(form)", program)
        self.assertIn("ToolSetupWizardForm", program)
        self.assertIn("WindowsInstallerRunner.RunAsync", program)
        self.assertIn("InstalledEditorLauncher.ValidateAsync", program)
        self.assertNotIn("RunExpressInstallAsync", program)
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

    def test_runner_keeps_safe_msi_lifecycle_and_validates_installed_product(self) -> None:
        runner = RUNNER.read_text(encoding="utf-8")
        self.assertIn('Path.Combine(systemDirectory, "msiexec.exe")', runner)
        self.assertIn("FileName = windowsInstallerPath", runner)
        self.assertIn("UseShellExecute = false", runner)
        self.assertIn("!Path.IsPathFullyQualified(systemDirectory)", runner)
        self.assertNotIn('FileName = "msiexec.exe"', runner)
        self.assertIn("Arguments = BuildWindowsInstallerArguments(payload, options, logPath)", runner)
        self.assertIn('InstallerOperation.InstallOrUpgrade => "/i"', runner)
        self.assertIn('InstallerOperation.Repair => "/fvamus"', runner)
        self.assertIn('InstallerOperation.Uninstall => "/x"', runner)
        self.assertIn('"/qn"', runner)
        self.assertIn('"/norestart"', runner)
        self.assertIn('FormatInstallerPropertyArgument("INSTALL_ROOT", options.InstallRoot)', runner)
        self.assertIn('return $"{propertyName}={QuoteProcessArgument(value)}";', runner)
        self.assertIn("QuoteProcessArgument(logPath)", runner)
        self.assertIn("builder.Append('\"')", runner)
        self.assertIn("1639 =>", runner)
        self.assertIn("Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)", runner)
        self.assertIn("EnsureDirectoryExists(logDirectory)", runner)
        self.assertIn("CopyLogToEvidenceIfRequested(logPath, evidenceLogPath)", runner)
        self.assertIn('Path.Combine(options.EvidenceRoot, "installer-logs", logFileName)', runner)
        self.assertIn("Stack<string> missingDirectories", runner)
        self.assertIn("FileAttributes.ReparsePoint", runner)
        self.assertIn('Arguments = "--self-test"', runner)
        self.assertIn("InstalledEditorValidationResult", runner)
        self.assertIn("WaitForExitAsync(timeout.Token)", runner)
        self.assertIn("TimeSpan.FromMinutes(2)", runner)
        self.assertNotIn('Verb = "runas"', runner)
        self.assertNotRegex(runner, re.compile(r"\bcmd(?:\.exe)?\b", re.IGNORECASE))
        self.assertNotRegex(runner, re.compile(r"\bpowershell(?:\.exe)?\b", re.IGNORECASE))

    def test_desktop_shortcut_targets_only_installed_foa_sdk_launcher(self) -> None:
        runner = RUNNER.read_text(encoding="utf-8")
        self.assertIn("CreateDesktopShortcut", runner)
        self.assertIn('Environment.SpecialFolder.DesktopDirectory', runner)
        self.assertIn('Path.Combine(desktop, "FOA-SDK.lnk")', runner)
        self.assertIn('Type.GetTypeFromProgID("WScript.Shell"', runner)
        self.assertIn('"TargetPath"', runner)
        self.assertIn('"WorkingDirectory"', runner)
        self.assertIn('"IconLocation"', runner)
        self.assertIn("ResolveLauncherPath(installRoot)", runner)

    def test_normal_wizard_is_folder_install_validate_finish_only(self) -> None:
        wizard = WIZARD.read_text(encoding="utf-8")
        options = OPTIONS.read_text(encoding="utf-8")

        self.assertIn("Install FOA-SDK", wizard)
        self.assertIn("Install location", wizard)
        self.assertIn('NewSecondaryButton("Browse")', wizard)
        self.assertIn("ShowNewFolderButton = true", wizard)
        self.assertIn("Installing FOA-SDK", wizard)
        self.assertIn("Validating installation", wizard)
        self.assertIn("Checking installed files and startup requirements", wizard)
        self.assertIn("InstalledEditorLauncher.ValidateAsync", wizard)
        self.assertIn('Text = "Open FOA-SDK"', wizard)
        self.assertIn('Text = "Create desktop shortcut"', wizard)
        self.assertIn("InstalledEditorLauncher.CreateDesktopShortcut", wizard)
        self.assertIn("InstalledEditorLauncher.Launch", wizard)
        self.assertIn("FOA-SDK is ready", wizard)
        self.assertIn("installed files passed validation", wizard)
        self.assertIn("FOA-SDK Setup", wizard)
        self.assertNotIn("Reviewed MSI", wizard)
        self.assertNotIn("SHA-256", wizard)
        self.assertNotIn("MSI payload", wizard)
        self.assertNotIn("MSI log", wizard)
        self.assertNotIn("Review Package", wizard)
        self.assertNotIn("Run MSI Payload", wizard)
        self.assertNotIn("Open separate Tool Setup Wizard", wizard)
        self.assertNotIn("ToolSetupWizardLauncher", wizard)
        self.assertNotIn("O3DE Editor:", wizard)
        self.assertNotIn("Unity Editor:", wizard)
        self.assertNotIn("TG install:", wizard)
        self.assertIn('"Programs",\n        "FOA-SDK"', options)
        self.assertIn("bool openToolWizardAfterInstall = false", options)

    def test_tool_wizard_remains_separate_maintenance_surface(self) -> None:
        program = PROGRAM.read_text(encoding="utf-8")
        installer = WIZARD.read_text(encoding="utf-8")
        tool_profile = TOOL_PROFILE.read_text(encoding="utf-8")
        tool_wizard = TOOL_WIZARD.read_text(encoding="utf-8")

        self.assertLess(
            program.index("options.ToolWizardOnly"),
            program.index("InstallerPayload.Resolve"),
        )
        self.assertIn("options.SaveToolProfile", program)
        self.assertIn("ToolSetupProfile.Save", program)
        self.assertNotIn("ToolSetupWizardLauncher", installer)
        self.assertIn("ToolSetupProfile.Save", tool_wizard)
        self.assertIn("FOA-SDK Tool Setup Wizard", tool_wizard)
        self.assertIn("O3DE Editor:", tool_wizard)
        self.assertIn("Unity Editor:", tool_wizard)
        self.assertIn("TG install:", tool_wizard)
        self.assertIn("tool-profile.local.json", tool_profile)
        self.assertIn("conversion_execution_allowed = false", tool_profile)
        self.assertIn("deployment_execution_allowed = false", tool_profile)
        self.assertNotIn("WindowsInstallerRunner.RunAsync", tool_wizard)
        self.assertNotIn("InstallerPayload.Resolve", tool_wizard)

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
            self.assertIn("EnableCompressionInSingleFile=true", contents)
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
        self.assertIn("Invoke-FoaWindowsFunctionalReadiness.ps1", workflow)
        self.assertIn("windows-functional-readiness", workflow)
        self.assertIn("-StagedManifest $stagedManifest", workflow)

    def test_functional_readiness_script_proves_user_flow_and_captures_evidence(self) -> None:
        script = FUNCTIONAL_READINESS_SCRIPT.read_text(encoding="utf-8")
        self.assertIn("foa.sdk.windows_functional_readiness.v1", script)
        self.assertIn("installer-wizard-smoke", script)
        self.assertIn("installer-clean-install", script)
        self.assertIn("tool-profile-save", script)
        self.assertIn("installed-launcher-self-test", script)
        self.assertIn("installer-repair", script)
        self.assertIn("installer-uninstall", script)
        self.assertIn("functional-readiness-summary.json", script)
        self.assertIn("tool-profile.local.json", script)
        self.assertIn("installer-logs", script)
        self.assertIn("ProcessStartInfo", script)
        self.assertIn("--save-tool-profile", script)
        self.assertIn("--evidence-root", script)
        self.assertIn("ready_for_authoring", script)
        self.assertIn("MSI uninstall removed external workspace data", script)

    def test_readme_documents_simple_normal_flow_and_hidden_maintenance_surface(self) -> None:
        readme = README.read_text(encoding="utf-8")
        self.assertIn("FOA-SDK-Installer.exe", readme)
        self.assertIn("choose the install folder", readme.lower())
        self.assertIn("validates the installed product", readme.lower())
        self.assertIn("create desktop shortcut", readme.lower())
        self.assertIn("maintenance and automation", readme.lower())
        self.assertIn("no Python", readme)
        self.assertNotIn("review evidence only", readme)
        self.assertNotIn("Suite Wizard receipt", readme)

    def test_discovery_bridge_registers_wizard_tests(self) -> None:
        bridge = DISCOVERY_BRIDGE.read_text(encoding="utf-8")
        self.assertIn("test_windows_installer_wizard.py", bridge)
        self.assertIn("WindowsInstallerWizardTests", bridge)


if __name__ == "__main__":
    unittest.main()
