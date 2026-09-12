/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExecutionFramework/FrameworkToolExecutionAdapter.h"
#include "FoundationService.h"
#include "FrameworkExecutionTestFixtures.h"
#include "FrameworkPlannerTestFixtures.h"
#include "SourceEvidenceRegistry.h"
#include <AzCore/Interface/Interface.h>
#include <Execution/Platform/Windows/ToolSandbox_Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <winioctl.h>
using namespace TaintedGrailModdingSDK::ExecutionFramework;
using namespace TaintedGrailModdingSDK::ExecutionFramework::Tests;

namespace
{
    bool RemoveFixtureTree(const std::wstring& path, const AZStd::string& expectedIdentity, size_t depth = 0)
    {
        if (depth > 32)
        {
            return false;
        }
        {
            ET::Windows::PinnedPath pin;
            if (!pin.Open(ET::Windows::Utf8(path), true) || pin.m_identity != expectedIdentity)
            {
                return false;
            }
            std::error_code ec;
            for (std::filesystem::directory_iterator it(path, ec), end; it != end && !ec; it.increment(ec))
            {
                const auto attributes = GetFileAttributesW(it->path().c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                {
                    return false;
                }
                if (attributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    AZStd::string identity;
                    {
                        ET::Windows::PinnedPath child;
                        if (!child.Open(ET::Windows::Utf8(it->path().wstring()), true))
                        {
                            return false;
                        }
                        identity = child.m_identity;
                    }
                    if (!RemoveFixtureTree(it->path().wstring(), identity, depth + 1))
                    {
                        return false;
                    }
                }
            }
            if (ec)
            {
                return false;
            }
        }
        return ET::Windows::RemoveOwnedTree(path, expectedIdentity);
    }
    AZStd::string PhaseKey(CE::Phase phase)
    {
        AZStd::string token(CE::Token(phase));
        for (auto& c : token)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c += 'a' - 'A';
            }
        }
        return token;
    }
} // namespace

class FrameworkNative : public ::testing::Test
{
protected:
    Fixture m_fixture;
    TaintedGrailModdingSDK::GameProfile m_profile;
    AZStd::string m_root, m_identity, m_executableDigest;
    std::shared_ptr<ET::ToolResolvedConfiguration> m_current;
    void SetUp() override
    {
        m_profile.m_profileId = "profile.synthetic";
        m_profile.m_gameVersion = "1.0.0";
        m_profile.m_branch = "synthetic";
        m_profile.m_runtimeTarget = "Mono";
        m_fixture = Fixture(ProfileFingerprint(m_profile));
        wchar_t executable[32768]{};
        auto length = GetEnvironmentVariableW(L"FOA_M2_FIXTURE", executable, 32768);
        ASSERT_GT(length, 0u) << "Native M2 fixture required; no skips.";
        ASSERT_LT(length, 32768u);
        wchar_t temp[32768]{};
        ASSERT_GT(GetTempPathW(32768, temp), 0u);
        m_root = ET::Windows::Utf8(std::wstring(temp) + L"foa-m3-" + ET::Windows::Wide(ET::Windows::NewId()));
        ASSERT_TRUE(ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(m_root)));
        ET::Windows::PinnedPath pin;
        ASSERT_TRUE(pin.Open(m_root, true));
        m_identity = pin.m_identity;
        ASSERT_TRUE(ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(m_root + "/target")));
        ET::ToolResolvedConfiguration c;
        c.m_providerId = m_fixture.m_host.m_binding.m_providerId;
        c.m_providerVersion = "1.0.0";
        c.m_probeId = m_fixture.m_host.m_command.m_probeId;
        c.m_toolVersion = "1.0.0";
        c.m_discoveryStatus = ET::DiscoveryStatus::Installed;
        c.m_executablePath = ET::Windows::Utf8(executable);
        AZ::u64 bytes = 0;
        ASSERT_TRUE(ET::Windows::HashFile(executable, c.m_executableDigest, bytes));
        m_executableDigest = c.m_executableDigest;
        c.m_configurationFingerprint = Digest("configuration.native");
        c.m_roots = { { "target", m_root + "/target" } };
        m_current = std::make_shared<ET::ToolResolvedConfiguration>(c);
        m_fixture.m_host.m_resolve = [current = m_current](auto& output)
        {
            output = *current;
            return true;
        };
    }
    void TearDown() override
    {
        if (!m_identity.empty())
        {
            EXPECT_TRUE(RemoveFixtureTree(ET::Windows::Wide(m_root), m_identity));
        }
    }
    void Configure(FrameworkExecutionService& service, bool confirmation = true)
    {
        ASSERT_TRUE(service.Providers().Register(m_fixture.m_host));
        Qualification q;
        q.m_bindingFingerprint = m_fixture.m_host.m_binding.m_fingerprint;
        q.m_profileFingerprint = m_fixture.m_context.m_profileFingerprint;
        q.m_executableDigest = m_executableDigest;
        q.m_observationId = "observation.fixture";
        q.m_evidenceIds = { "evidence.reviewed" };
        q.m_from = Clock::now();
        q.m_until = q.m_from + std::chrono::hours(1);
        ASSERT_TRUE(service.Providers().ReviewQualification(q));
        ASSERT_TRUE(service.Policy().SetPolicy(m_fixture.Policy(confirmation)));
        ASSERT_TRUE(service.Open());
    }
    void ConfigureChain(FrameworkExecutionService& service, bool failMiddle)
    {
        m_fixture.m_descriptor.m_inputContracts = { "data.text" };
        m_fixture.m_descriptor.m_sideEffects.push_back(CE::SideEffect::READ_ONLY);
        m_fixture.m_descriptor.m_requiredPhases = { CE::Phase::MATERIALIZE, CE::Phase::BUILD, CE::Phase::PACKAGE };
        m_fixture.m_descriptor.m_terminalPhase = CE::Phase::PACKAGE;
        ASSERT_TRUE(Seal(m_fixture.m_descriptor));
        m_fixture.m_request.m_terminalPhase = CE::Phase::PACKAGE;
        ASSERT_TRUE(Seal(m_fixture.m_request));
        {
            FrameworkExecutionRepository repository;
            ASSERT_TRUE(repository.Open(m_root + "/store", m_fixture.m_context));
        }
        auto policy = m_fixture.Policy(false);
        policy.m_bindingFingerprints.clear();
        CE::Phase previous = CE::Phase::INVALID;
        for (auto phase : m_fixture.m_descriptor.m_requiredPhases)
        {
            const auto token = PhaseKey(phase);
            auto host = m_fixture.m_host;
            auto& binding = host.m_binding;
            binding.m_id = "binding." + token;
            binding.m_phase = phase;
            binding.m_inputContracts = { "data.text" };
            binding.m_sideEffects = m_fixture.m_descriptor.m_sideEffects;
            host.m_command.m_inputKinds = { "text/plain" };
            host.m_provider.m_commands[0].m_inputKinds = { "text/plain" };
            binding.m_providerFingerprint = FrameworkProviderService::DescriptorFingerprint(host.m_provider);
            ASSERT_TRUE(Seal(binding));
            auto rootId = "root." + token;
            auto path = m_root + "/store/payloads/" + rootId;
            ASSERT_TRUE(ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(path)));
            m_current->m_roots.push_back({ rootId, path });
            auto preview = host.m_preview;
            host.m_preview = [preview, binding, previous, failMiddle](const auto& request, PhasePreview& output)
            {
                auto result = preview(request, output);
                if (!result)
                {
                    return result;
                }
                auto& p = output.m_phase;
                auto token = PhaseKey(binding.m_phase);
                p.m_id = "phase." + token;
                p.m_rollback.m_id = "rollback." + token;
                Seal(p.m_rollback);
                p.m_phase = binding.m_phase;
                p.m_binding = CE::Reference(binding).GetValue();
                p.m_providerFingerprint = binding.m_providerFingerprint;
                auto& expected = p.m_expectedOutputs[0];
                expected.m_id = "artifact." + token;
                expected.m_storageRootId = "root." + token;
                expected.m_producerPhaseId = p.m_id;
                expected.m_expectedDigest = Digest("hello\n");
                Seal(expected);
                if (previous != CE::Phase::INVALID)
                {
                    CE::ArtifactReferenceV1 input;
                    input.m_id = "artifact." + PhaseKey(previous);
                    input.m_payloadContractId = "data.text";
                    input.m_ownerPackId = request.m_packId;
                    input.m_storageRootId = "root." + PhaseKey(previous);
                    input.m_relativePath = "result.txt";
                    input.m_digest = Digest("hello\n");
                    input.m_byteSize = 6;
                    input.m_custodianId = "custodian.framework";
                    input.m_lifecycle = CE::ArtifactLifecycle::VERIFIED;
                    Seal(input);
                    p.m_inputs = { input };
                    output.m_invocation.m_inputs = {
                        { input.m_id, input.m_storageRootId, input.m_relativePath, "text/plain", input.m_digest, input.m_byteSize }
                    };
                }
                if (failMiddle && binding.m_phase == CE::Phase::BUILD)
                {
                    output.m_invocation.m_arguments[0].m_value = "exit";
                }
                Seal(p);
                return Result{};
            };
            ASSERT_TRUE(service.Providers().Register(host));
            Qualification qualification;
            qualification.m_bindingFingerprint = binding.m_fingerprint;
            qualification.m_executableDigest = m_executableDigest;
            qualification.m_profileFingerprint = m_fixture.m_context.m_profileFingerprint;
            qualification.m_observationId = "observation." + token;
            qualification.m_evidenceIds = { "evidence.reviewed" };
            qualification.m_from = Clock::now();
            qualification.m_until = qualification.m_from + std::chrono::hours(1);
            ASSERT_TRUE(service.Providers().ReviewQualification(qualification));
            policy.m_bindingFingerprints.push_back(binding.m_fingerprint);
            previous = phase;
        }
        ASSERT_TRUE(service.Policy().SetPolicy(policy));
        ASSERT_TRUE(service.Open());
    }
    Snapshot Wait(FrameworkExecutionService& service, Snapshot snapshot)
    {
        const auto deadline = Clock::now() + std::chrono::seconds(45);
        while (Clock::now() < deadline)
        {
            EXPECT_TRUE(service.Status(snapshot.m_executionId, snapshot));
            if (!snapshot.m_canCancel)
            {
                return snapshot;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        ADD_FAILURE() << "Framework execution deadline exceeded";
        return snapshot;
    }
};
TEST_F(FrameworkNative, ActualSupervisorSuccessCustodyReceiptAndReopen)
{
    AZStd::string execution;
    {
        FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
        Configure(service);
        CE::CapabilityExecutionPlanV1 plan;
        auto preview = service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan);
        ASSERT_TRUE(preview) << static_cast<int>(preview.m_error);
        ASSERT_TRUE(service.Confirm(plan.m_fingerprint, "actor.host", std::chrono::seconds(60)));
        Snapshot submitted;
        ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
        execution = submitted.m_executionId;
        auto result = Wait(service, submitted);
        EXPECT_EQ(result.m_state, CE::ExecutionState::SUCCEEDED) << static_cast<int>(result.m_error);
        auto history = service.History(0, 64);
        ASSERT_EQ(history.size(), 1u);
        ASSERT_TRUE(history[0].m_receipt);
        EXPECT_EQ(history[0].m_tools.size(), 1u);
        ASSERT_EQ(history[0].m_receipt->m_phaseReceipts[0].m_outputs.size(), 1u);
        CandidateProjection candidate;
        ASSERT_TRUE(ProjectCandidate(history[0], m_profile, candidate));
        EXPECT_EQ(candidate.m_source.m_importStatus, "warning");
        TaintedGrailModdingSDK::SourceEvidenceRegistry registry;
        AZStd::string registrationError;
        ASSERT_TRUE(registry.RegisterSource(candidate.m_source, &registrationError)) << registrationError.c_str();
        for (const auto& evidence : candidate.m_evidence)
        {
            ASSERT_TRUE(registry.RegisterCandidateEvidence(evidence, &registrationError)) << registrationError.c_str();
        }
        EXPECT_EQ(registry.GetCandidateEvidence().size(), 1u);
        EXPECT_TRUE(registry.GetEvidence().empty());
        auto wrongProfile = m_profile;
        wrongProfile.m_gameVersion = "2.0.0";
        EXPECT_EQ(ProjectCandidate(history[0], wrongProfile, candidate).m_error, Error::Drifted);
        Snapshot reused;
        ASSERT_TRUE(service.Submit(plan.m_fingerprint, reused));
        EXPECT_EQ(reused.m_executionId, execution);
        EXPECT_EQ(Wait(service, reused).m_state, CE::ExecutionState::SUCCEEDED);
        EXPECT_EQ(service.History(0, 64).size(), 1u);
    }
    FrameworkExecutionService reopened(m_fixture.m_context, m_root + "/store");
    Configure(reopened);
    Snapshot history;
    ASSERT_TRUE(reopened.Status(execution, history));
    EXPECT_EQ(history.m_state, CE::ExecutionState::SUCCEEDED);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(reopened.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    Snapshot denied;
    ASSERT_TRUE(reopened.Submit(plan.m_fingerprint, denied));
    EXPECT_EQ(Wait(reopened, denied).m_error, Error::AuthorizationRequired);
}
TEST_F(FrameworkNative, InterruptedIntentIsHistoryAndNeverReplayed)
{
    auto plan = m_fixture.Plan();
    StoredAttempt intent;
    intent.m_executionId = "execution.interrupted";
    intent.m_operationKey = plan.m_plan.m_fingerprint;
    intent.m_attempt = 1;
    intent.m_plan = plan.m_plan;
    {
        FrameworkExecutionRepository repository;
        ASSERT_TRUE(repository.Open(m_root + "/store", m_fixture.m_context));
        ASSERT_TRUE(repository.Begin(intent));
    }
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service);
    Snapshot snapshot;
    ASSERT_TRUE(service.Status(intent.m_executionId, snapshot));
    EXPECT_EQ(snapshot.m_error, Error::Quarantined);
    EXPECT_FALSE(service.Busy());
    EXPECT_EQ(service.History(0, 64)[0].m_tools.size(), 0u);
}
TEST_F(FrameworkNative, InterruptedFrameworkReconcilesRealSupervisorObservationWithoutReplay)
{
    CE::CapabilityExecutionPlanV1 plan;
    AZStd::string toolId;
    {
        FrameworkExecutionService planning(m_fixture.m_context, m_root + "/planning");
        Configure(planning, false);
        ASSERT_TRUE(planning.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
        PreparedPhase phase;
        ASSERT_TRUE(planning.Providers().Prepare(m_fixture.m_host, m_fixture.m_request, phase));
        FrameworkExecutionRepository repository;
        ASSERT_TRUE(repository.Open(m_root + "/store", m_fixture.m_context));
        StoredAttempt attempt;
        attempt.m_executionId = "execution.interrupted";
        attempt.m_operationKey = plan.m_fingerprint;
        attempt.m_attempt = 1;
        attempt.m_plan = plan;
        ASSERT_TRUE(repository.Begin(attempt));
        auto invocation = phase.m_preview.m_invocation;
        toolId = invocation.m_attemptId = "tool.interrupted";
        ASSERT_TRUE(repository.PhaseIntent(attempt.m_executionId, 1, phase.m_preview.m_phase.m_fingerprint, invocation));
        FrameworkToolExecutionAdapter adapter(planning.Providers(), m_root + "/store/supervisor");
        std::atomic_bool cancelled{ false };
        ET::ToolInvocationRecordV2 observed;
        ASSERT_TRUE(adapter.Run(
            phase,
            invocation,
            cancelled,
            []
            {
                return true;
            },
            observed));
        ASSERT_TRUE(ET::ToolSucceeded(observed));
        // Cut after actual M2 completion, before Framework custody or final transaction publication.
    }
    {
        FrameworkExecutionService recovered(m_fixture.m_context, m_root + "/store");
        Configure(recovered, false);
        auto deadline = Clock::now() + std::chrono::seconds(12);
        AZStd::vector<StoredAttempt> history;
        do
        {
            history = recovered.History(0, 64);
            if (!history[0].m_recoveredTools.empty())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (Clock::now() < deadline);
        ASSERT_EQ(history.size(), 1u);
        ASSERT_EQ(history[0].m_recoveredTools.size(), 1u);
        EXPECT_EQ(history[0].m_recoveredTools[0].m_status.m_attemptId, toolId);
        EXPECT_TRUE(ET::ToolSucceeded(history[0].m_recoveredTools[0]));
        EXPECT_FALSE(history[0].m_receipt);
        EXPECT_TRUE(history[0].m_quarantined);
        EXPECT_FALSE(recovered.Busy());
        CE::CapabilityExecutionPlanV1 refreshed;
        ASSERT_TRUE(recovered.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, refreshed));
        Snapshot blocked;
        EXPECT_EQ(recovered.Submit(refreshed.m_fingerprint, blocked, true).m_error, Error::Quarantined);
    }
    FrameworkExecutionRepository reopened;
    ASSERT_TRUE(reopened.Open(m_root + "/store", m_fixture.m_context));
    EXPECT_EQ(reopened.Read(0, 64)[0].m_recoveredTools.size(), 1u);
    EXPECT_FALSE(reopened.Read(0, 64)[0].m_receipt);
}
TEST_F(FrameworkNative, ExclusiveWriterAndCorruptionAreRejected)
{
    {
        FrameworkExecutionRepository first;
        ASSERT_TRUE(first.Open(m_root + "/store", m_fixture.m_context));
        FrameworkExecutionRepository second;
        EXPECT_EQ(second.Open(m_root + "/store", m_fixture.m_context).m_error, Error::Busy);
    }
    ASSERT_TRUE(ET::Windows::WriteFileAtomic(ET::Windows::Wide(m_root + "/store/header.json"), "{\"version\":99}"));
    FrameworkExecutionRepository repository;
    EXPECT_EQ(repository.Open(m_root + "/store", m_fixture.m_context).m_error, Error::CorruptStore);
}
TEST_F(FrameworkNative, NativeFailureObservationsNeverBecomeArtifacts)
{
    const auto original = m_fixture.m_host.m_preview;
    for (const char* mode : { "hang", "exit", "malformed", "wrong-hash", "extra" })
    {
        SCOPED_TRACE(mode);
        m_fixture.m_host.m_preview = [original, mode](const auto& request, auto& output)
        {
            auto result = original(request, output);
            output.m_invocation.m_arguments[0].m_value = mode;
            if (AZStd::string_view(mode) == "hang")
            {
                output.m_invocation.m_timeoutMilliseconds = 250;
            }
            return result;
        };
        const auto store = m_root + "/failure." + mode;
        {
            FrameworkExecutionService service(m_fixture.m_context, store);
            Configure(service, false);
            CE::CapabilityExecutionPlanV1 plan;
            ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
            Snapshot submitted;
            ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
            const auto result = Wait(service, submitted);
            EXPECT_EQ(result.m_state, CE::ExecutionState::FAILED);
            auto history = service.History(0, 64);
            ASSERT_EQ(history.size(), 1u);
            ASSERT_TRUE(history[0].m_receipt);
            ASSERT_EQ(history[0].m_tools.size(), 1u);
            const auto& tool = history[0].m_tools[0];
            EXPECT_FALSE(ET::ToolSucceeded(tool));
            if (AZStd::string_view(mode) == "hang")
            {
                EXPECT_EQ(tool.m_status.m_outcome, ET::ToolOutcome::TimedOut);
            }
            else if (AZStd::string_view(mode) == "exit")
            {
                EXPECT_EQ(tool.m_status.m_outcome, ET::ToolOutcome::ExitedNonzero);
            }
            else
            {
                EXPECT_EQ(tool.m_status.m_verification, ET::ToolVerification::Failed);
            }
            EXPECT_TRUE(history[0].m_receipt->m_phaseReceipts[0].m_outputs.empty());
            EXPECT_EQ(history[0].m_receipt->m_outcome, CE::Outcome::FAILED);
            CandidateProjection candidate;
            ASSERT_TRUE(ProjectCandidate(history[0], m_profile, candidate));
            EXPECT_EQ(candidate.m_evidence[0].m_claim, "FAILED");
            EXPECT_EQ(candidate.m_evidence[0].m_confidence, "candidate");
        }
        FrameworkExecutionRepository reopened;
        ASSERT_TRUE(reopened.Open(store, m_fixture.m_context));
        EXPECT_EQ(reopened.Read(0, 64)[0].m_receipt->m_outcome, CE::Outcome::FAILED);
    }
}
TEST_F(FrameworkNative, MissingHeaderAndInterruptedPublicationFailClosed)
{
    auto prepared = m_fixture.Plan();
    StoredAttempt attempt;
    attempt.m_executionId = "execution.publication";
    attempt.m_operationKey = prepared.m_plan.m_fingerprint;
    attempt.m_attempt = 1;
    attempt.m_plan = prepared.m_plan;
    const auto store = m_root + "/store";
    {
        FrameworkExecutionRepository repository;
        ASSERT_TRUE(repository.Open(store, m_fixture.m_context));
        ASSERT_TRUE(repository.Begin(attempt));
        // A partial final write is retained as an interrupted intent, never a receipt.
        ASSERT_TRUE(
            ET::Windows::WriteFileAtomic(ET::Windows::Wide(store + "/transactions/execution.publication.a1/finished.json"), "partial"));
    }
    {
        FrameworkExecutionRepository reopened;
        ASSERT_TRUE(reopened.Open(store, m_fixture.m_context));
        const auto history = reopened.Read(0, 64);
        ASSERT_EQ(history.size(), 1u);
        EXPECT_TRUE(history[0].m_quarantined);
        EXPECT_FALSE(history[0].m_receipt);
    }
    ASSERT_TRUE(
        ET::Windows::WriteFileAtomic(
            ET::Windows::Wide(store + "/transactions/execution.publication.a1/committed"), Digest("wrong-marker")));
    {
        FrameworkExecutionRepository corrupted;
        EXPECT_EQ(corrupted.Open(store, m_fixture.m_context).m_error, Error::CorruptStore);
    }
    ASSERT_TRUE(DeleteFileW(ET::Windows::Wide(store + "/header.json").c_str()));
    FrameworkExecutionRepository missingIdentity;
    EXPECT_EQ(missingIdentity.Open(store, m_fixture.m_context).m_error, Error::CorruptStore);
    EXPECT_EQ(GetFileAttributesW(ET::Windows::Wide(store + "/header.json").c_str()), INVALID_FILE_ATTRIBUTES);
}
TEST_F(FrameworkNative, FinalPublicationFaultRetainsQuarantineAndContextVeto)
{
    auto original = m_fixture.m_host.m_preview;
    m_fixture.m_host.m_preview = [original](const auto& request, auto& output)
    {
        auto result = original(request, output);
        output.m_invocation.m_arguments[0].m_value = "hang";
        return result;
    };
    AZStd::string execution;
    {
        FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
        Configure(service, false);
        CE::CapabilityExecutionPlanV1 plan;
        ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
        Snapshot submitted;
        ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
        execution = submitted.m_executionId;
        auto deadline = Clock::now() + std::chrono::seconds(15);
        do
        {
            ASSERT_TRUE(service.Status(execution, submitted));
            if (submitted.m_toolStage == ET::ToolStage::Running)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (Clock::now() < deadline);
        ASSERT_EQ(submitted.m_toolStage, ET::ToolStage::Running);
        const auto destination = m_root + "/store/transactions/" + execution + ".a1/finished.json";
        ASSERT_TRUE(ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(destination)));
        ASSERT_TRUE(service.Cancel(execution));
        EXPECT_EQ(Wait(service, submitted).m_error, Error::Quarantined);
        EXPECT_FALSE(service.CanChangeContext());
        EXPECT_FALSE(service.History(0, 64)[0].m_receipt);
        EXPECT_NE(GetFileAttributesW(ET::Windows::Wide(destination).c_str()) & FILE_ATTRIBUTE_DIRECTORY, 0u);
    }
    FrameworkExecutionRepository reopened;
    ASSERT_TRUE(reopened.Open(m_root + "/store", m_fixture.m_context));
    EXPECT_TRUE(reopened.Read(0, 64)[0].m_quarantined);
    EXPECT_FALSE(reopened.Read(0, 64)[0].m_receipt);
}
TEST_F(FrameworkNative, RevokedQualificationPreventsAnyAttempt)
{
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    ASSERT_TRUE(service.Confirm(plan.m_fingerprint, "actor.host", std::chrono::seconds(60)));
    service.Providers().RevokeQualification(m_fixture.m_host.m_binding.m_fingerprint);
    Snapshot submitted;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    EXPECT_EQ(Wait(service, submitted).m_error, Error::Unqualified);
    EXPECT_TRUE(service.History(0, 64).empty());
}
TEST_F(FrameworkNative, ConfigurationDriftRequiresAnotherPreview)
{
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    ASSERT_TRUE(service.Confirm(plan.m_fingerprint, "actor.host", std::chrono::seconds(60)));
    m_current->m_configurationFingerprint = Digest("changed.configuration");
    Snapshot submitted;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    EXPECT_EQ(Wait(service, submitted).m_error, Error::Drifted);
    EXPECT_TRUE(service.History(0, 64).empty());
}
TEST_F(FrameworkNative, CancellationAndJoinedShutdownPreserveActualOutcome)
{
    auto preview = m_fixture.m_host.m_preview;
    m_fixture.m_host.m_preview = [preview](const auto& r, auto& out)
    {
        auto result = preview(r, out);
        out.m_invocation.m_arguments[0].m_value = "hang";
        return result;
    };
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service, false);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    Snapshot submitted;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    const auto ready = Clock::now() + std::chrono::seconds(15);
    do
    {
        ASSERT_TRUE(service.Status(submitted.m_executionId, submitted));
        if (submitted.m_toolStage == ET::ToolStage::Running)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (Clock::now() < ready && submitted.m_canCancel);
    ASSERT_EQ(submitted.m_toolStage, ET::ToolStage::Running);
    auto start = Clock::now();
    ASSERT_TRUE(service.Cancel(submitted.m_executionId));
    const auto cancelCallMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    EXPECT_LT(cancelCallMs, 10.0);
    service.Shutdown();
    EXPECT_LT(std::chrono::duration<double>(Clock::now() - start).count(), 6.0);
    ASSERT_TRUE(service.Status(submitted.m_executionId, submitted));
    EXPECT_EQ(submitted.m_state, CE::ExecutionState::CANCELLED);
    auto history = service.History(0, 64);
    ASSERT_EQ(history.size(), 1u);
    ASSERT_EQ(history[0].m_tools.size(), 1u);
    EXPECT_EQ(history[0].m_tools[0].m_status.m_cleanup, ET::ToolCleanup::Complete);
    EXPECT_EQ(history[0].m_receipt->m_outcome, CE::Outcome::CANCELLED);
    std::printf(
        "M3 cancel call %.3f ms; joined shutdown %.3f s\n", cancelCallMs, std::chrono::duration<double>(Clock::now() - start).count());
}
TEST_F(FrameworkNative, ArtifactHardlinkAndContentDriftPreventCompletedReuse)
{
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service, false);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    Snapshot submitted;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    ASSERT_EQ(Wait(service, submitted).m_state, CE::ExecutionState::SUCCEEDED);
    auto file = ET::Windows::Wide(m_root + "/store/payloads/root.artifacts/result.txt");
    ASSERT_TRUE(CreateHardLinkW(ET::Windows::Wide(m_root + "/second-link.txt").c_str(), file.c_str(), nullptr));
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    EXPECT_EQ(Wait(service, submitted).m_error, Error::Drifted);
    EXPECT_EQ(service.History(0, 64).size(), 1u);
    ASSERT_TRUE(DeleteFileW(ET::Windows::Wide(m_root + "/second-link.txt").c_str()));
    ASSERT_TRUE(ET::Windows::WriteFileAtomic(file, "changed"));
    EXPECT_EQ(service.History(0, 64)[0].m_receipt->m_outcome, CE::Outcome::SUCCEEDED); // Historic observation is not rewritten.
}
TEST_F(FrameworkNative, SparseExcessMetadataIsRejectedBeforeReopen)
{
    {
        FrameworkExecutionRepository repository;
        ASSERT_TRUE(repository.Open(m_root + "/store", m_fixture.m_context));
        ET::Windows::Handle file(CreateFileW(
            ET::Windows::Wide(m_root + "/store/oversize.dat").c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        ASSERT_TRUE(file);
        DWORD returned = 0;
        ASSERT_TRUE(DeviceIoControl(file.Get(), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr));
        LARGE_INTEGER size{};
        size.QuadPart = MaximumMetadata + 1;
        ASSERT_TRUE(SetFilePointerEx(file.Get(), size, nullptr, FILE_BEGIN));
        ASSERT_TRUE(SetEndOfFile(file.Get()));
        file.Reset();
        EXPECT_EQ(repository.CheckCapacity(0).m_error, Error::StoreFull);
    }
    FrameworkExecutionRepository reopened;
    EXPECT_EQ(reopened.Open(m_root + "/store", m_fixture.m_context).m_error, Error::StoreFull);
}
TEST_F(FrameworkNative, HundredSubmitCancelReopenCyclesDoNotGrowHandles)
{
    auto preview = m_fixture.m_host.m_preview;
    m_fixture.m_host.m_preview = [preview](const auto& r, auto& out)
    {
        auto result = preview(r, out);
        out.m_invocation.m_arguments[0].m_value = "hang";
        return result;
    };
    DWORD baseline = 0, finalHandles = 0;
    ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &baseline));
    auto started = Clock::now();
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        auto store = m_root + "/cycle." + AZStd::string::format("%d", cycle);
        AZStd::string execution;
        {
            FrameworkExecutionService service(m_fixture.m_context, store);
            Configure(service, false);
            CE::CapabilityExecutionPlanV1 plan;
            ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
            Snapshot submitted;
            ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
            execution = submitted.m_executionId;
            Snapshot attached;
            ASSERT_TRUE(service.Submit(plan.m_fingerprint, attached));
            ASSERT_EQ(execution, attached.m_executionId);
            const auto deadline = Clock::now() + std::chrono::seconds(15);
            do
            {
                ASSERT_TRUE(service.Status(execution, submitted));
                if (submitted.m_toolStage == ET::ToolStage::Running)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } while (Clock::now() < deadline && submitted.m_canCancel);
            ASSERT_EQ(submitted.m_toolStage, ET::ToolStage::Running);
            ASSERT_TRUE(service.Cancel(execution));
            ASSERT_EQ(Wait(service, submitted).m_state, CE::ExecutionState::CANCELLED);
            ASSERT_EQ(service.History(0, 64).size(), 1u);
        }
        {
            FrameworkExecutionService reopened(m_fixture.m_context, store);
            Configure(reopened, false);
            Snapshot history;
            ASSERT_TRUE(reopened.Status(execution, history));
            ASSERT_EQ(history.m_state, CE::ExecutionState::CANCELLED);
            ASSERT_FALSE(reopened.Busy());
        }
        if (cycle == 0)
        {
            ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &baseline));
        }
    }
    ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &finalHandles));
    EXPECT_LE(finalHandles, baseline + 2);
    std::printf(
        "M3 100 submit/cancel/reopen cycles: %.3f s, handles %lu -> %lu\n",
        std::chrono::duration<double>(Clock::now() - started).count(),
        baseline,
        finalHandles);
}
TEST_F(FrameworkNative, MaterializeBuildPackageUseVerifiedPriorOutputs)
{
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    ConfigureChain(service, false);
    CE::CapabilityExecutionPlanV1 plan;
    auto preview = service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan);
    ASSERT_TRUE(preview) << static_cast<int>(preview.m_error);
    ASSERT_EQ(plan.m_phases.size(), 3u);
    Snapshot submitted;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    const auto finished = Wait(service, submitted);
    ASSERT_EQ(finished.m_state, CE::ExecutionState::SUCCEEDED) << static_cast<int>(finished.m_error);
    auto history = service.History(0, 64);
    ASSERT_EQ(history.size(), 1u);
    ASSERT_EQ(history[0].m_tools.size(), 3u);
    for (const auto& tool : history[0].m_tools)
    {
        EXPECT_TRUE(ET::ToolSucceeded(tool));
    }
    ASSERT_EQ(history[0].m_receipt->m_phaseReceipts.size(), 3u);
    for (const auto& phase : history[0].m_receipt->m_phaseReceipts)
    {
        EXPECT_EQ(phase.m_outputs.size(), 1u);
    }
}
TEST_F(FrameworkNative, FailedMiddlePhaseStopsDownstreamAndExplicitRetryRetainsLineage)
{
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    ConfigureChain(service, true);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    Snapshot submitted;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted));
    ASSERT_EQ(Wait(service, submitted).m_state, CE::ExecutionState::FAILED);
    EXPECT_EQ(service.Submit(plan.m_fingerprint, submitted).m_error, Error::Conflict);
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, submitted, true));
    ASSERT_EQ(submitted.m_attempt, 2u);
    ASSERT_EQ(Wait(service, submitted).m_state, CE::ExecutionState::FAILED);
    auto history = service.History(0, 64);
    ASSERT_EQ(history.size(), 2u);
    for (const auto& attempt : history)
    {
        ASSERT_EQ(attempt.m_tools.size(), 2u);
        ASSERT_TRUE(attempt.m_receipt);
        EXPECT_EQ(attempt.m_receipt->m_phaseReceipts[0].m_outcome, CE::Outcome::SUCCEEDED);
        EXPECT_EQ(attempt.m_receipt->m_phaseReceipts[1].m_outcome, CE::Outcome::FAILED);
        EXPECT_EQ(attempt.m_receipt->m_phaseReceipts[2].m_outcome, CE::Outcome::BLOCKED);
    }
    EXPECT_EQ(history[0].m_operationKey, history[1].m_operationKey);
}
TEST_F(FrameworkNative, MaximumCollectionPreviewIsBoundedAndOversizeEmbeddingIsTyped)
{
    for (size_t index = 0; index < CE::MaximumCollection; ++index)
    {
        CE::OptionV1 option;
        option.m_id = "option." + AZStd::string::format("%zu", index);
        option.m_value = AZStd::string(4096, 'x');
        ASSERT_TRUE(Seal(option));
        m_fixture.m_request.m_options.push_back(option);
    }
    ASSERT_TRUE(Seal(m_fixture.m_request));
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service, false);
    CE::CapabilityExecutionPlanV1 plan;
    const auto start = Clock::now();
    auto result = service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan);
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    EXPECT_EQ(result.m_error, Error::Unsupported);
    EXPECT_LE(elapsed, 500.0);
    EXPECT_TRUE(service.History(0, 64).empty());
    std::printf("M3 maximum-collection preview: %.3f ms (typed unsupported receipt embedding)\n", elapsed);
}
TEST_F(FrameworkNative, BoundedQueueAndPendingDuplicatesNeverLaunchExtraWork)
{
    auto preview = m_fixture.m_host.m_preview;
    m_fixture.m_host.m_preview = [preview](const auto& request, auto& output)
    {
        auto result = preview(request, output);
        output.m_invocation.m_arguments[0].m_value = "hang";
        output.m_phase.m_expectedOutputs[0].m_id = "artifact." + request.m_id;
        output.m_phase.m_expectedOutputs[0].m_storageRootId = "root." + request.m_id;
        Seal(output.m_phase.m_expectedOutputs[0]);
        Seal(output.m_phase);
        return result;
    };
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service, false);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, m_fixture.m_request, {}, plan));
    Snapshot active;
    ASSERT_TRUE(service.Submit(plan.m_fingerprint, active));
    auto deadline = Clock::now() + std::chrono::seconds(15);
    do
    {
        ASSERT_TRUE(service.Status(active.m_executionId, active));
        if (active.m_toolStage == ET::ToolStage::Running)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (Clock::now() < deadline);
    ASSERT_EQ(active.m_toolStage, ET::ToolStage::Running);
    for (size_t index = 0; index <= MaximumQueue; ++index)
    {
        auto request = m_fixture.m_request;
        request.m_id = "request.queued." + AZStd::string::format("%zu", index);
        ASSERT_TRUE(Seal(request));
        ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, request, {}, plan));
        Snapshot submitted;
        auto result = service.Submit(plan.m_fingerprint, submitted);
        if (index == MaximumQueue)
        {
            EXPECT_EQ(result.m_error, Error::QueueFull);
        }
        else
        {
            ASSERT_TRUE(result);
            Snapshot duplicate;
            ASSERT_TRUE(service.Submit(plan.m_fingerprint, duplicate));
            EXPECT_EQ(duplicate.m_executionId, submitted.m_executionId);
        }
    }
    service.Shutdown();
    EXPECT_FALSE(service.Busy());
    EXPECT_EQ(service.History(0, 64).size(), 1u);
}
namespace
{
    StoredAttempt HistoryIntent(const Fixture& fixture, size_t index, size_t optionCount)
    {
        auto prepared = fixture.Plan();
        auto& request = prepared.m_request;
        const auto suffix = AZStd::string::format("%zu", index);
        request.m_id = "request.history." + suffix;
        for (size_t option = 0; option < optionCount; ++option)
        {
            CE::OptionV1 value;
            value.m_id = "option." + AZStd::string::format("%zu", option);
            value.m_value = AZStd::string(4096, 'x');
            EXPECT_TRUE(Seal(value));
            request.m_options.push_back(AZStd::move(value));
        }
        EXPECT_TRUE(Seal(request));
        auto reference = CE::Reference(request).GetValue();
        auto& plan = prepared.m_plan;
        plan.m_id = "plan.history." + suffix;
        plan.m_request = reference;
        plan.m_support.m_request = reference;
        EXPECT_TRUE(Seal(plan.m_support));
        plan.m_qualification.m_request = reference;
        EXPECT_TRUE(Seal(plan.m_qualification));
        plan.m_environment.m_request = reference;
        EXPECT_TRUE(Seal(plan.m_environment));
        plan.m_policy.m_request = reference;
        EXPECT_TRUE(Seal(plan.m_policy));
        plan.m_authorizationIntent.m_scope = reference;
        EXPECT_TRUE(Seal(plan.m_authorizationIntent));
        auto& output = plan.m_phases[0].m_expectedOutputs[0];
        output.m_id = "artifact.history." + suffix;
        output.m_relativePath = suffix + "/result.txt";
        EXPECT_TRUE(Seal(output));
        EXPECT_TRUE(Seal(plan.m_phases[0]));
        EXPECT_TRUE(Seal(plan));
        StoredAttempt attempt;
        attempt.m_executionId = "execution.history." + suffix;
        attempt.m_operationKey = plan.m_fingerprint;
        attempt.m_attempt = 1;
        attempt.m_plan = AZStd::move(plan);
        return attempt;
    }
} // namespace
TEST_F(FrameworkNative, CachedStatusAndCancelRemainBoundedAt1024Lineages)
{
    {
        FrameworkExecutionRepository repository;
        ASSERT_TRUE(repository.Open(m_root + "/store", m_fixture.m_context));
        for (size_t index = 0; index < MaximumLineages; ++index)
        {
            ASSERT_TRUE(repository.Begin(HistoryIntent(m_fixture, index, 0)));
        }
        EXPECT_EQ(repository.Begin(HistoryIntent(m_fixture, MaximumLineages, 0)).m_error, Error::StoreFull);
    }
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service);
    EXPECT_EQ(service.Page(0, 1024).size(), 64u);
    EXPECT_FALSE(service.CanChangeContext());
    std::vector<double> statusMs, cancelMs;
    for (size_t sample = 0; sample < 1024; ++sample)
    {
        auto id = "execution.history." + AZStd::string::format("%zu", sample);
        Snapshot snapshot;
        auto begin = Clock::now();
        EXPECT_TRUE(service.Status(id, snapshot));
        statusMs.push_back(std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
        begin = Clock::now();
        EXPECT_EQ(service.Cancel(id).m_error, Error::Conflict);
        cancelMs.push_back(std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
    }
    std::sort(statusMs.begin(), statusMs.end());
    std::sort(cancelMs.begin(), cancelMs.end());
    EXPECT_LT(statusMs[972], 10.0);
    EXPECT_LT(cancelMs[972], 10.0);
    std::printf("M3 1024 lineages: status p95 %.3f ms, cancel p95 %.3f ms\n", statusMs[972], cancelMs[972]);
}
TEST_F(FrameworkNative, MaximumMetadataReopensWithoutReplayWithinBudget)
{
    size_t count = 0;
    AZ::u64 bytes = 0;
    {
        FrameworkExecutionRepository repository;
        ASSERT_TRUE(repository.Open(m_root + "/store", m_fixture.m_context));
        for (; count < MaximumLineages; ++count)
        {
            auto result = repository.Begin(HistoryIntent(m_fixture, count, 10));
            if (!result)
            {
                EXPECT_EQ(result.m_error, Error::StoreFull);
                break;
            }
        }
        bytes = repository.Bytes();
        EXPECT_TRUE(repository.Healthy());
        EXPECT_GT(bytes, MaximumMetadata - 2 * CE::MaximumCanonicalBytes);
        EXPECT_LE(bytes, MaximumMetadata);
    }
    for (int pass = 0; pass < 2; ++pass)
    {
        auto start = Clock::now();
        FrameworkExecutionRepository reopened;
        ASSERT_TRUE(reopened.Open(m_root + "/store", m_fixture.m_context));
        auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        EXPECT_LE(elapsed, 10.0);
        std::printf(
            "M3 metadata reopen pass %d: %llu bytes, %zu lineages, %.3f s\n", pass, static_cast<unsigned long long>(bytes), count, elapsed);
    }
}
AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

// Loaded only from the compiled test DLL by the disposable Editor smoke fixture.
// The shared host interface points to the actual activated Editor Foundation instance.
namespace
{
    AZStd::string s_editorExecution;
}
extern "C" __declspec(dllexport) int FOAM3EditorStart(const char* root, const char* executable, const char* mode)
{
    using namespace TaintedGrailModdingSDK;
    auto* foundation = AZ::Interface<FoundationService>::Get();
    if (!foundation || !root || !executable || !mode || (AZStd::string(mode) != "success" && AZStd::string(mode) != "hang"))
    {
        return 1;
    }
    WorkspaceModel workspace;
    workspace.m_workspaceId = "workspace.synthetic";
    GameProfile profile;
    profile.m_profileId = "profile.synthetic";
    profile.m_gameVersion = "1.0.0";
    profile.m_branch = "synthetic";
    profile.m_runtimeTarget = "Mono";
    workspace.m_gameProfiles = { profile };
    workspace.m_activeGameProfileId = profile.m_profileId;
    if (!foundation->SetWorkspace(workspace))
    {
        return 2;
    }
    PackManifest pack;
    pack.m_packId = "pack.synthetic";
    pack.m_displayName = "Synthetic M3 acceptance";
    pack.m_ownerId = "owner.fixture";
    pack.m_version = "1.0.0";
    AZStd::string error;
    if (!foundation->SetActivePack(pack, &error))
    {
        return 3;
    }
    Fixture fixture(foundation->GetFrameworkProfileFingerprint());
    ET::ToolResolvedConfiguration resolved;
    resolved.m_providerId = fixture.m_host.m_binding.m_providerId;
    resolved.m_providerVersion = "1.0.0";
    resolved.m_probeId = fixture.m_host.m_command.m_probeId;
    resolved.m_toolVersion = "1.0.0";
    resolved.m_discoveryStatus = ET::DiscoveryStatus::Installed;
    resolved.m_executablePath = executable;
    AZ::u64 bytes = 0;
    if (!ET::Windows::HashFile(ET::Windows::Wide(resolved.m_executablePath), resolved.m_executableDigest, bytes))
    {
        return 4;
    }
    resolved.m_configurationFingerprint = Digest("editor.synthetic");
    resolved.m_roots = { { "target", AZStd::string(root) + "/target" } };
    fixture.m_host.m_resolve = [resolved](auto& output)
    {
        output = resolved;
        return true;
    };
    auto preview = fixture.m_host.m_preview;
    fixture.m_host.m_preview = [preview, mode = AZStd::string(mode)](const auto& request, auto& output)
    {
        auto result = preview(request, output);
        output.m_invocation.m_arguments[0].m_value = mode;
        return result;
    };
    fixture.m_host.m_previewContractId = AZStd::string("preview.editor.") + mode;
    Qualification qualification;
    qualification.m_bindingFingerprint = fixture.m_host.m_binding.m_fingerprint;
    qualification.m_profileFingerprint = fixture.m_context.m_profileFingerprint;
    qualification.m_executableDigest = resolved.m_executableDigest;
    qualification.m_observationId = "observation.editor-fixture";
    qualification.m_evidenceIds = { "evidence.reviewed" };
    qualification.m_from = Clock::now();
    qualification.m_until = qualification.m_from + std::chrono::hours(1);
    if (!foundation->ConfigureFrameworkExecution(
            fixture.m_context, AZStd::string(root) + "/store", { fixture.m_host }, { qualification }, fixture.Policy(false), &error))
    {
        return 5;
    }
    auto* service = foundation->GetFrameworkExecution();
    CE::CapabilityExecutionPlanV1 plan;
    auto result = service->Preview(fixture.m_descriptor, fixture.m_request, {}, plan);
    if (!result)
    {
        return 100 + static_cast<int>(result.m_error);
    }
    Snapshot snapshot;
    result = service->Submit(plan.m_fingerprint, snapshot);
    if (!result)
    {
        return 200 + static_cast<int>(result.m_error);
    }
    s_editorExecution = snapshot.m_executionId;
    return 0;
}
extern "C" __declspec(dllexport) int FOAM3EditorStatus()
{
    auto* foundation = AZ::Interface<TaintedGrailModdingSDK::FoundationService>::Get();
    if (!foundation || !foundation->GetFrameworkExecution())
    {
        return -1;
    }
    Snapshot snapshot;
    if (!foundation->GetFrameworkExecution()->Status(s_editorExecution, snapshot))
    {
        return -2;
    }
    if (snapshot.m_canCancel)
    {
        return 0;
    }
    return snapshot.m_state == CE::ExecutionState::SUCCEEDED ? 1 : -100 - static_cast<int>(snapshot.m_error);
}
extern "C" __declspec(dllexport) int FOAM3EditorArtifactCount()
{
    auto* foundation = AZ::Interface<TaintedGrailModdingSDK::FoundationService>::Get();
    if (!foundation || !foundation->GetFrameworkExecution())
    {
        return -1;
    }
    auto history = foundation->GetFrameworkExecution()->History(0, 64);
    int count = 0;
    for (const auto& attempt : history)
    {
        if (attempt.m_receipt)
        {
            for (const auto& phase : attempt.m_receipt->m_phaseReceipts)
            {
                count += static_cast<int>(phase.m_outputs.size());
            }
        }
    }
    return count;
}
extern "C" __declspec(dllexport) int FOAM3EditorContextVeto()
{
    auto* foundation = AZ::Interface<TaintedGrailModdingSDK::FoundationService>::Get();
    if (!foundation)
    {
        return 0;
    }
    auto workspace = foundation->GetWorkspace();
    workspace.m_workspaceId = "workspace.other";
    return !foundation->SetWorkspace(workspace) ? 1 : 0;
}

extern "C" __declspec(dllexport) int FOAM3EditorNativeStage()
{
    auto* foundation = AZ::Interface<TaintedGrailModdingSDK::FoundationService>::Get();
    if (!foundation || !foundation->GetFrameworkExecution())
    {
        return -1;
    }
    Snapshot snapshot;
    if (!foundation->GetFrameworkExecution()->Status(s_editorExecution, snapshot))
    {
        return -2;
    }
    return static_cast<int>(snapshot.m_toolStage);
}


TEST_F(FrameworkNative, M4PlannerSourceIsConsumedOnlyByTheExactFrameworkPreview)
{
    auto input = PlannerTests::MakeBuildRequest();
    m_fixture = Fixture(ProfileFingerprint(input.m_profile));
    m_fixture.m_context.m_packId = input.m_pack.m_packId;
    m_fixture.m_request.m_packId = input.m_pack.m_packId;
    ASSERT_TRUE(Seal(m_fixture.m_request));
    m_fixture.m_host.m_resolve = [current = m_current](auto& output) { output = *current; return true; };
    FrameworkPlannerService planners;
    auto bound = planners.BindBuild(m_fixture.m_request, input);
    ASSERT_TRUE(bound.IsSuccess()) << bound.GetError().c_str();
    auto snapshot = std::make_shared<PlannerSnapshot>(bound.TakeValue());
    const auto original = m_fixture.m_host.m_preview;
    int accepted = 0;
    m_fixture.m_host.m_previewContractId = "foa.planner.build.v1";
    m_fixture.m_host.m_preview = [snapshot, original, &accepted](const auto& request, PhasePreview& output)
    {
        auto source = snapshot->ReadSource(PlannerSourceKind::Build, CE::Phase::BUILD, request);
        if (!source.IsSuccess()) { return Result{Error::Drifted}; }
        if (!CE::Validate(source.GetValue()->m_reference).IsSuccess()) { return Result{Error::Invalid}; }
        ++accepted;
        return original(request, output);
    };
    FrameworkExecutionService service(m_fixture.m_context, m_root + "/store");
    Configure(service);
    CE::CapabilityExecutionPlanV1 plan;
    ASSERT_TRUE(service.Preview(m_fixture.m_descriptor, snapshot->GetRequest(), {}, plan));
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(plan.m_request.m_fingerprint, snapshot->GetRequest().m_fingerprint);
    EXPECT_EQ(plan.m_authorizationIntent.m_state, CE::AuthorizationState::PENDING);
    auto changed = snapshot->GetRequest();
    changed.m_id = "request.changed";
    ASSERT_TRUE(Seal(changed));
    EXPECT_EQ(service.Preview(m_fixture.m_descriptor, changed, {}, plan).m_error, Error::Drifted);
    EXPECT_EQ(accepted, 1);
    EXPECT_TRUE(service.Page(0, 16).empty());
}
