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

from validate_catalog_schema2 import (  # noqa: E402
    CatalogSchema2ContractError,
    validate_catalog_schema2,
)


class CatalogSchema2ValidatorTests(unittest.TestCase):
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

    def _append(self, relative_path: str, content: str) -> None:
        path = self.repo_root / relative_path
        path.write_text(path.read_text(encoding="utf-8") + content, encoding="utf-8")

    def _write_valid_contract(self) -> None:
        source_root = "Gems/TaintedGrailModdingSDK/Code/Source/"
        tools_root = "Gems/TaintedGrailModdingSDK/Tools/"

        self._write(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_core_files.cmake",
            "\n".join(
                (
                    "Source/PopulationModels.cpp",
                    "Source/PopulationModels.h",
                    "Source/CatalogDatabase.cpp",
                    "Source/CatalogDatabaseIntegrity.cpp",
                    "Source/CatalogDatabasePopulation.cpp",
                )
            ),
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_files.cmake",
            "Source/CatalogPersistenceService.cpp\n"
            "Source/CatalogPersistenceService.h\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "Tests/CatalogSchemaMigrationPersistenceTests.cpp\n"
            "Tests/PopulationAuthoringTests.cpp\n"
            "Tests/PopulationCatalogTests.cpp\n",
        )
        self._write(
            source_root + "PopulationModels.h",
            "LegacyCatalogSchemaVersion = 1\n"
            "PopulationCatalogSchemaVersion = 2\n"
            "Schema 1 remains a load-only migration input\n"
            "CurrentCatalogSchemaVersion =\nPopulationCatalogSchemaVersion;\n",
        )
        self._write(source_root + "PopulationModels.cpp", "population models\n")
        self._write(
            source_root + "FoundationModels.h",
            "m_schemaVersion = CurrentCatalogSchemaVersion\n"
            "AZStd::vector<PopulationActorProfile> m_actorProfiles\n"
            "AZStd::vector<PopulationTroopProfile> m_troopProfiles\n"
            "AZStd::vector<PopulationTroopMember> m_troopMembers\n",
        )
        self._write(
            source_root + "FoundationModels.cpp",
            'Field("ActorProfiles", &CatalogDocument::m_actorProfiles)\n'
            'Field("TroopProfiles", &CatalogDocument::m_troopProfiles)\n'
            'Field("TroopMembers", &CatalogDocument::m_troopMembers)\n'
            "m_schemaVersion == LegacyCatalogSchemaVersion\n"
            "m_schemaVersion == CurrentCatalogSchemaVersion\n",
        )
        self._write(
            source_root + "CatalogDatabase.cpp",
            "document.m_actorProfiles = m_populationActorProfiles\n"
            "document.m_troopProfiles = m_populationTroopProfiles\n"
            "document.m_troopMembers = m_populationTroopMembers\n"
            "document.m_schemaVersion = CurrentCatalogSchemaVersion\n",
        )
        for filename in (
            "CatalogDatabaseIntegrity.cpp",
            "CatalogDatabasePopulation.cpp",
            "CatalogDatabasePopulationPart1.inl",
            "CatalogDatabasePopulationPart2.inl",
            "CatalogDatabasePopulationPart3.inl",
        ):
            self._write(source_root + filename, "bounded Core population logic\n")
        self._write(source_root + "CatalogPersistenceService.h", "persistence service\n")
        self._write(
            source_root + "CatalogPersistenceService.cpp",
            "ReadCatalogSchemaVersion\n"
            "DetectCatalogSchemaVersion\n"
            "SerializePlainCatalog\n"
            'FindMember("SchemaVersion")\n'
            'FindMember("ClassData")\n'
            "Plain canonical catalog documents require an explicit SchemaVersion.\n"
            "Catalog SchemaVersion must be an unsigned 32-bit integer.\n"
            "Catalog schema version %u is unsupported; this editor supports schema 1 migration and schema 2.\n"
            "Canonical catalog saves require schema 2; schema 1 is a load-only migration input.\n"
            "document.m_schemaVersion != CurrentCatalogSchemaVersion\n"
            "settings.m_keepDefaults = true\n"
            "AZ::JsonSerialization::Store\n"
            "WriteJsonString\n"
            "QSaveFile file\n"
            "file.setDirectWriteFallback(false)\n"
            "file.cancelWriting()\n"
            "file.commit()\n"
            "DetectCatalogSchemaVersion(jsonResult.GetValue())\n"
            "document.m_schemaVersion = detectedSchemaVersion;\n"
            "PersistenceJsonUtils::LoadObjectFromFile(document, path)\n"
            "document.m_schemaVersion != detectedSchemaVersion\n"
            "Catalog SchemaVersion changed during deserialization; the document was not loaded.\n"
            "detectedSchemaVersion == LegacyCatalogSchemaVersion\n"
            "Catalog schema 1 cannot contain population collections.\n"
            "ValidatePersistedIdentity(document)\n"
            "NormalizeLegacyValidationHistory(document);\n"
            "NormalizeLegacyGovernanceState(document);\n"
            "return AZ::Success(AZStd::move(document));\n",
        )

        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Tests/CatalogSchemaMigrationPersistenceTests.cpp",
            "TEST_F(Fixture,\n"
            "    SchemaOnePreviewMigratesToSchemaTwoWithEmptyPopulationAndPreservesLegacyProjection)\n"
            "{\n"
            "    ASSERT_EQ(loaded.m_schemaVersion, LegacyCatalogSchemaVersion);\n"
            "    persistence.Save(loaded, root);\n"
            "    EXPECT_FALSE(unvalidatedSave.IsSuccess());\n"
            "    QFile::exists(path);\n"
            "    ReplaceFromBoundDocument(loaded);\n"
            "    BuildDocument(workspace);\n"
            "    EXPECT_EQ(migrated.m_schemaVersion, PopulationCatalogSchemaVersion);\n"
            "}\n"
            "\n    TEST_F(Fixture, PlainCatalogRejectsMissingMalformedAndFutureSchemaVersions) {}\n"
            "\n    TEST_F(Fixture,\n"
            "        LegacySerializationEnvelopeWithoutSchemaVersionMigratesAsSchemaOne)\n"
            "{\n"
            "    normalizedLegacy.m_schemaVersion;\n"
            "    LegacyCatalogSchemaVersion;\n"
            "    ReplaceFromBoundDocument(normalizedLegacy);\n"
            "    BuildDocument(workspace);\n"
            "    PopulationCatalogSchemaVersion;\n"
            "}\n"
            "\n    TEST_F(Fixture, SchemaOnePopulationCollectionsAreRejectedWithoutReplacement) {}\n"
            "\n    TEST_F(Fixture,\n"
            "        WriterRejectsSchemaOneAndWritesPlainSchemaTwoWithExplicitPopulationArrays)\n"
            "{\n"
            "    LegacyCatalogSchemaVersion;\n"
            "    persistence.Save(legacyDocument, root);\n"
            "    EXPECT_FALSE(legacySave.IsSuccess());\n"
            "    BuildDocument(workspace);\n"
            "    PopulationCatalogSchemaVersion;\n"
            "}\n"
            "\n    TEST_F(Fixture, SchemaTwoSaveLoadSaveIsByteStable) {}\n"
            "\n    TEST_F(Fixture, SchemaTwoSaveClearLoadAndReplacePreservesCanonicalState) {}\n"
            "\n    TEST_F(Fixture, MalformedSchemaTwoDocumentDoesNotReplacePublishedCatalog) {}\n"
            "\n    TEST_F(Fixture, PopulationCandidateSaveFailureDoesNotPublish) {}\n"
            '"ActorProfiles"\n"TroopProfiles"\n"TroopMembers"\n',
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationCatalogTests.cpp",
            "ClosedKindsAndRolesRoundTripAndRejectUnknownValues\n"
            "ActorProfilesAcceptExactResolvedAndUnresolvedTemplates\n"
            "ActorProfilesRejectKindsBindingsBoundsAndDuplicates\n"
            "TroopsAcceptExactResolvedAndUnresolvedLeaders\n"
            "TroopProfilesRejectKindsLeaderBindingsAndBounds\n"
            "MembersRejectBindingsBoundsRolesAndDuplicateConflicts\n"
            "CollectionsAreCanonicalAndDoNotMutateInputs\n"
            "CompleteCatalogIntegrityAcceptsExactComposition\n"
            "CompleteCatalogIntegrityRejectsEvidenceAndCompositionGaps\n"
            "PopulationUpsertsDoNotGrantGovernanceOrActionAuthority\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationAuthoringTests.cpp",
            "ActorProfilePersistsPublishesSnapshotAndNotifiesOnce\n"
            "TroopDefinitionIsAtomicOrderIndependentAndPreservesOmittedMembers\n"
            "StandaloneUpdatesPreserveCompleteCatalogIntegrity\n"
            "StructuralFailuresAndLinkOwnerMovesDoNotPublish\n"
            "ContextEvidenceAndPrimaryOwnershipFailClosed\n"
            "WrongProfileSourceAndCrossPackReferencesAreHandledExplicitly\n"
            "CandidateContextRejectsUnboundWorkspaceAndPackWithoutMutation\n"
            "PersistenceFailureLeavesPublishedCatalogUnchangedAndSilent\n"
            "ReflectPopulationPersistenceTypes\n"
            "AZ::JsonSystemComponent::Reflect\n"
            "AZ::ComponentApplication m_application\n"
            "service.ClearActivePack()\n"
            "validCandidate.IsSuccess()\n"
            "Unable to create the catalog directory\n",
        )
        self._write(
            tools_root + "validate_catalog_tests.py",
            '"Tests/CatalogSchemaMigrationPersistenceTests.cpp"\n'
            '"Tests/PopulationAuthoringTests.cpp"\n'
            '"Tests/PopulationCatalogTests.cpp"\n'
            '"Source/CatalogDatabaseIntegrity.cpp"\n'
            '"Source/CatalogDatabasePopulation.cpp"\n'
            '"Source/PopulationModels.cpp"\n'
            '"Source/FoundationPopulationService.cpp"\n'
            '"Source/PopulationAuthoringService.cpp"\n'
            "SchemaOnePreviewMigratesToSchemaTwoWithEmptyPopulationAndPreservesLegacyProjection\n"
            "SchemaTwoSaveLoadSaveIsByteStable\n"
            "PopulationCandidateSaveFailureDoesNotPublish\n"
            "TroopDefinitionIsAtomicOrderIndependentAndPreservesOmittedMembers\n"
            "PopulationUpsertsDoNotGrantGovernanceOrActionAuthority\n"
            "ReplacesMemberSet\n",
        )
        self._write(
            tools_root + "validate_foundation.py",
            'source_root.glob("*.inl")\n',
        )
        self._write(
            tools_root + "run_local_validation.py",
            '"validate_catalog_schema2.py"\n',
        )
        self._write(
            ".github/workflows/tainted-grail-sdk-foundation.yml",
            "Test catalog schema-2 validator\n"
            "test_validate_catalog_schema2.py\n"
            "Validate catalog schema-1 migration and schema-2 persistence\n"
            "validate_catalog_schema2.py\n",
        )
        self._write(
            "docs/tainted-grail-sdk/DATA_FORMATS.md",
            '## Workspace document\n"SchemaVersion": 1\n'
            '## Canonical catalog document\n"SchemaVersion": 2\n'
            '"ActorProfiles": []\n"TroopProfiles": []\n'
            '"TroopMembers": []\nSchema-1 migration is read-only and fail-closed\n'
            "Population actor profile\nPopulation troop profile\nPopulation troop member\n"
            "no population contract invokes FoA\n"
            "A loaded schema-1 candidate remains schema 1\n"
            "Directly saving that load result is refused\n"
            "successful bound replacement followed by `BuildDocument` produces a schema-2 document\n"
            "## Catalog record\n",
        )
        self._write(
            "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md",
            "Status: active implementation\n"
            "actor/troop contracts, reflection\n"
            "CatalogDatabase validation, queries\n"
            "schema-1 migration, schema-2-only writing\n"
            "4. **Complete** \u2014 Framework evidence-bound authoring, atomic troop-definition bootstrap\n"
            "5. **Complete** \u2014 Core and Framework positive/negative population-authoring test sources "
            "and compiled-target wiring\n"
            "6. **Complete** \u2014 immutable population action-lane derivation, "
            "Actor and Troop Editor pane, and lifecycle registration\n"
            "7. **Next** \u2014 deterministic synthetic population fixture\n"
            "do not claim that an exact-head compiled test run exists\n"
            "loaded candidate remains schema 1\n"
            "direct save is refused\n"
            "successful bound replacement and `BuildDocument`\n",
        )
        self._write(
            "ROADMAP.md",
            "### Actors and population\n"
            "Status: active development. Core contracts, CatalogDatabase integration\n"
            "evidence-bound Framework candidate publication and positive/negative Core and\n"
            "Framework population-authoring test sources with compiled-target wiring are implemented\n"
            "The immutable seven-lane population action contract and registered Actor and Troop Editor pane are also implemented\n"
            "The deterministic synthetic fixture and complete local-validation integration are next\n",
        )
        self._write(
            "docs/tainted-grail-sdk/CATALOG_GUIDE.md",
            "Catalog schema 1 is a read-only compatibility input\n"
            "Catalog saves write explicit schema 2\nLegacy O3DE catalog envelopes\n"
            "loaded candidate remains schema 1\n"
            "Directly saving it is refused\n"
            "successful bound replacement followed by `BuildDocument`\n",
        )
        self._write(
            "Gems/TaintedGrailModdingSDK/Preview/README.md",
            "Loading and compatibility normalization retain schema 1\n"
            "A direct save of that load result is refused\n"
            "successful bound replacement followed by `BuildDocument`\n"
            "plain schema 2\n",
        )
        self._write(
            "docs/tainted-grail-sdk/GOVERNANCE_HARDENING.md",
            "governance values remain string-compatible\ncurrent catalog saves write schema 2\n",
        )
        self._write(
            "docs/tainted-grail-sdk/README.md",
            "Actor and Troop Editor Design ACTOR_TROOP_EDITOR_DESIGN.md "
            "completed Core, schema-2 persistence, Framework candidate-publication, "
            "population-authoring test-source, immutable action-lane, and registered Actor/Troop pane units\n",
        )

    def test_valid_contract_passes(self) -> None:
        validate_catalog_schema2(self.repo_root)

    def test_rejects_current_schema_bound_to_legacy(self) -> None:
        path = self.repo_root / "Gems/TaintedGrailModdingSDK/Code/Source/PopulationModels.h"
        text = path.read_text(encoding="utf-8").replace(
            "CurrentCatalogSchemaVersion =\nPopulationCatalogSchemaVersion;",
            "CurrentCatalogSchemaVersion = LegacyCatalogSchemaVersion;\n"
            "PopulationCatalogSchemaVersion;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(CatalogSchema2ContractError, "must not remain bound"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_stale_population_test_implementation_status(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
        text = path.read_text(encoding="utf-8").replace(
            "5. **Complete** \u2014 Core and Framework positive/negative population-authoring test sources",
            "5. **Pending** \u2014 Core and Framework positive/negative population-authoring test sources",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(CatalogSchema2ContractError, r"5\. \*\*Complete"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_stale_editor_complete_status(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
        text = path.read_text(encoding="utf-8").replace(
            "6. **Complete** \u2014 immutable population action-lane derivation",
            "6. **Pending** \u2014 immutable population action-lane derivation",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(CatalogSchema2ContractError, r"6\. \*\*Complete"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_stale_replacement_semantics(self) -> None:
        self._append(
            "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationAuthoringTests.cpp",
            "ReplacesMemberSet\n",
        )
        with self.assertRaisesRegex(CatalogSchema2ContractError, "ReplacesMemberSet"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_missing_population_persistence_fixture(self) -> None:
        path = (
            self.repo_root
            / "Gems/TaintedGrailModdingSDK/Code/Tests/PopulationAuthoringTests.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "ReflectPopulationPersistenceTypes",
            "missing population persistence reflection",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError,
            "ReflectPopulationPersistenceTypes",
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_non_atomic_catalog_writer(self) -> None:
        path = self.repo_root / "Gems/TaintedGrailModdingSDK/Code/Source/CatalogPersistenceService.cpp"
        path.write_text(
            path.read_text(encoding="utf-8").replace(
                "file.setDirectWriteFallback(false)", "direct fallback removed"
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(CatalogSchema2ContractError, "setDirectWriteFallback"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_schema_detection_after_deserialization(self) -> None:
        path = self.repo_root / "Gems/TaintedGrailModdingSDK/Code/Source/CatalogPersistenceService.cpp"
        text = path.read_text(encoding="utf-8")
        detection = "DetectCatalogSchemaVersion(jsonResult.GetValue())\n"
        text = text.replace(detection, "").replace(
            "PersistenceJsonUtils::LoadObjectFromFile(document, path)\n",
            "PersistenceJsonUtils::LoadObjectFromFile(document, path)\n" + detection,
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "ordered fragment|required ordering"
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_premature_schema_two_promotion_in_persistence(self) -> None:
        path = (
            self.repo_root
            / "Gems/TaintedGrailModdingSDK/Code/Source/CatalogPersistenceService.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "NormalizeLegacyGovernanceState(document);\n",
            "NormalizeLegacyGovernanceState(document);\n"
            "document.m_schemaVersion = PopulationCatalogSchemaVersion;\n",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "exactly once from detectedSchemaVersion"
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_missing_pre_validation_schema_one_assertion(self) -> None:
        path = (
            self.repo_root
            / "Gems/TaintedGrailModdingSDK/Code/Tests/CatalogSchemaMigrationPersistenceTests.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "ASSERT_EQ(loaded.m_schemaVersion, LegacyCatalogSchemaVersion);",
            "ASSERT_EQ(loaded.m_schemaVersion, PopulationCatalogSchemaVersion);",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "Schema-1 preview migration|Schema-1 load, refusal"
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_build_document_before_bound_replacement(self) -> None:
        path = (
            self.repo_root
            / "Gems/TaintedGrailModdingSDK/Code/Tests/CatalogSchemaMigrationPersistenceTests.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "    ReplaceFromBoundDocument(loaded);\n"
            "    BuildDocument(workspace);\n",
            "    BuildDocument(workspace);\n"
            "    ReplaceFromBoundDocument(loaded);\n",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "Schema-1 preview migration|Schema-1 load, refusal"
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_missing_direct_schema_one_save_refusal(self) -> None:
        path = (
            self.repo_root
            / "Gems/TaintedGrailModdingSDK/Code/Tests/CatalogSchemaMigrationPersistenceTests.cpp"
        )
        text = path.read_text(encoding="utf-8").replace(
            "    EXPECT_FALSE(unvalidatedSave.IsSuccess());\n",
            "    EXPECT_TRUE(unvalidatedSave.IsSuccess());\n",
            1,
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "Schema-1 preview migration|Schema-1 load, refusal"
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_documentation_that_promotes_during_load(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/DATA_FORMATS.md"
        text = path.read_text(encoding="utf-8").replace(
            "A loaded schema-1 candidate remains schema 1",
            "A loaded schema-1 candidate is promoted to schema 2",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "Public data-format contract"
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_missing_malformed_input_coverage(self) -> None:
        path = (
            self.repo_root
            / "Gems/TaintedGrailModdingSDK/Code/Tests/CatalogSchemaMigrationPersistenceTests.cpp"
        )
        path.write_text(
            path.read_text(encoding="utf-8").replace(
                "PlainCatalogRejectsMissingMalformedAndFutureSchemaVersions", "removed"
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(CatalogSchema2ContractError, "PlainCatalogRejects"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_runtime_coupling_in_core_population(self) -> None:
        self._append(
            "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabasePopulationPart2.inl",
            "QProcess\n",
        )
        with self.assertRaisesRegex(CatalogSchema2ContractError, "QProcess"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_missing_local_validation_integration(self) -> None:
        self._write(
            "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py",
            "validator removed\n",
        )
        with self.assertRaisesRegex(CatalogSchema2ContractError, "exactly 1"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_conditional_schema_one_document_builder(self) -> None:
        self._append(
            "Gems/TaintedGrailModdingSDK/Code/Source/CatalogDatabase.cpp",
            "hasPopulationData ? PopulationCatalogSchemaVersion : LegacyCatalogSchemaVersion\n",
        )
        with self.assertRaisesRegex(CatalogSchema2ContractError, "hasPopulationData"):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_swapped_workspace_and_catalog_schema_versions(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/DATA_FORMATS.md"
        text = path.read_text(encoding="utf-8")
        text = text.replace('"SchemaVersion": 1', '"SchemaVersion": swap', 1)
        text = text.replace('"SchemaVersion": 2', '"SchemaVersion": 1', 1)
        text = text.replace('"SchemaVersion": swap', '"SchemaVersion": 2', 1)
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError, "Workspace data-format section"
        ):
            validate_catalog_schema2(self.repo_root)


if __name__ == "__main__":
    unittest.main()
