#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the FoA game-data intake bridge boundary."""

from __future__ import annotations

import importlib.util
import shutil
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_ROOT = REPO_ROOT / "Gems/TaintedGrailModdingSDK/Tools"
TOOL_PATH = TOOLS_ROOT / "foa_game_data_intake.py"


class FoAGameDataIntakeValidationError(RuntimeError):
    """Raised when the read-only game-data intake bridge is incomplete or unsafe."""


def read_required(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file():
        raise FoAGameDataIntakeValidationError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise FoAGameDataIntakeValidationError(f"{label} is missing required fragment: {fragment}")


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise FoAGameDataIntakeValidationError(f"{label} contains prohibited fragment: {fragment}")


def load_tool_module():
    spec = importlib.util.spec_from_file_location("foa_game_data_intake", TOOL_PATH)
    if spec is None or spec.loader is None:
        raise FoAGameDataIntakeValidationError("Unable to load foa_game_data_intake.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def validate(root: Path = REPO_ROOT) -> None:
    tool = read_required(root, "Gems/TaintedGrailModdingSDK/Tools/foa_game_data_intake.py")
    tests = read_required(root, "Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_game_data_intake.py")
    guide = read_required(root, "docs/tainted-grail-sdk/FOA_GAME_DATA_INTAKE.md")

    for fragment in (
        "provider.foa-local-capture",
        "foa.local-game-data-intake",
        "Sources/{source_id}/source.tgsource.json",
        "Sources/{source_id}/evidence.tgevidence.json",
        "Catalog/Candidates/{source_id}.tgcatalog-candidates.json",
        "NativeRefExact",
        "RuntimePermissionGranted",
        "CatalogPromotionAllowed",
        "GameMutationAllowed",
        "RuntimeInvocationAllowed",
        "SaveAccessAllowed",
        "no_unvalidated_runtime_use",
        "Duplicate observation IDs are refused",
        "duplicate-native-ref",
    ):
        require(tool, fragment, "FoA game-data intake tool")

    for fragment in (
        "test_capture_generates_source_evidence_and_catalog_candidates",
        "test_duplicate_native_refs_create_blocking_candidate_issue",
        "test_runtime_permission_escalation_is_rejected",
        "test_synthetic_records_require_pack_ownership_and_no_native_ref",
        "test_cli_capture_and_verify_succeed",
    ):
        require(tests, fragment, "FoA game-data intake tests")

    for fragment in (
        "Read-only FoA game-data intake",
        "native IDs enter as candidate bindings",
        "load Unity assemblies",
        "execute BepInEx or Harmony",
        "promote catalog records",
        "grant runtime permission",
    ):
        require(guide, fragment, "FoA game-data intake guide")

    for prohibited in (
        "subprocess.run",
        "os.system",
        "ctypes",
        "clr",
        "UnityEngine",
        "shutil.copytree",
    ):
        reject(tool, prohibited, "FoA game-data intake tool")

    module = load_tool_module()
    temporary = Path(tempfile.mkdtemp(prefix="foa-game-data-intake-validation-"))
    try:
        output = temporary / "fixture"
        module.generate_fixture(output)
        manifest = module.verify_output(output)
        authority = manifest["OperationalAuthority"]
        for key in (
            "GameMutationAllowed",
            "RuntimeInvocationAllowed",
            "SaveAccessAllowed",
            "CatalogPromotionAllowed",
            "RuntimePermissionGranted",
        ):
            if authority[key] is not False:
                raise FoAGameDataIntakeValidationError(f"Fixture authority must keep {key}=false.")
    finally:
        shutil.rmtree(temporary, ignore_errors=True)


def main() -> int:
    try:
        validate()
    except FoAGameDataIntakeValidationError as exc:
        print(f"FoA game-data intake validation failed: {exc}", file=sys.stderr)
        return 1
    print("FoA game-data intake bridge boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
