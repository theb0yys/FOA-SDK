/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "FrameworkSyntheticTarget.h"
namespace TaintedGrailModdingSDK::ExecutionFramework
{
    struct SyntheticWorkflow
    {
        CE::CapabilityDescriptorV1 m_descriptor;
        CE::CapabilityExecutionRequestV1 m_request;
        AZStd::vector<HostBinding> m_bindings;
        AZStd::string m_executableDigest;
    };
    // Initializes the context-bound store and synthetic source/staging. Grants no policy or qualification.
    Result PrepareSyntheticWorkflow(
        const Context&,
        const AZStd::string& store,
        const AZStd::string& executable,
        const std::shared_ptr<FrameworkSyntheticTarget>&,
        SyntheticWorkflow&,
        CE::Phase faultPhase = CE::Phase::INVALID,
        const AZStd::string& fault = "normal");
} // namespace TaintedGrailModdingSDK::ExecutionFramework
