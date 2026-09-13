/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FrameworkExecutionPolicyService.h"
#include "FrameworkExecutionRepository.h"
#include "FrameworkProviderService.h"

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    class FrameworkSyntheticTarget;
    class FrameworkExecutionService
    {
    public:
        FrameworkExecutionService(Context context, AZStd::string privateRoot);
        FrameworkExecutionService(Context context, AZStd::string privateRoot, std::shared_ptr<FrameworkSyntheticTarget> synthetic);
        Result RecoverSynthetic(const AZStd::string& planFingerprint, CE::RollbackReceiptV1&);
        ~FrameworkExecutionService();
        FrameworkProviderService& Providers();
        FrameworkExecutionPolicyService& Policy();
        // Setup, preview and history reads are blocking worker/host operations, never Editor event handlers.
        Result Open();
        Result Preview(
            const CE::CapabilityDescriptorV1&,
            const CE::CapabilityExecutionRequestV1&,
            const AZStd::vector<AZStd::string>& workspaceDefaults,
            CE::CapabilityExecutionPlanV1&);
        Result Confirm(const AZStd::string& planFingerprint, const AZStd::string& actor, std::chrono::seconds lifetime);
        Result Submit(const AZStd::string& planFingerprint, Snapshot&, bool explicitRetry = false);
        Result Cancel(const AZStd::string& executionId);
        Result Status(const AZStd::string& executionId, Snapshot&) const;
        AZStd::vector<Snapshot> Page(size_t offset, size_t count) const;
        AZStd::vector<StoredAttempt> History(size_t offset, size_t count) const;
        bool Busy() const;
        bool CanChangeContext() const;
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework
