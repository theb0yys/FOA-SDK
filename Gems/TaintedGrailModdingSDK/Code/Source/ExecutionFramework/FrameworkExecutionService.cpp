/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionService.h"
#include "ExecutionSynthetic/FrameworkSyntheticTarget.h"
#include "FrameworkArtifactRepository.h"
#include "FrameworkTargetOwnershipLedger.h"
#include "FrameworkToolExecutionAdapter.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <condition_variable>
#include <deque>
#include <thread>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        CE::FailureRecordV1 Failure(const AZStd::string& id, Error error)
        {
            CE::FailureRecordV1 f;
            f.m_id = "failure." + id;
            f.m_code = "error.framework." + AZStd::string::format("%u", static_cast<unsigned>(error));
            f.m_message = "The Framework attempt did not complete. Inspect the retained phase observation.";
            f.m_retryable = error == Error::ToolFailed || error == Error::Cancelled;
            Seal(f);
            return f;
        }
        template<class T>
        void Decision(T& value, const CE::UpstreamReferenceV1& request, const AZStd::vector<PreparedPhase>& phases, const char* id)
        {
            value.m_id = id;
            value.m_request = request;
            value.m_reason = "reason.framework-preview";
            value.m_evidenceIds = { "evidence.framework-profile" };
            for (const auto& phase : phases)
            {
                value.m_bindingFingerprints.push_back(phase.m_binding.m_fingerprint);
            }
        }
    } // namespace
    struct FrameworkExecutionService::Impl
    {
        Context m_context;
        AZStd::string m_root;
        std::shared_ptr<FrameworkSyntheticTarget> m_synthetic;
        FrameworkProviderService m_providers;
        FrameworkExecutionPolicyService m_policy;
        FrameworkExecutionRepository m_repository;
        FrameworkTargetOwnershipLedger m_ledger;
        FrameworkArtifactRepository m_artifacts;
        std::unique_ptr<FrameworkToolExecutionAdapter> m_tools;
        struct Work
        {
            std::shared_ptr<const PreparedPlan> m_plan;
            Snapshot m_status;
            std::atomic_bool m_cancelled{ false };
            bool m_reuse = false;
            AZStd::optional<StoredAttempt> m_completed;
        };
        mutable std::mutex m_mutex;
        std::condition_variable m_changed;
        AZStd::vector<std::shared_ptr<const PreparedPlan>> m_previews;
        AZStd::vector<std::shared_ptr<Work>> m_work;
        std::deque<std::shared_ptr<Work>> m_queue;
        std::thread m_worker;
        bool m_stopping = false, m_open = false;
        Impl(Context context, AZStd::string root, std::shared_ptr<FrameworkSyntheticTarget> synthetic)
            : m_context(AZStd::move(context))
            , m_root(AZStd::move(root))
            , m_synthetic(AZStd::move(synthetic))
            , m_providers(static_cast<bool>(m_synthetic))
            , m_artifacts(m_root)
        {
        }
        void Status(const std::shared_ptr<Work>& work, CE::ExecutionState state, Error error = Error::None, const AZStd::string& phase = {})
        {
            std::lock_guard lock(m_mutex);
            work->m_status.m_state = state;
            work->m_status.m_error = error;
            work->m_status.m_phaseId = phase;
            work->m_status.m_canCancel =
                state == CE::ExecutionState::READY || state == CE::ExecutionState::EXECUTING || state == CE::ExecutionState::VERIFYING;
        }
        Result Current(const PreparedPlan& plan) const
        {
            CE::CapabilityAuthorizationReceiptV1 authorization;
            auto result = m_policy.Authorize(plan, authorization);
            if (!result)
            {
                return result;
            }
            for (const auto& phase : plan.m_phases)
            {
                result = m_providers.Recheck(phase);
                if (!result)
                {
                    return result;
                }
            }
            return {};
        }
        void Execute(const std::shared_ptr<Work>& work)
        {
            const auto& prepared = *work->m_plan;
            if (work->m_cancelled)
            {
                Status(work, CE::ExecutionState::CANCELLED, Error::Cancelled);
                return;
            }
            auto current = Current(prepared);
            if (!current)
            {
                Status(work, CE::ExecutionState::REJECTED, current.m_error);
                return;
            }
            if (work->m_reuse && work->m_completed)
            {
                if (m_synthetic && !m_synthetic->Check(false))
                {
                    Status(work, CE::ExecutionState::ENVIRONMENT_DRIFTED, Error::Drifted);
                    return;
                }
                for (const auto& phase : work->m_completed->m_receipt->m_phaseReceipts)
                {
                    for (const auto& artifact : phase.m_outputs)
                    {
                        if (!m_artifacts.Verify(artifact) || !m_ledger.Owns(artifact))
                        {
                            Status(work, CE::ExecutionState::ENVIRONMENT_DRIFTED, Error::Drifted);
                            return;
                        }
                    }
                }
                Status(work, CE::ExecutionState::SUCCEEDED);
                return;
            }
            if (m_synthetic)
            {
                auto begin = m_synthetic->Begin(prepared.m_plan);
                if (!begin)
                {
                    Status(work, CE::ExecutionState::REJECTED, begin.m_error);
                    return;
                }
            }
            StoredAttempt stored;
            stored.m_executionId = work->m_status.m_executionId;
            stored.m_operationKey = prepared.m_plan.m_fingerprint;
            stored.m_attempt = work->m_status.m_attempt;
            stored.m_plan = prepared.m_plan;
            auto reservation = m_ledger.Reserve(stored.m_plan);
            if (!reservation)
            {
                Status(work, CE::ExecutionState::REJECTED, reservation.m_error);
                return;
            }
            auto begun = m_repository.Begin(stored);
            if (!begun)
            {
                m_ledger.Release(stored.m_plan.m_fingerprint, true);
                Status(work, CE::ExecutionState::FAILED, begun.m_error);
                return;
            }
            CE::CapabilityExecutionReceiptV1 receipt;
            receipt.m_id = stored.m_executionId;
            receipt.m_plan = CE::Reference(stored.m_plan).GetValue();
            auto allowed = m_policy.Authorize(prepared, receipt.m_authorization);
            if (!allowed)
            {
                Status(work, CE::ExecutionState::FAILED, Error::Quarantined);
                return;
            }
            receipt.m_startedAt = UtcNow();
            receipt.m_verification = CE::VerificationState::NOT_CHECKED;
            receipt.m_assessment = CE::AssessmentState::NOT_ASSESSED;
            receipt.m_promotion = CE::PromotionState::NOT_PROMOTED;
            receipt.m_releaseDecision = CE::ReleaseDecisionState::NOT_DECIDED;
            Error error = Error::None;
            bool cleanup = true;
            for (const auto& preparedPhase : prepared.m_phases)
            {
                const auto& phase = preparedPhase.m_preview.m_phase;
                CE::CapabilityPhaseReceiptV1 phaseReceipt;
                phaseReceipt.m_id = "receipt." + stored.m_executionId + "." + AZStd::string(CE::Token(phase.m_phase));
                phaseReceipt.m_executionId = stored.m_executionId;
                phaseReceipt.m_plan = receipt.m_plan;
                phaseReceipt.m_phasePlan = CE::Reference(phase).GetValue();
                phaseReceipt.m_providerId = phase.m_providerId;
                phaseReceipt.m_providerVersion = phase.m_providerVersion;
                phaseReceipt.m_commandId = phase.m_commandId;
                phaseReceipt.m_providerFingerprint = phase.m_providerFingerprint;
                phaseReceipt.m_configurationFingerprint = phase.m_configurationFingerprint;
                phaseReceipt.m_environmentFingerprint = phase.m_environmentFingerprint;
                phaseReceipt.m_cleanup = CE::CleanupState::NOT_REQUIRED;
                if (work->m_cancelled && error == Error::None)
                {
                    error = Error::Cancelled;
                }
                if (error != Error::None)
                {
                    phaseReceipt.m_outcome = CE::Outcome::BLOCKED;
                    phaseReceipt.m_phaseState = CE::PhaseState::BLOCKED;
                    Seal(phaseReceipt);
                    receipt.m_phaseReceipts.push_back(AZStd::move(phaseReceipt));
                    continue;
                }
                if (m_synthetic && !m_synthetic->Check(phase.m_phase >= CE::Phase::LAUNCH))
                {
                    error = Error::Drifted;
                    phaseReceipt.m_outcome = CE::Outcome::BLOCKED;
                    phaseReceipt.m_phaseState = CE::PhaseState::BLOCKED;
                    Seal(phaseReceipt);
                    receipt.m_phaseReceipts.push_back(AZStd::move(phaseReceipt));
                    continue;
                }
                Status(work, CE::ExecutionState::EXECUTING, Error::None, phase.m_id);
                auto invocation = preparedPhase.m_preview.m_invocation;
                invocation.m_attemptId = NewIdentity("tool");
                AZ::u64 admissionBytes = ET::ToolMaxArtifactBytes; // Upper bound for M2's staged executable.
                for (const auto& input : invocation.m_inputs)
                {
                    admissionBytes += input.m_bytes;
                }
                for (const auto& companion : preparedPhase.m_configuration.m_companions)
                {
                    admissionBytes += companion.m_bytes;
                }
                for (const auto& expected : invocation.m_outputs)
                {
                    admissionBytes += 2 * expected.m_maxBytes;
                }
                auto intent = m_repository.CheckCapacity(admissionBytes, &work->m_cancelled);
                if (intent)
                {
                    intent = m_repository.PhaseIntent(stored.m_executionId, stored.m_attempt, phase.m_fingerprint, invocation);
                }
                ET::ToolInvocationRecordV2 tool;
                auto started = UtcNow();
                Result run = intent;
                if (run)
                {
                    run = m_tools->Run(
                        preparedPhase,
                        invocation,
                        work->m_cancelled,
                        [this, &prepared]
                        {
                            return static_cast<bool>(Current(prepared));
                        },
                        tool,
                        [this, &work](const auto& status)
                        {
                            std::lock_guard lock(m_mutex);
                            work->m_status.m_toolStage = status.m_stage;
                        });
                }
                if (!run)
                {
                    error = run.m_error;
                    phaseReceipt.m_outcome = CE::Outcome::BLOCKED;
                    phaseReceipt.m_phaseState = CE::PhaseState::BLOCKED;
                    phaseReceipt.m_failures.push_back(Failure(phase.m_id, error));
                    // Missing supervisor terminal evidence cannot release a potentially live reservation.
                    cleanup = false;
                }
                else
                {
                    stored.m_tools.push_back(tool);
                    auto encoded = ET::EncodeToolRecord(tool);
                    CE::PhaseExtensionReferenceV1 extension;
                    extension.m_id = "extension." + invocation.m_attemptId;
                    extension.m_extensionContractId = "foa.m2-observation.v2";
                    extension.m_canonicalJson = encoded.m_json;
                    extension.m_extensionFingerprint = encoded.m_fingerprint;
                    Seal(extension);
                    phaseReceipt.m_extension = extension;
                    phaseReceipt.m_attempt = stored.m_attempt;
                    phaseReceipt.m_startedAt = started;
                    phaseReceipt.m_finishedAt = UtcNow();
                    if (tool.m_exitCodeObserved)
                    {
                        phaseReceipt.m_exitCode = static_cast<AZ::s64>(tool.m_exitCode);
                    }
                    bool cleaned =
                        tool.m_status.m_cleanup == ET::ToolCleanup::Complete || tool.m_status.m_cleanup == ET::ToolCleanup::NotRequired;
                    cleanup = cleanup && cleaned;
                    phaseReceipt.m_cleanup = cleaned ? CE::CleanupState::SUCCEEDED : CE::CleanupState::FAILED;
                    if (!ET::ToolSucceeded(tool))
                    {
                        error = tool.m_status.m_outcome == ET::ToolOutcome::Cancelled ? Error::Cancelled : Error::ToolFailed;
                    }
                    else
                    {
                        Status(work, CE::ExecutionState::VERIFYING, Error::None, phase.m_id);
                        auto captured = m_artifacts.Capture(
                            phase,
                            invocation,
                            tool,
                            m_tools->Store(),
                            stored.m_executionId,
                            extension,
                            work->m_cancelled,
                            phaseReceipt.m_outputs,
                            [this](const auto& artifact)
                            {
                                return static_cast<bool>(m_ledger.Owns(artifact));
                            });
                        if (!captured)
                        {
                            error = captured.m_error;
                            cleanup = false;
                        }
                    }
                    if (error == Error::None && m_synthetic)
                    {
                        Result observed;
                        if (phase.m_phase == CE::Phase::DEPLOY)
                        {
                            // Provider completion does not preserve an expired or revoked write grant.
                            observed = work->m_cancelled ? Result{ Error::Cancelled } : Current(prepared);
                            if (observed)
                            {
                                const auto& desired = *phase.m_mutations[0].m_desiredArtifact;
                                observed = m_synthetic->Apply(stored.m_plan, m_artifacts.Path(desired), phaseReceipt.m_observations);
                            }
                        }
                        else if (phase.m_phase == CE::Phase::VERIFY)
                        {
                            observed = m_synthetic->Check(true);
                            receipt.m_verification = observed ? CE::VerificationState::PASSED : CE::VerificationState::FAILED;
                        }
                        else if (phase.m_phase == CE::Phase::ROLLBACK)
                        {
                            CE::RollbackReceiptV1 rollback;
                            observed = m_synthetic->Rollback(stored.m_plan, rollback);
                            if (!rollback.m_fingerprint.empty())
                                receipt.m_rollbackReceipts.push_back(rollback);
                            cleanup = cleanup && static_cast<bool>(observed);
                        }
                        if (!observed)
                            error = observed.m_error;
                    }
                    if (m_synthetic && phase.m_phase == CE::Phase::VERIFY && error != Error::None)
                        receipt.m_verification = CE::VerificationState::FAILED;
                    phaseReceipt.m_outcome = error == Error::None ? CE::Outcome::SUCCEEDED
                        : error == Error::Cancelled               ? CE::Outcome::CANCELLED
                                                                  : CE::Outcome::FAILED;
                    phaseReceipt.m_phaseState = error == Error::None ? CE::PhaseState::SUCCEEDED
                        : error == Error::Cancelled                  ? CE::PhaseState::CANCELLED
                                                                     : CE::PhaseState::FAILED;
                    if (error != Error::None)
                    {
                        phaseReceipt.m_failures.push_back(Failure(phase.m_id, error));
                    }
                }
                if (!Seal(phaseReceipt) || !CE::Validate(phaseReceipt, stored.m_plan, phase, phaseReceipt.m_extension).IsSuccess())
                {
                    m_repository.StopWrites();
                    Status(work, CE::ExecutionState::FAILED, Error::Quarantined);
                    return;
                }
                receipt.m_phaseReceipts.push_back(AZStd::move(phaseReceipt));
            }
            // The confirmed inverse remains bounded cleanup when forward work fails or is cancelled.
            if (m_synthetic && m_synthetic->Pending() && receipt.m_rollbackReceipts.empty())
            {
                CE::RollbackReceiptV1 rollback;
                auto restored = m_synthetic->Rollback(stored.m_plan, rollback);
                if (!rollback.m_fingerprint.empty())
                    receipt.m_rollbackReceipts.push_back(rollback);
                cleanup = cleanup && static_cast<bool>(restored);
                if (!restored && error == Error::None)
                    error = restored.m_error;
            }
            receipt.m_finishedAt = UtcNow();
            receipt.m_outcome = error == Error::None ? CE::Outcome::SUCCEEDED
                : error == Error::Cancelled          ? CE::Outcome::CANCELLED
                                                     : CE::Outcome::FAILED;
            receipt.m_state = error == Error::None ? CE::ExecutionState::SUCCEEDED
                : error == Error::Cancelled        ? CE::ExecutionState::CANCELLED
                                                   : CE::ExecutionState::FAILED;
            if (error != Error::None && !receipt.m_rollbackReceipts.empty())
            {
                receipt.m_state = receipt.m_rollbackReceipts.back().m_state == CE::RollbackState::SUCCEEDED
                    ? CE::ExecutionState::ROLLED_BACK
                    : CE::ExecutionState::ROLLBACK_FAILED;
            }
            if (error != Error::None)
            {
                receipt.m_failures.push_back(Failure(stored.m_executionId, error));
            }
            if (!Seal(receipt))
            {
                m_repository.StopWrites();
                Status(work, CE::ExecutionState::FAILED, Error::Quarantined);
                return;
            }
            stored.m_receipt = receipt;
            stored.m_quarantined = !cleanup;
            if (!m_ledger.Observe(stored.m_plan, receipt, false))
            {
                m_repository.StopWrites();
                Status(work, CE::ExecutionState::FAILED, Error::Quarantined);
                return;
            }
            auto committed = m_repository.Commit(stored);
            if (!committed)
            {
                m_repository.StopWrites();
                Status(work, CE::ExecutionState::FAILED, Error::Quarantined);
                return;
            }
            auto ownership = m_ledger.Observe(stored.m_plan, receipt);
            if (!ownership)
            {
                m_repository.StopWrites();
                Status(work, CE::ExecutionState::FAILED, Error::Quarantined);
                return;
            }
            m_ledger.Release(stored.m_plan.m_fingerprint, cleanup);
            {
                std::lock_guard lock(m_mutex);
                work->m_completed = stored;
                work->m_status.m_receiptFingerprint = receipt.m_fingerprint;
            }
            Status(work, receipt.m_state, cleanup ? error : Error::Quarantined);
        }
        void Loop()
        {
            // Observe only positive M2 recovery results. Missing/unknown cleanup retains quarantine.
            auto reconciliationDeadline = Clock::now() + std::chrono::seconds(10);
            while (true)
            {
                bool unresolved = false;
                for (size_t offset = 0;; offset += 64)
                {
                    auto page = m_repository.Read(offset, 64);
                    if (page.empty())
                    {
                        break;
                    }
                    for (const auto& attempt : page)
                    {
                        if ((!attempt.m_receipt || attempt.m_quarantined) &&
                            attempt.m_recoveredTools.size() < attempt.m_phaseIntents.size())
                        {
                            unresolved = true;
                        }
                    }
                }
                if (!unresolved || !m_repository.Reconcile(m_tools->Records()))
                {
                    break;
                }
                std::unique_lock lock(m_mutex);
                if (m_stopping || Clock::now() >= reconciliationDeadline)
                {
                    break;
                }
                m_changed.wait_for(
                    lock,
                    std::chrono::milliseconds(10),
                    [&]
                    {
                        return m_stopping;
                    });
            }
            while (true)
            {
                std::shared_ptr<Work> work;
                {
                    std::unique_lock lock(m_mutex);
                    m_changed.wait(
                        lock,
                        [&]
                        {
                            return m_stopping || !m_queue.empty();
                        });
                    if (m_queue.empty())
                    {
                        if (m_stopping)
                        {
                            break;
                        }
                        continue;
                    }
                    work = m_queue.front();
                    m_queue.pop_front();
                }
                Execute(work);
            }
        }
    };
    FrameworkExecutionService::FrameworkExecutionService(Context context, AZStd::string root)
        : FrameworkExecutionService(AZStd::move(context), AZStd::move(root), {})
    {
    }
    FrameworkExecutionService::FrameworkExecutionService(
        Context context, AZStd::string root, std::shared_ptr<FrameworkSyntheticTarget> synthetic)
        : m_impl(std::make_unique<Impl>(AZStd::move(context), AZStd::move(root), AZStd::move(synthetic)))
    {
    }
    FrameworkExecutionService::~FrameworkExecutionService()
    {
        Shutdown();
    }
    FrameworkProviderService& FrameworkExecutionService::Providers()
    {
        return m_impl->m_providers;
    }
    FrameworkExecutionPolicyService& FrameworkExecutionService::Policy()
    {
        return m_impl->m_policy;
    }
    Result FrameworkExecutionService::Open()
    {
        if (m_impl->m_open)
        {
            return { Error::Busy };
        }
        auto result = m_impl->m_repository.Open(m_impl->m_root, m_impl->m_context);
        if (!result)
        {
            return result;
        }
        m_impl->m_providers.Finalize();
        size_t offset = 0;
        while (true)
        {
            auto page = m_impl->m_repository.Read(offset, 64);
            if (page.empty())
            {
                break;
            }
            offset += page.size();
            for (const auto& stored : page)
            {
                if (!m_impl->m_ledger.Reserve(stored.m_plan))
                {
                    return { Error::Conflict };
                }
                auto work = std::make_shared<Impl::Work>();
                work->m_status.m_executionId = stored.m_executionId;
                work->m_status.m_planFingerprint = stored.m_plan.m_fingerprint;
                work->m_status.m_attempt = stored.m_attempt;
                if (!stored.m_receipt || stored.m_quarantined)
                {
                    work->m_status.m_state = CE::ExecutionState::FAILED;
                    work->m_status.m_error = Error::Quarantined;
                }
                else
                {
                    if (!m_impl->m_ledger.Observe(stored.m_plan, *stored.m_receipt))
                    {
                        return { Error::Conflict };
                    }
                    m_impl->m_ledger.Release(stored.m_plan.m_fingerprint, true);
                    work->m_status.m_state = stored.m_receipt->m_state;
                    work->m_status.m_receiptFingerprint = stored.m_receipt->m_fingerprint;
                    work->m_completed = stored;
                }
                m_impl->m_work.push_back(work);
            }
        }
        m_impl->m_tools = std::make_unique<FrameworkToolExecutionAdapter>(m_impl->m_providers, m_impl->m_root + "/supervisor");
        m_impl->m_open = true;
        m_impl->m_worker = std::thread(
            [this]
            {
                m_impl->Loop();
            });
        return {};
    }
    Result FrameworkExecutionService::Preview(
        const CE::CapabilityDescriptorV1& descriptor,
        const CE::CapabilityExecutionRequestV1& request,
        const AZStd::vector<AZStd::string>& defaults,
        CE::CapabilityExecutionPlanV1& output)
    {
        if (!m_impl->m_context.Matches(request) || !m_impl->m_providers.Supports(descriptor) ||
            !CE::Validate(request, descriptor).IsSuccess() || defaults.size() > CE::MaximumPhases)
        {
            return { Error::Unsupported };
        }
        PreparedPlan p;
        p.m_descriptor = descriptor;
        p.m_request = request;
        AZStd::vector<CE::Phase> phases = descriptor.m_requiredPhases;
        if (AZStd::find(phases.begin(), phases.end(), request.m_terminalPhase) == phases.end())
        {
            phases.push_back(request.m_terminalPhase);
        }
        AZStd::sort(phases.begin(), phases.end());
        size_t index = 0;
        for (auto phase : phases)
        {
            if (phase > request.m_terminalPhase)
            {
                return { Error::Unsupported };
            }
            HostBinding h;
            auto resolved =
                m_impl->m_providers.Resolve(descriptor, request, phase, index < defaults.size() ? defaults[index] : AZStd::string{}, h);
            if (!resolved)
            {
                return resolved;
            }
            ++index;
            PreparedPhase prepared;
            auto ready = m_impl->m_providers.Prepare(h, request, prepared);
            if (!ready)
            {
                return ready;
            }
            if (m_impl->m_synthetic && !m_impl->m_synthetic->Accepts(prepared.m_preview.m_phase))
                return { Error::Invalid };
            p.m_phases.push_back(AZStd::move(prepared));
        }
        auto& plan = p.m_plan;
        plan.m_id = "plan." + request.m_id;
        plan.m_descriptor = CE::Reference(descriptor).GetValue();
        plan.m_request = CE::Reference(request).GetValue();
        plan.m_support.m_state = CE::SupportState::SUPPORTED;
        plan.m_qualification.m_state = CE::QualificationState::QUALIFIED;
        plan.m_environment.m_state = CE::EnvironmentState::AVAILABLE;
        Decision(plan.m_support, plan.m_request, p.m_phases, "decision.support");
        Decision(plan.m_qualification, plan.m_request, p.m_phases, "decision.qualification");
        Decision(plan.m_environment, plan.m_request, p.m_phases, "decision.environment");
        Decision(plan.m_policy, plan.m_request, p.m_phases, "decision.policy");
        m_impl->m_policy.Evaluate(p, plan.m_policy.m_state, p.m_policyRevision);
        if (m_impl->m_synthetic && plan.m_policy.m_state != CE::PolicyState::CONFIRMATION_REQUIRED)
            return { Error::PolicyDenied };
        plan.m_policy.m_evidenceIds.push_back(
            "evidence.policy." + AZStd::string::format("%llu", static_cast<unsigned long long>(p.m_policyRevision)));
        for (const auto& phase : p.m_phases)
        {
            plan.m_phases.push_back(phase.m_preview.m_phase);
            plan.m_qualification.m_evidenceIds.push_back(
                "evidence.qualification." + AZStd::string::format("%llu", static_cast<unsigned long long>(phase.m_qualificationRevision)));
        }
        plan.m_authorizationIntent.m_id = "authorization.intent";
        plan.m_authorizationIntent.m_scope = plan.m_request;
        plan.m_authorizationIntent.m_state =
            plan.m_policy.m_state == CE::PolicyState::ALLOWED ? CE::AuthorizationState::NOT_REQUIRED : CE::AuthorizationState::PENDING;
        plan.m_authorizationIntent.m_reason = "reason.preview-intent";
        plan.m_authorizationIntent.m_evidenceIds = { "evidence.framework-profile" };
        auto fingerprint = [](auto& value)
        {
            auto encoded = CE::Canonicalize(value);
            if (!encoded.IsSuccess())
            {
                return false;
            }
            value.m_fingerprint = encoded.GetValue().m_fingerprint;
            return true;
        };
        if (!fingerprint(plan.m_support) || !fingerprint(plan.m_qualification) || !fingerprint(plan.m_environment) ||
            !fingerprint(plan.m_policy) || !fingerprint(plan.m_authorizationIntent))
        {
            return { Error::Invalid };
        }
        auto encoded = CE::Canonicalize(plan);
        if (!encoded.IsSuccess())
        {
            return { Error::Invalid };
        }
        if (encoded.GetValue().m_json.size() > CE::MaximumEmbeddedBytes)
        {
            return { Error::Unsupported };
        }
        plan.m_fingerprint = encoded.GetValue().m_fingerprint;
        AZStd::vector<CE::CapabilityProviderBindingV1> bindings;
        for (const auto& phase : p.m_phases)
        {
            bindings.push_back(phase.m_binding);
        }
        if (!CE::Validate(plan, descriptor, request, bindings).IsSuccess())
        {
            return { Error::Invalid };
        }
        auto immutable = std::make_shared<const PreparedPlan>(AZStd::move(p));
        {
            std::lock_guard lock(m_impl->m_mutex);
            if (m_impl->m_stopping)
            {
                return { Error::Stopped };
            }
            for (const auto& old : m_impl->m_previews)
            {
                if (old->m_plan.m_fingerprint == immutable->m_plan.m_fingerprint)
                {
                    immutable = old;
                    break;
                }
            }
            if (AZStd::find(m_impl->m_previews.begin(), m_impl->m_previews.end(), immutable) == m_impl->m_previews.end())
            {
                if (m_impl->m_previews.size() >= MaximumLineages)
                {
                    return { Error::StoreFull };
                }
                m_impl->m_previews.push_back(immutable);
            }
        }
        output = immutable->m_plan;
        return {};
    }
    Result FrameworkExecutionService::Confirm(const AZStd::string& fingerprint, const AZStd::string& actor, std::chrono::seconds lifetime)
    {
        std::shared_ptr<const PreparedPlan> p;
        {
            std::lock_guard lock(m_impl->m_mutex);
            auto found = AZStd::find_if(
                m_impl->m_previews.begin(),
                m_impl->m_previews.end(),
                [&](const auto& value)
                {
                    return value->m_plan.m_fingerprint == fingerprint;
                });
            if (found == m_impl->m_previews.end())
            {
                return { Error::NotFound };
            }
            p = *found;
        }
        CE::CapabilityAuthorizationReceiptV1 receipt;
        return m_impl->m_policy.Confirm(*p, actor, lifetime, receipt);
    }
    Result FrameworkExecutionService::RecoverSynthetic(const AZStd::string& fingerprint, CE::RollbackReceiptV1& receipt)
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (!m_impl->m_synthetic || !m_impl->m_open || m_impl->m_stopping)
            return { Error::Unsupported };
        for (const auto& work : m_impl->m_work)
            if (work->m_status.m_canCancel)
                return { Error::Busy };
        for (const auto& p : m_impl->m_previews)
        {
            if (p->m_plan.m_fingerprint != fingerprint)
                continue;
            auto current = m_impl->Current(*p);
            if (!current)
                return current;
            return m_impl->m_synthetic->Rollback(p->m_plan, receipt);
        }
        return { Error::NotFound };
    }
    Result FrameworkExecutionService::Submit(const AZStd::string& fingerprint, Snapshot& output, bool retry)
    {
        std::lock_guard lock(m_impl->m_mutex);
        if (!m_impl->m_open || m_impl->m_stopping)
        {
            return { Error::Stopped };
        }
        auto preview = AZStd::find_if(
            m_impl->m_previews.begin(),
            m_impl->m_previews.end(),
            [&](const auto& p)
            {
                return p->m_plan.m_fingerprint == fingerprint;
            });
        if (preview == m_impl->m_previews.end())
        {
            return { Error::NotFound };
        }
        std::shared_ptr<Impl::Work> latest;
        for (const auto& work : m_impl->m_work)
        {
            if (work->m_status.m_planFingerprint == fingerprint)
            {
                if (work->m_status.m_canCancel)
                {
                    output = work->m_status;
                    return {};
                }
                if (!latest || latest->m_status.m_attempt < work->m_status.m_attempt)
                {
                    latest = work;
                }
            }
        }
        if (m_impl->m_queue.size() >= MaximumQueue)
        {
            return { Error::QueueFull };
        }
        if (latest && latest->m_status.m_error == Error::Quarantined)
        {
            return { Error::Quarantined };
        }
        if (latest && latest->m_status.m_state != CE::ExecutionState::SUCCEEDED && !retry)
        {
            return { Error::Conflict };
        }
        auto work = std::make_shared<Impl::Work>();
        work->m_plan = *preview;
        if (latest &&
            (latest->m_status.m_state == CE::ExecutionState::SUCCEEDED ||
             (!latest->m_completed && latest->m_status.m_state == CE::ExecutionState::REJECTED)))
        {
            work = latest;
            work->m_plan = *preview;
            work->m_reuse = latest->m_completed.has_value();
        }
        else
        {
            if (latest && latest->m_completed &&
                !AZStd::any_of(
                    latest->m_completed->m_receipt->m_failures.begin(),
                    latest->m_completed->m_receipt->m_failures.end(),
                    [](const auto& failure)
                    {
                        return failure.m_retryable;
                    }))
            {
                return { Error::Conflict };
            }
            if (latest && (latest->m_status.m_attempt >= MaximumAttempts || !latest->m_completed))
            {
                return { Error::Conflict };
            }
            if (m_impl->m_work.size() >= MaximumLineages * MaximumAttempts)
            {
                return { Error::StoreFull };
            }
            work->m_status.m_executionId = NewIdentity("execution");
            work->m_status.m_planFingerprint = fingerprint;
            work->m_status.m_attempt = latest ? latest->m_status.m_attempt + 1 : 1;
            m_impl->m_work.push_back(work);
        }
        work->m_cancelled = false;
        work->m_status.m_error = Error::None;
        work->m_status.m_toolStage = ET::ToolStage::Queued;
        work->m_status.m_state = CE::ExecutionState::READY;
        work->m_status.m_canCancel = true;
        m_impl->m_queue.push_back(work);
        output = work->m_status;
        m_impl->m_changed.notify_one();
        return {};
    }
    Result FrameworkExecutionService::Cancel(const AZStd::string& id)
    {
        std::lock_guard lock(m_impl->m_mutex);
        for (const auto& work : m_impl->m_work)
        {
            if (work->m_status.m_executionId == id)
            {
                if (!work->m_status.m_canCancel)
                {
                    return { Error::Conflict };
                }
                work->m_cancelled = true;
                work->m_status.m_state = CE::ExecutionState::CANCELLATION_REQUESTED;
                return {};
            }
        }
        return { Error::NotFound };
    }
    Result FrameworkExecutionService::Status(const AZStd::string& id, Snapshot& output) const
    {
        std::lock_guard lock(m_impl->m_mutex);
        for (const auto& work : m_impl->m_work)
        {
            if (work->m_status.m_executionId == id)
            {
                output = work->m_status;
                return {};
            }
        }
        return { Error::NotFound };
    }
    AZStd::vector<Snapshot> FrameworkExecutionService::Page(size_t offset, size_t count) const
    {
        std::lock_guard lock(m_impl->m_mutex);
        AZStd::vector<Snapshot> page;
        count = AZStd::min(count, size_t{ 64 });
        for (size_t i = offset; i < m_impl->m_work.size() && page.size() < count; ++i)
        {
            page.push_back(m_impl->m_work[i]->m_status);
        }
        return page;
    }
    AZStd::vector<StoredAttempt> FrameworkExecutionService::History(size_t offset, size_t count) const
    {
        return m_impl->m_repository.Read(offset, count);
    }
    bool FrameworkExecutionService::Busy() const
    {
        std::lock_guard lock(m_impl->m_mutex);
        for (const auto& work : m_impl->m_work)
        {
            if (work->m_status.m_canCancel)
            {
                return true;
            }
        }
        return false;
    }
    bool FrameworkExecutionService::CanChangeContext() const
    {
        std::lock_guard lock(m_impl->m_mutex);
        for (const auto& work : m_impl->m_work)
        {
            if (work->m_status.m_canCancel || work->m_status.m_error == Error::Quarantined)
            {
                return false;
            }
        }
        return true;
    }
    void FrameworkExecutionService::Shutdown()
    {
        if (!m_impl)
        {
            return;
        }
        {
            std::lock_guard lock(m_impl->m_mutex);
            m_impl->m_stopping = true;
            for (const auto& work : m_impl->m_work)
            {
                work->m_cancelled = true;
            }
        }
        m_impl->m_policy.Revoke();
        m_impl->m_changed.notify_all();
        if (m_impl->m_worker.joinable())
        {
            m_impl->m_worker.join();
        }
        if (m_impl->m_tools)
        {
            m_impl->m_tools->Shutdown();
        }
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
