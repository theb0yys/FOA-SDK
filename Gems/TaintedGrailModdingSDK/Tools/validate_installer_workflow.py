#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the external-engine Windows SDK MSI and executable-wizard contract."""

from __future__ import annotations

import sys
from pathlib import Path


class InstallerWorkflowValidationError(RuntimeError):
    """Raised when installer packaging can bypass the approved contract."""


PINNED_O3DE_COMMIT = "68683f23fb747380d3efa2424bd5f30242e9c5a2"
WINDOWS_RUNNER = "windows-2022"
CANONICAL_INSTALLER_WORKFLOW = ".github/workflows/tainted-grail-sdk-installer.yml"
TEMPORARY_INVENTORY_WORKFLOW = ".github/workflows/foa-sdk-installer-inventory-pr.yml"
LEGACY_INSTALLER_ROOT = "Gems/TaintedGrailModdingSDK/Installer"
OBSOLETE_INSTALLER_ROOTS = (
    "Installer/Bootstrapper",
    "Installer/Packages",
    "Installer/Suites",
    "Installer/SuiteWizard",
)
ALTERNATE_WORKFLOW_SIGNATURES = (
    ("developer_preview_installer.py inventory",),
    ("developer_preview_installer.py stage",),
    ("cpack --config", "-G WIX"),
    ("dotnet publish", "FOA-SDK-Installer.exe"),
    ("--target INSTALL", "O3DE_INSTALL_ENGINE_NAME=TaintedGrailFoASDK"),
)

REQUIRED_FILE_FRAGMENTS = {
    CANONICAL_INSTALLER_WORKFLOW: (
        "Automatic triggers are intentionally suspended",
        "workflow_dispatch:",
        "mode:",
        "approved_inventory_sha256:",
        "redistribution_reviewer:",
        "redistribution_reviewed_at_utc:",
        "redistribution_evidence:",
        PINNED_O3DE_COMMIT,
        f"runs-on: {WINDOWS_RUNNER}",
        '"O3DE_ROOT=$env:RUNNER_TEMP/o3de" >> $env:GITHUB_ENV',
        '"FOA_BUILD_ROOT=$env:RUNNER_TEMP/foa-build" >> $env:GITHUB_ENV',
        '"SDK_VALIDATION_BUILD=$env:RUNNER_TEMP/foa-build/tg-sdk-installer-validation"'
        " >> $env:GITHUB_ENV",
        '"SDK_PACKAGE_OUTPUT=$env:RUNNER_TEMP/tg-sdk-package-output" >> $env:GITHUB_ENV',
        '"WIX_EXTENSIONS=$env:RUNNER_TEMP/wix-extensions" >> $env:GITHUB_ENV',
        "git -C $env:O3DE_ROOT checkout --detach $env:O3DE_COMMIT",
        "run_local_validation.py",
        "--engine-root",
        "--ctest-build-dir",
        '-DLY_DISABLE_TEST_MODULES=OFF `\n            -DO3DE_FETCHCONTENT_FORCE_GIT=ON',
        "--target TaintedGrailModdingSDK.Catalog.Tests",
        "cmake -S $env:O3DE_ROOT",
        '"$env:O3DE_ROOT/scripts/license_scanner/license_scanner.py"',
        "-DO3DE_INSTALL_ENGINE_NAME=TaintedGrailFoASDK",
        '-DLY_DISABLE_TEST_MODULES=ON `\n            -DO3DE_FETCHCONTENT_FORCE_GIT=ON `',
        "--target INSTALL",
        "developer_preview_installer.py inventory",
        "developer_preview_installer.py stage",
        "developer_preview_installer.py verify-stage",
        "developer_preview_installer.py archive",
        "developer_preview_installer.py verify-archive",
        "dotnet tool install wix --version 4.0.4",
        "extension add --global WixToolset.UI.wixext/4.0.4",
        "cmake -S Installer/Packaging/Windows",
        "cpack --config",
        "-G WIX -B $env:SDK_PACKAGE_OUTPUT",
        "Get-ChildItem -LiteralPath $env:SDK_PACKAGE_OUTPUT -Filter *.msi",
        "Copy-Item -LiteralPath $msi.FullName -Destination (Join-Path $env:SDK_ARTIFACTS $msi.Name)",
        "Installer/Launcher/Windows/FOAInstallerLauncher.csproj",
        "InstallerMsiPath",
        "InstallerMsiChecksumPath",
        "BaseIntermediateOutputPath",
        "BaseOutputPath",
        "FOA-SDK-Installer.exe",
        "FOA-SDK-Installer.exe.sha256",
        "Get-FileHash",
        "verify_installer_artifacts.py",
        "--self-test",
        "--smoke-test",
        '"--operation", "install"',
        '"--operation", "repair"',
        '"--operation", "uninstall"',
        "Start-Process -FilePath $wizard -Wait -PassThru",
        "Installed MSI manifest differs from the exact reviewed staging manifest",
        "CreateShortcut($startMenuEntry)",
        "MSI Start Menu entry targets",
        "MSI repair did not restore the reviewed product-owned launcher bytes",
        "MSI uninstall left the product manifest installed",
        "external-workspace",
        "actions/upload-artifact@v4",
        "unsigned development installer artifacts",
    ),
    "Installer/README.md": (
        "Launcher/",
        "Packaging/",
        "Generated MSI files",
        "FOA-SDK-Installer.exe",
        "private temporary directory",
        "install/upgrade, repair, or uninstall",
        "no Git, Python, CMake",
        "were non-installing review prototypes",
    ),
    "Gems/TaintedGrailModdingSDK/Tools/developer_preview_installer.py": (
        'ENGINE_NAME = "TaintedGrailFoASDK"',
        'MANIFEST_NAME = "INSTALL_MANIFEST.json"',
        'SBOM_NAME = "SBOM.spdx.json"',
        "packageVerificationCode",
        "WINDOWS_RESERVED_NAMES",
        "is_reparse_point",
        "validate_destination",
        "derive_clean_repository_commit",
        "parse_json_bytes",
        '"rev-parse", "HEAD"',
        '"status", "--porcelain=v1"',
        "inventory_sha256",
        "Redistribution approval does not bind to the exact current inventory fingerprint",
        "Installer input changed after inventory review",
        "verify_stage",
        "archive_payload",
        "verify_archive",
        'subparsers.add_parser("inventory"',
        'subparsers.add_parser("stage"',
    ),
    "Gems/TaintedGrailModdingSDK/Tools/verify_installer_artifacts.py": (
        "expected_names",
        "Retained installer artifact set mismatch",
        "verify_checksum_pair",
        "installer.verify_archive",
        'root / "FOA-SDK-Installer.exe"',
    ),
    "Installer/Packaging/Windows/CMakeLists.txt": (
        "install(DIRECTORY",
        "INSTALL_MANIFEST.json",
        "tg_manifest_version",
        'set(CPACK_GENERATOR "WIX")',
        "CPACK_PACKAGE_EXECUTABLES",
        "TaintedGrailModdingEditorLauncher",
        "set(CPACK_WIX_VERSION 4)",
        "set(CPACK_WIX_ARCHITECTURE x64)",
        "set(CPACK_WIX_INSTALL_SCOPE perUser)",
        "EF05F481-EEED-4CE5-A623-0915EC28F92D",
        "string(\n    UUID CPACK_WIX_PRODUCT_GUID",
        "include(CPack)",
    ),
    "Installer/Packaging/Windows/.config/dotnet-tools.json": (
        '"wix"',
        '"version": "4.0.4"',
    ),
    "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt": (
        "NAME TaintedGrailModdingEditorLauncher APPLICATION",
        "taintedgrailmoddingsdk_installed_launcher_files.cmake",
    ),
    "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_installed_launcher_files.cmake": (
        "../../../Installer/Launcher/Windows/InstalledEditorLauncher.cpp",
        "../../../Installer/Launcher/Windows/InstalledEditorLauncher.rc",
    ),
    "Installer/Launcher/Windows/InstalledEditorLauncher.cpp": (
        "INSTALL_MANIFEST.json",
        "TaintedGrailModdingEditor",
        "--project-path",
        "--self-test",
        "CreateProcessW",
    ),
    "Installer/Launcher/Windows/FOAInstallerLauncher.csproj": (
        "net8.0-windows",
        "UseWindowsForms",
        "PublishSingleFile",
        "FOA.SDK.Payload.msi",
        "InstallerMsiChecksumPath",
    ),
    "Installer/Launcher/Windows/app.manifest": (
        'requestedExecutionLevel level="asInvoker"',
    ),
    "Installer/Launcher/Windows/Program.cs": (
        "InstallerPayload.Resolve",
        "InstallerWizardForm",
        "WindowsInstallerRunner.RunAsync",
    ),
    "Installer/Launcher/Windows/InstallerPayload.cs": (
        "FOA.SDK.Payload.msi",
        "FOA.SDK.Payload.msi.sha256",
        "SHA256.HashData",
        "FileAttributes.ReparsePoint",
        "FileShare.Read",
        'text.EndsWith("\\n", StringComparison.Ordinal)',
        "text.Contains('\\r')",
        'Split("  ", StringSplitOptions.None)',
        "parts.Length != 2",
        'Path.GetExtension(parts[1]), ".msi"',
        "return new InstallerPayload(captured, expected, temporaryRoot)",
    ),
    "Installer/Launcher/Windows/WindowsInstallerRunner.cs": (
        'FileName = "msiexec.exe"',
        "ArgumentList.Add",
        'InstallerOperation.InstallOrUpgrade => "/i"',
        'InstallerOperation.Repair => "/fa"',
        'InstallerOperation.Uninstall => "/x"',
        "INSTALL_ROOT=",
    ),
    "Installer/Launcher/Windows/InstallerWizardForm.cs": (
        "Install or upgrade the complete FOA-SDK",
        "Repair the installed FOA-SDK",
        "Uninstall FOA-SDK",
        "External workspaces are never removed",
        "Reviewed MSI SHA-256",
    ),
    "docs/tainted-grail-sdk/WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md": (
        "Approved design",
        "O3DE `INSTALL` target",
        "portable ZIP",
        "self-contained `FOA-SDK-Installer.exe`",
        "exact inventory fingerprint",
        "does not publish",
        "upgrade",
        "rollback",
    ),
    "docs/tainted-grail-sdk/INSTALLING_PREBUILT_SDK.md": (
        "Windows 64-bit",
        "INSTALL_MANIFEST.json",
        "SHA256SUMS",
        "FOA-SDK-Installer.exe.sha256",
        "repair",
        "uninstall",
        "unsigned",
        "source build",
    ),
}


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise InstallerWorkflowValidationError(f"Required installer file is missing: {path}") from exc


def validate_no_alternate_installer_workflows(repo_root: Path) -> None:
    workflows_root = repo_root / ".github/workflows"
    try:
        workflows = sorted(
            path
            for path in workflows_root.iterdir()
            if path.is_file() and path.suffix.casefold() in {".yml", ".yaml"}
        )
    except OSError as exc:
        raise InstallerWorkflowValidationError(
            f"Unable to enumerate repository workflows: {workflows_root}"
        ) from exc

    canonical = (repo_root / CANONICAL_INSTALLER_WORKFLOW).resolve(strict=False)
    for workflow_path in workflows:
        if workflow_path.resolve(strict=False) == canonical:
            continue
        text = read_text(workflow_path)
        for signature in ALTERNATE_WORKFLOW_SIGNATURES:
            if all(fragment in text for fragment in signature):
                relative = workflow_path.relative_to(repo_root).as_posix()
                raise InstallerWorkflowValidationError(
                    "Alternate workflow can execute the reviewed installer pipeline outside the canonical "
                    f"inventory/review/package route: {relative}; signature={signature!r}."
                )


def validate_installer_workflow(repo_root: Path) -> None:
    for relative_path, fragments in REQUIRED_FILE_FRAGMENTS.items():
        text = read_text(repo_root / relative_path)
        for fragment in fragments:
            if fragment not in text:
                raise InstallerWorkflowValidationError(
                    f"{relative_path} is missing required installer contract fragment {fragment!r}."
                )

    workflow = read_text(repo_root / CANONICAL_INSTALLER_WORKFLOW)
    forbidden = (
        "pull_request:",
        "push:",
        "self-hosted",
        "gh release create",
        "softprops/action-gh-release",
        "cmake -S .",
        "python scripts/license_scanner",
        "${{ github.workspace }}/build",
        LEGACY_INSTALLER_ROOT,
        "suite_wizard_receipt_host.py",
        "FOA_SDK_PYTHON",
        "review evidence only",
        "${{ runner.temp }}",
        "runs-on: windows-latest",
        "-G WIX -B $env:SDK_ARTIFACTS",
    )
    for fragment in forbidden:
        if fragment in workflow:
            raise InstallerWorkflowValidationError(
                f"Installer workflow contains forbidden automatic/product-root behavior {fragment!r}."
            )

    validate_no_alternate_installer_workflows(repo_root)

    if (repo_root / TEMPORARY_INVENTORY_WORKFLOW).exists():
        raise InstallerWorkflowValidationError(
            "Temporary installer inventory workflow bypasses the canonical reviewed inventory/package pipeline: "
            f"{TEMPORARY_INVENTORY_WORKFLOW}."
        )

    if (repo_root / LEGACY_INSTALLER_ROOT).exists():
        raise InstallerWorkflowValidationError(
            f"Legacy installer source root still exists: {LEGACY_INSTALLER_ROOT}."
        )

    for obsolete_root in OBSOLETE_INSTALLER_ROOTS:
        if (repo_root / obsolete_root).exists():
            raise InstallerWorkflowValidationError(
                f"Obsolete non-installing installer framework still exists: {obsolete_root}."
            )

    runner = read_text(repo_root / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py")
    for fragment in (
        '"validate_installer_workflow.py"',
        "resolve_engine_root",
        "source_policy_engine_root",
        "Gems/ExternalToolchain",
        "Gems/TaintedGrailModdingSDK",
    ):
        if fragment not in runner:
            raise InstallerWorkflowValidationError(
                f"Authoritative local gate is missing installer/external-engine fragment {fragment!r}."
            )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_installer_workflow(repo_root)
    except InstallerWorkflowValidationError as exc:
        print(f"Installer workflow validation failed: {exc}", file=sys.stderr)
        return 1
    print("External-engine MSI and executable installer-wizard validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
