# FOA-SDK Windows installer launcher

`Installer/Launcher/Windows/` owns the user-facing Windows entrypoint for the installer review flow.

It builds `FOA-SDK-Installer.exe`, a thin launcher that opens the reviewed Suite Wizard host:

```text
FOA-SDK-Installer.exe
  → Installer/SuiteWizard/Host/Source/suite_wizard_receipt_host.py
  → explicit suite and package review
  → explicit acknowledgement selection
  → caller-supplied confirmer identity and UTC time
  → canonical create-once receipt export
```

The former automatic quick-confirmation path is intentionally disabled. `QuickHost/` remains only as a compatibility alias to the same reviewed host; it does not silently accept acknowledgements, synthesize identity or time, duplicate receipt publication, or claim that a receipt is an installation.

## Default user flow

Normal double-click use opens the complete review host. The operator must:

1. review the exact suite, packages, compatibility, payload, warnings, and authority boundaries;
2. explicitly select every required acknowledgement;
3. provide the confirmer identity and UTC confirmation time;
4. create the exact review confirmation;
5. explicitly choose a receipt destination and export the canonical receipt.

A receipt is review evidence only. It does not copy payloads, install packages, request elevation, launch processes, mutate an installation, publish installation state, or grant runtime authority.

## Boundary

The launcher does not resolve a second plan, copy payloads, launch game/runtime processes, request elevation, coordinate lifecycle execution, publish installation state, mutate products, mutate saves, sign artifacts, publish to the network, mutate catalogues, or promote evidence.

Any later installer execution must separately pass through the admission-bound handoff, exact-capability PackageEngine token/session, payload, process/elevation, lifecycle, publication, registry, and editor-readiness gates.

## Build locally

Use the CMD entrypoint on Windows. It does not depend on PowerShell script execution policy.

From the repository root:

```bat
Installer\Launcher\Windows\build-foa-installer-launcher.cmd -Configuration Release -RuntimeIdentifier win-x64
```

If you are already in `Installer\Launcher\Windows`, run:

```bat
build-foa-installer-launcher.cmd -Configuration Release -RuntimeIdentifier win-x64
```

The expected output is:

```text
Installer/Launcher/Windows/artifacts/FOA-SDK-Installer.exe
```

For a self-contained binary:

```bat
Installer\Launcher\Windows\build-foa-installer-launcher.cmd -Configuration Release -RuntimeIdentifier win-x64 -SelfContained
```

The PowerShell script remains available for environments that explicitly allow scripts, but the CMD wrapper is the supported Windows front door.

## CI artifact

The workflow `FOA-SDK Installer Launcher Build` builds the same launcher on `windows-latest` and uploads this artifact:

```text
FOA-SDK-Installer-win-x64
```

That artifact contains:

```text
FOA-SDK-Installer.exe
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

`--advanced-review` remains accepted as a compatibility no-op because reviewed mode is now the only launcher mode.

## Smoke test

The CI-friendly smoke mode runs the complete graphical review/acknowledgement/confirmation/receipt smoke path and returns the host exit code:

```powershell
FOA-SDK-Installer.exe --installer-root C:\path\to\FOA-SDK\Installer --smoke-test
```
