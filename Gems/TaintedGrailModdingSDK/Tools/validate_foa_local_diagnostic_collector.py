#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the bounded FoA local diagnostic collector boundary."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]


class LocalDiagnosticCollectorValidationError(RuntimeError):
    """Raised when the local collector boundary is incomplete or unsafe."""


def read_required(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file():
        raise LocalDiagnosticCollectorValidationError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise LocalDiagnosticCollectorValidationError(f"{label} is missing required fragment: {fragment}")


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise LocalDiagnosticCollectorValidationError(f"{label} contains prohibited fragment: {fragment}")


def validate(root: Path = REPO_ROOT) -> None:
    collector = read_required(root, "Gems/TaintedGrailModdingSDK/Tools/foa_local_diagnostic_collector.py")
    tests = read_required(root, "Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_local_diagnostic_collector.py")
    docs = read_required(root, "docs/tainted-grail-sdk/FOA_LOCAL_DIAGNOSTIC_COLLECTOR.md")

    for fragment in (
        "does not recursively scan game directories",
        "does not recursively scan game directories, load Unity assemblies",
        "SOURCE_KIND = \"foa-local-diagnostic-capture\"",
        "DEFAULT_IDENTIFIER_EXPORT_NAME = \"foa-identifiers.json\"",
        "MAX_JSON_BYTES = 16 * 1024 * 1024",
        "MAX_FILE_HASH_BYTES = 256 * 1024 * 1024",
        "ALLOWED_MANAGED_FILE_NAMES",
        "ALLOWED_INSTALL_MARKERS",
        "validate_profile_paths",
        "ManagedAssembliesPath must remain inside the configured install path",
        "PluginPath must remain inside the configured install path",
        "ExtractedDataPath must remain inside the workspace root",
        "resolve_identifier_exports",
        "Identifier export paths must remain inside ExtractedDataPath",
        "assert_no_private_paths(capture)",
        "RecursiveScanAllowed\": False",
        "AssemblyLoadAllowed\": False",
        "RuntimeInvocationAllowed\": False",
        "GameMutationAllowed\": False",
        "SaveAccessAllowed\": False",
        "PromoteAutomatically\": False",
        "GrantsRuntimePermission\": False",
        "collect",
        "verify",
        "fixture",
    ):
        require(collector, fragment, "Collector boundary")

    for fragment in (
        "test_collect_generates_sanitized_capture_with_identifier_observations",
        "test_capture_verify_accepts_generated_output",
        "test_missing_install_path_is_rejected",
        "test_managed_path_outside_install_is_rejected",
        "test_identifier_export_outside_extracted_data_is_rejected",
        "test_identifier_export_runtime_permission_escalation_is_rejected",
        "test_export_locator_cannot_leak_absolute_paths",
        "test_cli_collect_and_verify_succeed",
        "test_fixture_command_generates_verifiable_capture",
    ):
        require(tests, fragment, "Collector test coverage")

    for fragment in (
        "bounded FoA local diagnostic collector",
        "foa-identifiers.json",
        "does not recursively scan",
        "does not load Unity assemblies",
        "does not execute BepInEx or Harmony",
        "does not mutate game files or saves",
        "does not promote catalog records",
        "does not grant runtime permission",
        "foa_game_data_intake.py capture",
    ):
        require(docs, fragment, "Collector documentation")

    for fragment in (
        "subprocess.run",
        "os.walk(",
        "rglob(",
        "import UnityEngine",
        "BepInEx" + ".Bootstrap",
        "import HarmonyLib",
    ):
        reject(collector, fragment, "Collector boundary")


def main() -> int:
    try:
        validate()
    except LocalDiagnosticCollectorValidationError as exc:
        print(f"FoA local diagnostic collector validation failed: {exc}", file=sys.stderr)
        return 1
    print("FoA local diagnostic collector boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
