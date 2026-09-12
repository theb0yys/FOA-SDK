/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "ExecutionFramework/FrameworkExecutionEvidenceProjection.h"
#include "ExecutionFramework/FrameworkExecutionService.h"
#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK::ExecutionFramework::Tests
{
    inline AZStd::string Digest(const char* s = "profile.synthetic")
    {
        return ET::ToolDigest(s);
    }
    template<class T>
    T Sealed(T value)
    {
        EXPECT_TRUE(Seal(value));
        return value;
    }
    struct Fixture
    {
        Context m_context{ "workspace.synthetic", "pack.synthetic", Digest() };
        CE::CapabilityDescriptorV1 m_descriptor;
        CE::CapabilityExecutionRequestV1 m_request;
        HostBinding m_host;
        explicit Fixture(AZStd::string profile = Digest())
        {
            m_context.m_profileFingerprint = AZStd::move(profile);
            auto& d = m_descriptor;
            d.m_id = "descriptor.synthetic";
            d.m_capabilityId = "capability.synthetic";
            d.m_outputContracts = { "data.text" };
            d.m_requiredPhases = { CE::Phase::BUILD };
            d.m_terminalPhase = CE::Phase::BUILD;
            d.m_sideEffects = { CE::SideEffect::STAGING_WRITE, CE::SideEffect::PROCESS_LAUNCH };
            d.m_exactProfileRequired = true;
            d.m_saveImpact = CE::SideEffect::READ_ONLY;
            d.m_rollbackRequired = CE::RollbackSupport::NONE;
            EXPECT_TRUE(Seal(d));
            auto& r = m_request;
            r.m_id = "request.synthetic";
            r.m_workspaceId = m_context.m_workspaceId;
            r.m_packId = m_context.m_packId;
            r.m_profileFingerprint = m_context.m_profileFingerprint;
            r.m_capabilityId = d.m_capabilityId;
            r.m_terminalPhase = CE::Phase::BUILD;
            EXPECT_TRUE(Seal(r));
            auto& b = m_host.m_binding;
            b.m_id = "binding.synthetic";
            b.m_capabilityId = d.m_capabilityId;
            b.m_phase = CE::Phase::BUILD;
            b.m_providerId = "provider.synthetic";
            b.m_providerVersion = "1.0.0";
            b.m_versionConstraint = "1.0.0";
            b.m_commandId = "command.run";
            b.m_providerFingerprint = Digest("provider.synthetic.v1");
            b.m_outputContracts = d.m_outputContracts;
            b.m_profileFingerprint = r.m_profileFingerprint;
            b.m_qualificationEvidence = { "evidence.reviewed" };
            b.m_sideEffects = d.m_sideEffects;
            b.m_cancellationSupported = true;
            b.m_rollbackSupport = CE::RollbackSupport::NONE;
            EXPECT_TRUE(Seal(b));
            auto& command = m_host.m_command;
            command.m_providerId = b.m_providerId;
            command.m_providerVersion = b.m_providerVersion;
            command.m_commandId = b.m_commandId;
            command.m_probeId = "probe.fixture";
            command.m_outputKinds = { "text/plain" };
            auto& provider = m_host.m_provider;
            provider.m_providerId = b.m_providerId;
            provider.m_providerVersion = b.m_providerVersion;
            ET::ExternalToolCommandDescriptor c;
            c.m_commandId = b.m_commandId;
            c.m_mode = ET::CommandMode::Batch;
            c.m_supportsCancellation = true;
            c.m_outputKinds = command.m_outputKinds;
            provider.m_commands.push_back(c);
            ET::ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = command.m_probeId;
            provider.m_discoveryProbes.push_back(probe);
            b.m_providerFingerprint = FrameworkProviderService::DescriptorFingerprint(provider);
            EXPECT_TRUE(Seal(b));
            m_host.m_previewContractId = "preview.fixture.v1";
            m_host.m_resolve = [](auto&)
            {
                return false;
            };
            m_host.m_preview = [b, command](const auto& request, PhasePreview& out)
            {
                auto& p = out.m_phase;
                p.m_id = "phase.build";
                p.m_phase = b.m_phase;
                p.m_capabilityId = b.m_capabilityId;
                p.m_profileFingerprint = b.m_profileFingerprint;
                p.m_binding = CE::Reference(b).GetValue();
                p.m_providerId = b.m_providerId;
                p.m_providerVersion = b.m_providerVersion;
                p.m_commandId = b.m_commandId;
                p.m_providerFingerprint = b.m_providerFingerprint;
                p.m_configurationFingerprint = Digest();
                p.m_environmentFingerprint = Digest();
                p.m_targetInventoryFingerprint = Digest("empty.target");
                p.m_rollback.m_id = "rollback.none";
                p.m_rollback.m_support = CE::RollbackSupport::NONE;
                Seal(p.m_rollback);
                CE::ExpectedArtifactV1 a;
                a.m_id = "artifact.result";
                a.m_payloadContractId = "data.text";
                a.m_ownerPackId = request.m_packId;
                a.m_storageRootId = "root.artifacts";
                a.m_relativePath = "result.txt";
                a.m_maximumByteSize = 1024 * 1024;
                a.m_custodianId = "custodian.framework";
                a.m_producerPhaseId = p.m_id;
                a.m_role = "role.result";
                a.m_mediaType = "text/plain";
                a.m_redistribution = CE::RedistributionState::PERMITTED;
                Seal(a);
                p.m_expectedOutputs = { a };
                Seal(p);
                auto& invocation = out.m_invocation;
                invocation.m_providerId = b.m_providerId;
                invocation.m_commandId = b.m_commandId;
                invocation.m_attemptId = "preview.intent";
                invocation.m_commandFingerprint = ET::CanonicalToolCommand(command).m_fingerprint;
                invocation.m_targetRootId = "target";
                invocation.m_arguments = { { ET::ToolArgumentKind::Literal, "success" } };
                invocation.m_outputs = { { "result", a.m_relativePath, a.m_mediaType, a.m_maximumByteSize } };
                invocation.m_fingerprint = ET::CanonicalToolRequest(invocation).m_fingerprint;
                return Result{};
            };
        }
        HostPolicy Policy(bool confirmation = true) const
        {
            HostPolicy p;
            p.m_context = m_context;
            p.m_bindingFingerprints = { m_host.m_binding.m_fingerprint };
            p.m_evidenceId = "evidence.host-policy";
            p.m_confirmationRequired = confirmation;
            p.m_until = Clock::now() + std::chrono::hours(2);
            return p;
        }
        PreparedPlan Plan() const
        {
            PreparedPlan p;
            p.m_descriptor = m_descriptor;
            p.m_request = m_request;
            PreparedPhase phase;
            phase.m_binding = m_host.m_binding;
            m_host.m_preview(m_request, phase.m_preview);
            p.m_phases = { phase };
            auto& plan = p.m_plan;
            plan.m_id = "plan.synthetic";
            plan.m_descriptor = CE::Reference(m_descriptor).GetValue();
            plan.m_request = CE::Reference(m_request).GetValue();
            auto decision = [&](auto& v, const char* id)
            {
                v.m_id = id;
                v.m_request = plan.m_request;
                v.m_reason = "reason.fixture";
                v.m_evidenceIds = { "evidence.fixture" };
                v.m_bindingFingerprints = { m_host.m_binding.m_fingerprint };
                EXPECT_TRUE(Seal(v));
            };
            plan.m_support.m_state = CE::SupportState::SUPPORTED;
            decision(plan.m_support, "decision.support");
            plan.m_qualification.m_state = CE::QualificationState::QUALIFIED;
            decision(plan.m_qualification, "decision.qualification");
            plan.m_environment.m_state = CE::EnvironmentState::AVAILABLE;
            decision(plan.m_environment, "decision.environment");
            plan.m_policy.m_state = CE::PolicyState::CONFIRMATION_REQUIRED;
            decision(plan.m_policy, "decision.policy");
            plan.m_authorizationIntent.m_id = "authorization.intent";
            plan.m_authorizationIntent.m_scope = plan.m_request;
            plan.m_authorizationIntent.m_state = CE::AuthorizationState::PENDING;
            plan.m_authorizationIntent.m_reason = "reason.preview";
            plan.m_authorizationIntent.m_evidenceIds = { "evidence.fixture" };
            EXPECT_TRUE(Seal(plan.m_authorizationIntent));
            plan.m_phases = { phase.m_preview.m_phase };
            EXPECT_TRUE(Seal(plan));
            EXPECT_TRUE(ValidateStoredPlan(plan));
            return p;
        }
    };
} // namespace TaintedGrailModdingSDK::ExecutionFramework::Tests
