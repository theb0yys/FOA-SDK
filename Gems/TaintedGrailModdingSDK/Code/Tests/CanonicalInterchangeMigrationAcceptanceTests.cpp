/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeAcceptanceFixtures.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK::Interchange
{
    using namespace AcceptanceFixtures;

    TEST(CanonicalInterchangeMigrationAcceptanceTests, AlreadyCurrentPreservesExactSourceBytes)
    {
        const AZStd::string source = DocumentsExampleBytes();
        ASSERT_FALSE(source.empty());
        const AZStd::string immutableCopy = source;
        const MigrationResultV1 result = MigrateCanonicalInterchange(source, 1);

        EXPECT_EQ(MigrationStatusV1::AlreadyCurrent, result.m_status);
        EXPECT_EQ(1, result.m_sourceVersion);
        EXPECT_EQ(1, result.m_targetVersion);
        EXPECT_EQ(source, result.m_candidateCanonicalJson);
        EXPECT_EQ(result.m_sourceDigest, result.m_candidateDigest);
        EXPECT_TRUE(result.m_steps.empty());
        EXPECT_FALSE(result.m_migrationPerformed);
        EXPECT_TRUE(result.m_validation.IsValid());
        EXPECT_TRUE(result.HasCandidate());
        EXPECT_EQ(immutableCopy, source);
    }

    TEST(CanonicalInterchangeMigrationAcceptanceTests, InvalidSourceReturnsNoCandidate)
    {
        const AZStd::string source = "{}";
        const MigrationResultV1 result = MigrateCanonicalInterchange(source, 1);
        EXPECT_EQ(MigrationStatusV1::SourceInvalid, result.m_status);
        EXPECT_FALSE(result.HasCandidate());
        EXPECT_TRUE(result.m_candidateCanonicalJson.empty());
        EXPECT_TRUE(ContainsIssue(result.m_validation, IssueCodeV1::MigrationSourceInvalid));
        EXPECT_EQ(CalculateCanonicalBytesDigestV1(source), result.m_sourceDigest);
    }

    TEST(CanonicalInterchangeMigrationAcceptanceTests, VersionDispatchFailsClosed)
    {
        const AZStd::string source = DocumentsExampleBytes();
        ASSERT_FALSE(source.empty());

        EXPECT_EQ(MigrationStatusV1::DowngradeForbidden,
            MigrateCanonicalInterchange(source, 0).m_status);

        const MigrationResultV1 adjacent = MigrateCanonicalInterchange(source, 2);
        EXPECT_EQ(MigrationStatusV1::MigratorUnavailable, adjacent.m_status);
        EXPECT_FALSE(adjacent.HasCandidate());
        EXPECT_TRUE(ContainsIssue(adjacent.m_validation, IssueCodeV1::MigrationUnavailable));

        EXPECT_EQ(MigrationStatusV1::UnsupportedTargetVersion,
            MigrateCanonicalInterchange(source, 3).m_status);

        AZStd::string unsupportedSource(source);
        const size_t versionPosition = unsupportedSource.find("\"schema_version\":1");
        ASSERT_NE(AZStd::string::npos, versionPosition);
        unsupportedSource.replace(
            versionPosition,
            AZStd::string_view("\"schema_version\":1").size(),
            "\"schema_version\":2");
        const MigrationResultV1 unsupported = MigrateCanonicalInterchange(unsupportedSource, 2);
        EXPECT_EQ(MigrationStatusV1::UnsupportedSourceVersion, unsupported.m_status);
        EXPECT_FALSE(unsupported.HasCandidate());
    }

    TEST(CanonicalInterchangeMigrationAcceptanceTests, ChangedIdentityCandidateIsSemanticDrift)
    {
        const AZStd::string source = DocumentsExampleBytes();
        ASSERT_FALSE(source.empty());
        AZStd::string candidate(source);
        const size_t producerPosition = candidate.find("\"producer\"");
        ASSERT_NE(AZStd::string::npos, producerPosition);
        candidate.insert(producerPosition, "\"display_name\":\"Changed\",");

        MigrationStepV1 step;
        step.m_sourceVersion = 1;
        step.m_targetVersion = 1;
        step.m_sourceDigest = CalculateCanonicalBytesDigestV1(source);
        step.m_candidateDigest = CalculateCanonicalBytesDigestV1(candidate);

        const auto result = ValidateMigrationCandidateV1(source, candidate, step);
        EXPECT_FALSE(result.IsValid());
        EXPECT_TRUE(ContainsIssue(result, IssueCodeV1::MigrationSemanticDrift));
    }
} // namespace TaintedGrailModdingSDK::Interchange
