# FOA-SDK Windows installer wizard

`Installer/Launcher/Windows/` builds `FOA-SDK-Installer.exe`, the self-contained Windows Forms front door for the prebuilt FOA-SDK. It runs natively on Windows x64 with no Python, repository checkout, source build, or separately installed .NET runtime.

The release artifact embeds one reviewed MSI and its canonical lowercase `.sha256` sidecar. On startup the executable captures the MSI in a private temporary directory, verifies the captured bytes, then presents install/upgrade, repair, and uninstall choices. Windows Installer remains responsible for all product-file and lifecycle changes.

The wizard runs as the current user. The MSI is per-user and does not require administrator elevation. The executable never launches FoA, deploys runtime adapters, mutates saves or workspaces, signs artifacts, or publishes a release.

After a successful install or repair, the result page can open the separate Tool Wizard. The Tool Wizard is a local readiness step for the user workspace, O3DE Editor, Unity conversion project, and local Tainted Grail install path. It is not part of the MSI lifecycle and can be opened directly with `--tool-wizard` without resolving an MSI.

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

Self-contained single-file output is the default. `--framework-dependent` is a development-only build option and is not the distributed artifact.

If no MSI is supplied at build time, the executable is a development shell and requires exactly one reviewed `.msi` plus its `.msi.sha256` sidecar beside the EXE, or an explicit `--msi` path. This mode must not be represented as the final user artifact.

## Run

Normal use is a double-click on `FOA-SDK-Installer.exe`. The supported command-line surface is:

```text
FOA-SDK-Installer.exe [--msi <reviewed.msi>]
  [--install-root <absolute-directory>]
  [--evidence-root <absolute-directory>]
  [--operation install|upgrade|repair|uninstall]
  [--quiet] [--smoke-test]
  [--launch-after-install|--no-launch-after-install]
  [--open-tool-wizard-after-install|--no-open-tool-wizard-after-install]
  [--tool-wizard] [--save-tool-profile]
  [--workspace-root <absolute-directory>]
  [--o3de-editor <Editor.exe>]
  [--unity-editor <Unity.exe>]
  [--unity-project <absolute-directory>]
  [--tainted-grail-install <absolute-directory>]
  [--no-dialog]
```

`--smoke-test` verifies payload resolution and constructs the wizard without applying MSI changes. The packaging smoke then uses `--quiet` for the real clean-install, repair, and uninstall lifecycle and checks the installed Editor launcher plus an external workspace sentinel.

`--tool-wizard --smoke-test` constructs the Tool Wizard without resolving an MSI. `--tool-wizard --save-tool-profile --no-dialog` saves the same local profile from command-line paths for Windows readiness automation. Normal Tool Wizard use saves `%LOCALAPPDATA%\FOA-SDK\ToolWizard\tool-profile.local.json` and creates the selected external workspace directory if needed. It records preview readiness only; conversion and deployment execution stay disabled until later reviewed flows.

Verbose MSI logs are written beneath `%LOCALAPPDATA%\FOA-SDK\Installer\Logs` by default, or beneath `<evidence-root>\installer-logs` when `--evidence-root` is supplied. A success code of 1641 or 3010 is reported as successful with a Windows restart required.

## Functional readiness smoke

The reusable Windows smoke path is:

```powershell
Installer\Tests\WindowsFunctionalReadiness\Invoke-FoaWindowsFunctionalReadiness.ps1 `
  -InstallerExe C:\reviewed\FOA-SDK-Installer.exe `
  -InstallRoot "$env:TEMP\installed-foa-sdk" `
  -EvidenceRoot "$env:TEMP\foa-sdk-readiness-evidence" `
  -StagedManifest C:\reviewed-stage\INSTALL_MANIFEST.json `
  -ExternalWorkspace "$env:TEMP\external-foa-workspace"
```

It proves the user flow without touching game files: installer wizard smoke, Tool Wizard smoke, clean install, installed `FOA-SDK.exe --self-test`, Tool Wizard profile save, repair after deliberately damaging the installed launcher, uninstall, external workspace preservation, MSI log capture, and `functional-readiness-summary.json`.

## Security and trust boundary

- Embedded and external MSI bytes are captured and SHA-256 verified before the absolute
  `%SystemRoot%\System32\msiexec.exe` path starts; `PATH` lookup is not used.
- External MSI paths must be regular `.msi` files, not symbolic links or reparse points.
- The install path must be absolute, must not be a filesystem root, and must not traverse an existing reparse-point directory.
- Process arguments use `ProcessStartInfo.ArgumentList`, not a concatenated command line.
- The executable requests `asInvoker`; it does not use `runas` or silently elevate.
- Current artifacts are unsigned. Verify provenance and supplied hashes; a checksum is not a code-signing identity.
