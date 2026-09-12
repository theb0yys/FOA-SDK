/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FrameworkProviderService.h"
#include <atomic>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    // Internal host admission object. No request bus or provider interface can arm it.
    class FrameworkPendingAdmission final : public ET::ToolExecutionAdmission
    {
    public:
        FrameworkPendingAdmission();
        ~FrameworkPendingAdmission();
        bool Arm(const ET::ToolAdmissionContext&, std::function<bool()> live);
        void Revoke();
        std::unique_ptr<ET::ToolAdmissionLease> Acquire(const ET::ToolAdmissionContext&) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
    class FrameworkToolExecutionAdapter
    {
    public:
        FrameworkToolExecutionAdapter(FrameworkProviderService&, AZStd::string privateStore);
        ~FrameworkToolExecutionAdapter();
        Result Run(
            const PreparedPhase&,
            ET::ToolInvocationRequestV2 request,
            const std::atomic_bool& cancelled,
            std::function<bool()> liveAdmission,
            ET::ToolInvocationRecordV2&,
            std::function<void(const ET::ToolInvocationStatusV2&)> progress = {});
        AZStd::vector<ET::ToolInvocationRecordV2> Records() const;
        void Shutdown();
        const AZStd::string& Store() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
