/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CanonicalFingerprint.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    TEST(CanonicalFingerprintTests, MatchesPublishedSha256Vectors)
    {
        EXPECT_EQ(
            CalculateCanonicalSha256(""),
            "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        EXPECT_EQ(
            CalculateCanonicalSha256("abc"),
            "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        EXPECT_TRUE(CanonicalSha256Matches(
            "abc",
            "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        EXPECT_FALSE(CanonicalSha256Matches(
            "abd",
            "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    }
} // namespace TaintedGrailModdingSDK
