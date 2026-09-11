/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */


#include "CapabilityExecutionValidation.h"
#include "CanonicalFingerprint.h"
#include <AzTest/AzTest.h>
#include <AzCore/std/algorithm.h>
namespace CE = TaintedGrailModdingSDK::CapabilityExecution;
using namespace CE;
namespace
{
    AZStd::string Digest(char c = 'a') { return "sha256:" + AZStd::string(64, c); }
    template<class T> T Seal(T value)
    {
        value.m_fingerprint.clear();
        auto result = Canonicalize(value);
        EXPECT_TRUE(result.IsSuccess()) << (result.IsSuccess() ? "" : result.GetError().c_str());
        if (result.IsSuccess()) { value.m_fingerprint = result.GetValue().m_fingerprint; }
        return value;
    }
    template<class T> UpstreamReferenceV1 Ref(const T& value)
    {
        auto result = Reference(value); EXPECT_TRUE(result.IsSuccess());
        return result.IsSuccess() ? result.GetValue() : UpstreamReferenceV1{};
    }
    void Valid(const ContractValidation& result)
    { EXPECT_TRUE(result.IsSuccess()) << (result.IsSuccess() ? "" : result.GetError().c_str()); }
    ArtifactReferenceV1 Artifact(const char* id, const char* path, char digest, ArtifactLifecycle state)
    {
        ArtifactReferenceV1 v; v.m_id = id; v.m_payloadContractId = "data.binary"; v.m_ownerPackId = "pack.test";
        v.m_storageRootId = "root.artifacts"; v.m_relativePath = path; v.m_digest = Digest(digest); v.m_byteSize = 12;
        v.m_custodianId = "custodian.test"; v.m_lifecycle = state; return Seal(v);
    }
    FailureRecordV1 Failure()
    {
        FailureRecordV1 v; v.m_id = "failure.test"; v.m_code = "error.synthetic"; v.m_message = "Synthetic failure.";
        return Seal(v);
    }
    struct Chain
    {
        CapabilityDescriptorV1 descriptor;
        AZStd::vector<CapabilityProviderBindingV1> bindings;
        CapabilityExecutionRequestV1 request;
        CapabilityExecutionPlanV1 plan;
        CapabilityAuthorizationReceiptV1 authorization;
        CapabilityExecutionReceiptV1 receipt;
        PhaseExtensionReferenceV1 extension;
        RollbackReceiptV1 rollback;
        ArtifactReferenceV1 original, produced, backup;
        Chain()
        {
            original = Artifact("artifact.source", "source.bin", 'a', ArtifactLifecycle::VERIFIED);
            backup = Artifact("artifact.backup", "backup.bin", 'b', ArtifactLifecycle::BACKUP);
            produced = Artifact("artifact.output", "output.bin", 'c', ArtifactLifecycle::PRODUCED);
            descriptor.m_id = "descriptor.test"; descriptor.m_capabilityId = "capability.test";
            descriptor.m_inputContracts = {"data.binary"}; descriptor.m_outputContracts = {"data.binary"};
            descriptor.m_requiredPhases = {Phase::BUILD, Phase::DEPLOY, Phase::VERIFY}; descriptor.m_terminalPhase = Phase::VERIFY;
            descriptor.m_sideEffects = {SideEffect::READ_ONLY, SideEffect::STAGING_WRITE, SideEffect::INSTALLATION_MUTATION};
            descriptor.m_exactProfileRequired = true; descriptor.m_saveImpact = SideEffect::READ_ONLY;
            descriptor.m_rollbackRequired = RollbackSupport::EXACT_RESTORE; descriptor = Seal(descriptor);
            for (Phase phase : descriptor.m_requiredPhases)
            {
                CapabilityProviderBindingV1 b; b.m_id = "binding." + AZStd::string(Token(phase));
                b.m_capabilityId = descriptor.m_capabilityId; b.m_phase = phase; b.m_providerId = "provider.synthetic";
                b.m_providerVersion = "1.0.0"; b.m_versionConstraint = "1.0.0";
                b.m_commandId = "command." + AZStd::string(Token(phase)); b.m_providerFingerprint = Digest('d');
                b.m_inputContracts = {"data.binary"}; b.m_outputContracts = {"data.binary"}; b.m_profileFingerprint = Digest('e');
                b.m_qualificationEvidence = {"evidence.synthetic"}; b.m_sideEffects = descriptor.m_sideEffects;
                b.m_rollbackSupport = RollbackSupport::EXACT_RESTORE; bindings.push_back(Seal(b));
            }
            request.m_id = "request.test"; request.m_workspaceId = "workspace.test"; request.m_packId = "pack.test";
            request.m_profileFingerprint = Digest('e'); request.m_capabilityId = descriptor.m_capabilityId;
            request.m_terminalPhase = Phase::VERIFY; request.m_inputs = {original, backup};
            for (const auto& b : bindings) { request.m_preferredBindings.push_back(Ref(b)); }
            OptionV1 option; option.m_id = "option.mode"; option.m_value = "synthetic"; request.m_options = {Seal(option)};
            request = Seal(request);
            plan.m_id = "plan.test"; plan.m_descriptor = Ref(descriptor); plan.m_request = Ref(request);
            plan.m_support.m_state = SupportState::SUPPORTED; plan.m_qualification.m_state = QualificationState::QUALIFIED;
            plan.m_environment.m_state = EnvironmentState::AVAILABLE; plan.m_policy.m_state = PolicyState::ALLOWED;
            auto decision = [&](auto value, const char* id)
            {
                value.m_id = id; value.m_request = Ref(request); value.m_reason = "reason.synthetic";
                value.m_evidenceIds = {"evidence.synthetic"};
                for (const auto& b : bindings) { value.m_bindingFingerprints.push_back(b.m_fingerprint); }
                return Seal(value);
            };
            plan.m_support = decision(plan.m_support, "decision.support");
            plan.m_qualification = decision(plan.m_qualification, "decision.qualification");
            plan.m_environment = decision(plan.m_environment, "decision.environment");
            plan.m_policy = decision(plan.m_policy, "decision.policy");
            plan.m_authorizationIntent.m_id = "authorization.intent"; plan.m_authorizationIntent.m_scope = Ref(request);
            plan.m_authorizationIntent.m_state = AuthorizationState::PENDING; plan.m_authorizationIntent.m_reason = "reason.pending";
            plan.m_authorizationIntent.m_evidenceIds = {"evidence.intent"}; plan.m_authorizationIntent = Seal(plan.m_authorizationIntent);
            for (const auto& b : bindings)
            {
                CapabilityPhasePlanV1 p; p.m_id = "phase." + AZStd::string(Token(b.m_phase)); p.m_phase = b.m_phase;
                p.m_capabilityId = b.m_capabilityId; p.m_profileFingerprint = b.m_profileFingerprint; p.m_binding = Ref(b);
                p.m_providerId = b.m_providerId; p.m_providerVersion = b.m_providerVersion; p.m_commandId = b.m_commandId;
                p.m_providerFingerprint = b.m_providerFingerprint; p.m_configurationFingerprint = Digest('f');
                p.m_environmentFingerprint = Digest('0'); p.m_targetInventoryFingerprint = Digest('1');
                p.m_rollback.m_id = "rollback." + AZStd::string(Token(b.m_phase)); p.m_rollback.m_support = RollbackSupport::NONE;
                if (b.m_phase == Phase::BUILD)
                {
                    p.m_inputs = {original};
                    ExpectedArtifactV1 e; e.m_id = produced.m_id; e.m_payloadContractId = produced.m_payloadContractId;
                    e.m_ownerPackId = produced.m_ownerPackId; e.m_storageRootId = produced.m_storageRootId;
                    e.m_relativePath = produced.m_relativePath; e.m_expectedDigest = produced.m_digest; e.m_maximumByteSize = 64;
                    e.m_custodianId = produced.m_custodianId; e.m_producerPhaseId = p.m_id; e.m_role = "role.binary";
                    e.m_mediaType = "application/octet-stream"; e.m_redistribution = RedistributionState::PERMITTED;
                    p.m_expectedOutputs = {Seal(e)};
                }
                if (b.m_phase == Phase::DEPLOY)
                {
                    p.m_inputs = {produced, backup};
                    TargetMutationClaimV1 mutation; mutation.m_id = "mutation.replace"; mutation.m_targetRootId = "root.target";
                    mutation.m_relativePath = "owned.bin"; mutation.m_operation = MutationOperation::REPLACE;
                    mutation.m_ownerPackId = request.m_packId; mutation.m_preimagePresence = PreimagePresence::PRESENT;
                    mutation.m_preimageFingerprint = backup.m_digest; mutation.m_preimageOwnerId = request.m_packId;
                    mutation.m_desiredArtifact = produced; mutation.m_backupArtifact = backup; mutation.m_rollbackStepId = "step.restore";
                    p.m_mutations = {Seal(mutation)};
                    RollbackStepV1 step; step.m_id = mutation.m_rollbackStepId; step.m_mutationId = mutation.m_id;
                    step.m_action = RollbackAction::RESTORE_BACKUP; step.m_targetRootId = mutation.m_targetRootId;
                    step.m_relativePath = mutation.m_relativePath; step.m_ownerPackId = mutation.m_ownerPackId;
                    step.m_expectedCurrentFingerprint = produced.m_digest; step.m_restoreFingerprint = backup.m_digest;
                    step.m_backupArtifact = backup; p.m_rollback.m_support = RollbackSupport::EXACT_RESTORE;
                    p.m_rollback.m_steps = {Seal(step)};
                }
                p.m_rollback = Seal(p.m_rollback); plan.m_phases.push_back(Seal(p));
            }
            plan = Seal(plan);
            authorization.m_id = "authorization.grant"; authorization.m_scope = Ref(plan); authorization.m_state = AuthorizationState::GRANTED;
            authorization.m_actorId = "actor.synthetic"; authorization.m_reason = "reason.synthetic"; authorization.m_evidenceIds = {"evidence.grant"};
            authorization.m_issuedAt = "2026-01-01T00:00:00Z"; authorization.m_expiresAt = "2026-01-02T00:00:00Z";
            authorization = Seal(authorization);
            extension.m_id = "extension.manifest"; extension.m_extensionContractId = "manifest.synthetic";
            extension.m_canonicalJson = "{}"; extension.m_extensionFingerprint = TaintedGrailModdingSDK::CalculateCanonicalSha256("{}");
            extension = Seal(extension);
            receipt.m_id = "execution.test"; receipt.m_plan = Ref(plan); receipt.m_authorization = authorization;
            receipt.m_outcome = Outcome::SUCCEEDED; receipt.m_state = ExecutionState::SUCCEEDED; receipt.m_verification = VerificationState::PASSED;
            receipt.m_assessment = AssessmentState::NOT_ASSESSED; receipt.m_promotion = PromotionState::NOT_PROMOTED;
            receipt.m_releaseDecision = ReleaseDecisionState::NOT_DECIDED;
            receipt.m_startedAt = "2026-01-01T00:01:00Z"; receipt.m_finishedAt = "2026-01-01T00:10:00Z";
            for (const auto& phase : plan.m_phases)
            {
                CapabilityPhaseReceiptV1 r; r.m_id = "receipt." + AZStd::string(Token(phase.m_phase)); r.m_executionId = receipt.m_id;
                r.m_plan = Ref(plan); r.m_phasePlan = Ref(phase); r.m_providerId = phase.m_providerId; r.m_providerVersion = phase.m_providerVersion;
                r.m_commandId = phase.m_commandId; r.m_providerFingerprint = phase.m_providerFingerprint;
                r.m_configurationFingerprint = phase.m_configurationFingerprint; r.m_environmentFingerprint = phase.m_environmentFingerprint;
                r.m_attempt = 1; r.m_outcome = Outcome::SUCCEEDED; r.m_phaseState = PhaseState::SUCCEEDED;
                r.m_startedAt = "2026-01-01T00:02:00Z"; r.m_finishedAt = "2026-01-01T00:03:00Z"; r.m_exitCode = 0;
                r.m_cleanup = CleanupState::NOT_REQUIRED;
                if (phase.m_phase == Phase::BUILD)
                {
                    ArtifactRecordV1 a; a.m_id = produced.m_id; a.m_artifact = produced; a.m_role = "role.binary";
                    a.m_mediaType = "application/octet-stream"; a.m_producerExecutionId = receipt.m_id;
                    a.m_producerPhaseId = phase.m_id; a.m_producerId = phase.m_providerId; a.m_sourceManifest = Ref(extension);
                    a.m_redistribution = RedistributionState::PERMITTED; r.m_outputs = {Seal(a)}; r.m_extension = extension;
                }
                if (phase.m_phase == Phase::DEPLOY)
                {
                    const auto& mutation = phase.m_mutations.front();
                    TargetObservationV1 o; o.m_id = "observation.target"; o.m_mutationId = mutation.m_id;
                    o.m_targetRootId = mutation.m_targetRootId; o.m_relativePath = mutation.m_relativePath;
                    o.m_presence = PreimagePresence::PRESENT; o.m_contentFingerprint = produced.m_digest;
                    o.m_ownerPackId = produced.m_ownerPackId; o.m_verification = VerificationState::PASSED; r.m_observations = {Seal(o)};
                }
                receipt.m_phaseReceipts.push_back(Seal(r));
            }
            receipt = Seal(receipt);
            RollbackStepReceiptV1 s; s.m_id = "receipt.restore"; s.m_stepId = "step.restore"; s.m_outcome = Outcome::SUCCEEDED;
            s.m_observedFingerprint = backup.m_digest; s.m_observedOwnerId = backup.m_ownerPackId;
            s.m_startedAt = "2026-01-01T00:04:00Z"; s.m_finishedAt = "2026-01-01T00:05:00Z";
            rollback.m_id = "receipt.rollback"; rollback.m_plan = Ref(plan); rollback.m_rollbackPlan = Ref(plan.m_phases[1].m_rollback);
            rollback.m_state = RollbackState::SUCCEEDED; rollback.m_steps = {Seal(s)}; rollback = Seal(rollback);
        }
        ContractValidation CheckPlan() const { return Validate(plan, descriptor, request, bindings); }
        ContractValidation CheckReceipt() const { return Validate(receipt, plan, authorization, {extension}); }
    };
}
TEST(CapabilityExecutionValidation, CompleteSyntheticChainAndRollback)
{
    Chain c; Valid(c.CheckPlan()); Valid(c.CheckReceipt()); Valid(Validate(c.rollback, c.plan, c.plan.m_phases[1]));
    auto r = c.receipt; r.m_outcome = Outcome::FAILED; r.m_state = ExecutionState::ROLLED_BACK;
    r.m_failures = {Failure()}; r.m_rollbackReceipts = {c.rollback}; r = Seal(r);
    Valid(Validate(r, c.plan, c.authorization, {c.extension}));
    EXPECT_EQ(r.m_outcome, Outcome::FAILED); EXPECT_EQ(r.m_promotion, PromotionState::NOT_PROMOTED);
}
TEST(CapabilityExecutionValidation, DecisionAxesRemainIndependent)
{
    Chain c; c.plan.m_support.m_state = SupportState::UNSUPPORTED; c.plan.m_support = Seal(c.plan.m_support);
    c.plan.m_policy.m_state = PolicyState::DENIED; c.plan.m_policy = Seal(c.plan.m_policy);
    c.plan.m_environment.m_state = EnvironmentState::DRIFTED; c.plan.m_environment = Seal(c.plan.m_environment);
    c.plan = Seal(c.plan); Valid(c.CheckPlan()); // structural consistency does not grant execution permission
    EXPECT_EQ(c.plan.m_qualification.m_state, QualificationState::QUALIFIED);
    EXPECT_EQ(c.plan.m_authorizationIntent.m_state, AuthorizationState::PENDING);
}
TEST(CapabilityExecutionValidation, CaptureMetadataDoesNotChangeSemanticIdentity)
{
    Chain c; auto r = c.request; r.m_capture.m_label = "Another display label";
    r.m_capture.m_capturedAt = "2026-02-01T00:00:00Z"; r.m_capture.m_diagnosticLocator = "logs/capture.json";
    EXPECT_EQ(Canonicalize(r).GetValue().m_fingerprint, c.request.m_fingerprint);
    r.m_options[0].m_value = "different"; r.m_options[0] = Seal(r.m_options[0]);
    EXPECT_NE(Canonicalize(r).GetValue().m_fingerprint, c.request.m_fingerprint);
}
TEST(CapabilityExecutionValidation, BindingAndProfileDriftRejectedWithoutMutatingInput)
{
    Chain c; const auto original = Canonicalize(c.plan).GetValue().m_json;
    auto p = c.plan.m_phases.front(); p.m_providerVersion = "1.0.1"; p = Seal(p);
    EXPECT_FALSE(Validate(p, c.bindings.front()).IsSuccess());
    p = c.plan.m_phases.front(); p.m_profileFingerprint = Digest('9'); p = Seal(p);
    EXPECT_FALSE(Validate(p, c.bindings.front()).IsSuccess());
    p = c.plan.m_phases.front(); p.m_commandId = "command.other"; p = Seal(p);
    EXPECT_FALSE(Validate(p, c.bindings.front()).IsSuccess());
    auto b = c.bindings.front(); b.m_versionConstraint = "2.0.0"; b = Seal(b); EXPECT_FALSE(Validate(b).IsSuccess());
    EXPECT_EQ(original, Canonicalize(c.plan).GetValue().m_json);
}
TEST(CapabilityExecutionValidation, MissingTerminalAndForwardDependencyRejected)
{
    Chain c; c.plan.m_phases.front().m_inputs = {c.produced}; c.plan.m_phases.front() = Seal(c.plan.m_phases.front());
    c.plan = Seal(c.plan); EXPECT_FALSE(c.CheckPlan().IsSuccess());
    Chain d; d.plan.m_phases.pop_back(); d.plan = Seal(d.plan); EXPECT_FALSE(d.CheckPlan().IsSuccess());
    Chain e; AZStd::reverse(e.plan.m_phases.begin(), e.plan.m_phases.end()); e.plan = Seal(e.plan); EXPECT_FALSE(e.CheckPlan().IsSuccess());
}
TEST(CapabilityExecutionValidation, WrongOwnerBackupAndPostimageGuardRejected)
{
    Chain c; auto mutation = c.plan.m_phases[1].m_mutations.front();
    mutation.m_preimageOwnerId = "pack.other"; mutation = Seal(mutation); EXPECT_FALSE(Validate(mutation).IsSuccess());
    mutation = c.plan.m_phases[1].m_mutations.front(); mutation.m_backupArtifact->m_digest = Digest('9');
    mutation.m_backupArtifact = Seal(*mutation.m_backupArtifact); mutation = Seal(mutation); EXPECT_FALSE(Validate(mutation).IsSuccess());
    auto p = c.plan.m_phases[1]; p.m_rollback.m_steps.front().m_expectedCurrentFingerprint = Digest('8');
    p.m_rollback.m_steps.front() = Seal(p.m_rollback.m_steps.front()); p.m_rollback = Seal(p.m_rollback); p = Seal(p);
    EXPECT_FALSE(Validate(p).IsSuccess());
}
TEST(CapabilityExecutionValidation, ExactAuthorizationAndReceiptBindings)
{
    Chain c; auto r = c.receipt; r.m_authorization.m_scope.m_fingerprint = Digest('9');
    r.m_authorization = Seal(r.m_authorization); r = Seal(r); EXPECT_FALSE(Validate(r, c.plan, c.authorization, {c.extension}).IsSuccess());
    auto phase = c.receipt.m_phaseReceipts.front(); phase.m_providerId = "provider.other"; phase = Seal(phase);
    EXPECT_FALSE(Validate(phase, c.plan, c.plan.m_phases.front(), c.extension).IsSuccess());
    phase = c.receipt.m_phaseReceipts.front(); phase.m_outputs.clear(); phase = Seal(phase);
    EXPECT_FALSE(Validate(phase, c.plan, c.plan.m_phases.front(), c.extension).IsSuccess());
    auto extension = c.extension; extension.m_canonicalJson = "{\"changed\":true}";
    extension.m_extensionFingerprint = TaintedGrailModdingSDK::CalculateCanonicalSha256(extension.m_canonicalJson); extension = Seal(extension);
    EXPECT_FALSE(Validate(c.receipt.m_phaseReceipts.front(), c.plan, c.plan.m_phases.front(), extension).IsSuccess());
}
TEST(CapabilityExecutionValidation, UnattemptedSkippedBlockedAndCancelledCannotClaimOutput)
{
    Chain c;
    for (const auto pair : {AZStd::pair{Outcome::NOT_ATTEMPTED, PhaseState::PENDING}, AZStd::pair{Outcome::SKIPPED, PhaseState::SKIPPED},
        AZStd::pair{Outcome::BLOCKED, PhaseState::BLOCKED}, AZStd::pair{Outcome::CANCELLED, PhaseState::CANCELLED}})
    {
        auto r = c.receipt.m_phaseReceipts.front(); r.m_outcome = pair.first; r.m_phaseState = pair.second;
        r.m_attempt = 0; r.m_startedAt.clear(); r.m_finishedAt.clear(); r.m_exitCode.reset(); r = Seal(r);
        EXPECT_FALSE(Validate(r).IsSuccess()); r.m_outputs.clear(); r = Seal(r); Valid(Validate(r));
    }
}
TEST(CapabilityExecutionValidation, FailurePartialAndRunningShapes)
{
    Chain c; auto r = c.receipt.m_phaseReceipts.front(); r.m_phaseState = PhaseState::FAILED;
    for (const auto outcome : {Outcome::FAILED, Outcome::PARTIAL})
    {
        r.m_outcome = outcome; r.m_failures.clear(); r = Seal(r); EXPECT_FALSE(Validate(r).IsSuccess());
        r.m_failures = {Failure()}; r = Seal(r); Valid(Validate(r));
    }
    r.m_failures.clear(); r.m_outcome = Outcome::RUNNING; r.m_phaseState = PhaseState::RUNNING; r.m_finishedAt.clear();
    r = Seal(r); Valid(Validate(r)); r.m_attempt = 0; r = Seal(r); EXPECT_FALSE(Validate(r).IsSuccess());
}
TEST(CapabilityExecutionValidation, RestoredOwnerAndIndependentFailureRemainRequired)
{
    Chain c; auto rollback = c.rollback; rollback.m_steps.front().m_observedOwnerId = "pack.other";
    rollback.m_steps.front() = Seal(rollback.m_steps.front()); rollback = Seal(rollback);
    EXPECT_FALSE(Validate(rollback, c.plan, c.plan.m_phases[1]).IsSuccess());
    auto r = c.receipt; r.m_state = ExecutionState::ROLLED_BACK; r.m_rollbackReceipts = {c.rollback}; r = Seal(r);
    EXPECT_FALSE(Validate(r).IsSuccess());
    r = c.receipt; r.m_verification = VerificationState::NOT_CHECKED; r = Seal(r);
    EXPECT_FALSE(Validate(r, c.plan, c.authorization, {c.extension}).IsSuccess());
}
TEST(CapabilityExecutionValidation, RedactedDiagnosticsAndTamperDetection)
{
    DiagnosticReferenceV1 d; d.m_id = "diagnostic.test"; d.m_storageRootId = "root.logs";
    d.m_relativePath = "logs/test.json"; d.m_digest = Digest(); d.m_redacted = false; d = Seal(d);
    EXPECT_FALSE(Validate(d).IsSuccess()); d.m_redacted = true; d = Seal(d); Valid(Validate(d));
    d.m_relativePath = "logs/changed.json"; EXPECT_FALSE(Validate(d).IsSuccess());
    EXPECT_FALSE(Reference(d).IsSuccess());
}

TEST(CapabilityExecutionValidation, ArtifactIdentitySizeAndOwnerMustMatchPlan)
{
    Chain c;
    for (int change = 0; change < 4; ++change)
    {
        auto r = c.receipt.m_phaseReceipts.front(); auto& a = r.m_outputs.front().m_artifact;
        if (change == 0) { a.m_digest = Digest('9'); }
        if (change == 1) { a.m_ownerPackId = "pack.other"; }
        if (change == 2) { a.m_byteSize = 65; }
        if (change == 3) { a.m_relativePath = "different.bin"; }
        a = Seal(a); r.m_outputs.front() = Seal(r.m_outputs.front()); r = Seal(r);
        EXPECT_FALSE(Validate(r, c.plan, c.plan.m_phases.front(), c.extension).IsSuccess());
    }
}
TEST(CapabilityExecutionValidation, ReceiptPhaseAndEventTimeMustBelongToExecution)
{
    Chain c; auto r = c.receipt; r.m_phaseReceipts.front().m_phasePlan = Ref(c.plan.m_phases[1]);
    r.m_phaseReceipts.front() = Seal(r.m_phaseReceipts.front()); r = Seal(r);
    EXPECT_FALSE(Validate(r, c.plan, c.authorization, {c.extension}).IsSuccess());
    r = c.receipt; r.m_phaseReceipts.front().m_finishedAt = "2026-01-01T01:00:00Z";
    r.m_phaseReceipts.front() = Seal(r.m_phaseReceipts.front()); r = Seal(r);
    EXPECT_FALSE(Validate(r, c.plan, c.authorization, {c.extension}).IsSuccess());
}
TEST(CapabilityExecutionValidation, FailedExecutionNeedsOriginalFailureAndRollbackCoverage)
{
    Chain c; auto r = c.receipt; r.m_outcome = Outcome::FAILED; r.m_state = ExecutionState::FAILED; r = Seal(r);
    EXPECT_FALSE(Validate(r).IsSuccess()); r.m_failures = {Failure()}; r = Seal(r); Valid(Validate(r));
    r.m_state = ExecutionState::ROLLED_BACK; r = Seal(r);
    EXPECT_FALSE(Validate(r, c.plan, c.authorization, {c.extension}).IsSuccess());
}

TEST(CapabilityExecutionValidation, MutationInverseOrderAndDuplicateTargets)
{
    Chain c; auto p = c.plan.m_phases[1];
    auto mutation = p.m_mutations.front(); mutation.m_id = "mutation.second"; mutation.m_relativePath = "second.bin";
    mutation.m_rollbackStepId = "step.second"; mutation = Seal(mutation); p.m_mutations.push_back(mutation);
    auto step = p.m_rollback.m_steps.front(); step.m_id = mutation.m_rollbackStepId; step.m_mutationId = mutation.m_id;
    step.m_relativePath = mutation.m_relativePath; step = Seal(step); p.m_rollback.m_steps.insert(p.m_rollback.m_steps.begin(), step);
    p.m_rollback = Seal(p.m_rollback); p = Seal(p); Valid(Validate(p));
    const auto original = p.m_fingerprint;
    AZStd::reverse(p.m_rollback.m_steps.begin(), p.m_rollback.m_steps.end());
    p.m_rollback = Seal(p.m_rollback); p = Seal(p); EXPECT_NE(original, p.m_fingerprint); EXPECT_FALSE(Validate(p).IsSuccess());
    p = c.plan.m_phases[1]; mutation.m_relativePath = "OWNED.bin"; mutation = Seal(mutation); p.m_mutations.push_back(mutation);
    step.m_relativePath = mutation.m_relativePath; step = Seal(step); p.m_rollback.m_steps.insert(p.m_rollback.m_steps.begin(), step);
    p.m_rollback = Seal(p.m_rollback); p = Seal(p); EXPECT_FALSE(Validate(p).IsSuccess());
}
TEST(CapabilityExecutionValidation, CreateAndRemoveHaveDifferentInverseShapes)
{
    Chain c; auto p = c.plan.m_phases[1]; auto& m = p.m_mutations.front(); auto& s = p.m_rollback.m_steps.front();
    m.m_operation = MutationOperation::CREATE; m.m_preimagePresence = PreimagePresence::ABSENT;
    m.m_preimageFingerprint.clear(); m.m_preimageOwnerId.clear(); m.m_backupArtifact.reset(); m = Seal(m);
    s.m_action = RollbackAction::REMOVE_CREATED; s.m_restoreFingerprint.clear(); s.m_backupArtifact.reset(); s = Seal(s);
    p.m_rollback = Seal(p.m_rollback); p = Seal(p); Valid(Validate(p));
    p = c.plan.m_phases[1]; auto& remove = p.m_mutations.front(); auto& restore = p.m_rollback.m_steps.front();
    remove.m_operation = MutationOperation::REMOVE; remove.m_desiredArtifact.reset(); remove = Seal(remove);
    restore.m_expectedCurrentFingerprint.clear(); restore = Seal(restore); p.m_rollback = Seal(p.m_rollback); p = Seal(p);
    Valid(Validate(p));
}

TEST(CapabilityExecutionValidation, ProducedLocationCannotOverwriteAnImmutableInput)
{
    Chain c; auto& phase = c.plan.m_phases.front();
    auto& output = phase.m_expectedOutputs.front();
    output.m_relativePath = c.original.m_relativePath;
    output = Seal(output); phase = Seal(phase);
    c.produced.m_relativePath = c.original.m_relativePath; c.produced = Seal(c.produced);
    auto& deploy = c.plan.m_phases[1]; deploy.m_inputs.front() = c.produced;
    deploy.m_mutations.front().m_desiredArtifact = c.produced; deploy.m_mutations.front() = Seal(deploy.m_mutations.front());
    deploy = Seal(deploy); c.plan = Seal(c.plan);
    EXPECT_FALSE(c.CheckPlan().IsSuccess());
}
TEST(CapabilityExecutionValidation, RollbackPlanIdentityMustIdentifyOnePhase)
{
    Chain c; auto& phase = c.plan.m_phases.back();
    phase.m_rollback.m_id = c.plan.m_phases.front().m_rollback.m_id;
    phase.m_rollback = Seal(phase.m_rollback); phase = Seal(phase); c.plan = Seal(c.plan);
    EXPECT_FALSE(Validate(c.plan).IsSuccess());
}
