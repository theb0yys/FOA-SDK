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

    TEST(CanonicalInterchangeValidationAcceptanceTests, StructuralFailureLeavesCallerOutputUntouched)
    {
        CanonicalInterchangeManifestV1 manifest;
        manifest.m_packageId.m_value = "sentinel.package";
        const auto result = ParseCanonicalManifestV1(
            R"json({"schema_version":1,"schema_version":1})json", manifest);
        EXPECT_FALSE(result.IsValid());
        EXPECT_TRUE(ContainsIssue(result, IssueCodeV1::DuplicateKey));
        EXPECT_EQ("sentinel.package", manifest.m_packageId.m_value);
    }

    TEST(CanonicalInterchangeValidationAcceptanceTests, ForbiddenAuthorityFieldIsRejected)
    {
        AZStd::string candidate = DocumentsExampleBytes();
        ASSERT_FALSE(candidate.empty());
        ASSERT_EQ('}', candidate.back());
        candidate.pop_back();
        candidate += ",\"runtime_authority\":true}";

        CanonicalInterchangeManifestV1 manifest;
        const auto result = ParseCanonicalManifestV1(candidate, manifest);
        EXPECT_FALSE(result.IsValid());
        EXPECT_TRUE(ContainsIssue(result, IssueCodeV1::ForbiddenField));
    }

    TEST(CanonicalInterchangeValidationAcceptanceTests, MissingAndCyclicDependenciesAreDeterministic)
    {
        const AZStd::string bytes = AssetExampleBytes();
        ASSERT_FALSE(bytes.empty());

        CanonicalInterchangeManifestV1 missing;
        ASSERT_TRUE(ParseCanonicalManifestV1(bytes, missing).IsValid());
        missing.m_payloads[0].m_dependencyIds.push_back({ "payload.missing" });
        EXPECT_TRUE(ContainsIssue(
            ValidateCanonicalManifestV1(missing), IssueCodeV1::DependencyMissing));

        CanonicalInterchangeManifestV1 cyclic;
        ASSERT_TRUE(ParseCanonicalManifestV1(bytes, cyclic).IsValid());
        cyclic.m_payloads[0].m_dependencyIds.push_back({ "payload.document" });
        cyclic.m_payloads[1].m_dependencyIds.push_back({ "payload.asset" });
        EXPECT_TRUE(ContainsIssue(
            ValidateCanonicalManifestV1(cyclic), IssueCodeV1::DependencyCycle));
    }

    TEST(CanonicalInterchangeValidationAcceptanceTests, DeclaredRevisionDriftIsRejected)
    {
        CanonicalInterchangeManifestV1 manifest;
        ASSERT_TRUE(ParseCanonicalManifestV1(AssetExampleBytes(), manifest).IsValid());
        manifest.m_documents[0].m_revisionFingerprint.m_sha256 = AZStd::string(64, '0');
        manifest.m_assets[0].m_revisionFingerprint.m_sha256 = AZStd::string(64, '0');

        const auto result = ValidateCanonicalManifestV1(manifest);
        EXPECT_FALSE(result.IsValid());
        EXPECT_TRUE(ContainsIssue(result, IssueCodeV1::RevisionMismatch));
    }

    TEST(CanonicalInterchangeValidationAcceptanceTests, IssueOrderingAndDuplicateCollapseAreStable)
    {
        CanonicalInterchangeValidationResultV1 result;
        result.m_issues = {
            { IssueSeverityV1::Error, AZStd::string(IssueCodeV1::DependencyMissing), "subject.b", "z", { "b", "a", "a" } },
            { IssueSeverityV1::Blocker, AZStd::string(IssueCodeV1::UnsupportedVersion), "subject.a", "a", {} },
            { IssueSeverityV1::Error, AZStd::string(IssueCodeV1::UnknownField), "subject.a", "b", {} },
            { IssueSeverityV1::Blocker, AZStd::string(IssueCodeV1::UnsupportedVersion), "subject.a", "a", {} }
        };

        SortCanonicalInterchangeIssuesV1(result);
        ASSERT_EQ(3, result.m_issues.size());
        EXPECT_EQ(IssueCodeV1::UnknownField, result.m_issues[0].m_code);
        EXPECT_EQ(IssueCodeV1::UnsupportedVersion, result.m_issues[1].m_code);
        EXPECT_EQ(IssueCodeV1::DependencyMissing, result.m_issues[2].m_code);
        ASSERT_EQ(2, result.m_issues[2].m_relatedIds.size());
        EXPECT_EQ("a", result.m_issues[2].m_relatedIds[0]);
        EXPECT_EQ("b", result.m_issues[2].m_relatedIds[1]);
    }
} // namespace TaintedGrailModdingSDK::Interchange
