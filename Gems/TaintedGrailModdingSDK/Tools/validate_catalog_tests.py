#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate catalog, governance, economy, adapter, work-order, workspace, and linked-target coverage."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_contains(text: str, fragment: str, path: Path) -> None:
    if fragment not in text:
        fail(f"Missing required fragment {fragment!r} in {path}")


def require_file(path: Path) -> str:
    if not path.is_file():
        fail(f"Required catalog/workspace test or source file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    text = require_file(path)
    for fragment in fragments:
        require_contains(text, fragment, path)
    return text


def require_text_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            fail(f"Missing required fragment {fragment!r} in {label}")


def manifest_entries(path: Path) -> set[str]:
    return set(re.findall(r"^\s+((?:Source|Tests)/[^\s\)]+)\s*$", require_file(path), re.MULTILINE))


def main() -> int:
    gem_root = Path(__file__).resolve().parents[1]
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"
    cmake_path = code_root / "CMakeLists.txt"
    test_manifest_path = code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    work_order_test_manifest_path = code_root / "taintedgrailmoddingsdk_work_order_tests_files.cmake"
    core_manifest_path = code_root / "taintedgrailmoddingsdk_core_files.cmake"
    framework_manifest_path = code_root / "taintedgrailmoddingsdk_framework_files.cmake"

    try:
        cmake = require_fragments(
            cmake_path,
            (
                "PAL_TRAIT_BUILD_TESTS_SUPPORTED",
                "PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED",
                "NAME ${gem_name}.Core.Static STATIC",
                "NAME ${gem_name}.Framework.Static STATIC",
                "NAME ${gem_name}.Catalog.Tests",
                "Gem::${gem_name}.Framework.Static",
                "taintedgrailmoddingsdk_catalog_tests_files.cmake",
                "taintedgrailmoddingsdk_path_policy_tests_files.cmake",
                "taintedgrailmoddingsdk_work_order_tests_files.cmake",
                "AZ::AzTest",
                "AZ::AzToolsFramework",
                "ly_add_googletest",
                "Tests/DeveloperPreviewSmokeTests.cpp",
                "Tests/WorkspaceSchemaServiceTests.cpp",
                "TG_SDK_PREVIEW_TEMPLATE_ROOT",
                "../Preview/Template",
            ),
        )
        if "Source/CatalogDatabase.cpp" in cmake:
            fail("Production source ownership must remain in manifests, not the test target block")

        test_entries = manifest_entries(test_manifest_path)
        expected_tests = {
            "Tests/AdapterContractTests.cpp",
            "Tests/CanonicalFingerprintTests.cpp",
            "Tests/CatalogDatabaseTests.cpp",
            "Tests/CatalogSchemaMigrationPersistenceTests.cpp",
            "Tests/CatalogGovernanceHardeningTests.cpp",
            "Tests/CatalogGovernanceServiceTests.cpp",
            "Tests/CatalogGovernanceTypesTests.cpp",
            "Tests/DeveloperPreviewSmokeTests.cpp",
            "Tests/EconomyAuthoringTests.cpp",
            "Tests/EconomyCoverageServiceTests.cpp",
            "Tests/EconomyDuplicateDetectionServiceTests.cpp",
            "Tests/FoundationWorkspaceIsolationTests.cpp",
            "Tests/Main.cpp",
            "Tests/PopulationActionLaneServiceTests.cpp",
            "Tests/PopulationAuthoringTests.cpp",
            "Tests/PopulationCatalogTests.cpp",
            "Tests/SourceImportServiceTests.cpp",
        }
        if test_entries != expected_tests:
            fail(
                f"Catalog test manifest mismatch: expected {sorted(expected_tests)}, "
                f"found {sorted(test_entries)}"
            )

        work_order_entries = manifest_entries(work_order_test_manifest_path)
        expected_work_order_entries = {
            "Tests/AdapterWorkOrderPlanningTests.cpp",
            "Tests/AdapterWorkOrderPlanningTestFixturePart1.inl",
            "Tests/AdapterWorkOrderPlanningTestFixturePart2.inl",
            "Tests/AdapterWorkOrderPlanningTestFixturePart3.inl",
            "Tests/AdapterWorkOrderPlanningTestFixturePart4.inl",
            "Tests/AdapterWorkOrderPlanningTestFixturePart5.inl",
            "Tests/AdapterWorkOrderPlanningTestsPart1.inl",
            "Tests/AdapterWorkOrderPlanningTestsPart2.inl",
            "Tests/AdapterWorkOrderPlanningTestsPart3.inl",
        }
        if work_order_entries != expected_work_order_entries:
            fail(
                f"Work-order test manifest mismatch: expected {sorted(expected_work_order_entries)}, "
                f"found {sorted(work_order_entries)}"
            )
        if any(entry.startswith("Source/") for entry in work_order_entries):
            fail("Work-order tests must link production targets instead of recompiling production sources")

        core_entries = manifest_entries(core_manifest_path)
        required_core = {
            "Source/AdapterCompatibilityService.cpp",
            "Source/AdapterContractRegistry.cpp",
            "Source/AdapterWorkOrderPlanningService.cpp",
            "Source/CatalogDatabase.cpp",
            "Source/CatalogDatabaseIntegrity.cpp",
            "Source/CatalogDatabasePopulation.cpp",
            "Source/CatalogGovernanceBlockerService.cpp",
            "Source/CatalogGovernanceTypes.cpp",
            "Source/CatalogTransactionService.cpp",
            "Source/EconomyBlockerService.cpp",
            "Source/EconomyCoverageService.cpp",
            "Source/EconomyDuplicateDetectionService.cpp",
            "Source/EconomyModels.cpp",
            "Source/FoundationModels.cpp",
            "Source/PopulationActionLaneService.cpp",
            "Source/PopulationModels.cpp",
            "Source/SourceEvidenceRegistry.cpp",
        }
        if not required_core.issubset(core_entries):
            fail(
                "Core manifest is missing catalog/economy/adapter/work-order ownership: "
                + ", ".join(sorted(required_core - core_entries))
            )

        framework_entries = manifest_entries(framework_manifest_path)
        required_framework = {
            "Source/CatalogGovernanceService.cpp",
            "Source/CatalogPersistenceService.cpp",
            "Source/CatalogPromotionService.cpp",
            "Source/EconomyAuthoringService.cpp",
            "Source/FoundationCatalogService.cpp",
            "Source/FoundationPopulationService.cpp",
            "Source/FoundationValidationService.cpp",
            "Source/PackPersistenceService.cpp",
            "Source/PopulationAuthoringService.cpp",
            "Source/SourceEvidencePersistenceService.cpp",
            "Source/SourceImportService.cpp",
            "Source/WorkspacePersistenceService.cpp",
        }
        if not required_framework.issubset(framework_entries):
            fail(
                "Framework manifest is missing catalog/persistence ownership: "
                + ", ".join(sorted(required_framework - framework_entries))
            )

        checks = {
            "AdapterContractTests.cpp": (
                "SemanticVersionsAndTypedCapabilitiesAreStrict",
                "EmptyRegistryAndMissingCapabilitiesFailClosed",
                "VersionMismatchPrecedesPermissionAndProofChecks",
                "PermissionMissingAndProofMissingRemainDistinct",
                "SupportedResultIsDeterministicAndDoesNotMutateInputs",
                'EXPECT_EQ(row->m_status, "version_mismatch")',
                'EXPECT_EQ(row->m_status, "permission_missing")',
                'EXPECT_EQ(row->m_status, "proof_missing")',
                'EXPECT_EQ(row->m_status, "supported")',
                "declarationCountBefore",
                "recordCountBefore",
            ),
            "CatalogDatabaseTests.cpp": (
                "SameDisplayNameDoesNotMergeDistinctRecordIds",
                "DuplicateExactNativeReferenceIsRejected",
                "SyntheticRecordRequiresOwningPack",
                "QueryFiltersEvidencePermissionAndBlockedState",
                "RelationshipRequiresKnownRecordsAndEvidence",
            ),
            "CatalogSchemaMigrationPersistenceTests.cpp": (
                "SchemaOnePreviewMigratesToSchemaTwoWithEmptyPopulationAndPreservesLegacyProjection",
                "PlainCatalogRejectsMissingMalformedAndFutureSchemaVersions",
                "LegacySerializationEnvelopeWithoutSchemaVersionMigratesAsSchemaOne",
                "SchemaOnePopulationCollectionsAreRejectedWithoutReplacement",
                "WriterRejectsSchemaOneAndWritesPlainSchemaTwoWithExplicitPopulationArrays",
                "SchemaTwoSaveLoadSaveIsByteStable",
                "SchemaTwoSaveClearLoadAndReplacePreservesCanonicalState",
                "MalformedSchemaTwoDocumentDoesNotReplacePublishedCatalog",
                "PopulationCandidateSaveFailureDoesNotPublish",
                "PopulationCatalogSchemaVersion",
                "LegacyCatalogSchemaVersion",
            ),
            "CatalogGovernanceServiceTests.cpp": (
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
            ),
            "CatalogGovernanceHardeningTests.cpp": (
                "UnknownEvidenceLeavesOriginalCatalogUnchanged",
                "WrongEvidenceSubjectLeavesOriginalCatalogUnchanged",
                "WrongEvidenceProfileLeavesOriginalCatalogUnchanged",
                "DuplicateGovernanceIdRollsBackStateChange",
                "DuplicateValidationIdRollsBackStateChange",
                "PersistenceFailureDoesNotPublishCandidate",
                "CorruptedDuplicateHistoryDocumentDoesNotReplaceCatalog",
                "InvalidTypedStateDocumentDoesNotReplaceCatalog",
                "injected persistence failure",
            ),
            "CatalogGovernanceTypesTests.cpp": (
                "KnownValuesRoundTripThroughTypedBoundary",
                "TypographicalValuesFailAtBoundary",
                "LegacySchemaValuesRemainRepresentable",
                'ParseValidationState("validted")',
                'ParseGovernanceAxis("operational-risk")',
                'ParseConfidenceLevel("unrated")',
            ),
            "EconomyAuthoringTests.cpp": (
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
            ),
            "EconomyCoverageServiceTests.cpp": (
                "ItemCoverageSeparatesVendorLootAndRewardDeterministically",
                "RecipeCoverageUsesLearnabilityCraftingAndRewardLanes",
                "CoveredRelationshipCannotHideBlockedRelationshipInSameLane",
                "CoverageFailsClosedForUnrelatedEvidenceAndOpenBlockersWithoutMutation",
                'EXPECT_EQ(vendor->m_status, "covered")',
                'EXPECT_EQ(loot->m_status, "partial")',
                'EXPECT_EQ(vendor->m_status, "blocked")',
                "recordCountBefore",
                "relationshipCountBefore",
                "sourceCountBefore",
                "evidenceCountBefore",
            ),
            "EconomyDuplicateDetectionServiceTests.cpp": (
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
            "DeveloperPreviewSmokeTests.cpp": (
                "DeveloperPreviewFixtureLoadSaveReopenPreservesCanonicalState",
                "ProofBackedAllowedUsageSurvivesCatalogLoad",
                "LegacyUnprovenAllowanceStillFailsClosed",
                "BuildRecipeStationEvidence",
                "BuildCanonicalSnapshot",
                "loaded = {}",
                "EXPECT_EQ(snapshotBefore, snapshotAfterResult.GetValue())",
            ),
            "FoundationServiceWorkspaceLoadTests.cpp": (
                "SuccessfulCandidatePublishesEveryWorkspaceObject",
                "WorkspaceDocumentFailurePreservesAllLiveState",
                "ImportIssueFailurePreservesAllLiveState",
                "CatalogValidationFailurePreservesAllLiveState",
                "StateSignature",
            ),
            "PopulationActionLaneServiceTests.cpp": (
                "ClosedVocabularyUsesDeterministicContractValues",
                "FixedLaneOrderAndHardUnavailableRuntimeLanes",
                "AuthoringRequiresResolvedWorkspacePackAndExactEvidence",
                "TroopCompositionPlanningAndSpawnCandidacyFailClosed",
                "SpawnCandidateRejectsUnvalidatedStaleMissingConflictedAndSupersededRecords",
                "ForbiddenAndRelevantBlockersTakePrecedenceDeterministically",
                "UnrelatedBlockersDoNotPoisonSubjectAndInputsRemainUnchanged",
                "MissingOrWrongSelectionFailsClosed",
                "noResolvedRoot",
                "PopulationActionLaneState::Unavailable",
                "PopulationActionLaneState::Forbidden",
                "recordCountBefore",
                "blockersBefore",
                "sourceCountBefore",
                "evidenceCountBefore",
            ),
            "PopulationAuthoringTests.cpp": (
                "ActorProfilePersistsPublishesSnapshotAndNotifiesOnce",
                "TroopDefinitionIsAtomicOrderIndependentAndPreservesOmittedMembers",
                "StandaloneUpdatesPreserveCompleteCatalogIntegrity",
                "StructuralFailuresAndLinkOwnerMovesDoNotPublish",
                "ContextEvidenceAndPrimaryOwnershipFailClosed",
                "WrongProfileSourceAndCrossPackReferencesAreHandledExplicitly",
                "CandidateContextRejectsUnboundWorkspaceAndPackWithoutMutation",
                "PersistenceFailureLeavesPublishedCatalogUnchangedAndSilent",
                "ReflectPopulationPersistenceTypes",
                "AZ::JsonSystemComponent::Reflect",
                "AZ::ComponentApplication m_application",
                "FoundationActorPublicationObserver",
                "service.ClearActivePack()",
                "validCandidate.IsSuccess()",
                "Unable to create the catalog directory",
            ),
            "PopulationCatalogTests.cpp": (
                "ClosedKindsAndRolesRoundTripAndRejectUnknownValues",
                "ActorProfilesAcceptExactResolvedAndUnresolvedTemplates",
                "ActorProfilesRejectKindsBindingsBoundsAndDuplicates",
                "TroopsAcceptExactResolvedAndUnresolvedLeaders",
                "TroopProfilesRejectKindsLeaderBindingsAndBounds",
                "MembersRejectBindingsBoundsRolesAndDuplicateConflicts",
                "CollectionsAreCanonicalAndDoNotMutateInputs",
                "CompleteCatalogIntegrityAcceptsExactComposition",
                "CompleteCatalogIntegrityRejectsEvidenceAndCompositionGaps",
                "PopulationUpsertsDoNotGrantGovernanceOrActionAuthority",
            ),
            "WorkspaceSchemaServiceTests.cpp": (
                "UnknownSchemaVersionIsRejected",
                "UnknownSchemaVersionCannotHideBehindLegacyEnvelope",
                "MalformedLegacyWorkspaceHasMigrationError",
                "UnsafeLegacyWorkspaceIsClearlyRejected",
                "SchemaOneRoundTripIsStable",
                "PreviewFixtureLegacyShapeMigratesAndRoundTrips",
            ),
        }
        for filename, fragments in checks.items():
            require_fragments(tests_root / filename, fragments)

        population_authoring_tests = require_file(tests_root / "PopulationAuthoringTests.cpp")
        if "ReplacesMemberSet" in population_authoring_tests:
            fail(
                "Population authoring tests must preserve additive troop-definition semantics; "
                "the stale ReplacesMemberSet expectation is forbidden"
            )

        work_order_tests = "\n".join(
            require_file(code_root / entry) for entry in sorted(work_order_entries)
        )
        require_text_fragments(
            work_order_tests,
            (
                "AnyNonSupportedCompatibilityRowRefusesWholePlan",
                "FullySupportedCatalogBuildsCanonicalPlanOnly",
                "AggregateSupportCannotLeakUnreviewedSubjectsIntoSteps",
                "RelationshipStepsBindResolvedTargetsAndProof",
                "MissingRelationshipProofRefusesWholePlan",
                "InvalidTypedPayloadEvidenceRefusesWholePlan",
                "CanonicalSerializationIsDeterministic",
                "PlanningDoesNotMutateInputs",
                'EXPECT_EQ(result.m_generatedPlanCount, 0)',
                'EXPECT_EQ(plan.m_steps.size(), 11)',
                "declarationCountBefore",
                "relationshipCountBefore",
            ),
            "production-linked adapter work-order tests",
        )

        economy_header = require_fragments(
            source_root / "EconomyAuthoringService.h",
            (
                "struct EconomyRecipeStationEvidenceRow",
                "BuildRecipeStationEvidence",
                "const SourceEvidenceRegistry& sourceRegistry",
                "const CatalogDatabase& catalog",
                "const AZStd::vector<BlockerRecord>& blockers",
            ),
        )
        del economy_header
        economy_source = require_fragments(
            source_root / "EconomyAuthoringService.cpp",
            (
                "supported evidence",
                "partial evidence",
                "missing evidence",
                "unresolved",
                "The evidence ID is unknown",
                "The evidence does not prove an exact station or learnability association",
                "The station is not validated, current, reference-complete, conflict-free, and non-superseded",
                "AZStd::sort(",
            ),
        )
        for forbidden in (
            "SaveCatalog(",
            "ApplyCatalogGovernanceDecision(",
            "ApplyCatalogValidationDecision(",
            "UpsertCatalogRelationship(",
        ):
            if forbidden in economy_source[economy_source.find("BuildRecipeStationEvidence"):]:
                fail(f"Recipe station evidence builder must remain read-only; found {forbidden!r}")

        require_fragments(
            source_root / "ItemRecipeEditorWidget.h",
            ("RefreshRecipeEvidence", "m_recipeEvidenceTable"),
        )
        require_fragments(
            source_root / "ItemRecipeEditorWidget.cpp",
            (
                "Station Visibility and Learnability Evidence - Read-only Research",
                "This view combines exact station IDs",
                "BuildRecipeStationEvidence(",
                "Blockers and reasons",
            ),
        )
        require_fragments(
            source_root / "EconomyCoverageService.cpp",
            ("BuildAcquisitionCoverage", '"covered"', '"partial"', '"blocked"', '"missing"'),
        )
        require_fragments(
            source_root / "EconomyDuplicateDetectionService.cpp",
            (
                "BuildCrossPackDuplicateReport",
                '"subject_ref"',
                '"recipe_duplicate_key"',
                "HasDistinctPackOwners",
                "packIds.size() >= 2",
            ),
        )
        require_fragments(
            source_root / "AdapterCompatibilityService.cpp",
            (
                "BuildCapabilityMatrix",
                '"supported"',
                '"unsupported"',
                '"version_mismatch"',
                '"permission_missing"',
                '"proof_missing"',
            ),
        )
        require_fragments(
            source_root / "AdapterContractRegistry.cpp",
            ("TryParseAdapterSemanticVersion", "IsAdapterVersionCompatible", '"item_grant"', '"rollback"'),
        )

        planner_parts = sorted(source_root.glob("AdapterWorkOrderPlanningServicePart*.inl"))
        planner = require_file(source_root / "AdapterWorkOrderPlanningService.h") + "\n" + require_file(
            source_root / "AdapterWorkOrderPlanningService.cpp"
        ) + "\n" + "\n".join(require_file(path) for path in planner_parts)
        require_text_fragments(
            planner,
            (
                "BuildPlans",
                "BuildCapabilityMatrix",
                "GroupIsSupported",
                'row->m_status != "supported"',
                "CollectReadySubjects",
                "CollectRelationshipValidationProof",
                "RecordPayloadIsComplete",
                "SerializeCanonicalPlan",
                "ExecutionAllowed",
                "m_executionAllowed = false",
            ),
            "adapter work-order planner",
        )

        compatibility = require_fragments(
            source_root / "PersistenceJsonUtils.h",
            ("ReadJsonFile", 'document.HasMember("Type")', "AZ::JsonSerialization::Load", "Processing::Completed"),
        )
        del compatibility
        require_fragments(
            source_root / "PopulationModels.h",
            (
                "LegacyCatalogSchemaVersion = 1",
                "PopulationCatalogSchemaVersion = 2",
                "CurrentCatalogSchemaVersion =",
                "PopulationCatalogSchemaVersion;",
            ),
        )
        require_fragments(
            source_root / "CatalogDatabase.cpp",
            (
                "document.m_actorProfiles = m_populationActorProfiles",
                "document.m_troopProfiles = m_populationTroopProfiles",
                "document.m_troopMembers = m_populationTroopMembers",
                "document.m_schemaVersion = CurrentCatalogSchemaVersion",
            ),
        )
        require_fragments(
            source_root / "CatalogPersistenceService.cpp",
            (
                "ReadCatalogSchemaVersion",
                "DetectCatalogSchemaVersion",
                "SerializePlainCatalog",
                "Plain canonical catalog documents require an explicit SchemaVersion.",
                "Catalog schema 1 cannot contain population collections.",
                "Canonical catalog saves require schema 2; schema 1 is a load-only migration input.",
                "settings.m_keepDefaults = true",
                "QSaveFile file",
                "file.setDirectWriteFallback(false)",
                "HasProofBackedAllowance",
                "FindLatestPermissionEvent",
                "HasValidatedPermissionBasis",
                'event->m_newValue == "allow"',
                "NormalizeLegacyValidationHistory(document);",
                "NormalizeLegacyGovernanceState(document);",
                "legacy_permission_review_required",
                "PersistenceJsonUtils::LoadObjectFromFile",
            ),
        )
        require_fragments(
            source_root / "WorkspacePersistenceService.cpp",
            (
                "WorkspaceSchemaService::CurrentSchemaVersion",
                "WorkspaceSchemaService::LegacySchemaVersion",
                "DetectSchemaVersion",
                "MigrateAndValidate",
                "QSaveFile",
            ),
        )
    except (OSError, RuntimeError) as exc:
        print(
            f"Tainted Grail catalog/governance/economy/adapter/work-order/workspace validation failed: {exc}",
            file=sys.stderr,
        )
        return 1

    print(
        "Tainted Grail catalog, governance, economy analysis, adapter contracts, work-order plans, "
        "population authoring test sources, atomic workspace, linked-target, and persistence-smoke "
        "contract passed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
