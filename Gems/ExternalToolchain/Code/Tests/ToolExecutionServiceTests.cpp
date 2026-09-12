/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "Execution/ToolExecutionRedactor.h"
#include "Execution/ToolExecutionService.h"
#include <AzTest/AzTest.h>
namespace ExternalToolchain
{
    TEST(ToolExecutionService, DefaultHostCannotMintExecutionPermission)
    {
        ToolExecutionService service;
        ToolExecutionCommandV2 c;
        c.m_providerId = "fixture";
        c.m_providerVersion = "1.0.0";
        c.m_commandId = "run";
        c.m_probeId = "exe";
        ExternalToolProviderDescriptor p;
        p.m_providerId = c.m_providerId;
        p.m_providerVersion = c.m_providerVersion;
        ExternalToolCommandDescriptor cmd;
        cmd.m_commandId = c.m_commandId;
        cmd.m_mode = CommandMode::Batch;
        p.m_commands.push_back(cmd);
        ExternalToolDiscoveryProbeDescriptor probe;
        probe.m_probeId = c.m_probeId;
        p.m_discoveryProbes.push_back(probe);
        service.ObserveProvider(p);
        ASSERT_TRUE(service.RegisterExecutionCommand(c));
        service.FinalizeRegistration();
        EXPECT_EQ(service.RegisterExecutionCommand(c).m_error, ToolError::RegistrationClosed);
        ToolInvocationRequestV2 r;
        r.m_attemptId = "attempt";
        r.m_providerId = c.m_providerId;
        r.m_commandId = c.m_commandId;
        r.m_targetRootId = "target";
        r.m_commandFingerprint = CanonicalToolCommand(c).m_fingerprint;
        r.m_fingerprint = CanonicalToolRequest(r).m_fingerprint;
        EXPECT_EQ(service.Submit(r).m_error, ToolError::Disabled);
        EXPECT_EQ(service.Cancel("unknown").m_error, ToolError::NotFound);
        EXPECT_EQ(service.EnumerateRecords(0, 65).m_error, ToolError::InvalidContract);
        service.Shutdown();
        EXPECT_EQ(service.Submit(r).m_error, ToolError::HostStopped);
    }
    TEST(ToolExecutionService, ExecutionRegistrationMustMatchARegisteredBatchProvider)
    {
        ToolExecutionService service;
        ToolExecutionCommandV2 c;
        c.m_providerId = "fixture";
        c.m_providerVersion = "1.0.0";
        c.m_commandId = "run";
        c.m_probeId = "exe";
        EXPECT_EQ(service.RegisterExecutionCommand(c).m_error, ToolError::ProviderMismatch);
    }
    TEST(ToolExecutionRedactor, SplitMarkersAreNeverPublished)
    {
        AZStd::string marker = "s:/synthetic/private";
        ToolExecutionRedactor redactor({ marker });
        AZStd::string output;
        for (char c : AZStd::string("prefix S:\\SYNTHETIC\\PRIVATE/file suffix"))
        {
            output += redactor.Push(&c, 1);
        }
        output += redactor.Push("", 0, true);
        EXPECT_EQ(output, "prefix [redacted]/file suffix");
    }
    TEST(ToolExecutionRedactor, ControlsAndMalformedUtf8AreRemoved)
    {
        ToolExecutionRedactor redactor({});
        AZStd::string bytes = "a\x1b[31mb\x1b[0m";
        bytes.push_back('\xff');
        auto output = redactor.Push(bytes.data(), bytes.size(), true);
        EXPECT_EQ(output, "ab?");
    }
} // namespace ExternalToolchain
