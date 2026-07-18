#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the read-only economy cross-pack duplicate report contract."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Iterable


class EconomyDuplicateContractError(RuntimeError):
    """Raised when the economy duplicate report contract is incomplete or unsafe."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise EconomyDuplicateContractError(f"Required economy duplicate file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise EconomyDuplicateContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise EconomyDuplicateContractError(
                f"{label} contains forbidden fragment {fragment!r}."
            )


def validate_economy_duplicate_detection(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = read_text(code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake")
    require_fragments(
        core_manifest,
        (
            "Source/EconomyDuplicateDetectionService.cpp",
            "Source/EconomyDuplicateDetectionService.h",
        ),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/EconomyDuplicateReportWidget.cpp",
            "Source/EconomyDuplicateReportWidget.h",
        ),
        "Editor manifest",
    )
    require_fragments(
        test_manifest,
        ("Tests/EconomyDuplicateDetectionServiceTests.cpp",),
        "Catalog test manifest",
    )

    service_header = read_text(source_root / "EconomyDuplicateDetectionService.h")
    service_source = read_text(source_root / "EconomyDuplicateDetectionService.cpp")
    service = service_header + "\n" + service_source
    require_fragments(
        service,
        (
            "class EconomyDuplicateDetectionService",
            "BuildCrossPackDuplicateReport",
            '"subject_ref"',
            '"recipe_duplicate_key"',
            '"review_required"',
            '"partial"',
            '"blocked"',
            "m_ownerPackId",
            "m_subjectRef",
            "m_duplicateKey",
            "HasDistinctPackOwners",
            "packIds.size() >= 2",
            "FindEconomyItem",
            "FindEconomyRecipe",
            "FindEvidence",
            "FindSource",
            "m_sourceFingerprint",
            "m_profileId",
            "m_gameVersion",
            "m_branch",
            "m_blockerIds",
        ),
        "Economy duplicate detection service",
    )
    reject_fragments(
        service,
        (
            "m_displayName",
            "tolower",
            "toLower",
            "LowerAscii",
            "levenshtein",
            "fuzzy",
            "regex",
            "#include <Q",
            "#include <Qt",
            "AzToolsFramework",
            "FoundationService.h",
            "Upsert",
            "Save",
            "Launch",
            "BepInEx",
            "Harmony",
        ),
        "Economy duplicate detection service",
    )

    widget = read_text(source_root / "EconomyDuplicateReportWidget.cpp")
    require_fragments(
        widget,
        (
            "Read-only authoring-time candidates",
            "Display names and fuzzy similarity are never identity signals",
            "does not merge records",
            "QAbstractItemView::NoEditTriggers",
            "BuildCrossPackDuplicateReport",
            "FoundationNotificationBus::Handler::BusConnect",
            'tr("Exact signal")',
            'tr("Exact match key")',
            'tr("Owner packs")',
            'tr("Status")',
        ),
        "Economy duplicate report widget",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "QLineEdit",
            "Upsert",
            "Save",
            "ApplyCatalogGovernanceDecision",
            "ApplyCatalogValidationDecision",
            "BuildAcquisitionRelationship",
        ),
        "Economy duplicate report widget",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "EconomyDuplicateReportWidget.h"',
            "EconomyDuplicateReportViewPaneName",
            "RegisterViewPane<EconomyDuplicateReportWidget>",
            "UnregisterViewPane(EconomyDuplicateReportViewPaneName)",
            "TaintedGrailModdingSDK.EconomyDuplicateReport",
        ),
        "Editor pane registration",
    )

    tests = read_text(code_root / "Tests/EconomyDuplicateDetectionServiceTests.cpp")
    require_fragments(
        tests,
        (
            "ExactSubjectRefFindsCrossPackItemsWithoutDisplayNameMatching",
            "ExactRecipeDuplicateKeyFindsDifferentSubjectsAcrossPacks",
            "SamePackAndCaseDifferentKeysDoNotCreateCrossPackGroups",
            "CandidateHealthEscalatesGroupFromPartialToBlocked",
            "ReportIsDeterministicAndDoesNotMutateInputs",
            'EXPECT_EQ(group->m_status, "review_required")',
            'EXPECT_EQ(report.m_groups[0].m_status, "partial")',
            'EXPECT_EQ(report.m_groups[0].m_status, "blocked")',
            "recordCountBefore",
            "itemCountBefore",
            "recipeCountBefore",
            "sourceCountBefore",
            "evidenceCountBefore",
        ),
        "Economy duplicate detection tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test economy duplicate detection validator",
            'test_validate_economy_duplicate_detection.py',
            "Validate economy duplicate detection contract",
            "validate_economy_duplicate_detection.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/ECONOMY_CROSS_PACK_DUPLICATES.md")
    require_fragments(
        design,
        (
            "Read-only",
            "exact `subjectRef`",
            "exact recipe `duplicateKey`",
            "distinct owner packs",
            "Display names",
            "review_required",
            "partial",
            "blocked",
            "Windows duplicate-review companion",
            "No runtime adapter",
        ),
        "Economy duplicate design",
    )

    build_graph = read_text(repo_root / "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md")
    require_fragments(
        build_graph,
        (
            "immutable economy acquisition coverage and cross-pack duplicate analysis",
            "read-only economy acquisition and duplicate-report dashboards",
        ),
        "Core/Framework build graph",
    )

    companion_root = gem_root / "Preview/DuplicateReview"
    companion_pack = read_text(companion_root / "preview.duplicate-companion.tgpack.json")
    companion_source = read_text(companion_root / "preview-duplicate-source.json")
    companion_readme = read_text(companion_root / "README.md")
    require_fragments(
        companion_pack,
        (
            '"PackId": "preview.duplicate-companion"',
            '"RuntimeActionsEnabled": false',
            '"TargetGameVersion": "preview-build-0"',
            '"TargetBranch": "preview"',
        ),
        "Duplicate review companion pack",
    )
    require_fragments(
        companion_source,
        (
            '"preview.evidence.duplicate.primary"',
            '"preview.evidence.duplicate.companion"',
            '"subject:preview:item:duplicate-review"',
        ),
        "Duplicate review source",
    )
    require_fragments(
        companion_readme,
        (
            "preview.item.duplicate.primary",
            "preview.item.duplicate.companion",
            "preview.developer-preview-0",
            "preview.duplicate-companion",
            "The report should show one exact `subject_ref` group",
            "Both candidates should be `partial`",
        ),
        "Duplicate review companion instructions",
    )

    docs_index = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_index,
        ("Economy Cross-Pack Duplicate Report", "ECONOMY_CROSS_PACK_DUPLICATES.md"),
        "Documentation index",
    )
    user_guide = read_text(repo_root / "docs/tainted-grail-sdk/USER_GUIDE.md")
    require_fragments(
        user_guide,
        (
            "Tainted Grail Economy Cross-Pack Duplicates",
            "exact subject references",
            "exact recipe duplicate keys",
            "never uses display-name similarity",
        ),
        "User guide",
    )
    manual_ui = read_text(repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")
    require_fragments(
        manual_ui,
        (
            "All eight TG SDK panes",
            "Tainted Grail Economy Cross-Pack Duplicates",
            "cross-pack duplicate candidates",
            "preview.duplicate-companion",
            "preview.evidence.duplicate.primary",
            "preview.evidence.duplicate.companion",
            "non-editable",
        ),
        "Windows manual UI smoke",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Economy cross-pack duplicate report",
            "Status: implemented, continuing hardening and Windows UI verification.",
            "work-order generation after adapter contracts exist",
        ),
        "Roadmap",
    )
    changelog = read_text(repo_root / "CHANGELOG.md")
    require_fragments(
        changelog,
        (
            "EconomyDuplicateDetectionService",
            "Tainted Grail Economy Cross-Pack Duplicates",
            "display-name or fuzzy matching",
        ),
        "Changelog",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_economy_duplicate_detection(repo_root)
    except (OSError, EconomyDuplicateContractError) as exc:
        print(f"Tainted Grail economy duplicate validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail economy duplicate detection passed: exact cross-pack signals, pure Core analysis, "
        "read-only Editor presentation, evidence and blocker handling, tests, workflow, companion data, and docs are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
