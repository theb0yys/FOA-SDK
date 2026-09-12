/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include <ExternalToolchain/ExternalToolchainTypes.h>
#include <ExternalToolchain/ToolExecutionTypes.h>
#include <chrono>
#include <functional>
#include <memory>

namespace ExternalToolchain
{
    // Host-private resolved paths never enter public records or the execution bus.
    struct ToolRootBinding
    {
        AZStd::string m_rootId, m_absolutePath;
    };
    struct ToolResolvedConfiguration
    {
        AZStd::string m_executablePath, m_executableDigest, m_configurationFingerprint;
        AZStd::string m_providerId, m_providerVersion, m_probeId, m_toolVersion;
        DiscoveryStatus m_discoveryStatus = DiscoveryStatus::NotRun;
        AZStd::vector<ToolRootBinding> m_roots;
        AZStd::vector<ToolFileReference> m_companions;
    };
    struct ToolAdmissionContext
    {
        AZStd::string m_attemptId, m_requestFingerprint, m_commandFingerprint, m_profileFingerprint;
        AZStd::string m_hostInstanceId, m_executableDigest, m_configurationFingerprint;
        AZStd::vector<AZStd::string> m_rootIdentities;
    };
    class ToolAdmissionLease
    {
    public:
        virtual ~ToolAdmissionLease() = default;
        // Implementations must bind exact context and expiry. The caller owns and consumes once.
        virtual bool Consume(const ToolAdmissionContext&, std::chrono::steady_clock::time_point) = 0;
    };
    class ToolExecutionAdmission
    {
    public:
        virtual ~ToolExecutionAdmission() = default;
        virtual std::unique_ptr<ToolAdmissionLease> Acquire(const ToolAdmissionContext&) = 0;
    };
    class DenyToolExecution final : public ToolExecutionAdmission
    {
    public:
        std::unique_ptr<ToolAdmissionLease> Acquire(const ToolAdmissionContext&) override
        {
            return {};
        }
    };
    struct ToolExecutionHostConfiguration
    {
        bool m_enabled = false;
        AZStd::string m_privateStoreRoot;
        std::shared_ptr<ToolExecutionAdmission> m_admission = std::make_shared<DenyToolExecution>();
        // A thread-safe host-owned resolver, not a provider callback or registration-bus method.
        std::function<bool(const ToolExecutionCommandV2&, ToolResolvedConfiguration&)> m_resolve;
    };
} // namespace ExternalToolchain
