/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeAcceptanceFixtures.h"

#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

#include <limits>

namespace TaintedGrailModdingSDK::Interchange
{
    using namespace AcceptanceFixtures;

    TEST(CanonicalInterchangeCanonicalAcceptanceTests, Utf8EscapingAndKnownDigestAreExact)
    {
        const AZStd::string snowman("Avalon \xE2\x98\x83", 10);
        EXPECT_TRUE(IsValidUtf8V1(snowman));
        EXPECT_FALSE(IsValidUtf8V1(AZStd::string("\xC3\x28", 2)));

        AZStd::string output;
        EXPECT_TRUE(AppendCanonicalPresentationStringV1(output, "A\n\"B\\C"));
        EXPECT_EQ("\"A\\n\\\"B\\\\C\"", output);

        const auto digest = CalculateCanonicalBytesDigestV1("{}");
        EXPECT_EQ(
            "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
            digest.m_hex);
        EXPECT_TRUE(CanonicalBytesDigestMatchesV1("{}", digest));
        EXPECT_FALSE(CanonicalBytesDigestMatchesV1("{ }", digest));
    }

    TEST(CanonicalInterchangeCanonicalAcceptanceTests, FiniteNumbersAndNegativeZeroAreCanonical)
    {
        AZStd::string output;
        EXPECT_TRUE(FormatCanonicalFiniteNumberV1(-0.0, output));
        EXPECT_EQ("0", output);
        EXPECT_TRUE(FormatCanonicalFiniteNumberV1(1.25, output));
        EXPECT_EQ("1.25", output);
        EXPECT_FALSE(FormatCanonicalFiniteNumberV1(
            std::numeric_limits<double>::infinity(), output));
        EXPECT_FALSE(FormatCanonicalFiniteNumberV1(
            std::numeric_limits<double>::quiet_NaN(), output));
    }

    TEST(CanonicalInterchangeCanonicalAcceptanceTests, SerializationDoesNotMutateCallerCollections)
    {
        const AZStd::string bytes = AssetExampleBytes();
        ASSERT_FALSE(bytes.empty());
        CanonicalInterchangeManifestV1 manifest;
        ASSERT_TRUE(ParseCanonicalManifestV1(bytes, manifest).IsValid());
        ASSERT_EQ(2, manifest.m_payloads.size());

        AZStd::swap(manifest.m_payloads[0], manifest.m_payloads[1]);
        manifest.m_assets[0].m_subjectReferences.push_back("subject.example");
        const AZStd::string firstBefore = manifest.m_payloads[0].m_payloadId.m_value;
        const AZStd::string secondBefore = manifest.m_payloads[1].m_payloadId.m_value;
        const size_t subjectsBefore = manifest.m_assets[0].m_subjectReferences.size();

        const AZStd::string canonical = SerializeCanonicalManifestV1(manifest);
        ASSERT_FALSE(canonical.empty());
        EXPECT_EQ(firstBefore, manifest.m_payloads[0].m_payloadId.m_value);
        EXPECT_EQ(secondBefore, manifest.m_payloads[1].m_payloadId.m_value);
        EXPECT_EQ(subjectsBefore, manifest.m_assets[0].m_subjectReferences.size());
        EXPECT_LT(canonical.find("\"payload_id\":\"payload.asset\""),
            canonical.find("\"payload_id\":\"payload.document\""));
    }

    TEST(CanonicalInterchangeCanonicalAcceptanceTests, PublicDocumentsExampleRoundTripsThroughCpp)
    {
        const AZStd::string bytes = DocumentsExampleBytes();
        ASSERT_FALSE(bytes.empty());
        CanonicalInterchangeManifestV1 first;
        ASSERT_TRUE(ParseCanonicalManifestV1(bytes, first).IsValid());
        ASSERT_EQ(1, first.m_documents.size());
        EXPECT_TRUE(CanonicalManifestBytesMatchV1(bytes, first));
        EXPECT_EQ(
            first.m_documents[0].m_revisionFingerprint,
            CalculateDocumentRevisionFingerprintV1(first, first.m_documents[0]));

        first.m_displayName = "Documents Example";
        const AZStd::string canonical = SerializeCanonicalManifestV1(first);
        ASSERT_FALSE(canonical.empty());
        CanonicalInterchangeManifestV1 second;
        ASSERT_TRUE(ParseCanonicalManifestV1(canonical, second).IsValid());
        EXPECT_EQ(canonical, SerializeCanonicalManifestV1(second));
    }

    TEST(CanonicalInterchangeCanonicalAcceptanceTests, PublicAssetExampleRoundTripsThroughCpp)
    {
        const AZStd::string bytes = AssetExampleBytes();
        ASSERT_FALSE(bytes.empty());
        CanonicalInterchangeManifestV1 first;
        ASSERT_TRUE(ParseCanonicalManifestV1(bytes, first).IsValid());
        ASSERT_EQ(1, first.m_assets.size());
        EXPECT_TRUE(CanonicalManifestBytesMatchV1(bytes, first));
        EXPECT_EQ(
            first.m_assets[0].m_revisionFingerprint,
            CalculateAssetRevisionFingerprintV1(first, first.m_assets[0]));

        first.m_displayName = "Asset Example";
        const AZStd::string canonical = SerializeCanonicalManifestV1(first);
        ASSERT_FALSE(canonical.empty());
        CanonicalInterchangeManifestV1 second;
        ASSERT_TRUE(ParseCanonicalManifestV1(canonical, second).IsValid());
        EXPECT_EQ(canonical, SerializeCanonicalManifestV1(second));
    }
} // namespace TaintedGrailModdingSDK::Interchange
