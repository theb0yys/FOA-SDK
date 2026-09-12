/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ToolExecutionService.h"
#include "ToolExecutionJournal.h"
#include "ToolProcessBackend.h"
#include <AzCore/Math/Uuid.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace ExternalToolchain
{
    struct ToolExecutionService::Impl
    {
        struct Entry
        {
            ToolInvocationRequestV2 m_request;
            ToolExecutionCommandV2 m_command;
            ToolInvocationRecordV2 m_record;
            AZStd::string m_out, m_err;
            std::atomic_bool m_cancel{ false };
        };
        ToolExecutionHostConfiguration m_configuration;
        ToolExecutionJournal m_journal;
        AZStd::string m_hostInstance = AZ::Uuid::CreateRandom().ToString<AZStd::string>(false, false);
        mutable std::mutex m_mutex;
        std::condition_variable m_changed;
        std::map<AZStd::string, ExternalToolProviderDescriptor> m_providers;
        std::map<AZStd::string, ToolExecutionCommandV2> m_commands;
        std::map<AZStd::string, std::shared_ptr<Entry>> m_entries;
        std::deque<std::shared_ptr<Entry>> m_queue;
        std::set<AZStd::string> m_activeProviders, m_activeTargets, m_blockedTargets;
        std::thread m_workers[2];
        bool m_finalized = false, m_stopping = false, m_ready = false;
        ToolError m_initializationError = ToolError::Busy;

        explicit Impl(ToolExecutionHostConfiguration c)
            : m_configuration(AZStd::move(c))
        {
            if (m_configuration.m_enabled)
            {
                m_workers[0] = std::thread(
                    [this]
                    {
                        Initialize();
                        Work();
                    });
                m_workers[1] = std::thread(
                    [this]
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_changed.wait(
                            lock,
                            [this]
                            {
                                return m_ready || m_stopping;
                            });
                        lock.unlock();
                        Work();
                    });
            }
        }
        ~Impl()
        {
            Stop();
        }
        void Initialize()
        {
            auto result = m_journal.Open(m_configuration.m_privateStoreRoot);
            AZStd::vector<ToolInvocationRecordV2> records;
            if (result)
            {
                result = MakeToolProcessBackend()->Recover(m_journal);
            }
            if (result)
            {
                result = m_journal.Load(records);
            }
            std::map<AZStd::string, std::shared_ptr<Entry>> restored;
            if (result)
            {
                for (auto& r : records)
                {
                    auto entry = std::make_shared<Entry>();
                    entry->m_record = AZStd::move(r);
                    auto logs = m_journal.ReadLogs(entry->m_record.m_status.m_attemptId, entry->m_out, entry->m_err);
                    if (!logs)
                    {
                        entry->m_record.m_status.m_persistence = ToolPersistence::Failed;
                        entry->m_record.m_status.m_error = logs.m_error;
                    }
                    restored.emplace(entry->m_record.m_status.m_attemptId, AZStd::move(entry));
                }
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            m_entries = std::move(restored);
            m_initializationError = result.m_error;
            m_ready = true;
            m_changed.notify_all();
        }
        void Stop()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopping = true;
                for (auto& [id, e] : m_entries)
                {
                    (void)id;
                    e->m_cancel = true;
                }
                m_changed.notify_all();
            }
            for (auto& worker : m_workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }
        void Work()
        {
            for (;;)
            {
                std::shared_ptr<Entry> e;
                bool reserved = false, unavailable = false;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    auto available = [this]
                    {
                        return std::find_if(
                            m_queue.begin(),
                            m_queue.end(),
                            [this](const auto& candidate)
                            {
                                return candidate->m_cancel ||
                                    (!m_activeProviders.count(candidate->m_request.m_providerId) &&
                                     !m_activeTargets.count(candidate->m_request.m_targetRootId));
                            });
                    };
                    m_changed.wait(
                        lock,
                        [&]
                        {
                            return (m_stopping && m_queue.empty()) || available() != m_queue.end();
                        });
                    if (m_stopping && m_queue.empty())
                    {
                        return;
                    }
                    auto it = available();
                    e = *it;
                    m_queue.erase(it);
                    unavailable = m_initializationError != ToolError::None || m_blockedTargets.count(e->m_request.m_targetRootId);
                    if (!e->m_cancel && !unavailable)
                    {
                        m_activeProviders.insert(e->m_request.m_providerId);
                        m_activeTargets.insert(e->m_request.m_targetRootId);
                        reserved = true;
                    }
                }
                ToolInvocationRecordV2 record = e->m_record;
                if (unavailable)
                {
                    record.m_status.m_error = ToolError::CleanupFailed;
                    record.m_status.m_cleanup = ToolCleanup::NotRequired;
                }
                else if (e->m_cancel)
                {
                    record.m_status.m_outcome = ToolOutcome::Cancelled;
                    record.m_status.m_cleanup = ToolCleanup::NotRequired;
                }
                else
                {
                    ToolRunCallbacks callbacks;
                    callbacks.m_observe = [this, e](const ToolInvocationRecordV2& r)
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        AZ::u64 next = e->m_record.m_status.m_sequence + 1;
                        e->m_record = r;
                        e->m_record.m_status.m_sequence = std::max(next, r.m_status.m_sequence);
                    };
                    callbacks.m_beginDrain = [this, e]
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        e->m_record.m_status.m_stage = ToolStage::Draining;
                        ++e->m_record.m_status.m_sequence;
                        return e->m_cancel.load();
                    };
                    callbacks.m_log = [this, e](bool err, const AZStd::string& text)
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        auto& log = err ? e->m_err : e->m_out;
                        size_t take = std::min(ToolMaxLogBytes - log.size(), text.size());
                        log.append(text.data(), take);
                        if (take < text.size())
                        {
                            if (err)
                            {
                                e->m_record.m_stderrTruncated = true;
                            }
                            else
                            {
                                e->m_record.m_stdoutTruncated = true;
                            }
                        }
                    };
                    record = MakeToolProcessBackend()->Run(
                        e->m_request, e->m_command, m_configuration, m_hostInstance, m_journal, e->m_cancel, callbacks);
                }
                record.m_status.m_stage = ToolStage::Terminal;
                record.m_finishedUtcMilliseconds = static_cast<AZ::u64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    record.m_stdoutTruncated |= e->m_record.m_stdoutTruncated;
                    record.m_stderrTruncated |= e->m_record.m_stderrTruncated;
                    record.m_status.m_sequence = std::max(record.m_status.m_sequence, e->m_record.m_status.m_sequence) + 1;
                }
                auto logs = m_journal.SaveLogs(record.m_status.m_attemptId, e->m_out, e->m_err);
                record.m_status.m_persistence =
                    logs && record.m_status.m_persistence != ToolPersistence::Failed ? ToolPersistence::Durable : ToolPersistence::Failed;
                if (!logs)
                {
                    record.m_status.m_error = logs.m_error;
                }
                auto saved = m_journal.Save(record);
                if (!saved)
                {
                    record.m_status.m_persistence = ToolPersistence::Failed;
                    record.m_status.m_error = saved.m_error;
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    e->m_record = AZStd::move(record);
                    if (e->m_record.m_status.m_persistence == ToolPersistence::Failed)
                    {
                        m_initializationError = ToolError::JournalFailed;
                    }
                    if (e->m_record.m_status.m_cleanup == ToolCleanup::Failed)
                    {
                        m_blockedTargets.insert(e->m_request.m_targetRootId);
                        m_initializationError = ToolError::CleanupFailed;
                    }
                    if (reserved)
                    {
                        m_activeProviders.erase(e->m_request.m_providerId);
                        m_activeTargets.erase(e->m_request.m_targetRootId);
                    }
                    m_changed.notify_all();
                }
            }
        }
    };

    ToolExecutionService::ToolExecutionService(ToolExecutionHostConfiguration c)
        : m_impl(std::make_unique<Impl>(AZStd::move(c)))
    {
    }
    ToolExecutionService::~ToolExecutionService()
    {
        Shutdown();
    }
    void ToolExecutionService::Connect()
    {
        ToolExecutionRequestBus::Handler::BusConnect();
    }
    void ToolExecutionService::Shutdown()
    {
        ToolExecutionRequestBus::Handler::BusDisconnect();
        m_impl->Stop();
    }
    void ToolExecutionService::ObserveProvider(const ExternalToolProviderDescriptor& p)
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        if (!m_impl->m_finalized && m_impl->m_providers.size() < 256)
        {
            m_impl->m_providers.emplace(p.m_providerId, p);
        }
    }
    void ToolExecutionService::FinalizeRegistration()
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        m_impl->m_finalized = true;
    }
    ToolResult ToolExecutionService::RegisterExecutionCommand(const ToolExecutionCommandV2& c)
    {
        auto canonical = CanonicalToolCommand(c);
        if (!canonical)
        {
            return { canonical.m_error };
        }
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        if (m_impl->m_finalized)
        {
            return { ToolError::RegistrationClosed };
        }
        auto provider = m_impl->m_providers.find(c.m_providerId);
        if (provider == m_impl->m_providers.end() || provider->second.m_providerVersion != c.m_providerVersion ||
            std::none_of(
                provider->second.m_commands.begin(),
                provider->second.m_commands.end(),
                [&](const auto& cmd)
                {
                    return cmd.m_commandId == c.m_commandId && cmd.m_mode == CommandMode::Batch;
                }) ||
            std::none_of(
                provider->second.m_discoveryProbes.begin(),
                provider->second.m_discoveryProbes.end(),
                [&](const auto& p)
                {
                    return p.m_probeId == c.m_probeId && p.m_kind == DiscoveryProbeKind::File;
                }))
        {
            return { ToolError::ProviderMismatch };
        }
        if (m_impl->m_commands.size() >= 256)
        {
            return { ToolError::QueueFull };
        }
        if (!m_impl->m_commands.emplace(c.m_providerId + "/" + c.m_commandId, c).second)
        {
            return { ToolError::Duplicate };
        }
        return {};
    }
    ToolResult ToolExecutionService::Submit(const ToolInvocationRequestV2& r)
    {
        // Cardinality/byte checks precede all copies. No filesystem, discovery or process work here.
        auto canonical = CanonicalToolRequest(r);
        if (!canonical)
        {
            return { canonical.m_error };
        }
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        if (m_impl->m_stopping)
        {
            return { ToolError::HostStopped };
        }
        if (!m_impl->m_configuration.m_enabled)
        {
            return { ToolError::Disabled };
        }
        if (!m_impl->m_ready)
        {
            return { ToolError::Busy };
        }
        if (m_impl->m_initializationError != ToolError::None)
        {
            return { m_impl->m_initializationError };
        }
        if (!m_impl->m_finalized)
        {
            return { ToolError::RegistrationClosed };
        }
        auto command = m_impl->m_commands.find(r.m_providerId + "/" + r.m_commandId);
        if (command == m_impl->m_commands.end())
        {
            return { ToolError::ProviderMismatch };
        }
        auto valid = ValidateToolRequest(r, command->second);
        if (!valid)
        {
            return valid;
        }
        if (m_impl->m_entries.count(r.m_attemptId))
        {
            return { ToolError::Duplicate };
        }
        if (m_impl->m_blockedTargets.count(r.m_targetRootId))
        {
            return { ToolError::CleanupFailed };
        }
        if (m_impl->m_queue.size() >= 16)
        {
            return { ToolError::QueueFull };
        }
        if (m_impl->m_entries.size() >= ToolMaxRecords)
        {
            return { ToolError::StoreFull };
        }
        auto e = std::make_shared<Impl::Entry>();
        e->m_request = r;
        e->m_command = command->second;
        e->m_record.m_status.m_attemptId = r.m_attemptId;
        e->m_record.m_status.m_sequence = 1;
        e->m_record.m_requestFingerprint = r.m_fingerprint;
        e->m_record.m_commandFingerprint = r.m_commandFingerprint;
        e->m_record.m_profileFingerprint = ToolDigest(ToolExecutionProfile);
        m_impl->m_entries.emplace(r.m_attemptId, e);
        m_impl->m_queue.push_back(e);
        m_impl->m_changed.notify_all();
        return {};
    }
    ToolResult ToolExecutionService::GetStatus(const AZStd::string& id, ToolInvocationStatusV2& status) const
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        auto it = m_impl->m_entries.find(id);
        if (it == m_impl->m_entries.end())
        {
            return { ToolError::NotFound };
        }
        status = it->second->m_record.m_status;
        return {};
    }
    ToolResult ToolExecutionService::Cancel(const AZStd::string& id)
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        auto it = m_impl->m_entries.find(id);
        if (it == m_impl->m_entries.end())
        {
            return { ToolError::NotFound };
        }
        if (it->second->m_record.m_status.m_stage < ToolStage::Draining)
        {
            it->second->m_cancel = true;
        }
        m_impl->m_changed.notify_all();
        return {};
    }
    ToolLogPage ToolExecutionService::ReadLog(const AZStd::string& id, bool err, AZ::u64 cursor) const
    {
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        auto it = m_impl->m_entries.find(id);
        if (it == m_impl->m_entries.end())
        {
            return { ToolError::NotFound, {}, {}, false };
        }
        const auto& e = *it->second;
        const auto& log = err ? e.m_err : e.m_out;
        if (cursor > log.size())
        {
            return { ToolError::InvalidContract, {}, {}, false };
        }
        size_t take = std::min(ToolMaxLogPage, log.size() - static_cast<size_t>(cursor));
        return { ToolError::None,
                 log.substr(static_cast<size_t>(cursor), take),
                 cursor + take,
                 err ? e.m_record.m_stderrTruncated : e.m_record.m_stdoutTruncated };
    }
    ToolRecordPage ToolExecutionService::EnumerateRecords(size_t offset, size_t count) const
    {
        if (count > 64)
        {
            return { ToolError::InvalidContract, {}, offset };
        }
        std::lock_guard<std::mutex> lock(m_impl->m_mutex);
        if (offset > m_impl->m_entries.size())
        {
            return { ToolError::InvalidContract, {}, offset };
        }
        ToolRecordPage page;
        auto it = m_impl->m_entries.begin();
        std::advance(it, offset);
        while (it != m_impl->m_entries.end() && page.m_records.size() < count)
        {
            page.m_records.push_back(it++->second->m_record);
        }
        page.m_nextOffset = offset + page.m_records.size();
        return page;
    }
} // namespace ExternalToolchain
