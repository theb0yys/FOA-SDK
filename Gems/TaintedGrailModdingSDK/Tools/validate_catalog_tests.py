#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate catalog, governance, economy, preview-smoke test registration, and safety coverage."""

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
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"
    cmake_path = code_root / "CMakeLists.txt"
    manifest_path = code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    database_tests_path = tests_root / "CatalogDatabaseTests.cpp"
    governance_tests_path = tests_root / "CatalogGovernanceServiceTests.cpp"
    hardening_tests_path = tests_root / "CatalogGovernanceHardeningTests.cpp"
    types_tests_path = tests_root / "CatalogGovernanceTypesTests.cpp"
    economy_tests_path = tests_root / "EconomyAuthoringTests.cpp"
    preview_smoke_tests_path = tests_root / "DeveloperPreviewSmokeTests.cpp"
    economy_header_path = source_root / "EconomyAuthoringService.h"
    economy_source_path = source_root / "EconomyAuthoringService.cpp"
    editor_header_path = source_root / "ItemRecipeEditorWidget.h"
    editor_source_path = source_root / "ItemRecipeEditorWidget.cpp"
    catalog_persistence_path = source_root / "CatalogPersistenceService.cpp"
    persistence_compatibility_path = source_root / "PersistenceJsonUtils.h"

    try:
        required_paths = (
            cmake_path,
            manifest_path,
            database_tests_path,
            governance_tests_path,
            hardening_tests_path,
            types_tests_path,
            economy_tests_path,
            preview_smoke_tests_path,
            economy_header_path,
            economy_source_path,
            editor_header_path,
            editor_source_path,
            catalog_persistence_path,
            persistence_compatibility_path,
        )
        for path in required_paths:
            if not path.is_file():
                fail(f"Required catalog/governance/economy/preview test or source file is missing: {path}")

        cmake = cmake_path.read_text(encoding="utf-8")
        for fragment in (
            "PAL_TRAIT_BUILD_TESTS_SUPPORTED",
            "PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED",
            "NAME ${gem_name}.Catalog.Tests",
            "taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "AZ::AzTest",
            "AZ::AzToolsFramework",
            "ly_add_googletest",
            "Tests/DeveloperPreviewSmokeTests.cpp",
            "TG_SDK_PREVIEW_TEMPLATE_ROOT",
            "../Preview/Template",
        ):
            require_contains(cmake, fragment, cmake_path)

        manifest = manifest_path.read_text(encoding="utf-8")
        entries = set(re.findall(r"^\s+([^\s\)]+)\s*$", manifest, re.MULTILINE))
        expected = {
            "Source/CatalogDatabase.cpp", "Source/CatalogDatabase.h",
            "Source/CatalogGovernanceService.cpp", "Source/CatalogGovernanceService.h",
            "Source/CatalogGovernanceTypes.cpp", "Source/CatalogGovernanceTypes.h",
            "Source/CatalogPersistenceService.cpp", "Source/CatalogPersistenceService.h",
            "Source/CatalogTransactionService.cpp", "Source/CatalogTransactionService.h",
            "Source/EconomyAuthoringService.cpp", "Source/EconomyAuthoringService.h",
            "Source/EconomyModels.cpp", "Source/EconomyModels.h",
            "Source/FoundationModels.cpp", "Source/FoundationModels.h",
            "Source/PackPersistenceService.cpp", "Source/PackPersistenceService.h",
            "Source/SourceEvidencePersistenceService.cpp", "Source/SourceEvidencePersistenceService.h",
            "Source/SourceEvidenceRegistry.cpp", "Source/SourceEvidenceRegistry.h",
            "Source/WorkspacePersistenceService.cpp", "Source/WorkspacePersistenceService.h",
            "Tests/CatalogDatabaseTests.cpp",
            "Tests/CatalogGovernanceHardeningTests.cpp",
            "Tests/CatalogGovernanceServiceTests.cpp",
            "Tests/CatalogGovernanceTypesTests.cpp",
            "Tests/DeveloperPreviewSmokeTests.cpp",
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
            "RecipeStationEvidenceCombinesSourcesDeterministically",
            "RecipeStationEvidenceFailsClosedForUnknownAndUnresolvedEvidence",
            "RecipeStationEvidenceBlocksStaleStationWithoutMutatingCatalog",
            'm_persistenceMode = "native_template"',
            'm_relationshipKind = "sold_by"',
            '"no_unvalidated_runtime_use"',
            'EXPECT_EQ(rows[0].m_status, "supported evidence")',
            'EXPECT_EQ(rows[1].m_status, "partial evidence")',
            "EXPECT_EQ(catalog.GetGovernanceHistory().size(), governanceBefore)",
        ):
            require_contains(economy_tests, fragment, economy_tests_path)

        economy_header = economy_header_path.read_text(encoding="utf-8")
        for fragment in (
            "struct EconomyRecipeStationEvidenceRow",
            "BuildRecipeStationEvidence",
            "const SourceEvidenceRegistry& sourceRegistry",
            "const CatalogDatabase& catalog",
            "const AZStd::vector<BlockerRecord>& blockers",
        ):
            require_contains(economy_header, fragment, economy_header_path)

        economy_source = economy_source_path.read_text(encoding="utf-8")
        for fragment in (
            "supported evidence", "partial evidence", "missing evidence", "unresolved",
            "The evidence ID is unknown", "The evidence belongs to an unrelated subject",
            "The station is not validated, current, reference-complete, conflict-free, and non-superseded",
            "AZStd::sort(",
        ):
            require_contains(economy_source, fragment, economy_source_path)
        for forbidden in (
            "SaveCatalog(", "ApplyCatalogGovernanceDecision(",
            "ApplyCatalogValidationDecision(", "UpsertCatalogRelationship(",
        ):
            if forbidden in economy_source[economy_source.find("BuildRecipeStationEvidence"):]:
                fail(f"Recipe station evidence builder must remain read-only; found {forbidden!r}")

        editor_header = editor_header_path.read_text(encoding="utf-8")
        editor_source = editor_source_path.read_text(encoding="utf-8")
        for fragment in ("RefreshRecipeEvidence", "m_recipeEvidenceTable"):
            require_contains(editor_header, fragment, editor_header_path)
        for fragment in (
            "Station Visibility and Learnability Evidence — Read-only Research",
            "This view combines exact station IDs",
            "BuildRecipeStationEvidence(",
            "Blockers and reasons",
        ):
            require_contains(editor_source, fragment, editor_source_path)

        smoke_tests = preview_smoke_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "DeveloperPreviewFixtureLoadSaveReopenPreservesCanonicalState",
            "ProofBackedAllowedUsageSurvivesCatalogLoad",
            "LegacyUnprovenAllowanceStillFailsClosed",
            "WorkspacePersistenceService",
            "PackPersistenceService",
            "SourceEvidencePersistenceService",
            "CatalogPersistenceService",
            "BuildRecipeStationEvidence",
            "BuildCanonicalSnapshot",
            "loaded = {}",
            "EXPECT_EQ(snapshotBefore, snapshotAfterResult.GetValue())",
        ):
            require_contains(smoke_tests, fragment, preview_smoke_tests_path)
        for forbidden in (
            "FoA.exe", "BepInEx", "HarmonyLib", "WriteProcessMemory", "Deployment/",
        ):
            if forbidden in smoke_tests:
                fail(f"Developer Preview smoke test crosses the runtime boundary: {forbidden}")

        compatibility = persistence_compatibility_path.read_text(encoding="utf-8")
        for fragment in (
            "ReadJsonFile",
            'document.HasMember("Type")',
            "AZ::JsonSerialization::Load",
            "Processing::Completed",
        ):
            require_contains(compatibility, fragment, persistence_compatibility_path)

        persistence = catalog_persistence_path.read_text(encoding="utf-8")
        for fragment in (
            "HasProofBackedAllowance",
            "FindLatestPermissionEvent",
            "HasValidatedPermissionBasis",
            'event->m_newValue == "allow"',
            'NormalizeLegacyValidationHistory(document);',
            'NormalizeLegacyGovernanceState(document);',
            'legacy_permission_review_required',
            "PersistenceJsonUtils::LoadObjectFromFile",
        ):
            require_contains(persistence, fragment, catalog_persistence_path)
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail catalog/governance/economy/preview smoke validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail catalog, governance, economy, and Developer Preview persistence-smoke contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
