# FOA-SDK Windows Installer

`FOA-SDK-Installer.exe` is the unsigned development installer entry point.

The Windows x64 single-file application embeds the generated per-user MSI and its SHA-256 sidecar, extracts them into a private temporary directory, verifies the captured MSI bytes, and invokes Windows Installer for install/upgrade, repair, or uninstall.

The development build does not require repository-wide validation, compiled test targets, manual inventory approval, or lifecycle smoke tests before producing the EXE. It does not publish a release, sign binaries, deploy mods, launch Tainted Grail, or access saves.
