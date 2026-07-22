/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeTypes.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK::Interchange
{
    TEST(CanonicalInterchangeTypesAcceptanceTests, SemanticIdentifiersUseClosedGrammar)
    {
        EXPECT_TRUE(IsSemanticIdV1("asset.example"));
        EXPECT_TRUE(IsSemanticIdV1("foa.actor_profile-v1"));
        EXPECT_FALSE(IsSemanticIdV1("asset"));
        EXPECT_FALSE(IsSemanticIdV1("Asset.example"));
        EXPECT_FALSE(IsSemanticIdV1("asset..example"));
        EXPECT_FALSE(IsSemanticIdV1("asset/example"));
    }

    TEST(CanonicalInterchangeTypesAcceptanceTests, VersionAndDigestTokensFailClosed)
    {
        EXPECT_TRUE(IsExactVersionTokenV1("v2.7.0"));
        EXPECT_TRUE(IsExactVersionTokenV1("schema-v1"));
        EXPECT_FALSE(IsExactVersionTokenV1("2.7.0"));
        EXPECT_FALSE(IsExactVersionTokenV1("v1..2"));
        EXPECT_FALSE(IsExactVersionTokenV1("version 1"));

        EXPECT_TRUE(IsSha256HexV1(
            "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a"));
        EXPECT_FALSE(IsSha256HexV1(
            "44136FA355B3678A1146AD16F7E8649E94FB4FC21FE77E8310C060F61CAAFF8A"));
        EXPECT_FALSE(IsSha256HexV1("44136fa3"));
    }

    TEST(CanonicalInterchangeTypesAcceptanceTests, PackagePathsRejectTraversalAliasesAndPlatformForms)
    {
        EXPECT_TRUE(IsRelativePackagePathV1("documents/example.json"));
        EXPECT_TRUE(IsRelativePackagePathV1("assets/example.mesh"));
        EXPECT_FALSE(IsRelativePackagePathV1("../example.json"));
        EXPECT_FALSE(IsRelativePackagePathV1("/absolute/example.json"));
        EXPECT_FALSE(IsRelativePackagePathV1("C:/example.json"));
        EXPECT_FALSE(IsRelativePackagePathV1("assets\\example.mesh"));
        EXPECT_FALSE(IsRelativePackagePathV1("con/file.json"));
        EXPECT_FALSE(IsRelativePackagePathV1("assets/example. "));
    }

    TEST(CanonicalInterchangeTypesAcceptanceTests, ClosedEnumTokensRoundTrip)
    {
        HostKindV1 host = HostKindV1::HostNeutral;
        EXPECT_TRUE(TryParseToken("unity-editor", host));
        EXPECT_EQ(HostKindV1::UnityEditor, host);
        EXPECT_EQ(AZStd::string_view("unity-editor"), ToToken(host));
        EXPECT_FALSE(TryParseToken("runtime", host));

        MappingOperationV1 operation = MappingOperationV1::Rename;
        EXPECT_TRUE(TryParseToken("tombstone", operation));
        EXPECT_EQ(MappingOperationV1::Tombstone, operation);
        EXPECT_EQ(AZStd::string_view("tombstone"), ToToken(operation));
    }

    TEST(CanonicalInterchangeTypesAcceptanceTests, CanonicalBasisDefaultsAreExact)
    {
        const CanonicalSpatialBasisV1 spatial;
        EXPECT_EQ("right-handed", spatial.m_handedness);
        EXPECT_EQ("+x", spatial.m_rightAxis);
        EXPECT_EQ("+y", spatial.m_forwardAxis);
        EXPECT_EQ("+z", spatial.m_upAxis);
        EXPECT_EQ("metre", spatial.m_linearUnit);
        EXPECT_EQ("radian", spatial.m_angularUnit);
        EXPECT_EQ("column-vector", spatial.m_vectorConvention);
        EXPECT_EQ("matrix-on-left", spatial.m_multiplicationConvention);
        EXPECT_EQ("column-major", spatial.m_storageOrder);
        EXPECT_EQ("second", CanonicalTemporalBasisV1{}.m_timeUnit);
    }
} // namespace TaintedGrailModdingSDK::Interchange
