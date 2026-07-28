#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the FoA identifier-export contract boundary."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOL_PATH = "Gems/TaintedGrailModdingSDK/Tools/foa_identifier_export.py"
TEST_PATH = "Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_identifier_export.py"
DOC_PATH = "docs/tainted-grail-sdk/FOA_IDENTIFIER_EXPORT.md"


class IdentifierExportValidationError(RuntimeError):
    """Raised when the identifier-export contract boundary is incomplete."""


def read_required(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file():
        raise IdentifierExportValidationError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise IdentifierExportValidationError(f"{label} is missing required fragment: {fragment}")


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise IdentifierExportValidationError(f"{label} contains prohibited fragment: {fragment}")


def validate(root: Path = REPO_ROOT) -> None:
    tool = read_required(root, TOOL_PATH)
    tests = read_required(root, TEST_PATH)
    doc = read_required(root, DOC_PATH)

    for fragment in (
        'DOCUMENT_KIND = "foa-identifier-export"',
        'DEFAULT_EXPORT_NAME = "foa-identifiers.json"',
        'ALLOWED_CLAIM_IDS',
        'native_ref_exact',
        'unity_guid',
        'addressable_key',
        'managed_type_name',
        'ExtractedDataPath must remain inside the workspace root',
        'Identifier export file must remain inside ExtractedDataPath',
        'PromoteAutomatically',
        'GrantsRuntimePermission',
        'Duplicate NativeRefExact values are refused',
        'RecursiveScanAllowed',
        'AssemblyLoadAllowed',
        'RuntimeInvocationAllowed',
        'GameMutationAllowed',
        'SaveAccessAllowed',
        'CatalogPromotionAllowed',
        'RuntimePermissionGranted',
    ):
        require(tool, fragment, "Identifier export tool")

    for fragment in (
        "test_verify_accepts_profile_bound_export",
        "test_profile_mismatch_is_rejected",
        "test_runtime_permission_escalation_is_rejected",
        "test_millisecond_capture_time_is_rejected",
        "test_duplicate_native_refs_are_rejected",
        "test_export_must_remain_inside_extracted_data_path",
        "test_cli_normalize_and_verify_succeed",
        "test_fixture_command_generates_verifiable_export",
    ):
        require(tests, fragment, "Identifier export tests")

    for fragment in (
        "# FoA Identifier Export Contract",
        "foa-identifiers.json",
        "ExtractedDataPath",
        "native_ref_exact",
        "addressable_key",
        "managed_type_name",
        "does not scan",
        "load Unity assemblies",
        "grant runtime permission",
        "foa_local_diagnostic_collector.py",
        "foa_game_data_intake.py",
    ):
        require(doc, fragment, "Identifier export documentation")

    for prohibited in (
        "subprocess.",
        "requests.",
        "urllib.",
        "socket.",
        "clr.",
        "LoadLibrary",
        "import Harmony",
        "import BepInEx",
        "import UnityEngine",
        "Assembly.Load",
    ):
        reject(tool, prohibited, "Identifier export tool")


def main() -> int:
    try:
        validate()
    except IdentifierExportValidationError as exc:
        print(f"FoA identifier export validation failed: {exc}", file=sys.stderr)
        return 1
    print("FoA identifier export contract boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
