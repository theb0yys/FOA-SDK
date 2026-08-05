#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations

import argparse
from pathlib import Path
import sys


class ValidationError(RuntimeError):
    pass


def require(text: str, token: str, path: Path) -> None:
    if token not in text:
        raise ValidationError(f"{path}: required token is missing: {token}")


def forbid(text: str, token: str, path: Path) -> None:
    if token in text:
        raise ValidationError(f"{path}: forbidden token is present: {token}")


def validate(root: Path) -> None:
    code = root / "Gems" / "TaintedGrailModdingSDK" / "Code"
    source = code / "Source"
    design = root / "docs" / "tainted-grail-sdk" / "ACTOR_EQUIPMENT_APPEARANCE_PREVIEW_DESIGN.md"
    required = [
        design,
        source / "ActorAppearancePreviewService.h",
        source / "ActorAppearancePreviewService.cpp",
        source / "ActorAppearanceBindingService.h",
        source / "ActorAppearanceBindingService.cpp",
        source / "ActorAppearancePreviewWidget.h",
        source / "ActorAppearancePreviewWidget.cpp",
        source / "ActorTroopEditorFeatureTabs.cpp",
        source / "FoundationServiceActorAppearance.cpp",
        code / "Tests" / "ActorAppearancePreviewServiceTests.cpp",
        code / "taintedgrailmoddingsdk_actor_appearance_preview_core_files.cmake",
        code / "taintedgrailmoddingsdk_actor_appearance_preview_framework_files.cmake",
        code / "taintedgrailmoddingsdk_actor_appearance_preview_tests_files.cmake",
    ]
    missing = [str(path.relative_to(root)) for path in required if not path.is_file()]
    if missing:
        raise ValidationError("missing Stage 8 files: " + ", ".join(missing))

    widget = (source / "ActorAppearancePreviewWidget.cpp").read_text(encoding="utf-8")
    feature_tabs = (source / "ActorTroopEditorFeatureTabs.cpp").read_text(encoding="utf-8")
    binding = (source / "ActorAppearanceBindingService.cpp").read_text(encoding="utf-8")
    foundation = (source / "FoundationServiceActorAppearance.cpp").read_text(encoding="utf-8")
    core = (source / "ActorAppearancePreviewService.cpp").read_text(encoding="utf-8")
    tests = (code / "Tests" / "ActorAppearancePreviewServiceTests.cpp").read_text(encoding="utf-8")
    cmake = (code / "CMakeLists.txt").read_text(encoding="utf-8")
    editor_manifest = (code / "taintedgrailmoddingsdk_editor_files.cmake").read_text(encoding="utf-8")

    for token in (
        "MaximumModelBytes = 16 * 1024 * 1024",
        "MaximumEntries = 10000",
        "foa-asset-browser-pane-model",
        "ImportProofEvidenceConsumed",
        "RawConversionFileConsumed",
        "RawO3dePreviewSourceConsumed",
        "OperationalAuthority",
        "RequiresExplicitBindingStep",
        "CatalogPromotionAllowed",
        "RuntimePermissionGranted",
        "ActorAppearanceBoundaryWarning",
        "ActorAppearanceLivePreview",
        "ActorAppearanceEquipmentTable",
        "ActorAppearanceBindPortrait",
        "ActorAppearanceBindModel",
        "BindActorAppearancePreview",
    ):
        require(widget, token, source / "ActorAppearancePreviewWidget.cpp")

    require(feature_tabs, "showEvent", source / "ActorTroopEditorFeatureTabs.cpp")
    require(feature_tabs, "AddFeatureTab", source / "ActorTroopEditorFeatureTabs.cpp")
    forbid(feature_tabs, "installEventFilter", source / "ActorTroopEditorFeatureTabs.cpp")
    forbid(widget, "installEventFilter", source / "ActorAppearancePreviewWidget.cpp")

    for token in (
        "actor_uses_portrait_preview",
        "actor_uses_model_preview",
        "FindEvidence",
        "FindSource",
        "m_sourceFingerprint",
        "no_unvalidated_runtime_use",
    ):
        require(binding, token, source / "ActorAppearanceBindingService.cpp")

    require(foundation, "PersistCatalogCandidate", source / "FoundationServiceActorAppearance.cpp")
    require(foundation, "m_catalog =", source / "FoundationServiceActorAppearance.cpp")
    if foundation.index("PersistCatalogCandidate") > foundation.index("m_catalog ="):
        raise ValidationError("Foundation appearance command must persist before publishing the catalog")

    for token in (
        "equips_head",
        "equips_main_hand",
        "equips_off_hand",
        "equips_two_hand",
        "m_relationshipsExamined",
    ):
        require(core, token, source / "ActorAppearancePreviewService.cpp")
    require(tests, "PerformanceGuardExaminesEachRelationshipOnce", code / "Tests" / "ActorAppearancePreviewServiceTests.cpp")

    for token in (
        "taintedgrailmoddingsdk_actor_appearance_preview_core_files.cmake",
        "taintedgrailmoddingsdk_actor_appearance_preview_framework_files.cmake",
        "taintedgrailmoddingsdk_actor_appearance_preview_tests_files.cmake",
    ):
        require(cmake, token, code / "CMakeLists.txt")
    for token in (
        "ActorAppearancePreviewWidget.cpp",
        "ActorTroopEditorFeatureTabs.cpp",
    ):
        require(editor_manifest, token, code / "taintedgrailmoddingsdk_editor_files.cmake")

    combined = "\n".join((widget, feature_tabs, binding, foundation, core))
    for forbidden in (
        "BepInEx",
        "Harmony",
        "ProcessLauncher",
        "PromoteEvidenceToCatalog",
        "PromoteCandidateEvidence",
        "Deploy",
        "SaveGame",
    ):
        forbid(combined, forbidden, Path("Stage8 source set"))

    population_models = (source / "PopulationModels.h").read_text(encoding="utf-8")
    forbid(population_models, "Equipment", source / "PopulationModels.h")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate Stage 8 actor appearance preview boundaries")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    try:
        validate(args.root.resolve())
    except (OSError, ValidationError) as exc:
        print(f"Stage 8 actor appearance preview validation failed: {exc}", file=sys.stderr)
        return 1
    print("Stage 8 actor appearance preview static boundary validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
