/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionTestFixtures.h"
using namespace TaintedGrailModdingSDK::ExecutionFramework;
using namespace TaintedGrailModdingSDK::ExecutionFramework::Tests;

TEST(FrameworkCodec, ExactCanonicalPlanRoundTrip)
{
    Fixture f;
    const auto plan = f.Plan();
    auto bytes = CE::Canonicalize(plan.m_plan).GetValue().m_json;
    CE::CapabilityExecutionPlanV1 decoded;
    ASSERT_TRUE(Decode(bytes, decoded));
    EXPECT_TRUE(ValidateStoredPlan(decoded));
    EXPECT_EQ(decoded.m_fingerprint, plan.m_plan.m_fingerprint);
    EXPECT_EQ(CE::Canonicalize(decoded).GetValue().m_json, bytes);
}
TEST(FrameworkCodec, UnknownDuplicateFutureAndMalformedFieldsAreRejectedAtomically)
{
    Fixture f;
    auto bytes = CE::Canonicalize(f.m_descriptor).GetValue().m_json;
    auto unchanged = f.m_descriptor;
    auto unknown = bytes;
    unknown.insert(1, "\"unknown\":true,");
    EXPECT_FALSE(Decode(unknown, unchanged));
    auto duplicate = bytes;
    duplicate.insert(1, "\"version\":1,");
    EXPECT_FALSE(Decode(duplicate, unchanged));
    auto future = bytes;
    future.replace(future.find("\"version\":1"), 11, "\"version\":2");
    EXPECT_FALSE(Decode(future, unchanged));
    auto reordered = " " + bytes;
    EXPECT_FALSE(Decode(reordered, unchanged));
    EXPECT_EQ(unchanged.m_fingerprint, f.m_descriptor.m_fingerprint);
}
TEST(FrameworkCodec, BoundedBeforeDomAllocation)
{
    CE::CapabilityDescriptorV1 output;
    EXPECT_FALSE(Decode(AZStd::string(CE::MaximumCanonicalBytes + 1, ' '), output));
    EXPECT_FALSE(Decode(AZStd::string(1000, '[') + AZStd::string(1000, ']'), output));
}
TEST(FrameworkCodec, TamperedEmbeddedReferenceIsRejected)
{
    Fixture f;
    auto plan = f.Plan();
    auto bytes = CE::Canonicalize(plan.m_plan).GetValue().m_json;
    auto pos = bytes.find("workspace.synthetic");
    ASSERT_NE(pos, AZStd::string::npos);
    bytes.replace(pos, 19, "workspace.different");
    CE::CapabilityExecutionPlanV1 output;
    EXPECT_FALSE(Decode(bytes, output));
}
