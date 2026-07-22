#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the external-engine Windows SDK installer and suite-root contract."""

from __future__ import annotations

import sys
from pathlib import Path


class InstallerWorkflowValidationError(RuntimeError):
    """Raised when installer packaging can bypass the approved contract."""


PINNED_O3DE_COMMIT = "68683f23fb747380d3efa2424bd5f30242e9c5a2"
LEGACY_INSTALLER_ROOT = "Gems/TaintedGrailModdingSDK/Installer"

REQUIRED_FILE_FRAGMENTS = {
    ".github/workflows/tainted-grail-sdk-installer.yml": (
        "Automatic triggers are intentionally suspended",
        "workflow_dispatch:",
        "mode:",
        "approved_inventory_sha256:",
        "redistribution_reviewer:",
        "redistribution_reviewed_at_utc:",
        "redistribution_evidence:",
        PINNED_O3DE_COMMIT,
        "O3DE_ROOT: ${{ runner.temp }}/o3de",
        "FOA_BUILD_ROOT: ${{ runner.temp }}/foa-build",
        "git -C $env:O3DE_ROOT checkout --detach $env:O3DE_COMMIT",
        "run_local_validation.py",
        "--engine-root",
        "cmake -S $env:O3DE_ROOT",
        '"$env:O3DE_ROOT/scripts/license_scanner/license_scanner.py"',
        "-DO3DE_INSTALL_ENGINE_NAME=TaintedGrailFoASDK",
        "-DLY_DISABLE_TEST_MODULES=ON",
        "--target INSTALL",
        "developer_preview_installer.py inventory",
        "developer_preview_installer.py stage",
        "developer_preview_installer.py verify-stage",
        "developer_preview_installer.py archive",
        "developer_preview_installer.py verify-archive",
        "dotnet tool install wix --version 4.0.4",
        "cmake -S Installer/Packaging/Windows",
        "cpack --config",
        "msiexec.exe",
        "--self-test",
        "MSI Start Menu entry was not created",
        "MSI uninstall left the product launcher installed",
        "external-workspace",
        "actions/upload-artifact@v4",
        "unsigned development installer artifacts",
    ),
    "Installer/README.md": (
        "SuiteWizard/",
        "Bootstrapper/",
        "Suites/",
        "Packages/",
        "Launcher/",
        "Packaging/",
        "Generated output belongs beneath the external `foa-build/` root",
        "A selection never grants game-launch",
    ),
    "Installer/suite.schema.json": (
        '"schema_version"',
        '"suite_id"',
        '"packages"',
        '"unreviewed_packages_allowed"',
        '"const": false',
    ),
    "Installer/package.schema.json": (
        '"package_id"',
        '"payload"',
        '"preserve_external_workspaces"',
        '"runtime_execution"',
        '"publication"',
        '"const": false',
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
    "docs/tainted-grail-sdk/WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md": (
        "Approved design",
        "O3DE `INSTALL` target",
        "portable ZIP",
        "exact inventory fingerprint",
        "does not publish",
        "upgrade",
        "rollback",
    ),
    "docs/tainted-grail-sdk/INSTALLING_PREBUILT_SDK.md": (
        "Windows 64-bit",
        "INSTALL_MANIFEST.json",
        "SHA256SUMS",
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


def validate_installer_workflow(repo_root: Path) -> None:
    for relative_path, fragments in REQUIRED_FILE_FRAGMENTS.items():
        text = read_text(repo_root / relative_path)
        for fragment in fragments:
            if fragment not in text:
                raise InstallerWorkflowValidationError(
                    f"{relative_path} is missing required installer contract fragment {fragment!r}."
                )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-installer.yml")
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
    )
    for fragment in forbidden:
        if fragment in workflow:
            raise InstallerWorkflowValidationError(
                f"Installer workflow contains forbidden automatic/product-root behavior {fragment!r}."
            )

    if (repo_root / LEGACY_INSTALLER_ROOT).exists():
        raise InstallerWorkflowValidationError(
            f"Legacy installer source root still exists: {LEGACY_INSTALLER_ROOT}."
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
    print("External-engine installer and suite-root validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
