#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the prebuilt Windows SDK installer and artifact workflow contract."""

from __future__ import annotations

import sys
from pathlib import Path


class InstallerWorkflowValidationError(RuntimeError):
    """Raised when installer packaging can bypass the approved contract."""


REQUIRED_FILE_FRAGMENTS = {
    ".github/workflows/tainted-grail-sdk-installer.yml": (
        "Automatic triggers are intentionally suspended",
        "workflow_dispatch:",
        "mode:",
        "approved_inventory_sha256:",
        "redistribution_reviewer:",
        "redistribution_reviewed_at_utc:",
        "redistribution_evidence:",
        "-DO3DE_INSTALL_ENGINE_NAME=TaintedGrailFoASDK",
        "-DLY_DISABLE_TEST_MODULES=ON",
        "--target INSTALL",
        "license_scanner.py",
        "developer_preview_installer.py inventory",
        "developer_preview_installer.py stage",
        "developer_preview_installer.py verify-stage",
        "developer_preview_installer.py archive",
        "developer_preview_installer.py verify-archive",
        "dotnet tool install wix --version 4.0.4",
        "cpack --config",
        "msiexec.exe",
        "--self-test",
        "MSI Start Menu entry was not created",
        "MSI uninstall left the product launcher installed",
        "external-workspace",
        "actions/upload-artifact@v4",
        "unsigned development installer artifacts",
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
    "Gems/TaintedGrailModdingSDK/Installer/CMakeLists.txt": (
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
    "Gems/TaintedGrailModdingSDK/Installer/.config/dotnet-tools.json": (
        '"wix"',
        '"version": "4.0.4"',
    ),
    "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt": (
        "NAME TaintedGrailModdingEditorLauncher APPLICATION",
        "taintedgrailmoddingsdk_installed_launcher_files.cmake",
    ),
    "Gems/TaintedGrailModdingSDK/Installer/Source/Windows/InstalledEditorLauncher.cpp": (
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
    for forbidden in ("pull_request:", "push:", "self-hosted", "gh release create", "softprops/action-gh-release"):
        if forbidden in workflow:
            raise InstallerWorkflowValidationError(
                f"Installer workflow contains forbidden automatic/public action {forbidden!r}."
            )

    runner = read_text(repo_root / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py")
    if '"validate_installer_workflow.py"' not in runner:
        raise InstallerWorkflowValidationError(
            "Installer validator is not registered in the authoritative local gate."
        )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_installer_workflow(repo_root)
    except InstallerWorkflowValidationError as exc:
        print(f"Installer workflow validation failed: {exc}", file=sys.stderr)
        return 1
    print("Installer workflow validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
