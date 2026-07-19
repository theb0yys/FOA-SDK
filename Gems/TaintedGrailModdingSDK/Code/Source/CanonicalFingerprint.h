/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    // Returns lowercase sha256:<64 hex> over the exact UTF-8 bytes stored in value.
    AZStd::string CalculateCanonicalSha256(const AZStd::string& value);

    bool CanonicalSha256Matches(
        const AZStd::string& value,
        const AZStd::string& fingerprint);
} // namespace TaintedGrailModdingSDK
