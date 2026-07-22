#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the catalog schema-1 migration and schema-2 persistence gate."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable


class CatalogSchema2ContractError(RuntimeError):
    """Raised when the catalog schema-2 contract is incomplete or unsafe."""


def read_text(path: Path) -> str:
    if not path.is_file():
        raise CatalogSchema2ContractError(
            f"Required catalog schema-2 file is missing: {path}"
        )
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise CatalogSchema2ContractError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: Iterable[str], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise CatalogSchema2ContractError(
                f"{label} contains forbidden fragment {fragment!r}."
            )


def require_order(text: str, fragments: tuple[str, ...], label: str) -> None:
    position = -1
    for fragment in fragments:
        next_position = text.find(fragment, position + 1)
        if next_position < 0:
            raise CatalogSchema2ContractError(
                f"{label} is missing ordered fragment {fragment!r}."
            )
        if next_position <= position:
            raise CatalogSchema2ContractError(
                f"{label} does not preserve required ordering at {fragment!r}."
            )
        position = next_position


def require_exact_count(text: str, fragment: str, count: int, label: str) -> None:
    actual = text.count(fragment)
    if actual != count:
        raise CatalogSchema2ContractError(
            f"{label} must contain {fragment!r} exactly {count} time(s), found {actual}."
        )


def section_from_marker(text: str, marker: str, terminator: str, label: str) -> str:
    start = text.find(marker)
    if start < 0:
        raise CatalogSchema2ContractError(
            f"{label} is missing required marker {marker!r}."
        )
    end = text.find(terminator, start + len(marker))
    return text[start:] if end < 0 else text[start:end]


def validate_catalog_schema2(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"
    tools_root = gem_root / "Tools"

    core_manifest = read_text(code_root / "taintedgrailmoddingsdk_core_files.cmake")
    framework_manifest = read_text(
        code_root / "taintedgrailmoddingsdk_framework_files.cmake"
    )
    test_manifest = read_text(
        code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    )
    require_fragments(
        core_manifest,
        (
            "Source/PopulationModels.cpp",
            "Source/PopulationModels.h",
            "Source/CatalogDatabase.cpp",
            "Source/CatalogDatabaseIntegrity.cpp",
            "Source/CatalogDatabasePopulation.cpp",
        ),
        "Core manifest",
    )
    require_fragments(
        framework_manifest,
        (
            "Source/CatalogPersistenceService.cpp",
            "Source/CatalogPersistenceService.h",
        ),
        "Framework manifest",
    )
    require_fragments(
        test_manifest,
        (
            "Tests/CatalogSchemaMigrationPersistenceTests.cpp",
            "Tests/PopulationAuthoringTests.cpp",
            "Tests/PopulationCatalogTests.cpp",
        ),
        "Catalog test manifest",
    )

    population_header = read_text(source_root / "PopulationModels.h")
    require_fragments(
        population_header,
        (
            "LegacyCatalogSchemaVersion = 1",
            "PopulationCatalogSchemaVersion = 2",
            "CurrentCatalogSchemaVersion =",
            "PopulationCatalogSchemaVersion;",
            "Schema 1 remains a load-only migration input",
        ),
        "Population schema constants",
    )
    if re.search(
        r"CurrentCatalogSchemaVersion\s*=\s*LegacyCatalogSchemaVersion",
        population_header,
    ):
        raise CatalogSchema2ContractError(
            "CurrentCatalogSchemaVersion must not remain bound to legacy schema 1."
        )

    foundation_header = read_text(source_root / "FoundationModels.h")
    foundation_source = read_text(source_root / "FoundationModels.cpp")
    require_fragments(
        foundation_header,
        (
            "m_schemaVersion = CurrentCatalogSchemaVersion",
            "AZStd::vector<PopulationActorProfile> m_actorProfiles",
            "AZStd::vector<PopulationTroopProfile> m_troopProfiles",
            "AZStd::vector<PopulationTroopMember> m_troopMembers",
        ),
        "Catalog document model",
    )
    require_fragments(
        foundation_source,
        (
            'Field("ActorProfiles", &CatalogDocument::m_actorProfiles)',
            'Field("TroopProfiles", &CatalogDocument::m_troopProfiles)',
            'Field("TroopMembers", &CatalogDocument::m_troopMembers)',
            "m_schemaVersion == LegacyCatalogSchemaVersion",
            "m_schemaVersion == CurrentCatalogSchemaVersion",
        ),
        "Catalog document reflection",
    )

    database = read_text(source_root / "CatalogDatabase.cpp")
    require_fragments(
        database,
        (
            "document.m_actorProfiles = m_populationActorProfiles",
            "document.m_troopProfiles = m_populationTroopProfiles",
            "document.m_troopMembers = m_populationTroopMembers",
            "document.m_schemaVersion = CurrentCatalogSchemaVersion",
        ),
        "Catalog document builder",
    )
    reject_fragments(
        database,
        (
            "hasPopulationData",
            "? PopulationCatalogSchemaVersion",
            ": LegacyCatalogSchemaVersion",
        ),
        "Catalog document builder",
    )

    persistence = read_text(source_root / "CatalogPersistenceService.cpp")
    require_fragments(
        persistence,
        (
            "ReadCatalogSchemaVersion",
            "DetectCatalogSchemaVersion",
            "SerializePlainCatalog",
            'FindMember("SchemaVersion")',
            'FindMember("ClassData")',
            "Plain canonical catalog documents require an explicit SchemaVersion.",
            "Catalog SchemaVersion must be an unsigned 32-bit integer.",
            "Catalog schema version %u is unsupported; this editor supports schema 1 migration and schema 2.",
            "Catalog SchemaVersion changed during deserialization; the document was not loaded.",
            "Catalog schema 1 cannot contain population collections.",
            "Canonical catalog saves require schema 2; schema 1 is a load-only migration input.",
            "document.m_schemaVersion != CurrentCatalogSchemaVersion",
            "document.m_schemaVersion = detectedSchemaVersion",
            "detectedSchemaVersion == LegacyCatalogSchemaVersion",
            "NormalizeLegacyValidationHistory(document);",
            "NormalizeLegacyGovernanceState(document);",
            "settings.m_keepDefaults = true",
            "AZ::JsonSerialization::Store",
            "WriteJsonString",
            "QSaveFile file",
            "file.setDirectWriteFallback(false)",
            "file.cancelWriting()",
            "file.commit()",
        ),
        "Catalog schema-2 persistence",
    )
    reject_fragments(
        persistence,
        (
            "AZ::JsonSerializationUtils::SaveObjectToFile(&document",
            "QProcess",
            "BepInEx",
            "HarmonyLib",
            "ProcessLaunchInfo",
            "document.m_schemaVersion = CurrentCatalogSchemaVersion",
        ),
        "Catalog schema-2 persistence and retained schema-1 load state",
    )
    schema_assignments = [
        match.strip()
        for match in re.findall(
            r"document\.m_schemaVersion\s*=\s*([^;]+);",
            persistence,
        )
    ]
    if schema_assignments != ["detectedSchemaVersion"]:
        raise CatalogSchema2ContractError(
            "Catalog persistence must assign document.m_schemaVersion exactly once from "
            "detectedSchemaVersion; schema-2 promotion belongs to successful bound "
            "replacement and BuildDocument."
        )
    require_order(
        persistence,
        (
            "DetectCatalogSchemaVersion(jsonResult.GetValue())",
            "document.m_schemaVersion = detectedSchemaVersion",
            "PersistenceJsonUtils::LoadObjectFromFile(document, path)",
            "document.m_schemaVersion != detectedSchemaVersion",
            "Catalog schema 1 cannot contain population collections.",
            "ValidatePersistedIdentity(document)",
            "NormalizeLegacyValidationHistory(document);",
            "NormalizeLegacyGovernanceState(document);",
            "return AZ::Success(AZStd::move(document));",
        ),
        "Catalog load/normalization sequence",
    )

    core_population_paths = (
        "PopulationModels.cpp",
        "PopulationModels.h",
        "CatalogDatabase.cpp",
        "CatalogDatabaseIntegrity.cpp",
        "CatalogDatabasePopulation.cpp",
        "CatalogDatabasePopulationPart1.inl",
        "CatalogDatabasePopulationPart2.inl",
        "CatalogDatabasePopulationPart3.inl",
    )
    core_population = "\n".join(
        read_text(source_root / relative_path) for relative_path in core_population_paths
    )
    reject_fragments(
        core_population,
        (
            "#include <Q",
            "#include <Qt",
            "AzToolsFramework",
            "QProcess",
            "BepInEx",
            "HarmonyLib",
            "ProcessLaunchInfo",
            "record.m_displayName ==",
            "record.m_displayName !=",
        ),
        "Core population/schema source family",
    )

    tests = read_text(tests_root / "CatalogSchemaMigrationPersistenceTests.cpp")
    require_fragments(
        tests,
        (
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
            '"ActorProfiles"',
            '"TroopProfiles"',
            '"TroopMembers"',
        ),
        "Compiled catalog schema-2 tests",
    )

    population_catalog_tests = read_text(tests_root / "PopulationCatalogTests.cpp")
    require_fragments(
        population_catalog_tests,
        (
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
        "Compiled Core population-authoring tests",
    )
    population_authoring_tests = read_text(tests_root / "PopulationAuthoringTests.cpp")
    require_fragments(
        population_authoring_tests,
        (
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
            "service.ClearActivePack()",
            "validCandidate.IsSuccess()",
            "Unable to create the catalog directory",
        ),
        "Compiled Framework population-authoring tests",
    )
    reject_fragments(
        population_authoring_tests,
        ("ReplacesMemberSet",),
        "Compiled Framework population-authoring tests",
    )

    migration_test = section_from_marker(
        tests,
        "SchemaOnePreviewMigratesToSchemaTwoWithEmptyPopulationAndPreservesLegacyProjection",
        "\n    TEST_F(",
        "Schema-1 preview migration test",
    )
    require_fragments(
        migration_test,
        (
            "QFile::exists(",
            "EXPECT_FALSE(unvalidatedSave.IsSuccess());",
            "LegacyCatalogSchemaVersion",
            "PopulationCatalogSchemaVersion",
        ),
        "Schema-1 preview migration test",
    )
    require_order(
        migration_test,
        (
            "ASSERT_EQ(loaded.m_schemaVersion, LegacyCatalogSchemaVersion);",
            "persistence.Save(",
            "EXPECT_FALSE(unvalidatedSave.IsSuccess());",
            "QFile::exists(",
            "ReplaceFromBoundDocument(",
            "BuildDocument(",
            "EXPECT_EQ(migrated.m_schemaVersion, PopulationCatalogSchemaVersion);",
        ),
        "Schema-1 load, refusal, replacement, and promotion test",
    )

    legacy_envelope_test = section_from_marker(
        tests,
        "LegacySerializationEnvelopeWithoutSchemaVersionMigratesAsSchemaOne",
        "\n    TEST_F(",
        "Legacy envelope migration test",
    )
    require_order(
        legacy_envelope_test,
        (
            "normalizedLegacy.m_schemaVersion",
            "LegacyCatalogSchemaVersion",
            "ReplaceFromBoundDocument(",
            "BuildDocument(",
            "PopulationCatalogSchemaVersion",
        ),
        "Legacy envelope retained-version and bound-promotion test",
    )

    writer_test = section_from_marker(
        tests,
        "WriterRejectsSchemaOneAndWritesPlainSchemaTwoWithExplicitPopulationArrays",
        "\n    TEST_F(",
        "Schema-2 writer test",
    )
    require_order(
        writer_test,
        (
            "LegacyCatalogSchemaVersion",
            "persistence.Save(",
            "EXPECT_FALSE(legacySave.IsSuccess());",
            "BuildDocument(",
            "PopulationCatalogSchemaVersion",
        ),
        "Schema-1 direct-save refusal and schema-2 writer test",
    )

    catalog_validator = read_text(tools_root / "validate_catalog_tests.py")
    require_fragments(
        catalog_validator,
        (
            '"Tests/CatalogSchemaMigrationPersistenceTests.cpp"',
            '"Tests/PopulationAuthoringTests.cpp"',
            '"Tests/PopulationCatalogTests.cpp"',
            '"Source/CatalogDatabaseIntegrity.cpp"',
            '"Source/CatalogDatabasePopulation.cpp"',
            '"Source/PopulationModels.cpp"',
            '"Source/FoundationPopulationService.cpp"',
            '"Source/PopulationAuthoringService.cpp"',
            "SchemaOnePreviewMigratesToSchemaTwoWithEmptyPopulationAndPreservesLegacyProjection",
            "SchemaTwoSaveLoadSaveIsByteStable",
            "PopulationCandidateSaveFailureDoesNotPublish",
            "TroopDefinitionIsAtomicOrderIndependentAndPreservesOmittedMembers",
            "PopulationUpsertsDoNotGrantGovernanceOrActionAuthority",
            "ReplacesMemberSet",
        ),
        "Aggregate catalog validator",
    )
    foundation_validator = read_text(tools_root / "validate_foundation.py")
    require_fragments(
        foundation_validator,
        ('source_root.glob("*.inl")',),
        "Foundation validator split-source coverage",
    )

    local_validation = read_text(tools_root / "run_local_validation.py")
    require_exact_count(
        local_validation,
        '"validate_catalog_schema2.py"',
        1,
        "Local validation entrypoint",
    )
    workflow = read_text(repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml")
    require_fragments(
        workflow,
        (
            "Test catalog schema-2 validator",
            'test_validate_catalog_schema2.py',
            "Validate catalog schema-1 migration and schema-2 persistence",
            "validate_catalog_schema2.py",
        ),
        "Focused workflow",
    )

    data_formats = read_text(repo_root / "docs/tainted-grail-sdk/DATA_FORMATS.md")
    require_fragments(
        data_formats,
        (
            '"SchemaVersion": 2',
            '"ActorProfiles": []',
            '"TroopProfiles": []',
            '"TroopMembers": []',
            "Schema-1 migration is read-only and fail-closed",
            "Population actor profile",
            "Population troop profile",
            "Population troop member",
            "no population contract invokes FoA",
            "A loaded schema-1 candidate remains schema 1",
            "Directly saving that load result is refused",
            "successful bound replacement followed by `BuildDocument` produces a schema-2 document",
        ),
        "Public data-format contract",
    )
    workspace_start = data_formats.find("## Workspace document")
    catalog_start = data_formats.find("## Canonical catalog document")
    catalog_end = data_formats.find("## Catalog record", catalog_start + 1)
    if workspace_start < 0 or catalog_start <= workspace_start or catalog_end <= catalog_start:
        raise CatalogSchema2ContractError(
            "Data Formats must keep distinct ordered workspace and canonical catalog sections."
        )
    workspace_section = data_formats[workspace_start:catalog_start]
    catalog_section = data_formats[catalog_start:catalog_end]
    require_fragments(
        workspace_section,
        ('"SchemaVersion": 1',),
        "Workspace data-format section",
    )
    reject_fragments(
        workspace_section,
        ('"SchemaVersion": 2',),
        "Workspace data-format section",
    )
    require_fragments(
        catalog_section,
        (
            '"SchemaVersion": 2',
            '"ActorProfiles": []',
            '"TroopProfiles": []',
            '"TroopMembers": []',
        ),
        "Canonical catalog data-format section",
    )
    reject_fragments(
        catalog_section,
        ('"SchemaVersion": 1',),
        "Canonical catalog data-format section",
    )
    actor_design = read_text(
        repo_root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
    )
    require_fragments(
        actor_design,
        (
            "Status: implemented vertical slice",
            "actor/troop contracts, reflection",
            "CatalogDatabase validation, queries",
            "schema-1 migration, schema-2-only writing",
            "5. **Complete** \u2014 Core and Framework positive/negative population-authoring test sources",
            "compiled-target wiring",
            "6. **Complete** \u2014 immutable population action-lane derivation",
            "Actor and Troop Editor pane",
            "7. **Complete** \u2014 deterministic synthetic population fixture",
            "8. **Complete** \u2014 public user, architecture/data-format, release-readiness",
            "9. **Active acceptance gate** \u2014 exact-head O3DE configure/build",
            "twenty-six-pane",
            "does not claim that compiled tests have run",
            "loaded candidate remains schema 1",
            "direct save is refused",
            "successful bound replacement and `BuildDocument`",
        ),
        "Actor/troop implementation status",
    )
    reject_fragments(
        actor_design,
        (
            "Status: active implementation",
            "7. **Next** \u2014 deterministic synthetic population fixture",
            "twenty-three-pane checklist",
        ),
        "Actor/troop implementation status",
    )
    require_order(
        actor_design,
        (
            "5. **Complete** \u2014 Core and Framework positive/negative population-authoring test sources",
            "compiled-target wiring",
            "6. **Complete** \u2014 immutable population action-lane derivation",
            "7. **Complete** \u2014 deterministic synthetic population fixture",
            "8. **Complete** \u2014 public user, architecture/data-format, release-readiness",
            "9. **Active acceptance gate** \u2014 exact-head O3DE configure/build",
        ),
        "Actor/troop implementation sequence",
    )
    roadmap = read_text(repo_root / "ROADMAP.md")
    roadmap_population = section_from_marker(
        roadmap,
        "### Actors and population",
        "\n### ",
        "Roadmap population status",
    )
    require_fragments(
        roadmap_population,
        (
            "Status: implemented vertical slice, continuing hardening and exact-head host/UI verification.",
            "durable catalog schema-2 migration/persistence",
            "evidence-bound Framework candidate publication",
            "positive/negative production-linked population tests are implemented",
            "immutable seven-lane population action contract",
            "registered **Tainted Grail Actor and Troop Editor** pane",
            "project-owned deterministic schema-2 population fixture",
            "real Windows twenty-six-pane evidence pass remain the active acceptance gate",
            "Spawn and Encounter Editor is the next population authoring capability.",
        ),
        "Roadmap population status",
    )
    reject_fragments(
        roadmap_population,
        (
            "Status: active development. Core contracts, CatalogDatabase integration",
            "Framework population-authoring test sources with compiled-target wiring are implemented",
            "deterministic synthetic fixture and complete local-validation integration are next",
        ),
        "Roadmap population status",
    )
    catalog_guide = read_text(repo_root / "docs/tainted-grail-sdk/CATALOG_GUIDE.md")
    require_fragments(
        catalog_guide,
        (
            "Catalog schema 1 is a read-only compatibility input",
            "Catalog saves write explicit schema 2",
            "Legacy O3DE catalog envelopes",
            "loaded candidate remains schema 1",
            "Directly saving it is refused",
            "successful bound replacement followed by `BuildDocument`",
        ),
        "Catalog migration guide",
    )
    preview_readme = read_text(gem_root / "Preview/README.md")
    require_fragments(
        preview_readme,
        (
            "Loading and compatibility normalization retain schema 1",
            "A direct save of that load result is refused",
            "successful bound replacement followed by `BuildDocument`",
            "plain schema 2",
        ),
        "Developer Preview migration fixture documentation",
    )
    governance = read_text(repo_root / "docs/tainted-grail-sdk/GOVERNANCE_HARDENING.md")
    require_fragments(
        governance,
        (
            "governance values remain string-compatible",
            "current catalog saves",
            "write schema 2",
        ),
        "Governance compatibility contract",
    )
    docs_hub = read_text(repo_root / "docs/tainted-grail-sdk/README.md")
    require_fragments(
        docs_hub,
        (
            "Actor and Troop Editor Design",
            "ACTOR_TROOP_EDITOR_DESIGN.md",
            "approved population design and implementation history",
            "deterministic fixture",
            "registered Actor/Troop pane",
            "exact-head O3DE configure, build, compiled tests",
        ),
        "Documentation hub",
    )
def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_catalog_schema2(repo_root)
    except (OSError, CatalogSchema2ContractError) as exc:
        print(f"Tainted Grail catalog schema-2 validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail catalog schema-2 contract passed: retained schema-1 load state, "
        "validated schema-2 projection, schema-2-only atomic writing, population "
        "persistence, population test sources and compiled-target wiring, validators, "
        "and public documentation are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
