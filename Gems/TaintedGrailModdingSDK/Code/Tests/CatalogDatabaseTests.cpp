/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        CatalogRecord MakeNativeRecord(
            AZStd::string recordId,
            AZStd::string nativeRef,
            AZStd::string displayName,
            AZStd::string evidenceId)
        {
            CatalogRecord record;
            record.m_recordId = AZStd::move(recordId);
            record.m_domain = "test";
            record.m_recordKind = "test-record";
            record.m_subjectRef = "subject:";
            record.m_subjectRef += record.m_recordId;
            record.m_nativeRefExact = AZStd::move(nativeRef);
            record.m_identityKind = "native";
            record.m_displayName = AZStd::move(displayName);
            record.m_researchStage = "reviewed";
            record.m_confidence = "documented";
            record.m_operationalRisk = "unknown";
            record.m_validationState = "unvalidated";
            record.m_stalenessState = "unknown";
            record.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
            record.m_evidenceIds = { AZStd::move(evidenceId) };
            return record;
        }
    } // namespace

    TEST(TaintedGrailCatalogDatabaseTests, SameDisplayNameDoesNotMergeDistinctRecordIds)
    {
        CatalogDatabase catalog;
        AZStd::string error;

        EXPECT_TRUE(catalog.InsertNew(
            MakeNativeRecord("record.one", "native-ref-one", "Shared Display Name", "evidence.one"),
            &error));
        EXPECT_TRUE(catalog.InsertNew(
            MakeNativeRecord("record.two", "native-ref-two", "Shared Display Name", "evidence.two"),
            &error));

        CatalogQuery query;
        query.m_searchText = "Shared Display Name";
        const AZStd::vector<CatalogRecord> matches = catalog.Query(query);

        ASSERT_EQ(matches.size(), 2);
        EXPECT_EQ(matches[0].m_recordId, "record.one");
        EXPECT_EQ(matches[1].m_recordId, "record.two");
    }

    TEST(TaintedGrailCatalogDatabaseTests, DuplicateExactNativeReferenceIsRejected)
    {
        CatalogDatabase catalog;
        AZStd::string error;

        EXPECT_TRUE(catalog.InsertNew(
            MakeNativeRecord("record.one", "same-native-ref", "First", "evidence.one"),
            &error));
        EXPECT_FALSE(catalog.InsertNew(
            MakeNativeRecord("record.two", "same-native-ref", "Second", "evidence.two"),
            &error));
        EXPECT_NE(error.find("Exact native reference"), AZStd::string::npos);
    }

    TEST(TaintedGrailCatalogDatabaseTests, SyntheticRecordRequiresOwningPack)
    {
        CatalogDatabase catalog;
        CatalogRecord record;
        record.m_recordId = "record.synthetic";
        record.m_domain = "test";
        record.m_recordKind = "custom";
        record.m_subjectRef = "subject:synthetic";
        record.m_identityKind = "synthetic";
        record.m_evidenceIds = { "evidence.synthetic" };

        AZStd::string error;
        EXPECT_FALSE(catalog.InsertNew(record, &error));
        EXPECT_NE(error.find("owning pack"), AZStd::string::npos);
    }

    TEST(TaintedGrailCatalogDatabaseTests, QueryFiltersEvidencePermissionAndBlockedState)
    {
        CatalogDatabase catalog;
        CatalogRecord record = MakeNativeRecord(
            "record.filtered", "native-filtered", "Filtered", "evidence.filtered");
        record.m_validationState = "validated";
        record.m_stalenessState = "current";
        record.m_allowedUsages = { "display_only" };
        record.m_forbiddenUsages.clear();

        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(record, &error));

        CatalogQuery query;
        query.m_evidenceId = "evidence.filtered";
        query.m_permission = "display_only";
        query.m_validationState = "validated";
        query.m_stalenessState = "current";
        EXPECT_EQ(catalog.Query(query).size(), 1);

        query.m_blockedOnly = true;
        EXPECT_TRUE(catalog.Query(query).empty());
    }

    TEST(TaintedGrailCatalogDatabaseTests, RelationshipRequiresKnownRecordsAndEvidence)
    {
        CatalogDatabase catalog;
        AZStd::string error;
        ASSERT_TRUE(catalog.InsertNew(
            MakeNativeRecord("record.parent", "native-parent", "Parent", "evidence.parent"),
            &error));
        ASSERT_TRUE(catalog.InsertNew(
            MakeNativeRecord("record.child", "native-child", "Child", "evidence.child"),
            &error));

        CatalogRelationship relationship;
        relationship.m_relationshipId = "relationship.parent-child";
        relationship.m_fromRecordId = "record.parent";
        relationship.m_toRecordId = "record.child";
        relationship.m_relationshipKind = "contains";
        relationship.m_validationState = "unvalidated";
        relationship.m_stalenessState = "unknown";

        EXPECT_FALSE(catalog.UpsertRelationship(relationship, &error));
        relationship.m_evidenceIds = { "evidence.relationship" };
        EXPECT_TRUE(catalog.UpsertRelationship(relationship, &error));
        EXPECT_EQ(catalog.FindRelationshipsForRecord("record.parent").size(), 1);
        EXPECT_EQ(catalog.FindRelationshipsForRecord("record.child").size(), 1);
    }
} // namespace TaintedGrailModdingSDK
