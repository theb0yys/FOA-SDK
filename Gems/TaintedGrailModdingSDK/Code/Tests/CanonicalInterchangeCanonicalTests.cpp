/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeTestFixtures.h"

#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

#include <limits>

namespace TaintedGrailModdingSDK::Interchange
{
    using namespace TestFixtures;

    TEST(CanonicalInterchangeCanonicalTests, Utf8AndPresentationEscapingAreExact)
    {
        const AZStd::string snowman("Avalon \xE2\x98\x83", 10);
        EXPECT_TRUE(IsValidUtf8V1(snowman));
        const AZStd::string malformed("\xC3\x28", 2);
        EXPECT_FALSE(IsValidUtf8V1(malformed));

        AZStd::string output;
        EXPECT_TRUE(AppendCanonicalPresentationStringV1(output, "A\n\"B\\C"));
        EXPECT_EQ("\"A\\n\\\"B\\\\C\"", output);
    }

    TEST(CanonicalInterchangeCanonicalTests, FiniteNumbersAndNegativeZeroAreCanonical)
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

    TEST(CanonicalInterchangeCanonicalTests, KnownCanonicalDigestIsStable)
    {
        const auto digest = CalculateCanonicalBytesDigestV1("{}");
        EXPECT_EQ(
            "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
            digest.m_hex);
        EXPECT_TRUE(CanonicalBytesDigestMatchesV1("{}", digest));
        EXPECT_FALSE(CanonicalBytesDigestMatchesV1("{ }", digest));
    }

    TEST(CanonicalInterchangeCanonicalTests, NonEmptyDisplayNameGoldenBytesRoundTripExactly)
    {
        AZStd::string golden(MinimalDocumentsJson);
        const size_t producerPosition = golden.find("\"producer\"");
        ASSERT_NE(AZStd::string::npos, producerPosition);
        golden.insert(producerPosition, "\"display_name\":\"Fixture\",");

        CanonicalInterchangeManifestV1 manifest;
        const auto parsed = ParseCanonicalManifestV1(golden, manifest);
        ASSERT_TRUE(parsed.IsValid());
        EXPECT_EQ("Fixture", manifest.m_displayName);
        EXPECT_EQ(golden, SerializeCanonicalManifestV1(manifest));
        EXPECT_TRUE(CanonicalManifestBytesMatchV1(golden, manifest));
    }

    TEST(CanonicalInterchangeCanonicalTests, SerializationDoesNotMutateCallerCollections)
    {
        CanonicalInterchangeManifestV1 manifest;
        ASSERT_TRUE(ParseCanonicalManifestV1(MinimalAssetJson, manifest).IsValid());
        ASSERT_EQ(2, manifest.m_payloads.size());

        AZStd::swap(manifest.m_payloads[0], manifest.m_payloads[1]);
        manifest.m_assets[0].m_subjectReferences.push_back("subject.example");
        const AZStd::string firstPayloadBefore = manifest.m_payloads[0].m_payloadId.m_value;
        const AZStd::string secondPayloadBefore = manifest.m_payloads[1].m_payloadId.m_value;
        const size_t subjectsBefore = manifest.m_assets[0].m_subjectReferences.size();

        const AZStd::string canonical = SerializeCanonicalManifestV1(manifest);
        ASSERT_FALSE(canonical.empty());
        EXPECT_EQ(firstPayloadBefore, manifest.m_payloads[0].m_payloadId.m_value);
        EXPECT_EQ(secondPayloadBefore, manifest.m_payloads[1].m_payloadId.m_value);
        EXPECT_EQ(subjectsBefore, manifest.m_assets[0].m_subjectReferences.size());
        EXPECT_LT(canonical.find("\"payload_id\":\"payload.asset\""),
            canonical.find("\"payload_id\":\"payload.document\""));
    }

    TEST(CanonicalInterchangeCanonicalTests, PublicDocumentsExampleRoundTripsThroughCpp)
    {
        CanonicalInterchangeManifestV1 first;
        ASSERT_TRUE(ParseCanonicalManifestV1(MinimalDocumentsJson, first).IsValid());
        ASSERT_EQ(1, first.m_documents.size());
        EXPECT_EQ(
            first.m_documents[0].m_revisionFingerprint,
            CalculateDocumentRevisionFingerprintV1(first, first.m_documents[0]));

        const AZStd::string canonical = SerializeCanonicalManifestV1(first);
        ASSERT_FALSE(canonical.empty());
        CanonicalInterchangeManifestV1 second;
        ASSERT_TRUE(ParseCanonicalManifestV1(canonical, second).IsValid());
        EXPECT_EQ(canonical, SerializeCanonicalManifestV1(second));
        EXPECT_EQ(first.m_packageId, second.m_packageId);
    }

    TEST(CanonicalInterchangeCanonicalTests, PublicAssetExampleRoundTripsThroughCpp)
    {
        CanonicalInterchangeManifestV1 first;
        ASSERT_TRUE(ParseCanonicalManifestV1(MinimalAssetJson, first).IsValid());
        ASSERT_EQ(1, first.m_assets.size());
        EXPECT_EQ(
            first.m_assets[0].m_revisionFingerprint,
            CalculateAssetRevisionFingerprintV1(first, first.m_assets[0]));

        const AZStd::string canonical = SerializeCanonicalManifestV1(first);
        ASSERT_FALSE(canonical.empty());
        CanonicalInterchangeManifestV1 second;
        ASSERT_TRUE(ParseCanonicalManifestV1(canonical, second).IsValid());
        EXPECT_EQ(canonical, SerializeCanonicalManifestV1(second));
        EXPECT_EQ(first.m_assets[0].m_assetId, second.m_assets[0].m_assetId);
    }
} // namespace TaintedGrailModdingSDK::Interchange
