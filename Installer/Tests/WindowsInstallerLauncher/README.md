# Windows installer operational smoke

`Invoke-FoaInstallerWindowsSmoke.ps1` is the exact-head Windows operational gate for the FOA-SDK installer source.

It runs on `windows-2022` from the read-only PR validation workflow when Windows installer paths change. The smoke builds a small self-contained `FOA-SDK.exe` fixture, packages it into a fixture MSI, builds the exact `FOA-SDK-Installer.exe` from the reviewed source head, and exercises the installer without requiring the full O3DE distribution payload.

The smoke proves:

- the installer executable builds as a self-contained Windows application;
- the normal wizard can be constructed;
- clean installation succeeds;
- installed payload hashes and startup self-test pass before success is reported;
- a deliberately damaged installed launcher is restored by Repair;
- a deliberately incorrect checksum inventory is rejected;
- uninstall removes installer-owned files;
- the literal guided UI flow reaches `FOA-SDK is ready`, leaves `Open FOA-SDK` and `Create desktop shortcut` checked, and completes through `Finish`;
- the checked launch option invokes the installed `FOA-SDK.exe` fixture;
- the desktop shortcut is created and targets the installed `FOA-SDK.exe`;
- external workspace data survives the installer lifecycle.

The smoke is intentionally narrower than the canonical reviewed distribution-package workflow. It validates the installer implementation and user flow with a fixture MSI; it does not claim that the full O3DE payload has been inventory-reviewed, packaged, signed, released, or installed.

Run on Windows with:

```powershell
Installer\Tests\WindowsInstallerLauncher\Invoke-FoaInstallerWindowsSmoke.ps1 `
  -EvidenceRoot "$env:TEMP\foa-sdk-installer-smoke-evidence"
```

The script writes `windows-installer-smoke-summary.json` plus MSI logs under the evidence root. A successful process exit means every asserted smoke stage completed; failures are fail-closed and leave the summary status as `FAILED`.
