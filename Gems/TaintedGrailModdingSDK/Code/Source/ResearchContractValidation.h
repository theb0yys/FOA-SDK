/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/std/string/string.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    bool IsStableContractId(
        const AZStd::string& value,
        size_t maximumLength = 256);
    bool IsSafePersistenceId(
        const AZStd::string& value,
        size_t maximumLength = 160);
    bool IsSha256Fingerprint(const AZStd::string& value);
    bool IsStrictSemanticVersion(const AZStd::string& value);
    bool IsStrictUtcTimestamp(const AZStd::string& value);
    bool IsSupportedRuntimeTarget(const AZStd::string& value);
    bool IsUsableImportStatus(const AZStd::string& value);
    bool IsFiniteNonNegative(double value);
    bool IsFiniteProbability(double value, bool allowZero = false);
} // namespace TaintedGrailModdingSDK
