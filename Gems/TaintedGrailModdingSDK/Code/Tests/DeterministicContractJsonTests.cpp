/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "DeterministicContractJson.h"

#include <limits>

namespace TaintedGrailModdingSDK
{
    TEST(DeterministicContractJsonTests, SignedMinimumValueIsSerializedWithoutOverflow)
    {
        EXPECT_EQ(
            DeterministicContractJson::SignedString(
                std::numeric_limits<AZ::s64>::min()),
            "-9223372036854775808");
    }

    TEST(DeterministicContractJsonTests, NonFiniteDoublesRemainValidJsonTokens)
    {
        EXPECT_EQ(
            DeterministicContractJson::DoubleString(
                std::numeric_limits<double>::infinity()),
            "null");
        EXPECT_EQ(
            DeterministicContractJson::DoubleString(
                -std::numeric_limits<double>::infinity()),
            "null");
        EXPECT_EQ(
            DeterministicContractJson::DoubleString(
                std::numeric_limits<double>::quiet_NaN()),
            "null");
    }

    TEST(DeterministicContractJsonTests, FloatingSerializationUsesRoundTripPrecision)
    {
        EXPECT_EQ(DeterministicContractJson::DoubleString(1.5), "1.5");
        EXPECT_NE(
            DeterministicContractJson::DoubleString(1.0000000000000002),
            DeterministicContractJson::DoubleString(1.0));
    }

    TEST(DeterministicContractJsonTests, HighAndControlBytesAreEscapedDeterministically)
    {
        AZStd::string value;
        value.push_back(static_cast<char>(0x01));
        value.push_back(static_cast<char>(0x7f));
        value.push_back(static_cast<char>(0xc3));
        value.push_back(static_cast<char>(0xa9));
        AZStd::string output;
        DeterministicContractJson::AppendEscaped(output, value);
        EXPECT_EQ(output, "\"\\u0001\\u007f\\u00c3\\u00a9\"");
    }
} // namespace TaintedGrailModdingSDK
