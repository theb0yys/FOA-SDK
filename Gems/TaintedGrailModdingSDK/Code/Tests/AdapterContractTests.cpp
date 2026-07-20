/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterCompatibilityService.h"
#include "AdapterContractRegistry.h"
#include "CatalogDatabase.h"
#include "ResearchContractValidation.h"
#include "SourceEvidenceRegistry.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        GameProfile MakeProfile()
        {
            GameProfile profile;
            profile.m_profileId = "profile.adapter";
            profile.m_gameVersion = "game.adapter";
            profile.m_branch = "branch.adapter";
            profile.m_runtimeTarget = "Mono";
            return profile;
        }

        WorkspaceModel MakeWorkspace()
        {
            WorkspaceModel workspace;
            workspace.m_workspaceId = "workspace.adapter";
            workspace.m_activeGameProfileId = "profile.adapter";
            workspace.m_gameProfiles = { MakeProfile() };
            return workspace;
        }

        PackManifest MakePack(AZStd::string requiredVersion = "1.2.0")
        {
            PackManifest pack;
            pack.m_packId = "owner.adapter-pack";
            pack.m_requiredAdapterVersion = AZStd::move(requiredVersion);
            return pack;
        }

        SourceRecord MakeSource()
        {
            SourceRecord source;
            source.m_sourceId = "source.adapter";
            source.m_title = "Adapter contract test source";
            source.m_sourceKind = "structured_test_fixture";
            source.m_locator = "research/adapter-contract.json";
            source.m_fingerprint =
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            source.m_profileId = "profile.adapter";
            source.m_gameVersion = "game.adapter";
            source.m_branch = "branch.adapter";
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "adapter-contract-tests";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "adapter.contract.tests";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-01-01T00:00:00Z";
            source.m_importedAt = "2026-01-01T00:00:01Z";
            source.m_mediaType = "application/json";
            source.m_byteSize = 512;
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeEvidence(
            AZStd::string evidenceId,
            AZStd::string subjectRef)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = "source.adapter";
            evidence.m_sourceFingerprint =
                "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            evidence.m_profileId = "profile.adapter";
            evidence.m_gameVersion = "game.adapter";
            evidence.m_branch = "branch.adapter";
            evidence.m_subjectRef = AZStd::move(subjectRef);
            evidence.m_claim = "Project-owned adapter contract test evidence.";
            evidence.m_evidenceKind = "structured_record";
            evidence.m_confidence = "documented";
            evidence.m_locator = "research/adapter-contract.json";
            evidence.m_recordPath = "/records/0";
            evidence.m_extractedAt = "2026-01-01T00:00:00Z";
            return evidence;
        }

        AdapterDeclaration MakeDeclaration(
            AdapterCapability capability,
            AZStd::string version = "1.3.0")
        {
            AdapterDeclaration declaration;
            declaration.m_adapterId = "owner.adapter";
            declaration.m_displayName = "Adapter Contract Test";
            declaration.m_version = AZStd::move(version);
            declaration.m_runtimeTargets = { "Mono" };
            declaration.m_identityEvidenceIds = { "evidence.adapter.identity" };

            AdapterCapabilityDeclaration capabilityDeclaration;
            capabilityDeclaration.m_capability = capability;
            capabilityDeclaration.m_evidenceIds = { "evidence.adapter.capability" };
            declaration.m_capabilities = { capabilityDeclaration };
            return declaration;
        }

        SourceEvidenceRegistry MakeAdapterEvidenceRegistry(AdapterCapability capability)
        {
            SourceEvidenceRegistry registry;
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterSource(MakeSource(), &error));
            EXPECT_TRUE(registry.RegisterEvidence(
                MakeEvidence("evidence.adapter.identity", "adapter:owner.adapter"),
                &error));
            EXPECT_TRUE(registry.RegisterEvidence(
                MakeEvidence(
                    "evidence.adapter.capability",
                    "adapter:owner.adapter:capability:" + ToString(capability)),
                &error));
            return registry;
        }

        CatalogRecord MakeItem(AZStd::vector<AZStd::string> allowedUsages)
        {
            CatalogRecord record;
            record.m_recordId = "item.adapter";
            record.m_ownerPackId = "owner.adapter-pack";
            record.m_domain = "economy";
            record.m_recordKind = "item";
            record.m_subjectRef = "subject:adapter:item";
            record.m_identityKind = "source_scoped";
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "low";
            record.m_validationState = "validated";
            record.m_stalenessState = "current";
            record.m_allowedUsages = AZStd::move(allowedUsages);
            record.m_evidenceIds = { "evidence.item" };
            return record;
        }

        CatalogValidationEvent MakeValidation()
        {
            CatalogValidationEvent validation;
            validation.m_validationId = "validation.item";
            validation.m_subjectKind = "record";
            validation.m_subjectId = "item.adapter";
            validation.m_state = "validated";
            validation.m_method = "adapter-contract-test";
            validation.m_validator = "test.validator";
            validation.m_checkedAt = "2026-01-01T00:00:00Z";
            validation.m_profileId = "profile.adapter";
            validation.m_gameVersion = "game.adapter";
            validation.m_branch = "branch.adapter";
            validation.m_evidenceIds = { "evidence.item" };
            return validation;
        }

        CatalogGovernanceEvent MakePermission()
        {
            CatalogGovernanceEvent event;
            event.m_eventId = "governance.item.permission";
            event.m_subjectKind = "record";
            event.m_subjectId = "item.adapter";
            event.m_axis = "permission";
            event.m_newValue = "allow";
            event.m_usage = "existing_item_grant";
            event.m_evidenceIds = { "evidence.item" };
            event.m_validationIds = { "validation.item" };
            event.m_reviewer = "test.reviewer";
            event.m_decidedAt = "2026-01-01T00:00:01Z";
            return event;
        }

        const AdapterCapabilityMatrixRow* FindRow(
            const AdapterCapabilityMatrix& matrix,
            const AZStd::string& capability)
        {
            for (const AdapterCapabilityMatrixRow& row : matrix.m_rows)
            {
                if (row.m_capability == capability)
                {
                    return &row;
                }
            }
            return nullptr;
        }
    } // namespace

    TEST(TaintedGrailAdapterContractsTests, SemanticVersionsAndTypedCapabilitiesAreStrict)
    {
        AdapterSemanticVersion version;
        EXPECT_TRUE(TryParseAdapterSemanticVersion("1.2.3-alpha.1+build.9", version));
        EXPECT_FALSE(TryParseAdapterSemanticVersion("1.02.3", version));
        EXPECT_FALSE(TryParseAdapterSemanticVersion("1.2.3-01", version));
        EXPECT_FALSE(IsStrictSemanticVersion(
            AZStd::string(300, '9') + ".1.0"));
        EXPECT_TRUE(IsStrictUtcTimestamp("2026-07-20T10:00:00Z"));
        EXPECT_FALSE(IsStrictUtcTimestamp("2026-07-20T10:00:00.001Z"));
        EXPECT_TRUE(IsAdapterVersionCompatible("1.2.0", "1.5.0"));
        EXPECT_FALSE(IsAdapterVersionCompatible("1.2.0", "2.0.0"));
        EXPECT_TRUE(IsAdapterVersionCompatible("0.3.1", "0.3.9"));
        EXPECT_FALSE(IsAdapterVersionCompatible("0.3.1", "0.4.0"));

        const AdapterCapability capabilities[] = {
            AdapterCapability::ItemGrant,
            AdapterCapability::RecipeLearn,
            AdapterCapability::RecipeAppend,
            AdapterCapability::CustomItemRegistration,
            AdapterCapability::CustomRecipeRegistration,
            AdapterCapability::VendorMutation,
            AdapterCapability::LootMutation,
            AdapterCapability::RewardMutation,
            AdapterCapability::Persistence,
            AdapterCapability::Cleanup,
            AdapterCapability::Rollback,
        };
        for (AdapterCapability capability : capabilities)
        {
            AdapterCapability parsed;
            EXPECT_TRUE(TryParseAdapterCapability(ToString(capability), parsed));
            EXPECT_EQ(parsed, capability);
        }

        AdapterContractRegistry registry;
        AZStd::string error;
        EXPECT_TRUE(registry.RegisterDeclaration(
            MakeDeclaration(AdapterCapability::ItemGrant),
            &error));
        EXPECT_FALSE(registry.RegisterDeclaration(
            MakeDeclaration(AdapterCapability::ItemGrant),
            &error));
    }

    TEST(TaintedGrailAdapterContractsTests, EmptyRegistryAndMissingCapabilitiesFailClosed)
    {
        AdapterCompatibilityService service;
        AdapterContractRegistry emptyRegistry;
        SourceEvidenceRegistry sourceRegistry;
        CatalogDatabase catalog;

        const AdapterCapabilityMatrix emptyMatrix = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack() },
            emptyRegistry,
            sourceRegistry,
            catalog,
            {});
        EXPECT_EQ(emptyMatrix.m_rowCount, 11);
        EXPECT_EQ(emptyMatrix.m_unsupportedCount, 11);

        AdapterContractRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterDeclaration(
            MakeDeclaration(AdapterCapability::ItemGrant),
            &error));
        const AdapterCapabilityMatrix declaredMatrix = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack() },
            registry,
            MakeAdapterEvidenceRegistry(AdapterCapability::ItemGrant),
            catalog,
            {});
        EXPECT_EQ(declaredMatrix.m_unsupportedCount, 10);
        const AdapterCapabilityMatrixRow* itemGrant = FindRow(declaredMatrix, "item_grant");
        ASSERT_NE(itemGrant, nullptr);
        EXPECT_EQ(itemGrant->m_status, "permission_missing");
    }

    TEST(TaintedGrailAdapterContractsTests, VersionMismatchPrecedesPermissionAndProofChecks)
    {
        AdapterContractRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterDeclaration(
            MakeDeclaration(AdapterCapability::ItemGrant, "1.9.0"),
            &error));

        AdapterCompatibilityService service;
        const AdapterCapabilityMatrix matrix = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack("2.0.0") },
            registry,
            MakeAdapterEvidenceRegistry(AdapterCapability::ItemGrant),
            {},
            {});
        const AdapterCapabilityMatrixRow* row = FindRow(matrix, "item_grant");
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(row->m_status, "version_mismatch");
        EXPECT_EQ(matrix.m_versionMismatchCount, 1);
    }

    TEST(TaintedGrailAdapterContractsTests, PermissionMissingAndProofMissingRemainDistinct)
    {
        AdapterContractRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterDeclaration(
            MakeDeclaration(AdapterCapability::ItemGrant),
            &error));
        SourceEvidenceRegistry sourceRegistry =
            MakeAdapterEvidenceRegistry(AdapterCapability::ItemGrant);
        ASSERT_TRUE(sourceRegistry.RegisterEvidence(
            MakeEvidence("evidence.item", "subject:adapter:item"),
            &error));

        CatalogDatabase catalog;
        ASSERT_TRUE(catalog.InsertNew(MakeItem({}), &error));
        EconomyItemProfile itemProfile;
        itemProfile.m_recordId = "item.adapter";
        itemProfile.m_evidenceIds = { "evidence.item" };
        ASSERT_TRUE(catalog.UpsertEconomyItem(itemProfile, &error));

        AdapterCompatibilityService service;
        AdapterCapabilityMatrix matrix = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack() },
            registry,
            sourceRegistry,
            catalog,
            {});
        const AdapterCapabilityMatrixRow* row = FindRow(matrix, "item_grant");
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(row->m_status, "permission_missing");

        catalog.Clear();
        ASSERT_TRUE(catalog.InsertNew(MakeItem({ "existing_item_grant" }), &error));
        ASSERT_TRUE(catalog.UpsertEconomyItem(itemProfile, &error));
        matrix = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack() },
            registry,
            sourceRegistry,
            catalog,
            {});
        row = FindRow(matrix, "item_grant");
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(row->m_status, "proof_missing");
    }

    TEST(TaintedGrailAdapterContractsTests, SupportedResultIsDeterministicAndDoesNotMutateInputs)
    {
        AdapterContractRegistry registry;
        AZStd::string error;
        ASSERT_TRUE(registry.RegisterDeclaration(
            MakeDeclaration(AdapterCapability::ItemGrant),
            &error));
        SourceEvidenceRegistry sourceRegistry =
            MakeAdapterEvidenceRegistry(AdapterCapability::ItemGrant);
        ASSERT_TRUE(sourceRegistry.RegisterEvidence(
            MakeEvidence("evidence.item", "subject:adapter:item"),
            &error));

        CatalogDatabase catalog;
        ASSERT_TRUE(catalog.InsertNew(MakeItem({ "existing_item_grant" }), &error));
        EconomyItemProfile itemProfile;
        itemProfile.m_recordId = "item.adapter";
        itemProfile.m_evidenceIds = { "evidence.item" };
        ASSERT_TRUE(catalog.UpsertEconomyItem(itemProfile, &error));
        const WorkspaceModel workspace = MakeWorkspace();
        const GameProfile* profile = workspace.FindActiveGameProfile();
        ASSERT_NE(profile, nullptr);
        ASSERT_TRUE(catalog.AddValidationEventBound(
            MakeValidation(), workspace, *profile, sourceRegistry, &error));
        ASSERT_TRUE(catalog.AddGovernanceEventBound(
            MakePermission(), workspace, *profile, sourceRegistry, &error));

        const size_t declarationCountBefore = registry.GetDeclarations().size();
        const size_t sourceCountBefore = sourceRegistry.GetSources().size();
        const size_t evidenceCountBefore = sourceRegistry.GetEvidence().size();
        const size_t recordCountBefore = catalog.GetRecords().size();
        const size_t validationCountBefore = catalog.GetValidationHistory().size();
        const size_t governanceCountBefore = catalog.GetGovernanceHistory().size();

        AdapterCompatibilityService service;
        const AdapterCapabilityMatrix first = service.BuildCapabilityMatrix(
            workspace,
            { MakePack() },
            registry,
            sourceRegistry,
            catalog,
            {});
        BlockerRecord blocker;
        blocker.m_blockerId = "blocker.item";
        blocker.m_subjectRef = "subject:adapter:item";
        blocker.m_severity = "error";
        blocker.m_status = "open";
        blocker.m_reason = "Item grant safety review remains open.";
        blocker.m_affectedUsages = { "existing_item_grant" };
        const AdapterCapabilityMatrix blocked = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack() },
            registry,
            sourceRegistry,
            catalog,
            { blocker });
        const AdapterCapabilityMatrixRow* blockedRow = FindRow(blocked, "item_grant");
        ASSERT_NE(blockedRow, nullptr);
        EXPECT_EQ(blockedRow->m_status, "proof_missing");

        const AdapterCapabilityMatrix second = service.BuildCapabilityMatrix(
            MakeWorkspace(),
            { MakePack() },
            registry,
            sourceRegistry,
            catalog,
            {});
        const AdapterCapabilityMatrixRow* row = FindRow(first, "item_grant");
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(row->m_status, "supported");
        EXPECT_EQ(first.m_supportedCount, 1);
        ASSERT_EQ(first.m_rows.size(), second.m_rows.size());
        for (size_t index = 0; index < first.m_rows.size(); ++index)
        {
            EXPECT_EQ(first.m_rows[index].m_packId, second.m_rows[index].m_packId);
            EXPECT_EQ(first.m_rows[index].m_adapterId, second.m_rows[index].m_adapterId);
            EXPECT_EQ(first.m_rows[index].m_capability, second.m_rows[index].m_capability);
            EXPECT_EQ(first.m_rows[index].m_status, second.m_rows[index].m_status);
        }

        EXPECT_EQ(registry.GetDeclarations().size(), declarationCountBefore);
        EXPECT_EQ(sourceRegistry.GetSources().size(), sourceCountBefore);
        EXPECT_EQ(sourceRegistry.GetEvidence().size(), evidenceCountBefore);
        EXPECT_EQ(catalog.GetRecords().size(), recordCountBefore);
        EXPECT_EQ(catalog.GetValidationHistory().size(), validationCountBefore);
        EXPECT_EQ(catalog.GetGovernanceHistory().size(), governanceCountBefore);
    }
} // namespace TaintedGrailModdingSDK
