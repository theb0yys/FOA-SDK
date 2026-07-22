#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(TOOLS_ROOT))

import validate_population_contract_hardening as validator


FIXTURE_PATHS = (
    "Gems/TaintedGrailModdingSDK/Code/Source/PopulationEvidenceValidation.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/PopulationAuthoringService.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabase.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabaseBase.inl",
    "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabaseIntegrity.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabaseIntegrityBase.inl",
    "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabasePopulationPart3.inl",
    "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationContractHardeningTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationAuthoringTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationCatalogTests.cpp",
    (
        "Gems/TaintedGrailModdingSDK/Code/"
        "taintedgrailmoddingsdk_population_hardening_tests_files.cmake"
    ),
    "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_catalog_tests_files.cmake",
    "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
)


class PopulationContractHardeningValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="population-contract-hardening-validator-"
        )
        self.root = Path(self.temporary.name)
        for relative in FIXTURE_PATHS:
            source = REPO_ROOT / relative
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def mutate(self, relative: str, old: str, new: str) -> None:
        path = self.root / relative
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def assert_validation_fails(self, fragment: str) -> None:
        with self.assertRaisesRegex(
            validator.PopulationContractHardeningError,
            fragment,
        ):
            validator.validate(self.root)

    def test_repository_fixture_passes(self) -> None:
        validator.validate(self.root)

    def test_any_subject_shortcut_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationEvidenceValidation.h",
            "inline bool ValidatePopulationEvidenceCoverage(",
            "inline bool ValidateEvidenceForAnyPopulationSubject(",
        )
        self.assert_validation_fails("ValidatePopulationEvidenceCoverage")

    def test_missing_uncovered_subject_rejection_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationEvidenceValidation.h",
            "if (!covered[index])",
            "if (covered[index])",
        )
        self.assert_validation_fails("reject every uncovered binding")

    def test_workspace_path_policy_call_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/PopulationAuthoringService.cpp",
            "pathPolicy.ValidateWorkspacePaths(workspace, workspaceRoot)",
            "AZ::Success()",
        )
        self.assert_validation_fails("ValidateWorkspacePaths")

    def test_method_renaming_macro_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabase.cpp",
            '#include "CatalogDatabaseBase.inl"',
            "#define ReplaceFromDocument ReplaceFromDocumentWithoutPopulation\n"
            '#include "CatalogDatabaseBase.inl"',
        )
        self.assert_validation_fails("#define ReplaceFromDocument")

    def test_explicit_catalog_helper_definition_is_required(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabaseBase.inl",
            "bool CatalogDatabase::ReplaceFromDocumentWithoutPopulation(",
            "bool CatalogDatabase::ReplaceFromDocument(",
        )
        self.assert_validation_fails("ReplaceFromDocumentWithoutPopulation")

    def test_bound_helper_must_call_explicit_integrity_helper(self) -> None:
        self.mutate(
            (
                "Gems/TaintedGrailModdingSDK/Code/Source/"
                "CatalogDatabaseIntegrityBase.inl"
            ),
            "candidate.ValidateIntegrityWithoutPopulation(",
            "candidate.ValidateIntegrity(",
        )
        self.assert_validation_fails("candidate.ValidateIntegrityWithoutPopulation")

    def test_unconditional_member_minimum_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabaseIntegrity.cpp",
            "if (member.m_required)\n                {\n                    requiredMinimum += member.m_minimumCount;\n                }",
            "requiredMinimum += member.m_minimumCount;",
        )
        self.assert_validation_fails(r"if \(member.m_required\)")

    def test_required_zero_minimum_guard_fails_closed(self) -> None:
        self.mutate(
            (
                "Gems/TaintedGrailModdingSDK/Code/Source/"
                "CatalogDatabasePopulationPart3.inl"
            ),
            "&& (!member.m_required || member.m_minimumCount > 0);",
            ";",
        )
        self.assert_validation_fails(r"!member\.m_required")

    def test_missing_focused_compiled_test_fails_closed(self) -> None:
        path = (
            self.root
            / "Gems/TaintedGrailModdingSDK/Code/Tests/"
            "PopulationContractHardeningTests.cpp"
        )
        path.unlink()
        self.assert_validation_fails("Required file is missing")

    def test_removed_authoring_suite_fails_closed(self) -> None:
        self.mutate(
            (
                "Gems/TaintedGrailModdingSDK/Code/"
                "taintedgrailmoddingsdk_catalog_tests_files.cmake"
            ),
            "    Tests/PopulationAuthoringTests.cpp\n",
            "",
        )
        self.assert_validation_fails("PopulationAuthoringTests")

    def test_unlinked_hardening_manifest_fails_closed(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
            "            taintedgrailmoddingsdk_population_hardening_tests_files.cmake\n",
            "",
        )
        self.assert_validation_fails("population_hardening")


if __name__ == "__main__":
    unittest.main()
