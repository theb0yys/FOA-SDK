#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate catalog, governance, hardening, and economy test registration and safety coverage."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_contains(text: str, fragment: str, path: Path) -> None:
    if fragment not in text:
        fail(f"Missing required fragment {fragment!r} in {path}")


def main() -> int:
    gem_root = Path(__file__).resolve().parents[1]
    code_root = gem_root / "Code"
    cmake_path = code_root / "CMakeLists.txt"
    manifest_path = code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    database_tests_path = code_root / "Tests" / "CatalogDatabaseTests.cpp"
    governance_tests_path = code_root / "Tests" / "CatalogGovernanceServiceTests.cpp"
    hardening_tests_path = code_root / "Tests" / "CatalogGovernanceHardeningTests.cpp"
    types_tests_path = code_root / "Tests" / "CatalogGovernanceTypesTests.cpp"
    economy_tests_path = code_root / "Tests" / "EconomyAuthoringTests.cpp"

    try:
        for path in (
            cmake_path,
            manifest_path,
            database_tests_path,
            governance_tests_path,
            hardening_tests_path,
            types_tests_path,
            economy_tests_path,
        ):
            if not path.is_file():
                fail(f"Required catalog/governance/economy test file is missing: {path}")

        cmake = cmake_path.read_text(encoding="utf-8")
        for fragment in (
            "PAL_TRAIT_BUILD_TESTS_SUPPORTED",
            "PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED",
            "NAME ${gem_name}.Catalog.Tests",
            "taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "AZ::AzTest",
            "AZ::AzToolsFramework",
            "ly_add_googletest",
        ):
            require_contains(cmake, fragment, cmake_path)

        manifest = manifest_path.read_text(encoding="utf-8")
        entries = set(re.findall(r"^\s+([^\s\)]+)\s*$", manifest, re.MULTILINE))
        expected = {
            "Source/CatalogDatabase.cpp",
            "Source/CatalogDatabase.h",
            "Source/CatalogGovernanceService.cpp",
            "Source/CatalogGovernanceService.h",
            "Source/CatalogGovernanceTypes.cpp",
            "Source/CatalogGovernanceTypes.h",
            "Source/CatalogTransactionService.cpp",
            "Source/CatalogTransactionService.h",
            "Source/EconomyAuthoringService.cpp",
            "Source/EconomyAuthoringService.h",
            "Source/EconomyModels.cpp",
            "Source/EconomyModels.h",
            "Source/FoundationModels.cpp",
            "Source/FoundationModels.h",
            "Source/SourceEvidenceRegistry.cpp",
            "Source/SourceEvidenceRegistry.h",
            "Tests/CatalogDatabaseTests.cpp",
            "Tests/CatalogGovernanceHardeningTests.cpp",
            "Tests/CatalogGovernanceServiceTests.cpp",
            "Tests/CatalogGovernanceTypesTests.cpp",
            "Tests/EconomyAuthoringTests.cpp",
        }
        if entries != expected:
            fail(f"Catalog test manifest mismatch: expected {sorted(expected)}, found {sorted(entries)}")

        database_tests = database_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "SameDisplayNameDoesNotMergeDistinctRecordIds",
            "DuplicateExactNativeReferenceIsRejected",
            "SyntheticRecordRequiresOwningPack",
            "QueryFiltersEvidencePermissionAndBlockedState",
            "RelationshipRequiresKnownRecordsAndEvidence",
        ):
            require_contains(database_tests, fragment, database_tests_path)

        governance_tests = governance_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "ValidationDoesNotGrantUsagePermission",
            "PermissionRequiresCurrentStateAndValidatedProof",
            "StaleDecisionRevokesAllowedUsage",
            "SupersessionRevokesUsageAndRecordsReplacement",
            "RelationshipValidationAndPermissionUseSameProofGates",
            "CatalogGovernanceApplyResult",
            "CatalogValidationApplyResult",
            'm_axis = "permission"',
            'm_axis = "staleness"',
            'm_axis = "supersession"',
            'm_subjectKind = "relationship"',
        ):
            require_contains(governance_tests, fragment, governance_tests_path)

        hardening_tests = hardening_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "UnknownEvidenceLeavesOriginalCatalogUnchanged",
            "WrongEvidenceSubjectLeavesOriginalCatalogUnchanged",
            "WrongEvidenceProfileLeavesOriginalCatalogUnchanged",
            "DuplicateGovernanceIdRollsBackStateChange",
            "DuplicateValidationIdRollsBackStateChange",
            "PersistenceFailureDoesNotPublishCandidate",
            "CorruptedDuplicateHistoryDocumentDoesNotReplaceCatalog",
            "InvalidTypedStateDocumentDoesNotReplaceCatalog",
            "injected persistence failure",
            "validation.record.record.test.2",
            "governance.record.record.test.2",
        ):
            require_contains(hardening_tests, fragment, hardening_tests_path)

        types_tests = types_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "KnownValuesRoundTripThroughTypedBoundary",
            "TypographicalValuesFailAtBoundary",
            "LegacySchemaValuesRemainRepresentable",
            'ParseValidationState("validted")',
            'ParseGovernanceAxis("operational-risk")',
            'ParseConfidenceLevel("unrated")',
        ):
            require_contains(types_tests, fragment, types_tests_path)

        economy_tests = economy_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "ItemProfileRequiresCanonicalEconomyItem",
            "RecipeStationsRequireCanonicalStationRecords",
            "IngredientAndOutputJoinsValidateIdentityQuantityAndChance",
            "EconomyDataRoundTripsThroughCanonicalDocument",
            "AcquisitionRelationshipStartsUnvalidatedAndForbidden",
            "ActionLaneMatrixReflectsGovernedAllowedAndForbiddenState",
            'm_persistenceMode = "native_template"',
            'm_relationshipKind = "sold_by"',
            '"no_unvalidated_runtime_use"',
        ):
            require_contains(economy_tests, fragment, economy_tests_path)
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail catalog/governance/economy test validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail catalog, typed governance hardening, and economy test contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
