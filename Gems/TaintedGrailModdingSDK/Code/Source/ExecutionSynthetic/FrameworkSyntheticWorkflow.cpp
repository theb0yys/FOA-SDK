/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkSyntheticWorkflow.h"
#include "ExecutionFramework/FrameworkExecutionRepository.h"
#include "ExecutionFramework/FrameworkProviderService.h"
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
#include <Execution/Platform/Windows/ToolSandbox_Windows.h>
#endif
namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        AZStd::string Key(CE::Phase phase)
        {
            AZStd::string s(CE::Token(phase));
            for (auto& c : s)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            return s;
        }
        CE::ArtifactReferenceV1 Ref(const char* id, const char* root, const char* path, const char* bytes, const AZStd::string& owner)
        {
            CE::ArtifactReferenceV1 a;
            a.m_id = id;
            a.m_payloadContractId = "data.synthetic";
            a.m_ownerPackId = owner;
            a.m_storageRootId = root;
            a.m_relativePath = path;
            a.m_digest = ET::ToolDigest(bytes);
            a.m_byteSize = strlen(bytes);
            a.m_custodianId = "custodian.framework";
            a.m_lifecycle = CE::ArtifactLifecycle::VERIFIED;
            Seal(a);
            return a;
        }
    } // namespace
    Result PrepareSyntheticWorkflow(
        const Context& context,
        const AZStd::string& store,
        const AZStd::string& executable,
        const std::shared_ptr<FrameworkSyntheticTarget>& target,
        SyntheticWorkflow& output,
        CE::Phase faultPhase,
        const AZStd::string& fault)
    {
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        namespace W = ET::Windows;
        if (!target || context.m_profileFingerprint != FrameworkSyntheticTarget::Profile() || !CE::IsStableId(context.m_workspaceId) ||
            !CE::IsStableId(context.m_packId) || context.m_packId != target->Backup().m_ownerPackId ||
            (fault != "normal" && fault != "fail" && fault != "hang" && fault != "delay"))
            return { Error::Invalid };
        SyntheticWorkflow w;
        auto& d = w.m_descriptor;
        d.m_id = "descriptor.synthetic-spine";
        d.m_capabilityId = FrameworkSyntheticTarget::Capability;
        d.m_inputContracts = { "data.synthetic" };
        d.m_outputContracts = { "data.synthetic" };
        d.m_requiredPhases = { CE::Phase::BUILD,  CE::Phase::PACKAGE, CE::Phase::DEPLOY,
                               CE::Phase::LAUNCH, CE::Phase::VERIFY,  CE::Phase::ROLLBACK };
        d.m_terminalPhase = CE::Phase::ROLLBACK;
        d.m_sideEffects = { CE::SideEffect::READ_ONLY, CE::SideEffect::STAGING_WRITE, CE::SideEffect::PROCESS_LAUNCH };
        d.m_exactProfileRequired = true;
        d.m_saveImpact = CE::SideEffect::READ_ONLY;
        d.m_rollbackRequired = CE::RollbackSupport::EXACT_RESTORE;
        if (!Seal(d))
            return { Error::Invalid };
        auto& request = w.m_request;
        request.m_id = "request.synthetic-spine";
        request.m_capabilityId = d.m_capabilityId;
        request.m_workspaceId = context.m_workspaceId;
        request.m_packId = context.m_packId;
        request.m_profileFingerprint = context.m_profileFingerprint;
        request.m_terminalPhase = d.m_terminalPhase;
        auto seed = Ref("artifact.seed", "root.seed", "seed.txt", "value:42\n", context.m_packId);
        request.m_inputs = { seed, target->Backup() };
        if (!Seal(request))
            return { Error::Invalid };
        // Initialize or verify the M3 context before adding any source/staging directory.
        // The temporary repository writer is released before Foundation opens the service.
        {
            FrameworkExecutionRepository repository;
            auto opened = repository.Open(store, context);
            if (!opened)
                return opened;
        }
        auto directory = [](const AZStd::string& path)
        {
            W::PinnedPath pin;
            return pin.Open(path, true) || W::CreatePrivateDirectory(W::Wide(path));
        };
        if (!directory(store + "/seed"))
            return { Error::StorageFailed };
        auto seedPath = W::Wide(store + "/seed/seed.txt");
        AZStd::string seedBytes;
        if (GetFileAttributesW(seedPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            if (!W::WriteFileAtomic(seedPath, "value:42\n"))
                return { Error::StorageFailed };
        }
        else if (!W::ReadFileBounded(seedPath, 64, seedBytes) || seedBytes != "value:42\n")
            return { Error::Drifted };
        AZ::u64 exeBytes = 0;
        if (!W::HashFile(W::Wide(executable), w.m_executableDigest, exeBytes))
            return { Error::Invalid };
        AZStd::vector<CE::ArtifactReferenceV1> artifacts;
        const char* bytes[] = { FrameworkSyntheticTarget::Payload,
                                "FOA-PACKAGE-V1\nFOA-SYNTHETIC-V1\nVALUE=42\n",
                                FrameworkSyntheticTarget::Payload,
                                "observed=42\n",
                                "verified=42\n",
                                FrameworkSyntheticTarget::Baseline };
        const char* paths[] = { "payload.txt", "package.txt", "payload.txt", "result.txt", "verified.txt", "baseline.txt" };
        for (size_t n = 0; n < 6; ++n)
        {
            auto key = Key(d.m_requiredPhases[n]);
            auto a = Ref(("artifact." + key).c_str(), ("root." + key).c_str(), paths[n], bytes[n], context.m_packId);
            artifacts.push_back(a);
            if (!directory(store + "/payloads/" + a.m_storageRootId))
                return { Error::StorageFailed };
        }
        for (size_t n = 0; n < 6; ++n)
        {
            auto phase = d.m_requiredPhases[n];
            const auto key = Key(phase);
            HostBinding h;
            auto& b = h.m_binding;
            b.m_id = "binding.synthetic." + key;
            b.m_capabilityId = d.m_capabilityId;
            b.m_phase = phase;
            b.m_providerId = "provider.synthetic-spine";
            b.m_providerVersion = "1.0.0";
            b.m_versionConstraint = "1.0.0";
            b.m_commandId = "command." + key;
            b.m_inputContracts = d.m_inputContracts;
            b.m_outputContracts = d.m_outputContracts;
            b.m_profileFingerprint = context.m_profileFingerprint;
            b.m_qualificationEvidence = { "evidence.synthetic-native" };
            b.m_sideEffects = d.m_sideEffects;
            b.m_cancellationSupported = true;
            b.m_rollbackSupport = CE::RollbackSupport::EXACT_RESTORE;
            auto& command = h.m_command;
            command.m_providerId = b.m_providerId;
            command.m_providerVersion = b.m_providerVersion;
            command.m_commandId = b.m_commandId;
            command.m_probeId = "probe.synthetic-native";
            command.m_inputKinds = { "text/plain" };
            command.m_outputKinds = { "text/plain" };
            // Every binding shares one descriptor with all commands; command identity remains phase-specific.
            auto& provider = h.m_provider;
            provider.m_providerId = b.m_providerId;
            provider.m_providerVersion = b.m_providerVersion;
            for (auto member : d.m_requiredPhases)
            {
                ET::ExternalToolCommandDescriptor c;
                c.m_commandId = "command." + Key(member);
                c.m_mode = ET::CommandMode::Batch;
                c.m_supportsCancellation = true;
                c.m_inputKinds = { "text/plain" };
                c.m_outputKinds = { "text/plain" };
                provider.m_commands.push_back(c);
            }
            ET::ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = command.m_probeId;
            provider.m_discoveryProbes.push_back(probe);
            b.m_providerFingerprint = FrameworkProviderService::DescriptorFingerprint(provider);
            if (!Seal(b))
                return { Error::Invalid };
            ET::ToolResolvedConfiguration config;
            config.m_providerId = b.m_providerId;
            config.m_providerVersion = b.m_providerVersion;
            config.m_probeId = command.m_probeId;
            config.m_toolVersion = "1.0.0";
            config.m_discoveryStatus = ET::DiscoveryStatus::Installed;
            config.m_executablePath = executable;
            config.m_executableDigest = w.m_executableDigest;
            config.m_configurationFingerprint = ET::ToolDigest("foa-synthetic-config-v1/" + key);
            config.m_roots = { { "target.synthetic", target->Root() }, { "root.seed", store + "/seed" } };
            for (const auto& a : artifacts)
            {
                auto path = store + "/payloads/" + a.m_storageRootId;
                // M2 reads the deployed bytes, not the cached deploy output, for launch and verification.
                if (a.m_storageRootId == "root.deploy" && (phase == CE::Phase::LAUNCH || phase == CE::Phase::VERIFY))
                    path = target->Root();
                config.m_roots.push_back({ a.m_storageRootId, path });
            }
            h.m_resolve = [config](auto& current)
            {
                current = config;
                return true;
            };
            h.m_previewContractId = "preview.synthetic-spine.v1";
            h.m_preview = [b, command, artifacts, seed, backup = target->Backup(), inventory = target->Inventory(), n, faultPhase, fault](
                              const auto& r, PhasePreview& out)
            {
                auto& p = out.m_phase;
                const auto key = Key(b.m_phase);
                p.m_id = "phase." + key;
                p.m_phase = b.m_phase;
                p.m_capabilityId = b.m_capabilityId;
                p.m_profileFingerprint = b.m_profileFingerprint;
                p.m_binding = CE::Reference(b).GetValue();
                p.m_providerId = b.m_providerId;
                p.m_providerVersion = b.m_providerVersion;
                p.m_commandId = b.m_commandId;
                p.m_providerFingerprint = b.m_providerFingerprint;
                p.m_targetInventoryFingerprint = inventory;
                p.m_configurationFingerprint = ET::ToolDigest("config");
                p.m_environmentFingerprint = ET::ToolDigest("environment");
                p.m_rollback.m_id = "rollback." + key;
                p.m_rollback.m_support = CE::RollbackSupport::NONE;
                if (n == 0)
                    p.m_inputs = { seed };
                else if (n == 4)
                    p.m_inputs = { artifacts[2], artifacts[3] };
                else if (n == 5)
                    p.m_inputs = { backup };
                else
                    p.m_inputs = { artifacts[n - 1] };
                const auto& a = artifacts[n];
                CE::ExpectedArtifactV1 expected;
                expected.m_id = a.m_id;
                expected.m_payloadContractId = a.m_payloadContractId;
                expected.m_ownerPackId = r.m_packId;
                expected.m_storageRootId = a.m_storageRootId;
                expected.m_relativePath = a.m_relativePath;
                expected.m_expectedDigest = a.m_digest;
                expected.m_maximumByteSize = 65536;
                expected.m_custodianId = a.m_custodianId;
                expected.m_producerPhaseId = p.m_id;
                expected.m_role = "role.synthetic";
                expected.m_mediaType = "text/plain";
                expected.m_redistribution = CE::RedistributionState::PERMITTED;
                Seal(expected);
                p.m_expectedOutputs = { expected };
                if (b.m_phase == CE::Phase::DEPLOY)
                {
                    CE::TargetMutationClaimV1 m;
                    m.m_id = "mutation.synthetic-payload";
                    m.m_targetRootId = "target.synthetic";
                    m.m_relativePath = "payload.txt";
                    m.m_operation = CE::MutationOperation::REPLACE;
                    m.m_ownerPackId = r.m_packId;
                    m.m_preimagePresence = CE::PreimagePresence::PRESENT;
                    m.m_preimageFingerprint = backup.m_digest;
                    m.m_preimageOwnerId = r.m_packId;
                    m.m_desiredArtifact = a;
                    m.m_backupArtifact = backup;
                    m.m_rollbackStepId = "step.synthetic-restore";
                    Seal(m);
                    p.m_mutations = { m };
                    CE::RollbackStepV1 s;
                    s.m_id = m.m_rollbackStepId;
                    s.m_mutationId = m.m_id;
                    s.m_action = CE::RollbackAction::RESTORE_BACKUP;
                    s.m_targetRootId = m.m_targetRootId;
                    s.m_relativePath = m.m_relativePath;
                    s.m_ownerPackId = r.m_packId;
                    s.m_expectedCurrentFingerprint = a.m_digest;
                    s.m_restoreFingerprint = backup.m_digest;
                    s.m_backupArtifact = backup;
                    Seal(s);
                    p.m_rollback.m_support = CE::RollbackSupport::EXACT_RESTORE;
                    p.m_rollback.m_steps = { s };
                }
                Seal(p.m_rollback);
                Seal(p);
                auto& inv = out.m_invocation;
                inv.m_attemptId = "preview.intent";
                inv.m_providerId = b.m_providerId;
                inv.m_commandId = b.m_commandId;
                inv.m_targetRootId = "target.synthetic";
                inv.m_commandFingerprint = ET::CanonicalToolCommand(command).m_fingerprint;
                inv.m_arguments = { { ET::ToolArgumentKind::Literal, key } };
                for (const auto& input : p.m_inputs)
                {
                    inv.m_inputs.push_back(
                        { input.m_id, input.m_storageRootId, input.m_relativePath, "text/plain", input.m_digest, input.m_byteSize });
                    inv.m_arguments.push_back({ ET::ToolArgumentKind::Input, input.m_id });
                }
                inv.m_outputs = { { "output", a.m_relativePath, "text/plain", 65536 } };
                inv.m_arguments.push_back({ ET::ToolArgumentKind::Output, "output" });
                inv.m_arguments.push_back({ ET::ToolArgumentKind::Literal, b.m_phase == faultPhase ? fault : AZStd::string("normal") });
                inv.m_timeoutMilliseconds = b.m_phase == faultPhase && fault == "hang" ? 5000 : 10000;
                inv.m_fingerprint = ET::CanonicalToolRequest(inv).m_fingerprint;
                return Result{};
            };
            w.m_bindings.push_back(AZStd::move(h));
        }
        output = AZStd::move(w);
        return {};
#else
        (void)context;
        (void)store;
        (void)executable;
        (void)target;
        (void)output;
        (void)faultPhase;
        (void)fault;
        return { Error::Unsupported };
#endif
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
