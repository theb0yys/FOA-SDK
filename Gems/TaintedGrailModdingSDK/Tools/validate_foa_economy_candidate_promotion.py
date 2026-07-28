#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the FoA economy candidate-promotion workflow boundary."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]


class EconomyPromotionValidationError(RuntimeError):
    """Raised when economy candidate-promotion source or tests drift."""


def read_required(relative: str) -> str:
    path = REPO_ROOT / relative
    if not path.is_file():
        raise EconomyPromotionValidationError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise EconomyPromotionValidationError(f"{label} is missing required fragment: {fragment}")


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise EconomyPromotionValidationError(f"{label} contains prohibited fragment: {fragment}")


def validate() -> None:
    tool = read_required("Gems/TaintedGrailModdingSDK/Tools/foa_economy_candidate_promotion.py")
    tests = read_required("Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_economy_candidate_promotion.py")
    docs = read_required("docs/tainted-grail-sdk/FOA_ECONOMY_CANDIDATE_PROMOTION.md")

    for fragment in (
        'DOCUMENT_KIND = "foa-economy-candidate-promotion"',
        'INPUT_CANDIDATE_KIND = "foa-catalog-promotion-candidates"',
        'ECONOMY_RECORD_KINDS = {"item", "recipe", "station", "crafting_station", "interaction_target"}',
        'require_false(candidates, "PromotionAllowed")',
        'require_false(candidates, "RuntimePermissionGranted")',
        '"CatalogMutationAllowed": False',
        '"RuntimePermissionGranted": False',
        '"AdapterExecutionAllowed": False',
        'RESERVED_FORBIDDEN_USAGE = "no_unvalidated_runtime_use"',
        '"economy.recipe-profile-incomplete"',
        '"economy.missing-evidence"',
        '"economy.duplicate-native-ref"',
        '"economy.non-economy-record"',
        'def verify_document(',
        'def generate_fixture(',
    ):
        require(tool, fragment, "Economy promotion tool")

    for fragment in (
        "test_stage_generates_item_recipe_and_station_review_drafts",
        "test_profile_mismatch_is_rejected",
        "test_missing_evidence_blocks_record",
        "test_duplicate_native_ref_is_reported",
        "test_non_economy_record_is_rejected",
        "test_synthetic_item_requires_owner_and_no_native_ref",
        "test_input_candidate_issues_are_preserved",
        "test_tampered_authority_is_rejected_by_verify",
        "test_cli_stage_and_verify_succeed",
        "test_fixture_generates_verified_output",
    ):
        require(tests, fragment, "Economy promotion tests")

    for fragment in (
        "does not mutate the live catalog",
        "item, recipe, and station candidates",
        "source/evidence/catalog-candidate",
        "no_unvalidated_runtime_use",
        "not a runtime deployment path",
        "Recipe candidates remain incomplete",
    ):
        require(docs, fragment, "Economy promotion documentation")

    for prohibited in (
        "subprocess.",
        "os.system",
        "shutil.copytree",
        "Harmony",
        "UnityEditor",
        "GameMutationAllowed\": True",
        "RuntimePermissionGranted\": True",
        "CatalogMutationAllowed\": True",
    ):
        reject(tool, prohibited, "Economy promotion tool")


def main() -> int:
    try:
        validate()
    except EconomyPromotionValidationError as exc:
        print(f"FoA economy candidate promotion validation failed: {exc}", file=sys.stderr)
        return 1
    print("FoA economy candidate promotion boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
