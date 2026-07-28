#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the bounded FoA managed identifier exporter boundary."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS = REPO_ROOT / "Gems/TaintedGrailModdingSDK/Tools"
DOCS = REPO_ROOT / "docs/tainted-grail-sdk"


class ManagedIdentifierExporterValidationError(RuntimeError):
    """Raised when the managed identifier exporter slice drifts from its boundary."""


def read_required(relative: str) -> str:
    path = REPO_ROOT / relative
    if not path.is_file():
        raise ManagedIdentifierExporterValidationError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise ManagedIdentifierExporterValidationError(f"{label} missing required fragment: {fragment}")


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise ManagedIdentifierExporterValidationError(f"{label} contains prohibited fragment: {fragment}")


def validate() -> None:
    exporter = read_required("Gems/TaintedGrailModdingSDK/Tools/foa_managed_identifier_exporter.py")
    tests = read_required("Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_managed_identifier_exporter.py")
    docs = read_required("docs/tainted-grail-sdk/FOA_MANAGED_IDENTIFIER_EXPORTER.md")

    for fragment in (
        'ALLOWED_MANAGED_ASSEMBLIES = (',
        '"Assembly-CSharp.dll"',
        'DEFAULT_OUTPUT_NAME = foa_identifier_export.DEFAULT_EXPORT_NAME',
        'DEFAULT_SEED_NAME = "foa-managed-identifier-seeds.json"',
        'foa_identifier_export.normalize(raw_export)',
        'foa_identifier_export.load_export(input_path, workspace_path)',
        'ManagedAssembliesPath must remain inside the configured install path.',
        'ExtractedDataPath must remain inside the workspace root.',
        'Managed identifier seed files must remain inside ExtractedDataPath.',
        'RecursiveScanAllowed": False',
        'AssemblyLoadAllowed": False',
        'RuntimeInvocationAllowed": False',
        'GameMutationAllowed": False',
        'SaveAccessAllowed": False',
        'CatalogPromotionAllowed": False',
        'RuntimePermissionGranted": False',
        'No managed identifier observations were produced.',
    ):
        require(exporter, fragment, "managed identifier exporter")

    for fragment in (
        "subprocess",
        "importlib",
        "ctypes",
        "clr",
        "dnlib",
        "Mono.Cecil",
        "pefile",
        "os.walk",
        "rglob(",
        "glob(",
    ):
        reject(exporter, fragment, "managed identifier exporter")

    for fragment in (
        "test_export_creates_contract_and_managed_type_observations",
        "test_export_reads_seed_template_and_recipe_keys",
        "test_missing_install_is_rejected",
        "test_managed_path_must_remain_inside_install",
        "test_seed_path_must_remain_inside_extracted_data",
        "test_seed_runtime_permission_escalation_is_rejected",
        "test_private_path_leak_from_seed_is_rejected",
        "test_empty_observation_set_is_rejected",
        "test_cli_export_and_verify_succeed",
        "test_fixture_command_generates_verifiable_export",
    ):
        require(tests, fragment, "managed identifier exporter tests")

    for fragment in (
        "# FoA Managed Identifier Exporter",
        "foa_managed_identifier_exporter.py",
        "Assembly-CSharp.dll",
        "foa-managed-identifier-seeds.json",
        "foa-identifiers.json",
        "does not load assemblies as code",
        "does not decompile",
        "does not run Unity, FoA, BepInEx, or Harmony",
        "economy/item/recipe candidate promotion workflow",
    ):
        require(docs, fragment, "managed identifier exporter documentation")


def main() -> int:
    try:
        validate()
    except ManagedIdentifierExporterValidationError as exc:
        print(f"FoA managed identifier exporter validation failed: {exc}", file=sys.stderr)
        return 1
    print("FoA managed identifier exporter boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
