/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterDeploymentExecutionResultContracts.h"

namespace TaintedGrailModdingSDK
{
    // Serializes every contract-significant execution-result field except
    // m_resultFingerprint. The returned UTF-8 JSON is the sole payload hashed
    // for the result fingerprint.
    AZStd::string SerializeCanonicalDeploymentExecutionResult(
        const AdapterDeploymentExecutionResultEnvelope& envelope);

    AZStd::string CalculateDeploymentExecutionResultFingerprint(
        const AdapterDeploymentExecutionResultEnvelope& envelope);

    bool DeploymentExecutionResultFingerprintMatches(
        const AdapterDeploymentExecutionResultEnvelope& envelope);
} // namespace TaintedGrailModdingSDK
