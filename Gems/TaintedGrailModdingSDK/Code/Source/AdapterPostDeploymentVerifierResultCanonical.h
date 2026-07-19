/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPostDeploymentVerifierContracts.h"

namespace TaintedGrailModdingSDK
{
    // Serializes every contract-significant verifier-result field except
    // m_resultFingerprint. The returned UTF-8 JSON is the sole payload hashed
    // for the verifier-result fingerprint.
    AZStd::string SerializeCanonicalPostDeploymentVerifierResult(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope);

    AZStd::string CalculatePostDeploymentVerifierResultFingerprint(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope);

    bool PostDeploymentVerifierResultFingerprintMatches(
        const AdapterPostDeploymentVerifierResultEnvelope& envelope);
} // namespace TaintedGrailModdingSDK
