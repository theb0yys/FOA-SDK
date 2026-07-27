# FOA-SDK Windows installer wizard

`Installer/Launcher/Windows/` builds `FOA-SDK-Installer.exe`, the self-contained Windows Forms front door for the prebuilt FOA-SDK. It runs natively on Windows x64 with no Python, repository checkout, source build, separately installed .NET runtime, or separate user-handled MSI.

The user-facing artifact embeds one reviewed internal MSI and its canonical lowercase `.sha256` sidecar. On startup the executable captures the MSI in a private temporary directory, verifies the captured bytes, then presents install/upgrade, repair, and uninstall choices. Windows Installer remains responsible for all product-file and lifecycle changes, but the user contract remains the executable wizard.

The wizard runs as the current user. The internal MSI is per-user and does not require administrator elevation. The executable never launches FoA, deploys runtime adapters, mutates saves or workspaces, signs artifacts, or publishes a release.

## Build

The packaging workflow is the authoritative producer because it binds the exact reviewed internal MSI into the executable:

```powershell
Installer\Launcher\Windows\build-foa-installer-launcher.ps1 `
  -Configuration Release `
  -RuntimeIdentifier win-x64 `
  -InstallerMsi C:\reviewed\Tainted-Grail-FoA-SDK-0.1.0-windows-x64.msi `
  -InstallerMsiChecksum C:\reviewed\Tainted-Grail-FoA-SDK-0.1.0-windows-x64.msi.sha256 `
  -OutputDirectory C:\foa-build\installer-wizard
```

The CMD entry point avoids PowerShell execution-policy dependency for maintainer/developer builds:

```bat
Installer\Launcher\Windows\build-foa-installer-launcher.cmd -InstallerMsi C:\reviewed\Tainted-Grail-FoA-SDK-0.1.0-windows-x64.msi -OutputDirectory C:\foa-build\installer-wizard
```

Self-contained single-file output is the default and the only normal user artifact. `--framework-dependent` is a development-only build option and is not the distributed artifact.

If no MSI is supplied at build time, the executable is a development shell and requires exactly one reviewed `.msi` plus its `.msi.sha256` sidecar beside the EXE, or an explicit `--msi` path. This mode is for maintainer/developer diagnostics only and must not be represented as the final user artifact or normal install workflow.

## Run

Normal user use is a double-click on `FOA-SDK-Installer.exe`. The normal command-line surface is:

```text
FOA-SDK-Installer.exe
  [--install-root <absolute-directory>]
  [--operation install|upgrade|repair|uninstall]
  [--launch-after-install|--no-launch-after-install]
```

Maintainer/developer diagnostics may additionally use:

```text
FOA-SDK-Installer.exe [--msi <reviewed.msi>]
  [--quiet] [--smoke-test] [--no-dialog]
```

The `--msi` path is not a user workflow and must remain bound to one reviewed MSI plus its exact checksum sidecar. `--smoke-test` verifies payload resolution and constructs the wizard without applying MSI changes. The packaging smoke then uses `--quiet` for the real clean-install, repair, and uninstall lifecycle and checks the installed Editor launcher plus an external workspace sentinel.

Verbose MSI logs are written beneath `%LOCALAPPDATA%\FOA-SDK\Installer\Logs`. A success code of 1641 or 3010 is reported as successful with a Windows restart required.

## Security and trust boundary

- Embedded and developer-supplied MSI bytes are captured and SHA-256 verified before the absolute
  `%SystemRoot%\System32\msiexec.exe` path starts; `PATH` lookup is not used.
- External MSI paths are maintainer/developer diagnostics only and must be regular `.msi` files, not symbolic links or reparse points.
- The install path must be absolute, must not be a filesystem root, and must not traverse an existing reparse-point directory.
- Process arguments use `ProcessStartInfo.ArgumentList`, not a concatenated command line.
- The executable requests `asInvoker`; it does not use `runas` or silently elevate.
- Current artifacts are unsigned. Verify provenance and supplied hashes; a checksum is not a code-signing identity.
