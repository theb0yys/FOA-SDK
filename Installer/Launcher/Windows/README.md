# FOA-SDK Windows installer

`Installer/Launcher/Windows/` builds `FOA-SDK-Installer.exe`, the self-contained Windows front door for the prebuilt FOA-SDK. It runs natively on Windows x64 with no Python, repository checkout, source build, or separately installed .NET runtime.

## Normal user flow

A normal double-click presents one product setup flow:

1. choose the install folder;
2. select **Install**;
3. wait while FOA-SDK is installed;
4. wait while the installer validates the installed product automatically;
5. on the finish screen, leave **Open FOA-SDK Control Panel** selected for first-run setup, optionally choose **Open FOA-SDK**, and choose whether to **Create desktop shortcut**;
6. select **Finish**.

The normal UI does not expose MSI fingerprints, engine paths, project paths, tool profiles, package review terminology, repair/uninstall choices, or internal editor components. Those are implementation and maintenance concerns, not installation choices.

The installer validates the embedded MSI before Windows Installer is started. After Windows Installer succeeds, the **Validating installation** stage first hashes the installed payload against the packaged `SHA256SUMS` inventory. Every listed file must exist, remain inside the install root, avoid reparse-point traversal, and match its expected SHA-256. The integrity index must include both `INSTALL_MANIFEST.json` and the installed `FOA-SDK.exe`. Only after file integrity passes does setup run `FOA-SDK.exe --self-test` to verify the self-contained layout and writable per-user startup state. Setup reports the product ready only when both checks pass.

Start Menu entries for the Control Panel and Editor launcher are installed automatically. The finish-page desktop option creates a current-user `FOA-SDK.lnk` that targets only the installed `FOA-SDK.exe` launcher.

## Product entry point

The installed user-facing application entry point is:

```text
<install-root>\bin\Windows\profile\Default\FOA-SDK.exe
```

Users perform first-run setup through the installed `FOA-SDK-ControlPanel.exe`, then launch the Editor through `FOA-SDK.exe`, the Start Menu entry, or the optional desktop shortcut. Internal bundled editor/runtime files are not separate user-facing applications.

Internally, `FOA-SDK.exe` resolves and validates the complete self-contained product layout, materializes writable per-user application state, and starts the bundled editor host with the packaged FOA-SDK project. `FOA-SDK.exe --self-test` performs the required layout/startup validation without opening the editor.

## Maintenance and automation

Windows Installer remains the lifecycle authority underneath the simple UI. Repair, uninstall, quiet installation, evidence logging, smoke testing, and the separate Tool Setup Wizard remain available for maintenance and automation through the existing command-line surface and Windows **Installed apps** integration. They are intentionally absent from the normal setup flow.

Supported command-line options remain:

```text
FOA-SDK-Installer.exe [--msi <reviewed.msi>]
  [--install-root <absolute-directory>]
  [--evidence-root <absolute-directory>]
  [--operation install|upgrade|repair|uninstall]
  [--quiet] [--smoke-test]
  [--launch-after-install|--no-launch-after-install]
  [--open-control-panel-after-install|--no-open-control-panel-after-install]
  [--open-tool-wizard-after-install|--no-open-tool-wizard-after-install]
  [--tool-wizard] [--save-tool-profile]
  [--workspace-root <absolute-directory>]
  [--o3de-editor <Editor.exe>]
  [--unity-editor <Unity.exe>]
  [--unity-project <absolute-directory>]
  [--tainted-grail-install <absolute-directory>]
  [--no-dialog]
```

The installed Control Panel is the normal setup surface. The legacy Tool Setup Wizard remains available for maintenance compatibility and is never opened automatically by the normal installer.

## Build

The packaging workflow is the authoritative producer because it binds the exact reviewed MSI:

```powershell
Installer\Launcher\Windows\build-foa-installer-launcher.ps1 `
  -Configuration Release `
  -RuntimeIdentifier win-x64 `
  -InstallerMsi C:\reviewed\Tainted-Grail-FoA-SDK-0.1.0-windows-x64.msi `
  -InstallerMsiChecksum C:\reviewed\Tainted-Grail-FoA-SDK-0.1.0-windows-x64.msi.sha256 `
  -OutputDirectory C:\foa-build\installer-wizard
```

The CMD entry point avoids PowerShell execution-policy dependency:

```bat
Installer\Launcher\Windows\build-foa-installer-launcher.cmd -InstallerMsi C:\reviewed\Tainted-Grail-FoA-SDK-0.1.0-windows-x64.msi -OutputDirectory C:\foa-build\installer-wizard
```

Self-contained single-file output is the default. `--framework-dependent` is development-only and is not the distributed artifact.

If no MSI is supplied at build time, the executable is a development shell and requires exactly one reviewed `.msi` plus its `.msi.sha256` sidecar beside the EXE, or an explicit `--msi` path. This mode must not be represented as the final user artifact.

## Functional readiness smoke

The Windows readiness path remains:

```powershell
Installer\Tests\WindowsFunctionalReadiness\Invoke-FoaWindowsFunctionalReadiness.ps1 `
  -InstallerExe C:\reviewed\FOA-SDK-Installer.exe `
  -InstallRoot "$env:TEMP\installed-foa-sdk" `
  -EvidenceRoot "$env:TEMP\foa-sdk-readiness-evidence" `
  -StagedManifest C:\reviewed-stage\INSTALL_MANIFEST.json `
  -ExternalWorkspace "$env:TEMP\external-foa-workspace"
```

It exercises the hidden maintenance/automation path: wizard construction, clean install, installed launcher and Control Panel self-tests, versioned Setup Manager profile and redacted-report export, legacy Tool Wizard compatibility, repair, uninstall, external workspace preservation, and installer-log capture.

## Security and trust boundary

- Embedded and external MSI bytes are captured and SHA-256 verified before the absolute `%SystemRoot%\System32\msiexec.exe` path starts.
- External MSI paths must be regular `.msi` files, not symbolic links or reparse points.
- The install path must be absolute, must not be a filesystem root, and must not traverse an existing reparse-point directory.
- The executable requests `asInvoker`; it does not silently elevate.
- The installed payload must pass `SHA256SUMS` integrity verification and `FOA-SDK.exe --self-test` before the normal UI reports it ready.
- Current development artifacts are unsigned; artifact provenance remains a distribution concern rather than a normal installer-screen concept.
