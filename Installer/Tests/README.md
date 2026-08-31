# Installer tests

`Installer/Tests/WindowsInstallerLauncher/` owns the source-contract tests for the native executable installer wizard. `Installer/Tests/WindowsControlPanel/` owns the installed setup-manager contract tests. The authoritative local runner discovers both suites through the bridge tests in `Gems/TaintedGrailModdingSDK/Tools/tests/`.

The focused tests require a Windows Forms single-file project, optional reviewed MSI embedding, canonical SHA-256 verification, captured external payload bytes, safe `msiexec.exe` argument construction, per-user `asInvoker` behavior, lifecycle choices, external-workspace messaging, and self-contained build entry points. They also reject the deleted Python suite/receipt launcher contract.

Generated MSI files, installer executables, portable ZIP archives, logs, staging trees, registry captures, screenshots, and workspace sentinels belong beneath the external build/evidence root.

Static source-contract coverage does not replace a real Windows lifecycle smoke. A package candidate still requires executable-wizard construction from the reviewed MSI, staged and installed self-tests for `bin\Windows\profile\Default\FOA-SDK.exe` and `FOA-SDK-ControlPanel.exe`, versioned profile/redacted-report checks, repair, uninstall, and proof that an external workspace sentinel survives.
