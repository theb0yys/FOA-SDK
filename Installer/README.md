# FOA-SDK Windows Installer

`Installer/` owns the source that turns one reviewed prebuilt FOA-SDK payload into a self-contained Windows application setup. End users run `FOA-SDK-Installer.exe`; after installation they finish first-run setup in `FOA-SDK-ControlPanel.exe` and open the Editor through `FOA-SDK.exe`.

The normal user workflow requires no Git, Python, CMake, Visual Studio, engine checkout, project-file selection, or internal tool configuration.

Generated MSI files, portable ZIP archives, retained installer EXEs, staged payloads, build caches, logs, screenshots, signing material, and release uploads belong under the external `foa-build/` root or another reviewed output directory, never in this source tree.

## Layout

```text
Installer/
├── ControlPanel/ installed first-run setup and diagnostics application
├── Launcher/    installer UI and installed FOA-SDK launcher source
├── Packaging/   MSI packaging source for a verified staging payload
└── Tests/       installer contract and functional-readiness tests
```

## Normal installer lifecycle

The package workflow builds the canonical product payload once, records its exact inventory, stages and verifies those bytes, produces the MSI, and embeds that MSI into `FOA-SDK-Installer.exe`.

A normal double-click on the installer follows this product flow:

1. resolve and verify the embedded MSI privately;
2. show only the FOA-SDK installation folder;
3. run Windows Installer for the product files;
4. show **Validating installation** and run installed `FOA-SDK.exe --self-test`;
5. report success only when the installed product passes validation;
6. offer **Open FOA-SDK Control Panel** by default, optional **Open FOA-SDK**, and **Create desktop shortcut** on the finish screen.

Start Menu entries for the Control Panel and Editor launcher are installed automatically. The optional desktop shortcut targets only the installed `FOA-SDK.exe`.

MSI fingerprints, internal manifests, engine/project paths, tool profiles, repair/uninstall choices, logs, and other implementation details are deliberately absent from the normal setup UI.

## Maintenance lifecycle

Windows Installer remains the product-file lifecycle authority underneath the simple UI. It still owns install/upgrade, repair, uninstall, Programs and Features registration, Start Menu integration, and product-file restoration.

Repair, uninstall, quiet installation, diagnostic evidence logging, and the separate Tool Setup Wizard remain available to maintainers and readiness automation through command-line options and Windows Installed apps. They are not normal installation screens.

External workspaces, game diagnostics, generated output, staging, deployment roots, and game files remain outside the installation directory and are never installer-owned.

## Installed application startup

The installed product exposes two user-facing application entry points:

```text
<install-root>\FOA-SDK-ControlPanel.exe
<install-root>\bin\Windows\profile\Default\FOA-SDK.exe
```

`FOA-SDK-ControlPanel.exe` owns first-run workspace selection, explicit read-only game-folder validation, compatibility indication, plan preview, and redacted support reporting. `FOA-SDK.exe` resolves the complete self-contained install root, verifies the required packaged layout, prepares writable per-user application state, and starts the bundled editor host with the packaged FOA-SDK project.

`FOA-SDK.exe --self-test` performs the same required layout and writable-startup checks without opening the application. The installer uses that self-test automatically after installation or repair and fails closed when the installed product is incomplete.

## Updates and repair

Updating a prebuilt install means running a newer reviewed `FOA-SDK-Installer.exe` whose MSI uses the same Upgrade Code and a newer version. There is no automatic updater or background service.

Repair restores product-owned files from the reviewed MSI. Uninstall removes product-owned files and application registration while leaving external workspace data untouched.

## Boundaries

Installer selection or installation does not grant runtime execution, game deployment, save mutation, signing, publication, catalogue mutation, or evidence-promotion authority. The Control Panel inspects only a folder the user explicitly selects, never scans the machine or network, and does not modify, launch, or deploy to Fall of Avalon.

Current development artifacts remain unsigned until a separate release/signing decision. Payload integrity verification remains internal to setup rather than a normal user choice.

## Acceptance

Installer changes require the evidence applicable to the changed surface. For this product flow that includes, when runnable:

1. focused installer contract/static tests;
2. warning-free Windows Release build of `FOA-SDK-Installer.exe`;
3. clean install into a non-game folder;
4. automatic installed `FOA-SDK.exe --self-test` success;
5. finish-screen desktop shortcut creation and launch behavior;
6. repair and uninstall maintenance-path checks;
7. Windows UI confirmation that the normal flow contains only folder selection, install/progress/validation, and finish options.

See [Installing FOA-SDK on Windows](../docs/tainted-grail-sdk/INSTALLING_PREBUILT_SDK.md) for the user-facing flow and [Windows Installer and Prebuilt Artifact Workflow Design](../docs/tainted-grail-sdk/WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md) for packaging internals.
