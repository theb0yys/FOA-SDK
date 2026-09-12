/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FrameworkExecutionCodec.h"
#include <Execution/ToolExecutionAdmission.h>
#include <chrono>
#include <functional>
#include <memory>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace ET = ExternalToolchain;
    using Clock = std::chrono::steady_clock;
    enum class Error
    {
        None,
        Invalid,
        Unsupported,
        Closed,
        Collision,
        MissingProvider,
        AmbiguousProvider,
        Unqualified,
        PolicyDenied,
        AuthorizationRequired,
        Expired,
        Drifted,
        Busy,
        QueueFull,
        StoreFull,
        StorageFailed,
        CorruptStore,
        Quarantined,
        NotFound,
        Conflict,
        Cancelled,
        ToolFailed,
        Stopped
    };
    struct Result
    {
        Error m_error = Error::None;
        explicit operator bool() const
        {
            return m_error == Error::None;
        }
    };
    inline constexpr size_t MaximumLineages = 1024, MaximumAttempts = 64, MaximumQueue = 16;
    inline constexpr AZ::u64 MaximumMetadata = 256ULL * 1024 * 1024;
    inline constexpr AZ::u64 MaximumStore = 4ULL * 1024 * 1024 * 1024;
    inline constexpr char StoreContract[] = "foa-framework-execution-store-v1";

    struct Context
    {
        AZStd::string m_workspaceId, m_packId, m_profileFingerprint;
        bool Matches(const CE::CapabilityExecutionRequestV1& r) const
        {
            return m_workspaceId == r.m_workspaceId && m_packId == r.m_packId && m_profileFingerprint == r.m_profileFingerprint;
        }
    };
    struct PhasePreview
    {
        CE::CapabilityPhasePlanV1 m_phase;
        ET::ToolInvocationRequestV2 m_invocation;
    };
    struct HostBinding
    {
        CE::CapabilityProviderBindingV1 m_binding;
        ET::ExternalToolProviderDescriptor m_provider;
        ET::ToolExecutionCommandV2 m_command;
        AZStd::string m_previewContractId;
        // Both callbacks belong to the reviewed embedding host, never a plugin registration bus.
        std::function<bool(ET::ToolResolvedConfiguration&)> m_resolve;
        std::function<Result(const CE::CapabilityExecutionRequestV1&, PhasePreview&)> m_preview;
    };
    struct Qualification
    {
        AZStd::string m_bindingFingerprint, m_executableDigest, m_profileFingerprint;
        AZStd::string m_observationId;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZ::u64 m_revision = 0;
        Clock::time_point m_from, m_until;
    };
    struct PreparedPhase
    {
        CE::CapabilityProviderBindingV1 m_binding;
        PhasePreview m_preview;
        ET::ToolResolvedConfiguration m_configuration;
        AZStd::vector<AZStd::string> m_rootIdentities;
        AZ::u64 m_qualificationRevision = 0;
    };
    struct PreparedPlan
    {
        CE::CapabilityDescriptorV1 m_descriptor;
        CE::CapabilityExecutionRequestV1 m_request;
        CE::CapabilityExecutionPlanV1 m_plan;
        AZStd::vector<PreparedPhase> m_phases;
        AZ::u64 m_policyRevision = 0;
    };
    struct Snapshot
    {
        AZStd::string m_executionId, m_planFingerprint, m_phaseId, m_receiptFingerprint;
        CE::ExecutionState m_state = CE::ExecutionState::DRAFT;
        Error m_error = Error::None;
        AZ::u64 m_attempt = 0;
        bool m_canCancel = false;
        ET::ToolStage m_toolStage = ET::ToolStage::Queued;
    };
    AZStd::string UtcNow();
    AZStd::string NewIdentity(const char* prefix);
} // namespace TaintedGrailModdingSDK::ExecutionFramework
