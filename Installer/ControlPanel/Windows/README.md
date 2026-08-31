# FOA-SDK Control Panel

`FOA-SDK-ControlPanel.exe` is the installed setup and diagnostics application for the prebuilt Windows SDK. It is a self-contained WinForms application; normal users do not need a repository checkout, Python, or a separate .NET runtime.

## User flow

The Control Panel presents four focused pages:

- **Home** — product, workspace, game, and runtime-route status at a glance;
- **Setup** — choose an external workspace and an explicit Fall of Avalon folder;
- **Compatibility** — read the bounded local observations and the non-mutating plan preview;
- **Diagnostics** — export a redacted support report and review the authority boundary.

The installer opens this application by default after a successful install. It can also be reopened from the Start Menu as **FOA-SDK Control Panel**.

## First-release boundary

The Control Panel validates one user-selected game folder. It does not scan the machine, use the network, install a loader, convert assets, deploy files, launch the game, inspect saves, or grant runtime compatibility authority.

Mono and IL2CPP remain separate routes. Recognized marker files produce only `mono-indicated`, `il2cpp-indicated`, or `unknown`; an indication is never represented as runtime proof.

The versioned local profile is stored at:

```text
%LOCALAPPDATA%\FOA-SDK\SetupManager\setup-profile.local.json
```

An existing Tool Setup Wizard profile can be read as a one-way compatibility input when the new profile does not yet exist. The legacy file is not overwritten. Support reports use `foa.sdk.support_report.v1` and replace local paths with deterministic hashes.

## Build and validation

```powershell
dotnet publish Installer\ControlPanel\Windows\FOAControlPanel.csproj `
  --configuration Release `
  --runtime win-x64 `
  --self-contained true

FOA-SDK-ControlPanel.exe --self-test --no-dialog
FOA-SDK-ControlPanel.exe --smoke-test --no-dialog
```

Headless readiness automation may also use `--save-profile`, `--workspace-root`, `--game-install`, and `--export-report`. These switches preserve the same read-only boundary as the UI.
