#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the read-only economy acquisition coverage dashboard contract."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Iterable


class EconomyCoverageContractError(RuntimeError):
    """Raised when the economy coverage contract is incomplete or unsafe."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise EconomyCoverageContractError(f"Required economy coverage file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise EconomyCoverageContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise EconomyCoverageContractError(
                f"{label} contains forbidden fragment {fragment!r}."
            )


def validate_economy_coverage_dashboard(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read_text(code_root / "taintedgrailmoddingsdk_editor_files.cmake")
    test_manifest = read_text(code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake")
    require_fragments(
        core_manifest,
        ("Source/EconomyCoverageService.cpp", "Source/EconomyCoverageService.h"),
        "Core manifest",
    )
    require_fragments(
        editor_manifest,
        (
            "Source/EconomyCoverageDashboardWidget.cpp",
            "Source/EconomyCoverageDashboardWidget.h",
        ),
        "Editor manifest",
    )
    require_fragments(
        test_manifest,
        ("Tests/EconomyCoverageServiceTests.cpp",),
        "Catalog test manifest",
    )

    service_header = read_text(source_root / "EconomyCoverageService.h")
    service_source = read_text(source_root / "EconomyCoverageService.cpp")
    service = service_header + "\n" + service_source
    require_fragments(
        service,
        (
            "class EconomyCoverageService",
            "BuildAcquisitionCoverage",
            '"vendor"',
            '"loot"',
            '"reward"',
            '"learnability"',
            '"crafting"',
            '"covered"',
            '"partial"',
            '"blocked"',
            '"missing"',
            "FindEvidence",
            "FindSource",
            "m_sourceFingerprint",
            "m_profileId",
            "m_gameVersion",
            "m_branch",
            "m_blockerIds",
        ),
        "Economy coverage service",
    )
    reject_fragments(
        service,
        (
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
        "Economy coverage service",
    )

    widget = read_text(source_root / "EconomyCoverageDashboardWidget.cpp")
    require_fragments(
        widget,
        (
            "Read-only coverage",
            "Coverage does not grant permission or prove runtime behavior",
            "QAbstractItemView::NoEditTriggers",
            "BuildAcquisitionCoverage",
            "FoundationNotificationBus::Handler::BusConnect",
            'tr("Vendor")',
            'tr("Loot")',
            'tr("Reward")',
            'tr("Learnability")',
            'tr("Crafting")',
        ),
        "Economy coverage dashboard",
    )
    reject_fragments(
        widget,
        (
            "QPushButton",
            "Upsert",
            "Save",
            "ApplyCatalogGovernanceDecision",
            "ApplyCatalogValidationDecision",
            "BuildAcquisitionRelationship",
        ),
        "Economy coverage dashboard",
    )

    system_component = read_text(source_root / "TaintedGrailModdingSDKSystemComponent.cpp")
    require_fragments(
        system_component,
        (
            '#include "EconomyCoverageDashboardWidget.h"',
            "EconomyCoverageDashboardViewPaneName",
            "RegisterViewPane<EconomyCoverageDashboardWidget>",
            "UnregisterViewPane(EconomyCoverageDashboardViewPaneName)",
            "TaintedGrailModdingSDK.EconomyCoverageDashboard",
        ),
        "Editor pane registration",
    )

    tests = read_text(code_root / "Tests/EconomyCoverageServiceTests.cpp")
    require_fragments(
        tests,
        (
            "ItemCoverageSeparatesVendorLootAndRewardDeterministically",
            "RecipeCoverageUsesLearnabilityCraftingAndRewardLanes",
            "CoveredRelationshipCannotHideBlockedRelationshipInSameLane",
            "CoverageFailsClosedForUnrelatedEvidenceAndOpenBlockersWithoutMutation",
            'EXPECT_EQ(vendor->m_status, "covered")',
            'EXPECT_EQ(loot->m_status, "partial")',
            'EXPECT_EQ(reward->m_status, "missing")',
            'EXPECT_EQ(vendor->m_status, "blocked")',
            "recordCountBefore",
            "relationshipCountBefore",
            "sourceCountBefore",
            "evidenceCountBefore",
        ),
        "Economy coverage tests",
    )

    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test economy coverage dashboard validator",
            'test_validate_economy_coverage_dashboard.py',
            "Validate economy coverage dashboard contract",
            "validate_economy_coverage_dashboard.py",
        ),
        "Focused workflow",
    )

    design = read_text(repo_root / "docs/tainted-grail-sdk/ECONOMY_ACQUISITION_COVERAGE.md")
    require_fragments(
        design,
        (
            "Read-only",
            "vendor",
            "loot",
            "reward",
            "learnability",
            "crafting",
            "covered",
            "partial",
            "blocked",
            "missing",
            "No runtime adapter",
        ),
        "Economy coverage design",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    require_fragments(
        roadmap,
        (
            "Economy acquisition coverage dashboard",
            "authoring-time duplicate detection reports across packs",
        ),
        "Roadmap",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_economy_coverage_dashboard(repo_root)
    except (OSError, EconomyCoverageContractError) as exc:
        print(f"Tainted Grail economy coverage validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail economy coverage passed: Core analysis, read-only Editor presentation, "
        "evidence binding, blocker handling, tests, workflow, and documentation are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
