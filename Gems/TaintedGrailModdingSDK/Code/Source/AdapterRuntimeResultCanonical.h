/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterRuntimeResultContracts.h"

namespace TaintedGrailModdingSDK
{
    // Serializes every contract-significant runtime-result field except
    // m_resultFingerprint. The returned UTF-8 JSON is the sole payload hashed
    // for the result fingerprint.
    AZStd::string SerializeCanonicalRuntimeResult(
        const AdapterRuntimeResultEnvelope& envelope);

    AZStd::string CalculateRuntimeResultFingerprint(
        const AdapterRuntimeResultEnvelope& envelope);

    bool RuntimeResultFingerprintMatches(
        const AdapterRuntimeResultEnvelope& envelope);
} // namespace TaintedGrailModdingSDK
