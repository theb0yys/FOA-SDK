# Installer tests

`Installer/Tests/WindowsInstallerLauncher/` owns the source-contract tests for the native executable installer wizard. The authoritative local runner discovers those tests through `Gems/TaintedGrailModdingSDK/Tools/tests/test_installer_windows_launcher.py`.

The focused tests require a Windows Forms single-file project, optional reviewed MSI embedding, canonical SHA-256 verification, captured external payload bytes, safe `msiexec.exe` argument construction, per-user `asInvoker` behavior, lifecycle choices, external-workspace messaging, and self-contained build entry points. They also reject the deleted Python suite/receipt launcher contract.

Generated MSI files, installer executables, portable ZIP archives, logs, staging trees, registry captures, screenshots, and workspace sentinels belong beneath the external build/evidence root.

Static source-contract coverage does not replace a real Windows lifecycle smoke. A package candidate still requires executable-wizard construction from the reviewed MSI, clean install, installed-launcher self-test, repair, uninstall, and proof that an external workspace sentinel survives.
