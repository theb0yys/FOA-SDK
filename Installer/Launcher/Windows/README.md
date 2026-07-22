# FOA-SDK Windows installer launcher

`Installer/Launcher/Windows/` owns the user-facing Windows entrypoint for the installer flow.

It builds `FOA-SDK-Installer.exe`, a thin launcher that opens the quick installer by default:

```text
FOA-SDK-Installer.exe
  → Installer/SuiteWizard/QuickHost/Source/quick_installer_host.py
  → choose install location
  → one-click default install preparation
  → automatic confirmation timestamp and receipt export
```

The old detailed Suite Wizard receipt host is still available, but only as an advanced/audit path:

```powershell
FOA-SDK-Installer.exe --advanced-review
```

## Default user flow

Normal double-click use opens a quick installer screen with an **Install location** field, a **Browse…** button, and an **Install** button. That path:

- uses the reviewed default FOA-SDK suite;
- shows the default install location under the current user's local application data directory;
- lets the user choose another install location before clicking Install;
- keeps the selected default packages visible;
- handles the internal acknowledgement set automatically as part of the Install click;
- fills the confirmation identity from the local user account;
- fills the UTC timestamp automatically;
- writes a verified `.foa-receipt.json` under the local FOA-SDK receipt directory using binary create-once bytes.

The user should not have to manually type timestamps, hashes, acknowledgement IDs, or receipt metadata in the default path.

## Boundary

The launcher and quick host do not copy payloads, launch game/runtime processes, request elevation, coordinate lifecycle execution, publish installation state, mutate products, mutate saves, sign artifacts, publish to the network, mutate catalogues, or promote evidence.

They provide the user-facing executable and quick preparation flow. Any later installer execution still has to pass through the capability-gated PackageEngine, admission, handoff, lifecycle, publication, registry, and editor-readiness chain.

## Build

From PowerShell on Windows with the .NET SDK installed, run this from the repository root:

```powershell
.\Installer\Launcher\Windows\build-foa-installer-launcher.ps1 `
  -Configuration Release `
  -RuntimeIdentifier win-x64
```

If you are already in `Installer\Launcher\Windows`, run:

```powershell
.\build-foa-installer-launcher.ps1 `
  -Configuration Release `
  -RuntimeIdentifier win-x64
```

The expected output is:

```text
Installer/Launcher/Windows/artifacts/FOA-SDK-Installer.exe
```

For a self-contained binary:

```powershell
.\Installer\Launcher\Windows\build-foa-installer-launcher.ps1 `
  -Configuration Release `
  -RuntimeIdentifier win-x64 `
  -SelfContained
```

## Run

From the product checkout:

```powershell
.\Installer\Launcher\Windows\artifacts\FOA-SDK-Installer.exe
```

Or provide explicit paths:

```powershell
FOA-SDK-Installer.exe `
  --installer-root C:\path\to\FOA-SDK\Installer `
  --python C:\path\to\pythonw.exe
```

The launcher also checks `FOA_SDK_PYTHON` and bundled runtime locations under `runtime/python/pythonw.exe`.

## Advanced review

Use this only when you specifically need the full audit/receipt host:

```powershell
FOA-SDK-Installer.exe --advanced-review
```

That advanced path exposes the detailed review, acknowledgement, confirmation, and receipt tabs.

## Smoke test

The CI-friendly smoke mode runs the quick installer path, writes a temporary verified receipt, and returns the host exit code:

```powershell
FOA-SDK-Installer.exe --installer-root C:\path\to\FOA-SDK\Installer --smoke-test
```
