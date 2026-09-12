/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "ToolExecutionAdmission.h"
#include <ExternalToolchain/ExternalToolchainTypes.h>
#include <ExternalToolchain/ToolExecutionBus.h>
#include <memory>

namespace ExternalToolchain
{
    class ToolExecutionService final : public ToolExecutionRequestBus::Handler
    {
    public:
        explicit ToolExecutionService(ToolExecutionHostConfiguration configuration = {});
        ~ToolExecutionService() override;
        void Connect();
        void Shutdown();
        void ObserveProvider(const ExternalToolProviderDescriptor&);
        void FinalizeRegistration();
        ToolResult RegisterExecutionCommand(const ToolExecutionCommandV2&) override;
        ToolResult Submit(const ToolInvocationRequestV2&) override;
        ToolResult GetStatus(const AZStd::string&, ToolInvocationStatusV2&) const override;
        ToolResult Cancel(const AZStd::string&) override;
        ToolLogPage ReadLog(const AZStd::string&, bool, AZ::u64) const override;
        ToolRecordPage EnumerateRecords(size_t, size_t) const override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace ExternalToolchain
