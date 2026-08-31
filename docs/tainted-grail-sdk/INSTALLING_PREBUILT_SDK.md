# Installing FOA-SDK on Windows

## Current availability

FOA-SDK is still in development. Use the installer artifact supplied for the build you intend to test.

## Requirements

- Windows 64-bit;
- enough free disk space for FOA-SDK;
- a writable installation location.

No Git, Python, CMake, Visual Studio, engine checkout, separate editor installation, or source build is required for the normal prebuilt install.

## Install

1. Run `FOA-SDK-Installer.exe`.
2. Choose the folder where FOA-SDK should be installed.
3. Select **Install**.
4. Wait while the installer copies and registers FOA-SDK.
5. Wait for the automatic **Validating installation** stage. The installer checks the installed files and startup requirements before it reports success.
6. On the finish screen, keep **Open FOA-SDK Control Panel** selected for first-run setup. Choose whether to open the Editor immediately and **Create desktop shortcut**.
7. Select **Finish**.

Start Menu entries for **FOA-SDK Control Panel** and **FOA-SDK** are created automatically. The optional desktop shortcut points to the installed `FOA-SDK.exe` application.

The installer does not ask for engine paths, project files, tool profiles, game paths, package fingerprints, or internal configuration files. Game and workspace setup happens in the installed Control Panel after product installation succeeds.

## First-run Control Panel

1. Confirm or choose a workspace outside the FOA-SDK installation.
2. Choose the local Fall of Avalon folder you want inspected.
3. Select **Save setup**.
4. Review the compatibility indication and plan preview, then open FOA-SDK when ready.

The Control Panel checks only the folder you choose. It does not scan other drives or the network, install BepInEx, convert or deploy assets, change game files, launch the game, or inspect saves. Mono/IL2CPP results are local marker indications, not a claim that runtime mod loading has been verified.

Use **Export redacted report** when support evidence is needed. Exported reports hash local paths instead of exposing them.

## Opening FOA-SDK

Open FOA-SDK from any of these user-facing entry points:

- the Start Menu;
- the optional desktop shortcut;
- the installed `FOA-SDK.exe`.

Reopen first-run setup or diagnostics through **FOA-SDK Control Panel** in the Start Menu.

`FOA-SDK.exe` is the application entry point. The bundled editor/runtime implementation is internal to the installed product and should not be launched or configured separately.

On first launch, FOA-SDK prepares the writable per-user application state it needs automatically. Users should not need to correlate or edit internal JSON files to start the application.

## Installation validation

A successful file-copy stage is not enough for the installer to show **FOA-SDK is ready**.

After Windows installation completes, setup automatically runs the installed product's self-test. That check verifies the required self-contained product layout and the writable startup state needed to open FOA-SDK. If validation fails, setup reports the installation as incomplete instead of launching a broken editor.

## Updating, repairing, or uninstalling

For normal users, Windows **Settings → Apps → Installed apps** is the maintenance surface for uninstalling the product.

Running a newer FOA-SDK installer performs the supported update path for the installed application. Repair, uninstall, quiet setup, diagnostic logging, and development tool configuration remain available to maintainers and automated readiness tests, but are intentionally not part of the normal setup screens.

## Troubleshooting

- **Setup fails during installation:** close the installer, make sure the selected folder is writable, and run setup again.
- **Validation fails:** run the installer again or repair/reinstall FOA-SDK. Do not copy individual internal files from another build into the installation.
- **FOA-SDK does not open after a successful install:** launch it from the Start Menu or desktop shortcut. If it still fails, record the exact FOA-SDK build and the error shown by the application.
- **Desktop shortcut was not created:** FOA-SDK remains installed; open it from the Start Menu. The shortcut can be recreated by running setup again.
- **Updating an older development build fails:** uninstall the conflicting development build through Windows Installed apps, then install the replacement build.

The goal of the prebuilt package is that installing and opening FOA-SDK behaves like a normal self-contained Windows application. Internal package, engine, project, and validation machinery must remain implementation details rather than setup requirements.
