# Windows Installer Build Project

This directory converts one already verified staging payload into a Windows
Installer package. It does not select files from a source checkout or raw build
directory. `developer_preview_installer.py` owns inventory, redistribution
approval binding, staging, manifest generation, portable ZIP creation, and
verification.

The MSI project uses CPack's WiX generator with a stable project-owned Upgrade
Code, a version-derived Product Code, x64 architecture, and per-user install
scope. Standard Windows Installer repair, upgrade, and uninstall behavior owns
only the installed product payload; TG SDK workspaces remain external data.

WiX `4.0.4` is pinned as a build-only .NET tool in `.config/dotnet-tools.json`.
It is not included in the user payload. Restore it from this directory with
`dotnet tool restore`, then install the matching UI extension into an isolated
build cache before invoking CPack.
