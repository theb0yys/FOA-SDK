/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkToolExecutionAdapter.h"
#include <Execution/ToolExecutionService.h>
#include <mutex>
#include <thread>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        bool Same(const ET::ToolAdmissionContext& a, const ET::ToolAdmissionContext& b)
        {
            return a.m_attemptId == b.m_attemptId && a.m_requestFingerprint == b.m_requestFingerprint &&
                a.m_commandFingerprint == b.m_commandFingerprint && a.m_profileFingerprint == b.m_profileFingerprint &&
                a.m_hostInstanceId == b.m_hostInstanceId && a.m_executableDigest == b.m_executableDigest &&
                a.m_configurationFingerprint == b.m_configurationFingerprint && a.m_rootIdentities == b.m_rootIdentities;
        }
        struct Pending
        {
            ET::ToolAdmissionContext m_expected;
            std::function<bool()> m_live;
            std::atomic_bool m_acquired{ false }, m_revoked{ false };
        };
        class Lease final : public ET::ToolAdmissionLease
        {
        public:
            std::shared_ptr<Pending> m_pending;
            ET::ToolAdmissionContext m_context;
            Clock::time_point m_until = Clock::now() + std::chrono::seconds(10);
            bool m_used = false;
            bool Consume(const ET::ToolAdmissionContext& context, Clock::time_point now) override
            {
                if (m_used)
                {
                    return false;
                }
                m_used = true;
                return now < m_until && !m_pending->m_revoked && Same(context, m_context) && m_pending->m_live();
            }
        };
    } // namespace
    struct FrameworkPendingAdmission::Impl
    {
        std::mutex m_mutex;
        std::shared_ptr<Pending> m_pending;
    };
    FrameworkPendingAdmission::FrameworkPendingAdmission()
        : m_impl(std::make_unique<Impl>())
    {
    }
    FrameworkPendingAdmission::~FrameworkPendingAdmission()
    {
        Revoke();
    }
    bool FrameworkPendingAdmission::Arm(const ET::ToolAdmissionContext& expected, std::function<bool()> live)
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_pending || !live)
        {
            return false;
        }
        m_impl->m_pending = std::make_shared<Pending>();
        m_impl->m_pending->m_expected = expected;
        m_impl->m_pending->m_live = std::move(live);
        return true;
    }
    void FrameworkPendingAdmission::Revoke()
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (m_impl->m_pending)
        {
            m_impl->m_pending->m_revoked = true;
            m_impl->m_pending.reset();
        }
    }
    std::unique_ptr<ET::ToolAdmissionLease> FrameworkPendingAdmission::Acquire(const ET::ToolAdmissionContext& context)
    {
        std::shared_ptr<Pending> pending;
        {
            std::lock_guard lock(m_impl->m_mutex);
            pending = m_impl->m_pending;
        }
        if (!pending || pending->m_revoked || context.m_hostInstanceId.empty())
        {
            return {};
        }
        auto expected = pending->m_expected;
        expected.m_hostInstanceId = context.m_hostInstanceId;
        if (!Same(context, expected) || pending->m_acquired.exchange(true) || !pending->m_live())
        {
            return {};
        }
        auto lease = std::make_unique<Lease>();
        lease->m_pending = pending;
        lease->m_context = context;
        return lease;
    }
    struct FrameworkToolExecutionAdapter::Impl
    {
        AZStd::string m_store;
        std::shared_ptr<FrameworkPendingAdmission> m_admission = std::make_shared<FrameworkPendingAdmission>();
        std::unique_ptr<ET::ToolExecutionService> m_service;
        bool m_ready = false;
    };
    FrameworkToolExecutionAdapter::FrameworkToolExecutionAdapter(FrameworkProviderService& providers, AZStd::string root)
        : m_impl(std::make_unique<Impl>())
    {
        m_impl->m_store = AZStd::move(root);
        ET::ToolExecutionHostConfiguration configuration;
        configuration.m_enabled = true;
        configuration.m_privateStoreRoot = m_impl->m_store;
        configuration.m_admission = m_impl->m_admission;
        configuration.m_resolve = [&providers](const auto& command, auto& output)
        {
            return providers.CurrentConfiguration(command, output);
        };
        m_impl->m_service = std::make_unique<ET::ToolExecutionService>(configuration);
        m_impl->m_ready = true;
        for (const auto& binding : providers.Bindings())
        {
            m_impl->m_service->ObserveProvider(binding.m_provider);
            auto result = m_impl->m_service->RegisterExecutionCommand(binding.m_command);
            if (!result && result.m_error != ET::ToolError::Duplicate)
            {
                m_impl->m_ready = false;
            }
        }
        m_impl->m_service->FinalizeRegistration();
        // Deliberately no Connect(): the existing public M2 bus retains its deny-all host.
    }
    FrameworkToolExecutionAdapter::~FrameworkToolExecutionAdapter()
    {
        Shutdown();
    }
    Result FrameworkToolExecutionAdapter::Run(
        const PreparedPhase& phase,
        ET::ToolInvocationRequestV2 request,
        const std::atomic_bool& cancelled,
        std::function<bool()> live,
        ET::ToolInvocationRecordV2& output,
        std::function<void(const ET::ToolInvocationStatusV2&)> progress)
    {
        if (!m_impl->m_ready || !live || cancelled)
        {
            return { Error::Stopped };
        }
        ET::ToolAdmissionContext expected;
        expected.m_attemptId = request.m_attemptId;
        expected.m_requestFingerprint = request.m_fingerprint;
        expected.m_commandFingerprint = request.m_commandFingerprint;
        expected.m_profileFingerprint = ET::ToolDigest(ET::ToolExecutionProfile);
        expected.m_executableDigest = phase.m_configuration.m_executableDigest;
        expected.m_configurationFingerprint = phase.m_configuration.m_configurationFingerprint;
        expected.m_rootIdentities = phase.m_rootIdentities;
        if (!m_impl->m_admission->Arm(
                expected,
                [&cancelled, live = std::move(live)]
                {
                    return !cancelled && live();
                }))
        {
            return { Error::Busy };
        }
        struct Revoke
        {
            std::shared_ptr<FrameworkPendingAdmission> m_gate;
            ~Revoke()
            {
                m_gate->Revoke();
            }
        } revoke{ m_impl->m_admission };
        auto deadline = Clock::now() + std::chrono::seconds(10);
        ET::ToolResult submitted;
        do
        {
            submitted = m_impl->m_service->Submit(request);
            if (submitted || submitted.m_error != ET::ToolError::Busy)
            {
                break;
            }
            if (cancelled)
            {
                return { Error::Cancelled };
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (Clock::now() < deadline);
        if (!submitted)
        {
            return { Error::ToolFailed };
        }
        bool cancellationSent = false;
        while (true)
        {
            if (cancelled && !cancellationSent)
            {
                m_impl->m_admission->Revoke();
                m_impl->m_service->Cancel(request.m_attemptId);
                cancellationSent = true;
            }
            ET::ToolInvocationStatusV2 status;
            if (!m_impl->m_service->GetStatus(request.m_attemptId, status))
            {
                return { Error::ToolFailed };
            }
            if (progress)
            {
                progress(status);
            }
            if (status.m_stage == ET::ToolStage::Terminal)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        size_t offset = 0;
        do
        {
            auto page = m_impl->m_service->EnumerateRecords(offset, 64);
            if (page.m_error != ET::ToolError::None)
            {
                return { Error::ToolFailed };
            }
            for (const auto& record : page.m_records)
            {
                if (record.m_status.m_attemptId == request.m_attemptId)
                {
                    if (record.m_requestFingerprint != request.m_fingerprint ||
                        record.m_commandFingerprint != request.m_commandFingerprint ||
                        record.m_profileFingerprint != expected.m_profileFingerprint)
                    {
                        return { Error::Drifted };
                    }
                    output = record;
                    return {};
                }
            }
            if (page.m_records.empty() || page.m_nextOffset <= offset)
            {
                break;
            }
            offset = page.m_nextOffset;
        } while (offset < ET::ToolMaxRecords);
        return { Error::NotFound };
    }
    AZStd::vector<ET::ToolInvocationRecordV2> FrameworkToolExecutionAdapter::Records() const
    {
        AZStd::vector<ET::ToolInvocationRecordV2> records;
        for (size_t offset = 0; offset < ET::ToolMaxRecords;)
        {
            auto page = m_impl->m_service->EnumerateRecords(offset, 64);
            if (page.m_error != ET::ToolError::None || page.m_records.empty())
            {
                break;
            }
            for (auto& record : page.m_records)
            {
                if (record.m_status.m_stage == ET::ToolStage::Terminal)
                {
                    records.push_back(AZStd::move(record));
                }
            }
            if (page.m_nextOffset <= offset)
            {
                break;
            }
            offset = page.m_nextOffset;
        }
        // An empty snapshot does not prove that M2 initialization or cleanup completed.
        return records;
    }
    void FrameworkToolExecutionAdapter::Shutdown()
    {
        if (m_impl && m_impl->m_service)
        {
            m_impl->m_service->Shutdown();
            m_impl->m_ready = false;
        }
    }
    const AZStd::string& FrameworkToolExecutionAdapter::Store() const
    {
        return m_impl->m_store;
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
