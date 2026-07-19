/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPostDeploymentVerificationContracts.h"

namespace TaintedGrailModdingSDK
{
    // Builds a deterministic read-only report from already supplied metadata.
    // No verifier, adapter, deployment, launch, promotion, or publication occurs here.
    class AdapterPostDeploymentVerificationService
    {
    public:
        AdapterPostDeploymentVerificationReport BuildReport(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn) const;
    };
} // namespace TaintedGrailModdingSDK
