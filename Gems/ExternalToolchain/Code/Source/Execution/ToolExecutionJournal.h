/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include <ExternalToolchain/ToolExecutionTypes.h>
#include <memory>
#include <mutex>

namespace ExternalToolchain
{
    struct ToolRecoveryIntent
    {
        AZStd::string m_attemptId, m_stageName, m_profileName, m_stageIdentity;
        bool m_profileCreated = false;
    };
    class ToolExecutionJournal
    {
    public:
        ToolExecutionJournal();
        ~ToolExecutionJournal();
        ToolResult Open(const AZStd::string& privateRoot);
        ToolResult Save(const ToolInvocationRecordV2& record);
        ToolResult SaveLogs(const AZStd::string& attempt, const AZStd::string& out, const AZStd::string& err);
        ToolResult ReadLogs(const AZStd::string& attempt, AZStd::string& out, AZStd::string& err);
        ToolResult WriteIntent(const ToolRecoveryIntent& intent);
        ToolResult ReadIntents(AZStd::vector<ToolRecoveryIntent>& intents);
        ToolResult ClearIntent(const AZStd::string& attempt);
        ToolResult Load(AZStd::vector<ToolInvocationRecordV2>& records);
        AZStd::string Root() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        std::mutex m_mutex;
    };
} // namespace ExternalToolchain
