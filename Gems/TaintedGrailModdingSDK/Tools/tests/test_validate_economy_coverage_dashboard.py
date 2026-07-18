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

from validate_economy_coverage_dashboard import (  # noqa: E402
    EconomyCoverageContractError,
    validate_economy_coverage_dashboard,
)


class EconomyCoverageDashboardValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.repo_root = Path(self._temporary.name)
        self._write_valid_contract()

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def _write(self, relative_path: str, content: str) -> None:
        path = self.repo_root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _write_valid_contract(self) -> None:
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_core_files.cmake",
            "Source/EconomyCoverageService.cpp\nSource/EconomyCoverageService.h\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake",
            "Source/EconomyCoverageDashboardWidget.cpp\nSource/EconomyCoverageDashboardWidget.h\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "Tests/EconomyCoverageServiceTests.cpp\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Source/EconomyCoverageService.h",
            "class EconomyCoverageService { BuildAcquisitionCoverage; };\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Source/EconomyCoverageService.cpp",
            '"vendor" "loot" "reward" "learnability" "crafting" '
            '"covered" "partial" "blocked" "missing" '
            "FindEvidence FindSource m_sourceFingerprint m_profileId m_gameVersion m_branch m_blockerIds\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Source/EconomyCoverageDashboardWidget.cpp",
            'Read-only coverage Coverage does not grant permission or prove runtime behavior '
            "QAbstractItemView::NoEditTriggers BuildAcquisitionCoverage "
            "FoundationNotificationBus::Handler::BusConnect "
            'tr("Vendor") tr("Loot") tr("Reward") tr("Learnability") tr("Crafting")\n',
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedGrailModdingSDKSystemComponent.cpp",
            '#include "EconomyCoverageDashboardWidget.h"\n'
            "EconomyCoverageDashboardViewPaneName\n"
            "RegisterViewPane<EconomyCoverageDashboardWidget>\n"
            "UnregisterViewPane(EconomyCoverageDashboardViewPaneName)\n"
            "TaintedGrailModdingSDK.EconomyCoverageDashboard\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Tests/EconomyCoverageServiceTests.cpp",
            "ItemCoverageSeparatesVendorLootAndRewardDeterministically\n"
            "RecipeCoverageUsesLearnabilityCraftingAndRewardLanes\n"
            "CoveredRelationshipCannotHideBlockedRelationshipInSameLane\n"
            "CoverageFailsClosedForUnrelatedEvidenceAndOpenBlockersWithoutMutation\n"
            'EXPECT_EQ(vendor->m_status, "covered")\n'
            'EXPECT_EQ(loot->m_status, "partial")\n'
            'EXPECT_EQ(reward->m_status, "missing")\n'
            'EXPECT_EQ(vendor->m_status, "blocked")\n'
            "recordCountBefore relationshipCountBefore sourceCountBefore evidenceCountBefore\n",
        )
        self._write(
            ".github/workflows/tainted-grail-sdk-foundation.yml",
            "Test economy coverage dashboard validator\n"
            "test_validate_economy_coverage_dashboard.py\n"
            "Validate economy coverage dashboard contract\n"
            "validate_economy_coverage_dashboard.py\n",
        )
        self._write(
            "docs/tainted-grail-sdk/ECONOMY_ACQUISITION_COVERAGE.md",
            "Read-only vendor loot reward learnability crafting covered partial blocked missing No runtime adapter\n",
        )
        self._write(
            "ROADMAP.md",
            "Economy acquisition coverage dashboard\n"
            "authoring-time duplicate detection reports across packs\n",
        )

    def test_valid_contract_passes(self) -> None:
        validate_economy_coverage_dashboard(self.repo_root)

    def test_rejects_qt_in_core_service(self) -> None:
        path = self.repo_root / "Gems/TaintedGrailModdingSDK/Code/Source/EconomyCoverageService.cpp"
        path.write_text(path.read_text(encoding="utf-8") + "#include <QDateTime>\n", encoding="utf-8")
        with self.assertRaisesRegex(EconomyCoverageContractError, "forbidden fragment"):
            validate_economy_coverage_dashboard(self.repo_root)

    def test_rejects_mutating_dashboard_control(self) -> None:
        path = self.repo_root / "Gems/TaintedGrailModdingSDK/Code/Source/EconomyCoverageDashboardWidget.cpp"
        path.write_text(path.read_text(encoding="utf-8") + "QPushButton\n", encoding="utf-8")
        with self.assertRaisesRegex(EconomyCoverageContractError, "forbidden fragment"):
            validate_economy_coverage_dashboard(self.repo_root)

    def test_rejects_missing_non_mutation_test_contract(self) -> None:
        path = self.repo_root / "Gems/TaintedGrailModdingSDK/Code/Tests/EconomyCoverageServiceTests.cpp"
        path.write_text(
            path.read_text(encoding="utf-8").replace("relationshipCountBefore", "removed"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(EconomyCoverageContractError, "relationshipCountBefore"):
            validate_economy_coverage_dashboard(self.repo_root)


if __name__ == "__main__":
    unittest.main()
