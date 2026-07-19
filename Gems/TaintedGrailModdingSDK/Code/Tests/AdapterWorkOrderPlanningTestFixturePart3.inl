/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace
{
        const GameProfile& FixtureProfile(const ReadyFixture& fixture)
        {
            const GameProfile* profile = fixture.m_workspace.FindActiveGameProfile();
            EXPECT_NE(profile, nullptr);
            return *profile;
        }

        void AddRecordWithProof(
            ReadyFixture& fixture,
            const CatalogRecord& record,
            const AZStd::vector<AZStd::string>& usages)
        {
            AZStd::string error;
            EXPECT_TRUE(fixture.m_catalog.InsertNew(record, &error)) << error.c_str();
            EXPECT_TRUE(fixture.m_catalog.AddValidationEventBound(
                MakeValidation(
                    "validation.record." + record.m_recordId,
                    "record",
                    record.m_recordId,
                    record.m_evidenceIds.front()),
                fixture.m_workspace,
                FixtureProfile(fixture),
                fixture.m_sourceRegistry,
                &error)) << error.c_str();
            for (const AZStd::string& usage : usages)
            {
                EXPECT_TRUE(fixture.m_catalog.AddGovernanceEventBound(
                    MakePermission(
                        "permission." + record.m_recordId + "." + usage,
                        record,
                        usage),
                    fixture.m_workspace,
                    FixtureProfile(fixture),
                    fixture.m_sourceRegistry,
                    &error)) << error.c_str();
            }
        }

        void AddRelationshipWithProof(
            ReadyFixture& fixture,
            const CatalogRelationship& relationship,
            bool includeValidationProof)
        {
            AZStd::string error;
            EXPECT_TRUE(fixture.m_catalog.UpsertRelationship(relationship, &error))
                << error.c_str();
            if (includeValidationProof)
            {
                EXPECT_TRUE(fixture.m_catalog.AddValidationEventBound(
                    MakeValidation(
                        "validation.relationship." + relationship.m_relationshipId,
                        "relationship",
                        relationship.m_relationshipId,
                        relationship.m_evidenceIds.front()),
                    fixture.m_workspace,
                    FixtureProfile(fixture),
                    fixture.m_sourceRegistry,
                    &error)) << error.c_str();
            }
        }

} // namespace
