/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExecutionFramework/FrameworkExecutionEvidenceProjection.h"
#include "ExecutionFramework/FrameworkExecutionService.h"
#include "FoundationService.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    FoundationService::~FoundationService()
    {
        StopFrameworkExecution();
    }
    AZStd::string FoundationService::GetFrameworkProfileFingerprint() const
    {
        const auto* profile = m_workspace.FindActiveGameProfile();
        if (!profile)
        {
            return {};
        }
        return ExecutionFramework::ProfileFingerprint(*profile);
    }
    ExecutionFramework::FrameworkExecutionService* FoundationService::GetFrameworkExecution() const
    {
        return m_frameworkExecution.get();
    }
    bool FoundationService::CanChangeFrameworkContext() const
    {
        return !m_frameworkExecution || m_frameworkExecution->CanChangeContext();
    }
    void FoundationService::StopFrameworkExecution()
    {
        if (m_frameworkExecution)
        {
            m_frameworkExecution->Shutdown();
            m_frameworkExecution.reset();
            AZ_Printf("TaintedGrailModdingSDK", "Framework execution worker and supervisor joined.\n");
        }
    }
    bool FoundationService::ConfigureFrameworkExecution(
        const ExecutionFramework::Context& context,
        const AZStd::string& privateRoot,
        const AZStd::vector<ExecutionFramework::HostBinding>& bindings,
        const AZStd::vector<ExecutionFramework::Qualification>& qualifications,
        const ExecutionFramework::HostPolicy& policy,
        AZStd::string* error)
    {
        using namespace ExecutionFramework;
        if (!CanChangeFrameworkContext() || context.m_workspaceId != m_workspace.m_workspaceId || context.m_packId != m_activePackId ||
            context.m_profileFingerprint != GetFrameworkProfileFingerprint() || bindings.empty())
        {
            if (error)
            {
                *error = "The reviewed execution context must match the idle active workspace, pack and profile.";
            }
            return false;
        }
        StopFrameworkExecution();
        auto service = std::make_unique<FrameworkExecutionService>(context, privateRoot);
        Result result;
        for (const auto& binding : bindings)
        {
            result = service->Providers().Register(binding);
            if (!result)
            {
                break;
            }
        }
        if (result)
        {
            for (const auto& observation : qualifications)
            {
                result = service->Providers().ReviewQualification(observation);
                if (!result)
                {
                    break;
                }
            }
        }
        if (result)
        {
            result = service->Policy().SetPolicy(policy);
        }
        if (result)
        {
            result = service->Open();
        }
        if (!result)
        {
            if (error)
            {
                *error = AZStd::string::format("Framework execution setup refused (%u).", static_cast<unsigned>(result.m_error));
            }
            return false;
        }
        m_frameworkExecution = std::move(service);
        return true;
    }
} // namespace TaintedGrailModdingSDK
