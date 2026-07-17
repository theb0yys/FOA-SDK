/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceService.h"

#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        WorkspaceModel MakeWorkspace()
        {
            GameProfile profile;
            profile.m_profileId = "foa.test";
            profile.m_displayName = "FoA Test";
            profile.m_installPath = "/test/foa";
            profile.m_gameVersion = "1.0-test";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_managedAssembliesPath = "/test/foa/Managed";

            WorkspaceModel workspace;
            workspace.m_workspaceId = "test.workspace";
            workspace.m_rootPath = "/test/workspace";
            workspace.m_activeGameProfileId = profile.m_profileId;
            workspace.m_gameProfiles.push_back(profile);
            return workspace;
        }

        SourceEvidenceRegistry MakeRegistry(const AZStd::string& subjectRef)
        {
            SourceRecord source;
            source.m_sourceId = "source.test";
            source.m_fingerprint = "sha256:test";
            source.m_profileId = "foa.test";
            source.m_gameVersion = "1.0-test";
            source.m_branch = "mono";
            source.m_runtimeTarget = "Mono";
            source.m_importerId = "test.importer";
            source.m_importerVersion = "1.0.0";

            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence.test";
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = subjectRef;
            evidence.m_claim = "Test claim";

            SourceEvidenceRegistry registry;
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterSource(source, &error));
            EXPECT_TRUE(registry.RegisterEvidence(evidence, &error));
            return registry;
        }

        CatalogRecord MakeRecord(
            const AZStd::string& recordId,
            const AZStd::string& subjectRef,
            const AZStd::string& nativeRef)
        {
            CatalogRecord record;
            record.m_recordId = recordId;
            record.m_domain = "test";
            record.m_recordKind = "test-record";
            record.m_subjectRef = subjectRef;
            record.m_nativeRefExact = nativeRef;
            record.m_identityKind = "native";
            record.m_researchStage = "S5";
            record.m_confidence = "documented";
            record.m_operationalRisk = "medium";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { "evidence.test" };
            return record;
        }

        CatalogValidationApplyResult ValidateSubject(
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const CatalogGovernanceService& service,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& registry,
            const CatalogDatabase& catalog)
        {
            CatalogValidationRequest request;
            request.m_subjectKind = subjectKind;
            request.m_subjectId = subjectId;
            request.m_state = "validated";
            request.m_method = "unit-test";
            request.m_evidenceIds = { "evidence.test" };
            request.m_validator = "test-reviewer";

            AZ::Outcome<CatalogValidationApplyResult, AZStd::string> result = service.ApplyValidation(
                request,
                workspace,
                registry,
                catalog);
            EXPECT_TRUE(result.IsSuccess());
            return result.IsSuccess() ? result.TakeValue() : CatalogValidationApplyResult{};
        }
    } // namespace

    TEST(TaintedGrailCatalogGovernanceTests, ValidationDoesNotGrantUsagePermission)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const SourceEvidenceRegistry registry = MakeRegistry("subject:test");
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("record.test", "subject:test", "native:test"), &error));

        const CatalogGovernanceService service;
        const CatalogValidationApplyResult validated = ValidateSubject(
            "record", "record.test", service, workspace, registry, catalog);

        const CatalogRecord* original = catalog.FindByRecordId("record.test");
        ASSERT_NE(original, nullptr);
        EXPECT_EQ(original->m_validationState, "unvalidated");
        EXPECT_TRUE(catalog.GetValidationHistory().empty());

        const CatalogRecord* record = validated.m_catalog.FindByRecordId("record.test");
        ASSERT_NE(record, nullptr);
        EXPECT_EQ(record->m_validationState, "validated");
        EXPECT_TRUE(record->m_allowedUsages.empty());
        EXPECT_EQ(validated.m_catalog.GetValidationHistory().size(), 1);
    }

    TEST(TaintedGrailCatalogGovernanceTests, PermissionRequiresCurrentStateAndValidatedProof)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const SourceEvidenceRegistry registry = MakeRegistry("subject:test");
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("record.test", "subject:test", "native:test"), &error));

        const CatalogGovernanceService service;
        CatalogValidationApplyResult validated = ValidateSubject(
            "record", "record.test", service, workspace, registry, catalog);
        catalog = validated.m_catalog;

        CatalogGovernanceRequest permission;
        permission.m_subjectKind = "record";
        permission.m_subjectId = "record.test";
        permission.m_axis = "permission";
        permission.m_value = "allow";
        permission.m_usage = "display_only";
        permission.m_evidenceIds = { "evidence.test" };
        permission.m_validationIds = { validated.m_event.m_validationId };
        permission.m_reviewer = "test-reviewer";
        EXPECT_FALSE(service.ApplyDecision(permission, workspace, registry, catalog).IsSuccess());

        CatalogGovernanceRequest current;
        current.m_subjectKind = "record";
        current.m_subjectId = "record.test";
        current.m_axis = "staleness";
        current.m_value = "current";
        current.m_evidenceIds = { "evidence.test" };
        current.m_reviewer = "test-reviewer";
        AZ::Outcome<CatalogGovernanceApplyResult, AZStd::string> currentResult =
            service.ApplyDecision(current, workspace, registry, catalog);
        ASSERT_TRUE(currentResult.IsSuccess());
        catalog = currentResult.TakeValue().m_catalog;

        AZ::Outcome<CatalogGovernanceApplyResult, AZStd::string> permissionResult =
            service.ApplyDecision(permission, workspace, registry, catalog);
        ASSERT_TRUE(permissionResult.IsSuccess());
        catalog = permissionResult.TakeValue().m_catalog;

        const CatalogRecord* record = catalog.FindByRecordId("record.test");
        ASSERT_NE(record, nullptr);
        ASSERT_EQ(record->m_allowedUsages.size(), 1);
        EXPECT_EQ(record->m_allowedUsages.front(), "display_only");
    }

    TEST(TaintedGrailCatalogGovernanceTests, StaleDecisionRevokesAllowedUsage)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const SourceEvidenceRegistry registry = MakeRegistry("subject:test");
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("record.test", "subject:test", "native:test"), &error));

        const CatalogGovernanceService service;
        CatalogValidationApplyResult validated = ValidateSubject(
            "record", "record.test", service, workspace, registry, catalog);
        catalog = validated.m_catalog;

        CatalogGovernanceRequest current;
        current.m_subjectKind = "record";
        current.m_subjectId = "record.test";
        current.m_axis = "staleness";
        current.m_value = "current";
        current.m_evidenceIds = { "evidence.test" };
        current.m_reviewer = "test-reviewer";
        auto currentResult = service.ApplyDecision(current, workspace, registry, catalog);
        ASSERT_TRUE(currentResult.IsSuccess());
        catalog = currentResult.TakeValue().m_catalog;

        CatalogGovernanceRequest permission;
        permission.m_subjectKind = "record";
        permission.m_subjectId = "record.test";
        permission.m_axis = "permission";
        permission.m_value = "allow";
        permission.m_usage = "display_only";
        permission.m_evidenceIds = { "evidence.test" };
        permission.m_validationIds = { validated.m_event.m_validationId };
        permission.m_reviewer = "test-reviewer";
        auto permissionResult = service.ApplyDecision(permission, workspace, registry, catalog);
        ASSERT_TRUE(permissionResult.IsSuccess());
        catalog = permissionResult.TakeValue().m_catalog;

        CatalogGovernanceRequest stale = current;
        stale.m_value = "stale";
        auto staleResult = service.ApplyDecision(stale, workspace, registry, catalog);
        ASSERT_TRUE(staleResult.IsSuccess());
        catalog = staleResult.TakeValue().m_catalog;

        const CatalogRecord* record = catalog.FindByRecordId("record.test");
        ASSERT_NE(record, nullptr);
        EXPECT_TRUE(record->m_allowedUsages.empty());
        EXPECT_NE(
            AZStd::find(record->m_forbiddenUsages.begin(), record->m_forbiddenUsages.end(), "stale_or_unverified"),
            record->m_forbiddenUsages.end());
    }

    TEST(TaintedGrailCatalogGovernanceTests, SupersessionRevokesUsageAndRecordsReplacement)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const SourceEvidenceRegistry registry = MakeRegistry("subject:test");
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("record.test", "subject:test", "native:test"), &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeRecord("record.replacement", "subject:test", "native:replacement"),
            &error));

        const CatalogGovernanceService service;
        CatalogGovernanceRequest supersession;
        supersession.m_subjectKind = "record";
        supersession.m_subjectId = "record.test";
        supersession.m_axis = "supersession";
        supersession.m_value = "record.replacement";
        supersession.m_evidenceIds = { "evidence.test" };
        supersession.m_reviewer = "test-reviewer";
        auto result = service.ApplyDecision(supersession, workspace, registry, catalog);
        ASSERT_TRUE(result.IsSuccess());

        const CatalogRecord* original = catalog.FindByRecordId("record.test");
        ASSERT_NE(original, nullptr);
        EXPECT_TRUE(original->m_supersededByRecordId.empty());

        const CatalogRecord* record = result.GetValue().m_catalog.FindByRecordId("record.test");
        ASSERT_NE(record, nullptr);
        EXPECT_EQ(record->m_supersededByRecordId, "record.replacement");
        EXPECT_EQ(record->m_stalenessState, "stale");
        EXPECT_TRUE(record->m_allowedUsages.empty());
    }

    TEST(TaintedGrailCatalogGovernanceTests, RelationshipValidationAndPermissionUseSameProofGates)
    {
        const WorkspaceModel workspace = MakeWorkspace();
        const SourceEvidenceRegistry registry = MakeRegistry("subject:test");
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("record.test", "subject:test", "native:test"), &error));
        ASSERT_TRUE(catalog.InsertNew(MakeRecord("record.child", "subject:child", "native:child"), &error));

        CatalogRelationship relationship;
        relationship.m_relationshipId = "relationship.test-child";
        relationship.m_fromRecordId = "record.test";
        relationship.m_toRecordId = "record.child";
        relationship.m_relationshipKind = "contains";
        relationship.m_evidenceIds = { "evidence.test" };
        relationship.m_researchStage = "S4";
        relationship.m_confidence = "documented";
        relationship.m_operationalRisk = "medium";
        relationship.m_validationState = "unvalidated";
        relationship.m_stalenessState = "unknown";
        relationship.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
        ASSERT_TRUE(catalog.UpsertRelationship(relationship, &error));

        const CatalogGovernanceService service;
        CatalogValidationApplyResult validated = ValidateSubject(
            "relationship", relationship.m_relationshipId, service, workspace, registry, catalog);
        catalog = validated.m_catalog;

        const CatalogRelationship* validatedRelationship = catalog.FindRelationshipById(relationship.m_relationshipId);
        ASSERT_NE(validatedRelationship, nullptr);
        EXPECT_EQ(validatedRelationship->m_validationState, "validated");
        EXPECT_TRUE(validatedRelationship->m_allowedUsages.empty());

        CatalogGovernanceRequest current;
        current.m_subjectKind = "relationship";
        current.m_subjectId = relationship.m_relationshipId;
        current.m_axis = "staleness";
        current.m_value = "current";
        current.m_evidenceIds = { "evidence.test" };
        current.m_reviewer = "test-reviewer";
        auto currentResult = service.ApplyDecision(current, workspace, registry, catalog);
        ASSERT_TRUE(currentResult.IsSuccess());
        catalog = currentResult.TakeValue().m_catalog;

        CatalogGovernanceRequest permission;
        permission.m_subjectKind = "relationship";
        permission.m_subjectId = relationship.m_relationshipId;
        permission.m_axis = "permission";
        permission.m_value = "allow";
        permission.m_usage = "planning_only";
        permission.m_evidenceIds = { "evidence.test" };
        permission.m_validationIds = { validated.m_event.m_validationId };
        permission.m_reviewer = "test-reviewer";
        auto permissionResult = service.ApplyDecision(permission, workspace, registry, catalog);
        ASSERT_TRUE(permissionResult.IsSuccess());
        catalog = permissionResult.TakeValue().m_catalog;

        const CatalogRelationship* permitted = catalog.FindRelationshipById(relationship.m_relationshipId);
        ASSERT_NE(permitted, nullptr);
        ASSERT_EQ(permitted->m_allowedUsages.size(), 1);
        EXPECT_EQ(permitted->m_allowedUsages.front(), "planning_only");
    }
} // namespace TaintedGrailModdingSDK
