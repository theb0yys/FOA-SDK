#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from validate_economy_duplicate_detection import (  # noqa: E402
    EconomyDuplicateContractError,
    validate_economy_duplicate_detection,
)


class EconomyDuplicateDetectionValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        source = repo / "Gems/TaintedGrailModdingSDK/Code/Source"
        tests = repo / "Gems/TaintedGrailModdingSDK/Code/Tests"
        tools = repo / "Gems/TaintedGrailModdingSDK/Tools"
        docs = repo / "docs/tainted-grail-sdk"
        workflow = repo / ".github/workflows"
        source.mkdir(parents=True)
        tests.mkdir(parents=True)
        tools.mkdir(parents=True)
        docs.mkdir(parents=True)
        workflow.mkdir(parents=True)

        (source.parent / "taintedgrailmoddingsdk_core_files.cmake").write_text(
            "Source/EconomyDuplicateDetectionService.cpp\nSource/EconomyDuplicateDetectionService.h\n",
            encoding="utf-8",
        )
        (source.parent / "taintedgrailmoddingsdk_editor_files.cmake").write_text(
            "Source/EconomyDuplicateReportWidget.cpp\nSource/EconomyDuplicateReportWidget.h\n",
            encoding="utf-8",
        )
        (source.parent / "taintedgrailmoddingsdk_catalog_tests_files.cmake").write_text(
            "Tests/EconomyDuplicateDetectionServiceTests.cpp\n",
            encoding="utf-8",
        )
        service = """
class EconomyDuplicateDetectionService BuildCrossPackDuplicateReport
\"subject_ref\" \"recipe_duplicate_key\" \"review_required\" \"partial\" \"blocked\"
m_ownerPackId m_subjectRef m_duplicateKey HasDistinctPackOwners packIds.size() >= 2
FindEconomyItem FindEconomyRecipe FindEvidence FindSource m_sourceFingerprint m_profileId m_gameVersion m_branch m_blockerIds
"""
        (source / "EconomyDuplicateDetectionService.h").write_text(service, encoding="utf-8")
        (source / "EconomyDuplicateDetectionService.cpp").write_text(service, encoding="utf-8")
        (source / "EconomyDuplicateReportWidget.h").write_text("widget", encoding="utf-8")
        (source / "EconomyDuplicateReportWidget.cpp").write_text(
            """
Read-only authoring-time candidates
Display names and fuzzy similarity are never identity signals
does not merge records QAbstractItemView::NoEditTriggers BuildCrossPackDuplicateReport
FoundationNotificationBus::Handler::BusConnect tr(\"Exact signal\") tr(\"Exact match key\")
tr(\"Owner packs\") tr(\"Status\")
""",
            encoding="utf-8",
        )
        (source / "TaintedGrailModdingSDKSystemComponent.cpp").write_text(
            '#include "EconomyDuplicateReportWidget.h"\nEconomyDuplicateReportViewPaneName\n'
            "RegisterViewPane<EconomyDuplicateReportWidget>\n"
            "UnregisterViewPane(EconomyDuplicateReportViewPaneName)\n"
            "TaintedGrailModdingSDK.EconomyDuplicateReport\n",
            encoding="utf-8",
        )
        (tests / "EconomyDuplicateDetectionServiceTests.cpp").write_text(
            """
ExactSubjectRefFindsCrossPackItemsWithoutDisplayNameMatching
ExactRecipeDuplicateKeyFindsDifferentSubjectsAcrossPacks
SamePackAndCaseDifferentKeysDoNotCreateCrossPackGroups
CandidateHealthEscalatesGroupFromPartialToBlocked
ReportIsDeterministicAndDoesNotMutateInputs
EXPECT_EQ(group->m_status, \"review_required\")
EXPECT_EQ(report.m_groups[0].m_status, \"partial\")
EXPECT_EQ(report.m_groups[0].m_status, \"blocked\")
recordCountBefore itemCountBefore recipeCountBefore sourceCountBefore evidenceCountBefore
""",
            encoding="utf-8",
        )
        (workflow / "tainted-grail-sdk-foundation.yml").write_text(
            "Test economy duplicate detection validator\n"
            "test_validate_economy_duplicate_detection.py\n"
            "Validate economy duplicate detection contract\n"
            "validate_economy_duplicate_detection.py\n",
            encoding="utf-8",
        )
        (docs / "ECONOMY_CROSS_PACK_DUPLICATES.md").write_text(
            "Read-only exact `subjectRef` exact recipe `duplicateKey` distinct owner packs "
            "Display names review_required partial blocked Windows duplicate-review companion No runtime adapter",
            encoding="utf-8",
        )
        (docs / "CORE_FRAMEWORK_BUILD_GRAPH.md").write_text(
            "immutable economy acquisition coverage and cross-pack duplicate analysis\n"
            "read-only economy acquisition and duplicate-report dashboards\n",
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "Economy Cross-Pack Duplicate Report ECONOMY_CROSS_PACK_DUPLICATES.md",
            encoding="utf-8",
        )
        (docs / "USER_GUIDE.md").write_text(
            "Tainted Grail Economy Cross-Pack Duplicates exact subject references "
            "exact recipe duplicate keys never uses display-name similarity",
            encoding="utf-8",
        )
        companion = repo / "Gems/TaintedGrailModdingSDK/Preview/DuplicateReview"
        companion.mkdir(parents=True)
        (companion / "preview.duplicate-companion.tgpack.json").write_text(
            '{"PackId": "preview.duplicate-companion", "RuntimeActionsEnabled": false, '
            '"TargetGameVersion": "preview-build-0", "TargetBranch": "preview"}',
            encoding="utf-8",
        )
        (companion / "preview-duplicate-source.json").write_text(
            '"preview.evidence.duplicate.primary" "preview.evidence.duplicate.companion" '
            '"subject:preview:item:duplicate-review"',
            encoding="utf-8",
        )
        (companion / "README.md").write_text(
            "preview.item.duplicate.primary preview.item.duplicate.companion "
            "preview.developer-preview-0 preview.duplicate-companion "
            "The report should show one exact `subject_ref` group "
            "Both candidates should be `partial`",
            encoding="utf-8",
        )
        (docs / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md").write_text(
            "All eight TG SDK panes Tainted Grail Economy Cross-Pack Duplicates "
            "cross-pack duplicate candidates preview.duplicate-companion "
            "preview.evidence.duplicate.primary preview.evidence.duplicate.companion non-editable",
            encoding="utf-8",
        )
        (repo / "ROADMAP.md").write_text(
            "Economy cross-pack duplicate report\n"
            "Status: implemented, continuing hardening and Windows UI verification.\n"
            "work-order generation after adapter contracts exist\n",
            encoding="utf-8",
        )
        (repo / "CHANGELOG.md").write_text(
            "EconomyDuplicateDetectionService Tainted Grail Economy Cross-Pack Duplicates "
            "display-name or fuzzy matching",
            encoding="utf-8",
        )
        return repo

    def test_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_economy_duplicate_detection(self.make_repo(Path(temporary)))

    def test_display_name_matching_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            source = repo / "Gems/TaintedGrailModdingSDK/Code/Source/EconomyDuplicateDetectionService.cpp"
            source.write_text(source.read_text() + "\nm_displayName\n", encoding="utf-8")
            with self.assertRaisesRegex(EconomyDuplicateContractError, "m_displayName"):
                validate_economy_duplicate_detection(repo)

    def test_editing_controls_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            widget = repo / "Gems/TaintedGrailModdingSDK/Code/Source/EconomyDuplicateReportWidget.cpp"
            widget.write_text(widget.read_text() + "\nQPushButton\n", encoding="utf-8")
            with self.assertRaisesRegex(EconomyDuplicateContractError, "QPushButton"):
                validate_economy_duplicate_detection(repo)

    def test_distinct_pack_gate_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            source_root = repo / "Gems/TaintedGrailModdingSDK/Code/Source"
            for name in ("EconomyDuplicateDetectionService.cpp", "EconomyDuplicateDetectionService.h"):
                source = source_root / name
                source.write_text(
                    source.read_text().replace("packIds.size() >= 2", "packIds.size() >= 1"),
                    encoding="utf-8",
                )
            with self.assertRaisesRegex(EconomyDuplicateContractError, "packIds.size"):
                validate_economy_duplicate_detection(repo)

    def test_runtime_enabled_companion_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            pack = repo / "Gems/TaintedGrailModdingSDK/Preview/DuplicateReview/preview.duplicate-companion.tgpack.json"
            pack.write_text(pack.read_text().replace("false", "true"), encoding="utf-8")
            with self.assertRaisesRegex(EconomyDuplicateContractError, "RuntimeActionsEnabled"):
                validate_economy_duplicate_detection(repo)


if __name__ == "__main__":
    unittest.main()
