/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogGovernanceTypes.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    TEST(TaintedGrailCatalogGovernanceTypesTests, KnownValuesRoundTripThroughTypedBoundary)
    {
        const auto validation = ParseValidationState("validated");
        ASSERT_TRUE(validation.IsSuccess());
        EXPECT_EQ(ToString(validation.GetValue()), "validated");

        const auto maturity = ParseResearchStage("S7");
        ASSERT_TRUE(maturity.IsSuccess());
        EXPECT_EQ(ToString(maturity.GetValue()), "S7");

        const auto confidence = ParseConfidenceLevel("runtime_observed");
        ASSERT_TRUE(confidence.IsSuccess());
        EXPECT_EQ(ToString(confidence.GetValue()), "runtime_observed");
    }

    TEST(TaintedGrailCatalogGovernanceTypesTests, TypographicalValuesFailAtBoundary)
    {
        EXPECT_FALSE(ParseValidationState("validted").IsSuccess());
        EXPECT_FALSE(ParseGovernanceAxis("operational-risk").IsSuccess());
        EXPECT_FALSE(ParsePermissionDecision("allowed").IsSuccess());
        EXPECT_FALSE(ParseStalenessState("possibly_stale").IsSuccess());
    }

    TEST(TaintedGrailCatalogGovernanceTypesTests, LegacySchemaValuesRemainRepresentable)
    {
        const auto maturity = ParseResearchStage("unknown");
        ASSERT_TRUE(maturity.IsSuccess());
        EXPECT_EQ(ToString(maturity.GetValue()), "unknown");

        const auto confidence = ParseConfidenceLevel("unrated");
        ASSERT_TRUE(confidence.IsSuccess());
        EXPECT_EQ(ToString(confidence.GetValue()), "unrated");
    }
} // namespace TaintedGrailModdingSDK
