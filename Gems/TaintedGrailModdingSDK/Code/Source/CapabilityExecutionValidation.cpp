/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CapabilityExecutionValidation.h"
#include "CanonicalFingerprint.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/set.h>

namespace TaintedGrailModdingSDK::CapabilityExecution
{
    namespace
    {
        using Ids = AZStd::set<AZStd::string>;
        template<class T> bool Contains(const AZStd::vector<T>& values, const T& value)
        { return AZStd::find(values.begin(), values.end(), value) != values.end(); }
        template<class T> ContractValidation Check(const T& value)
        {
            auto canonical = Canonicalize(value);
            if (!canonical.IsSuccess()) { return AZ::Failure(canonical.GetError()); }
            if (value.m_fingerprint != canonical.GetValue().m_fingerprint)
            { return AZ::Failure(AZStd::string("Own canonical fingerprint mismatch.")); }
            return AZ::Success();
        }
        ContractValidation CheckReference(const UpstreamReferenceV1& reference)
        {
            if (!IsStableId(reference.m_id) || Token(reference.m_kind).empty()
                || reference.m_canonicalJson.size() > MaximumEmbeddedBytes || !IsDigest(reference.m_fingerprint)
                || CalculateCanonicalSha256(reference.m_canonicalJson) != reference.m_fingerprint)
            { return AZ::Failure(AZStd::string("Upstream canonical fingerprint mismatch.")); }
            return AZ::Success();
        }
        template<class T> bool Matches(const UpstreamReferenceV1& reference, const T& expected)
        {
            auto actual = Reference(expected);
            return actual.IsSuccess() && reference.m_id == actual.GetValue().m_id
                && reference.m_kind == actual.GetValue().m_kind
                && reference.m_fingerprint == actual.GetValue().m_fingerprint
                && reference.m_canonicalJson == actual.GetValue().m_canonicalJson;
        }
        template<class T> bool Same(const T& a, const T& b)
        {
            auto ca = Canonicalize(a), cb = Canonicalize(b);
            return ca.IsSuccess() && cb.IsSuccess() && a.m_fingerprint == ca.GetValue().m_fingerprint
                && b.m_fingerprint == cb.GetValue().m_fingerprint && ca.GetValue().m_json == cb.GetValue().m_json;
        }
        bool TimeShape(Outcome outcome, AZ::u64 attempt, const AZStd::string& start, const AZStd::string& finish)
        {
            if (outcome == Outcome::NOT_ATTEMPTED || outcome == Outcome::SKIPPED || outcome == Outcome::BLOCKED
                || (outcome == Outcome::CANCELLED && attempt == 0))
            { return attempt == 0 && start.empty() && finish.empty(); }
            if (!attempt || start.empty()) { return false; }
            if (outcome == Outcome::RUNNING) { return finish.empty(); }
            return !finish.empty() && finish >= start;
        }
        bool ResultShape(Outcome outcome, PhaseState state)
        {
            switch (outcome)
            {
            case Outcome::NOT_ATTEMPTED: return state == PhaseState::NOT_PLANNED || state == PhaseState::PENDING || state == PhaseState::READY;
            case Outcome::RUNNING: return state == PhaseState::RUNNING;
            case Outcome::SUCCEEDED: return state == PhaseState::SUCCEEDED;
            case Outcome::FAILED: case Outcome::PARTIAL: return state == PhaseState::FAILED;
            case Outcome::SKIPPED: return state == PhaseState::SKIPPED;
            case Outcome::BLOCKED: return state == PhaseState::BLOCKED;
            case Outcome::CANCELLED: return state == PhaseState::CANCELLED;
            default: return false;
            }
        }
        bool ArtifactMatchesExpected(const ArtifactReferenceV1& a, const ExpectedArtifactV1& e, bool requireDigest)
        {
            return a.m_id == e.m_id && a.m_payloadContractId == e.m_payloadContractId && a.m_ownerPackId == e.m_ownerPackId
                && a.m_storageRootId == e.m_storageRootId && a.m_relativePath == e.m_relativePath
                && a.m_custodianId == e.m_custodianId && a.m_byteSize <= e.m_maximumByteSize
                && (!requireDigest || !e.m_expectedDigest.empty())
                && (e.m_expectedDigest.empty() || a.m_digest == e.m_expectedDigest);
        }
        bool SameReference(const UpstreamReferenceV1& a, const UpstreamReferenceV1& b)
        { return a.m_id == b.m_id && a.m_kind == b.m_kind && a.m_canonicalJson == b.m_canonicalJson && a.m_fingerprint == b.m_fingerprint; }
    }
#define CE_REQUIRE(condition, message) do { if (!(condition)) { return AZ::Failure(AZStd::string(message)); } } while (false)
#define CE_CHECK(expression) do { auto checkedResult = (expression); if (!checkedResult.IsSuccess()) { return checkedResult; } } while (false)

    ContractValidation Validate(const CapabilityDescriptorV1& v)
    {
        CE_CHECK(Check(v));

        CE_REQUIRE(!v.m_sideEffects.empty(), "Capability must declare its side effects.");
        CE_REQUIRE(!v.m_requiredPhases.empty(), "A capability requires at least one phase.");
        for (const auto phase : v.m_optionalPhases)
        { CE_REQUIRE(!Contains(v.m_requiredPhases, phase), "Required and optional phases overlap."); }
        CE_REQUIRE(Contains(v.m_requiredPhases, v.m_terminalPhase) || Contains(v.m_optionalPhases, v.m_terminalPhase),
            "Terminal phase is not declared.");
        CE_REQUIRE(v.m_saveImpact == SideEffect::READ_ONLY || v.m_saveImpact == SideEffect::SAVE_MUTATION, "Invalid save-impact declaration.");
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityProviderBindingV1& v)
    {
        CE_CHECK(Check(v));

        CE_REQUIRE(!v.m_sideEffects.empty() && !v.m_qualificationEvidence.empty(), "Binding requires effect and evidence declarations.");
        CE_REQUIRE(v.m_providerVersion == v.m_versionConstraint, "M1 requires an exact pinned provider version constraint.");
        return AZ::Success();
    }

    ContractValidation Validate(const ArtifactReferenceV1& v)
    {
        CE_CHECK(Check(v));
        return AZ::Success();
    }

    ContractValidation Validate(const ExpectedArtifactV1& v)
    {
        CE_CHECK(Check(v));
        return AZ::Success();
    }

    ContractValidation Validate(const ArtifactRecordV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(Validate(v.m_artifact));
        CE_CHECK(CheckReference(v.m_sourceManifest));

        CE_REQUIRE(v.m_sourceManifest.m_kind == ContractKind::PHASE_EXTENSION_REFERENCE, "Source manifest must reference its exact extension wrapper.");
        CE_REQUIRE(v.m_id == v.m_artifact.m_id, "Artifact identity and record identity differ.");
        return AZ::Success();
    }

    ContractValidation Validate(const OptionV1& v)
    {
        CE_CHECK(Check(v));
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityExecutionRequestV1& v)
    {
        CE_CHECK(Check(v));
        for (const auto& child : v.m_inputs) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_preferredBindings) { CE_CHECK(CheckReference(child)); }
        for (const auto& child : v.m_options) { CE_CHECK(Validate(child)); }
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilitySupportDecisionV1& v)
    {
        CE_CHECK(Check(v));
        CE_REQUIRE(!v.m_evidenceIds.empty(), "Decision requires explicit evidence references.");
        CE_CHECK(CheckReference(v.m_request));

        CE_REQUIRE(v.m_request.m_kind == ContractKind::CAPABILITY_EXECUTION_REQUEST, "Decision must bind an exact semantic request.");
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityQualificationDecisionV1& v)
    {
        CE_CHECK(Check(v));
        CE_REQUIRE(!v.m_evidenceIds.empty(), "Decision requires explicit evidence references.");
        CE_CHECK(CheckReference(v.m_request));

        CE_REQUIRE(v.m_request.m_kind == ContractKind::CAPABILITY_EXECUTION_REQUEST, "Decision must bind an exact semantic request.");
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityEnvironmentDecisionV1& v)
    {
        CE_CHECK(Check(v));
        CE_REQUIRE(!v.m_evidenceIds.empty(), "Decision requires explicit evidence references.");
        CE_CHECK(CheckReference(v.m_request));

        CE_REQUIRE(v.m_request.m_kind == ContractKind::CAPABILITY_EXECUTION_REQUEST, "Decision must bind an exact semantic request.");
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityPolicyDecisionV1& v)
    {
        CE_CHECK(Check(v));
        CE_REQUIRE(!v.m_evidenceIds.empty(), "Decision requires explicit evidence references.");
        CE_CHECK(CheckReference(v.m_request));

        CE_REQUIRE(v.m_request.m_kind == ContractKind::CAPABILITY_EXECUTION_REQUEST, "Decision must bind an exact semantic request.");
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityAuthorizationReceiptV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(CheckReference(v.m_scope));

        CE_REQUIRE(!v.m_evidenceIds.empty(), "Authorization record requires explicit evidence references.");
        CE_REQUIRE(v.m_scope.m_kind == ContractKind::CAPABILITY_EXECUTION_REQUEST
            || v.m_scope.m_kind == ContractKind::CAPABILITY_EXECUTION_PLAN, "Authorization scope must be a request or exact plan.");
        if (v.m_state != AuthorizationState::PENDING && v.m_state != AuthorizationState::NOT_REQUIRED)
        {
            CE_REQUIRE(v.m_scope.m_kind == ContractKind::CAPABILITY_EXECUTION_PLAN, "A completed authorization observation requires an exact plan.");
            CE_REQUIRE(!v.m_actorId.empty() && !v.m_issuedAt.empty() && !v.m_expiresAt.empty()
                && v.m_expiresAt >= v.m_issuedAt, "Authorization event identity or validity interval is incomplete.");
        }
        else { CE_REQUIRE(v.m_expiresAt.empty() || (!v.m_issuedAt.empty() && v.m_expiresAt >= v.m_issuedAt), "Invalid authorization interval."); }
        return AZ::Success();
    }

    ContractValidation Validate(const TargetMutationClaimV1& v)
    {
        CE_CHECK(Check(v));
        if (v.m_desiredArtifact) { CE_CHECK(Validate(*v.m_desiredArtifact)); }
        if (v.m_backupArtifact) { CE_CHECK(Validate(*v.m_backupArtifact)); }

        if (v.m_operation == MutationOperation::CREATE)
        {
            CE_REQUIRE(v.m_preimagePresence == PreimagePresence::ABSENT && v.m_preimageFingerprint.empty()
                && v.m_preimageOwnerId.empty() && !v.m_backupArtifact && v.m_desiredArtifact, "Create must bind an absent preimage and desired artifact.");
        }
        else
        {
            CE_REQUIRE(v.m_preimagePresence == PreimagePresence::PRESENT && IsDigest(v.m_preimageFingerprint)
                && v.m_preimageOwnerId == v.m_ownerPackId && v.m_backupArtifact, "Replacement/removal requires an owned exact preimage and backup.");
            const auto& backup = *v.m_backupArtifact;
            CE_REQUIRE(backup.m_digest == v.m_preimageFingerprint && backup.m_ownerPackId == v.m_preimageOwnerId
                && backup.m_lifecycle == ArtifactLifecycle::BACKUP, "Immutable backup does not match the preimage.");
            CE_REQUIRE((v.m_operation == MutationOperation::REPLACE) == v.m_desiredArtifact.has_value(), "Replacement/removal desired artifact mismatch.");
        }
        if (v.m_desiredArtifact)
        { CE_REQUIRE(v.m_desiredArtifact->m_ownerPackId == v.m_ownerPackId, "Desired artifact belongs to another owner."); }
        return AZ::Success();
    }

    ContractValidation Validate(const RollbackStepV1& v)
    {
        CE_CHECK(Check(v));
        if (v.m_backupArtifact) { CE_CHECK(Validate(*v.m_backupArtifact)); }

        if (v.m_action == RollbackAction::RESTORE_BACKUP)
        {
            CE_REQUIRE(v.m_backupArtifact && IsDigest(v.m_restoreFingerprint)
                && v.m_backupArtifact->m_digest == v.m_restoreFingerprint
                && v.m_backupArtifact->m_ownerPackId == v.m_ownerPackId
                && v.m_backupArtifact->m_lifecycle == ArtifactLifecycle::BACKUP
                && v.m_compensationCommandId.empty(), "Restore step lacks the exact immutable backup.");
        }
        else if (v.m_action == RollbackAction::REMOVE_CREATED)
        {
            CE_REQUIRE(!v.m_backupArtifact && v.m_restoreFingerprint.empty()
                && IsDigest(v.m_expectedCurrentFingerprint) && v.m_compensationCommandId.empty(), "Created-file inverse is inconsistent.");
        }
        else { CE_REQUIRE(!v.m_compensationCommandId.empty(), "Compensation requires an exact command identity."); }
        return AZ::Success();
    }

    ContractValidation Validate(const RollbackPlanV1& v)
    {
        CE_CHECK(Check(v));
        for (const auto& child : v.m_steps) { CE_CHECK(Validate(child)); }

        if (v.m_support == RollbackSupport::NONE) { CE_REQUIRE(v.m_steps.empty(), "NONE rollback cannot declare steps."); }
        else { CE_REQUIRE(!v.m_steps.empty(), "Declared rollback support requires steps."); }
        Ids mutations;
        for (const auto& step : v.m_steps)
        {
            CE_REQUIRE(mutations.insert(step.m_mutationId).second, "Rollback repeats a mutation.");
            CE_REQUIRE((v.m_support == RollbackSupport::COMPENSATING) == (step.m_action == RollbackAction::COMPENSATE),
                "Rollback step and support level disagree.");
            if (v.m_support == RollbackSupport::CLEANUP_ONLY)
            { CE_REQUIRE(step.m_action == RollbackAction::REMOVE_CREATED, "Cleanup-only cannot claim preimage restoration."); }
        }
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityPhasePlanV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(CheckReference(v.m_binding));
        for (const auto& child : v.m_inputs) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_expectedOutputs) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_mutations) { CE_CHECK(Validate(child)); }
        CE_CHECK(Validate(v.m_rollback));

        CE_REQUIRE(v.m_binding.m_kind == ContractKind::CAPABILITY_PROVIDER_BINDING, "Phase requires a provider binding reference.");
        Ids outputs;
        for (const auto& output : v.m_expectedOutputs)
        { CE_REQUIRE(output.m_producerPhaseId == v.m_id && outputs.insert(output.m_id).second, "Output producer phase differs."); }
        for (const auto& input : v.m_inputs) { CE_REQUIRE(!outputs.count(input.m_id), "A phase cannot replace an input artifact identity."); }
        CE_REQUIRE(v.m_mutations.size() == v.m_rollback.m_steps.size(), "Every target mutation requires exactly one inverse step.");
        Ids targets;
        for (size_t i = 0; i < v.m_mutations.size(); ++i)
        {
            const auto& mutation = v.m_mutations[i];
            const auto& step = v.m_rollback.m_steps[v.m_mutations.size() - 1 - i];
            AZStd::string target = mutation.m_relativePath;
            for (auto& c : target) { if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; } }
            CE_REQUIRE(targets.insert(mutation.m_targetRootId + "/" + target).second, "A phase has colliding target mutations.");
            CE_REQUIRE(step.m_id == mutation.m_rollbackStepId && step.m_mutationId == mutation.m_id
                && step.m_targetRootId == mutation.m_targetRootId && step.m_relativePath == mutation.m_relativePath
                && step.m_ownerPackId == mutation.m_ownerPackId, "Rollback must be the exact reverse mutation sequence.");
            const auto desiredDigest = mutation.m_desiredArtifact ? mutation.m_desiredArtifact->m_digest : AZStd::string{};
            CE_REQUIRE(step.m_expectedCurrentFingerprint == desiredDigest, "Rollback lacks the desired postimage drift guard.");
            if (v.m_rollback.m_support != RollbackSupport::COMPENSATING)
            {
                if (mutation.m_operation == MutationOperation::CREATE)
                { CE_REQUIRE(step.m_action == RollbackAction::REMOVE_CREATED, "Create requires a remove-created inverse."); }
                else
                {
                    CE_REQUIRE(step.m_action == RollbackAction::RESTORE_BACKUP
                        && step.m_restoreFingerprint == mutation.m_preimageFingerprint && step.m_backupArtifact
                        && Same(*step.m_backupArtifact, *mutation.m_backupArtifact), "Inverse backup/preimage binding mismatch.");
                }
            }
        }
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityExecutionPlanV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(CheckReference(v.m_descriptor));
        CE_CHECK(CheckReference(v.m_request));
        CE_CHECK(Validate(v.m_support));
        CE_CHECK(Validate(v.m_qualification));
        CE_CHECK(Validate(v.m_environment));
        CE_CHECK(Validate(v.m_policy));
        CE_CHECK(Validate(v.m_authorizationIntent));
        for (const auto& child : v.m_phases) { CE_CHECK(Validate(child)); }

        CE_REQUIRE(v.m_descriptor.m_kind == ContractKind::CAPABILITY_DESCRIPTOR && v.m_request.m_kind == ContractKind::CAPABILITY_EXECUTION_REQUEST
            && !v.m_phases.empty(), "Execution plan requires descriptor, request and phases.");
        CE_REQUIRE(SameReference(v.m_support.m_request, v.m_request) && SameReference(v.m_qualification.m_request, v.m_request)
            && SameReference(v.m_environment.m_request, v.m_request) && SameReference(v.m_policy.m_request, v.m_request)
            && SameReference(v.m_authorizationIntent.m_scope, v.m_request), "Plan decision scopes differ from its request.");
        CE_REQUIRE(v.m_authorizationIntent.m_state == AuthorizationState::PENDING
            || v.m_authorizationIntent.m_state == AuthorizationState::NOT_REQUIRED, "The immutable plan contains authorization intent; exact-plan grants follow separately.");
        Ids fingerprints, rollbackIds; AZStd::set<Phase> phases;
        for (const auto& phase : v.m_phases)
        {
            CE_REQUIRE(phases.insert(phase.m_phase).second, "Execution plan repeats a phase.");
            CE_REQUIRE(rollbackIds.insert(phase.m_rollback.m_id).second, "Rollback plan identity must identify exactly one phase.");
            CE_REQUIRE(fingerprints.insert(phase.m_binding.m_fingerprint).second, "Execution plan reuses a phase-specific binding.");
        }
        for (const auto* decisions : {&v.m_support.m_bindingFingerprints, &v.m_qualification.m_bindingFingerprints,
            &v.m_environment.m_bindingFingerprints, &v.m_policy.m_bindingFingerprints})
        { CE_REQUIRE(Ids(decisions->begin(), decisions->end()) == fingerprints, "Decision provider-binding coverage differs from the plan."); }
        return AZ::Success();
    }

    ContractValidation Validate(const FailureRecordV1& v)
    {
        CE_CHECK(Check(v));
        return AZ::Success();
    }

    ContractValidation Validate(const DiagnosticReferenceV1& v)
    {
        CE_CHECK(Check(v));

        CE_REQUIRE(v.m_redacted, "Diagnostics must be declared redacted references, not raw output.");
        return AZ::Success();
    }

    ContractValidation Validate(const TargetObservationV1& v)
    {
        CE_CHECK(Check(v));

        if (v.m_presence == PreimagePresence::ABSENT)
        { CE_REQUIRE(v.m_contentFingerprint.empty() && v.m_ownerPackId.empty(), "Absent target cannot carry content or owner."); }
        else { CE_REQUIRE(IsDigest(v.m_contentFingerprint) && !v.m_ownerPackId.empty(), "Present target requires exact content and owner observations."); }
        return AZ::Success();
    }

    ContractValidation Validate(const PhaseExtensionReferenceV1& v)
    {
        CE_CHECK(Check(v));

        CE_REQUIRE(CalculateCanonicalSha256(v.m_canonicalJson) == v.m_extensionFingerprint, "Phase extension bytes and fingerprint differ.");
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityPhaseReceiptV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(CheckReference(v.m_plan));
        CE_CHECK(CheckReference(v.m_phasePlan));
        for (const auto& child : v.m_outputs) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_observations) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_failures) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_diagnostics) { CE_CHECK(Validate(child)); }
        if (v.m_extension) { CE_CHECK(Validate(*v.m_extension)); }

        CE_REQUIRE(v.m_plan.m_kind == ContractKind::CAPABILITY_EXECUTION_PLAN
            && v.m_phasePlan.m_kind == ContractKind::CAPABILITY_PHASE_PLAN, "Phase receipt requires exact plan references.");
        CE_REQUIRE(TimeShape(v.m_outcome, v.m_attempt, v.m_startedAt, v.m_finishedAt) && ResultShape(v.m_outcome, v.m_phaseState),
            "Phase attempt, timestamps and outcome/state disagree.");
        if (v.m_outcome == Outcome::NOT_ATTEMPTED || v.m_outcome == Outcome::SKIPPED || v.m_outcome == Outcome::BLOCKED || !v.m_attempt)
        { CE_REQUIRE(v.m_outputs.empty() && v.m_observations.empty() && !v.m_exitCode, "Unattempted phases cannot claim observed outputs or process exit."); }
        if (v.m_outcome == Outcome::FAILED || v.m_outcome == Outcome::PARTIAL)
        { CE_REQUIRE(!v.m_failures.empty(), "Failed/partial phase requires a failure record."); }
        if (v.m_outcome == Outcome::SUCCEEDED)
        { CE_REQUIRE(v.m_failures.empty() && (!v.m_exitCode || *v.m_exitCode == 0), "Successful phase cannot contain a failed exit or failure record."); }
        return AZ::Success();
    }

    ContractValidation Validate(const RollbackStepReceiptV1& v)
    {
        CE_CHECK(Check(v));
        for (const auto& child : v.m_failures) { CE_CHECK(Validate(child)); }

        const AZ::u64 attempted = (v.m_outcome == Outcome::NOT_ATTEMPTED || v.m_outcome == Outcome::SKIPPED
            || v.m_outcome == Outcome::BLOCKED || (v.m_outcome == Outcome::CANCELLED && v.m_startedAt.empty())) ? 0 : 1;
        CE_REQUIRE(TimeShape(v.m_outcome, attempted, v.m_startedAt, v.m_finishedAt), "Rollback step attempt timestamps disagree.");
        if (v.m_outcome == Outcome::FAILED || v.m_outcome == Outcome::PARTIAL)
        { CE_REQUIRE(!v.m_failures.empty(), "Failed rollback step needs failure details."); }
        if (!attempted)
        { CE_REQUIRE(v.m_observedFingerprint.empty() && v.m_observedOwnerId.empty(), "Unattempted rollback cannot claim target observations."); }
        return AZ::Success();
    }

    ContractValidation Validate(const RollbackReceiptV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(CheckReference(v.m_plan));
        CE_CHECK(CheckReference(v.m_rollbackPlan));
        for (const auto& child : v.m_steps) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_failures) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_diagnostics) { CE_CHECK(Validate(child)); }

        CE_REQUIRE(v.m_plan.m_kind == ContractKind::CAPABILITY_EXECUTION_PLAN
            && v.m_rollbackPlan.m_kind == ContractKind::ROLLBACK_PLAN, "Rollback receipt requires exact plan bindings.");
        if (v.m_state == RollbackState::NOT_REQUIRED || v.m_state == RollbackState::NOT_ATTEMPTED)
        { CE_REQUIRE(v.m_steps.empty() && v.m_failures.empty(), "Unattempted rollback cannot contain results."); }
        if (v.m_state == RollbackState::SUCCEEDED)
        { CE_REQUIRE(v.m_failures.empty() && !v.m_steps.empty(), "Successful rollback needs successful steps."); }
        bool failure = !v.m_failures.empty();
        for (const auto& step : v.m_steps)
        {
            failure = failure || !step.m_failures.empty();
            if (v.m_state == RollbackState::SUCCEEDED)
            { CE_REQUIRE(step.m_outcome == Outcome::SUCCEEDED, "Successful rollback has an incomplete step."); }
        }
        if (v.m_state == RollbackState::FAILED || v.m_state == RollbackState::PARTIAL)
        { CE_REQUIRE(failure, "Failed/partial rollback needs failure records."); }
        return AZ::Success();
    }

    ContractValidation Validate(const CapabilityExecutionReceiptV1& v)
    {
        CE_CHECK(Check(v));
        CE_CHECK(CheckReference(v.m_plan));
        CE_CHECK(Validate(v.m_authorization));
        if (v.m_assessmentReference) { CE_CHECK(CheckReference(*v.m_assessmentReference)); }
        for (const auto& child : v.m_failures) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_diagnostics) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_phaseReceipts) { CE_CHECK(Validate(child)); }
        for (const auto& child : v.m_rollbackReceipts) { CE_CHECK(Validate(child)); }

        CE_REQUIRE(v.m_plan.m_kind == ContractKind::CAPABILITY_EXECUTION_PLAN
            && SameReference(v.m_authorization.m_scope, v.m_plan), "Execution receipt requires exact-plan authorization scope.");
        const AZ::u64 attempted = v.m_startedAt.empty() ? 0 : 1;
        CE_REQUIRE(TimeShape(v.m_outcome, attempted, v.m_startedAt, v.m_finishedAt), "Execution outcome and event timestamps disagree.");
        if (v.m_outcome == Outcome::SUCCEEDED)
        {
            CE_REQUIRE(v.m_state == ExecutionState::SUCCEEDED && v.m_failures.empty()
                && v.m_verification != VerificationState::FAILED, "Successful execution has contradictory failure observations.");
        }
        else { CE_REQUIRE(v.m_state != ExecutionState::SUCCEEDED, "Non-successful execution cannot claim success state."); }
        if (v.m_state == ExecutionState::ROLLED_BACK || v.m_state == ExecutionState::ROLLBACK_FAILED)
        { CE_REQUIRE(v.m_outcome == Outcome::FAILED || v.m_outcome == Outcome::PARTIAL || v.m_outcome == Outcome::CANCELLED,
            "Rollback must preserve the original failure/cancellation outcome."); }
        if (!attempted)
        { CE_REQUIRE(v.m_phaseReceipts.empty() && v.m_rollbackReceipts.empty(), "Unattempted execution cannot carry observed phase or rollback receipts."); }
        if (v.m_outcome == Outcome::FAILED || v.m_outcome == Outcome::PARTIAL)
        {
            bool failed = !v.m_failures.empty();
            for (const auto& phase : v.m_phaseReceipts) { failed = failed || !phase.m_failures.empty(); }
            CE_REQUIRE(failed, "Failed/partial execution requires the original failure observations.");
        }
        if (v.m_assessment == AssessmentState::NOT_ASSESSED)
        { CE_REQUIRE(!v.m_assessmentReference, "Unassessed receipt cannot claim an assessment reference."); }
        else { CE_REQUIRE(v.m_assessmentReference, "Assessment observation requires an exact reference."); }
        return AZ::Success();
    }


    ContractValidation Validate(const CapabilityProviderBindingV1& v, const CapabilityDescriptorV1& descriptor)
    {
        CE_CHECK(Validate(v)); CE_CHECK(Validate(descriptor));
        CE_REQUIRE(v.m_capabilityId == descriptor.m_capabilityId
            && (Contains(descriptor.m_requiredPhases, v.m_phase) || Contains(descriptor.m_optionalPhases, v.m_phase)),
            "Provider binding capability or phase mismatch.");
        for (const auto& id : v.m_inputContracts) { CE_REQUIRE(Contains(descriptor.m_inputContracts, id), "Undeclared provider input contract."); }
        for (const auto& id : v.m_outputContracts) { CE_REQUIRE(Contains(descriptor.m_outputContracts, id), "Undeclared provider output contract."); }
        for (const auto effect : v.m_sideEffects) { CE_REQUIRE(Contains(descriptor.m_sideEffects, effect), "Undeclared provider side effect."); }
        CE_REQUIRE(v.m_rollbackSupport == descriptor.m_rollbackRequired, "Provider rollback support differs from the capability declaration.");
        return AZ::Success();
    }
    ContractValidation Validate(const CapabilityExecutionRequestV1& v, const CapabilityDescriptorV1& descriptor)
    {
        CE_CHECK(Validate(v)); CE_CHECK(Validate(descriptor));
        CE_REQUIRE(v.m_capabilityId == descriptor.m_capabilityId && v.m_terminalPhase == descriptor.m_terminalPhase,
            "Request capability or terminal phase mismatch.");
        for (const auto& input : v.m_inputs)
        { CE_REQUIRE(Contains(descriptor.m_inputContracts, input.m_payloadContractId), "Request contains an unsupported input contract."); }
        for (const auto& preferred : v.m_preferredBindings)
        { CE_REQUIRE(preferred.m_kind == ContractKind::CAPABILITY_PROVIDER_BINDING, "Preferred reference is not a provider binding."); }
        return AZ::Success();
    }
    ContractValidation Validate(const CapabilityPhasePlanV1& v, const CapabilityProviderBindingV1& binding)
    {
        CE_CHECK(Validate(v)); CE_CHECK(Validate(binding));
        CE_REQUIRE(Matches(v.m_binding, binding) && v.m_capabilityId == binding.m_capabilityId && v.m_phase == binding.m_phase
            && v.m_profileFingerprint == binding.m_profileFingerprint && v.m_providerId == binding.m_providerId
            && v.m_providerVersion == binding.m_providerVersion && v.m_commandId == binding.m_commandId
            && v.m_providerFingerprint == binding.m_providerFingerprint
            && (v.m_mutations.empty() || v.m_rollback.m_support == binding.m_rollbackSupport),
            "Phase does not bind the exact supplied provider, command, profile or rollback support.");
        for (const auto& input : v.m_inputs)
        { CE_REQUIRE(Contains(binding.m_inputContracts, input.m_payloadContractId), "Phase input contract is unsupported by its binding."); }
        for (const auto& output : v.m_expectedOutputs)
        { CE_REQUIRE(Contains(binding.m_outputContracts, output.m_payloadContractId), "Phase output contract is unsupported by its binding."); }
        if (!v.m_mutations.empty())
        {
            CE_REQUIRE(Contains(binding.m_sideEffects, SideEffect::WORKSPACE_WRITE) || Contains(binding.m_sideEffects, SideEffect::STAGING_WRITE)
                || Contains(binding.m_sideEffects, SideEffect::INSTALLATION_MUTATION) || Contains(binding.m_sideEffects, SideEffect::DESTRUCTIVE_DELETE),
                "Target mutations lack a declared write side effect.");
        }
        return AZ::Success();
    }
    ContractValidation Validate(const CapabilityExecutionPlanV1& v, const CapabilityDescriptorV1& descriptor,
        const CapabilityExecutionRequestV1& request, const AZStd::vector<CapabilityProviderBindingV1>& bindings)
    {
        CE_REQUIRE(bindings.size() <= MaximumPhases, "Selected binding count exceeds phase budget.");
        CE_CHECK(Validate(v)); CE_CHECK(Validate(request, descriptor));
        CE_REQUIRE(Matches(v.m_descriptor, descriptor) && Matches(v.m_request, request), "Plan upstream request/descriptor differs.");
        CE_REQUIRE(bindings.size() == v.m_phases.size(), "Supply exactly the selected phase bindings; unresolved candidates are not a plan.");
        AZStd::map<AZStd::string, const CapabilityProviderBindingV1*> byBinding;
        for (const auto& binding : bindings)
        {
            CE_CHECK(Validate(binding, descriptor));
            CE_REQUIRE(byBinding.emplace(binding.m_id, &binding).second, "Duplicate supplied provider binding.");
        }
        AZStd::map<AZStd::string, const ArtifactReferenceV1*> supplied;
        AZStd::map<AZStd::string, const ExpectedArtifactV1*> produced;
        for (const auto& input : request.m_inputs) { supplied.emplace(input.m_id, &input); }
        auto resolves = [&](const ArtifactReferenceV1& artifact)
        {
            const auto input = supplied.find(artifact.m_id);
            if (input != supplied.end()) { return Same(artifact, *input->second); }
            const auto output = produced.find(artifact.m_id);
            return output != produced.end() && ArtifactMatchesExpected(artifact, *output->second, true);
        };
        AZStd::set<Phase> phases; Ids selected, targets, locations;
        for (const auto& input : request.m_inputs)
        {
            AZStd::string path = input.m_relativePath;
            for (auto& c : path) { if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; } }
            CE_REQUIRE(locations.insert(input.m_storageRootId + "/" + path).second, "Input artifact storage locations alias.");
        }
        Phase previous = Phase::INVALID;
        for (const auto& phase : v.m_phases)
        {
            const auto found = byBinding.find(phase.m_binding.m_id);
            CE_REQUIRE(found != byBinding.end() && selected.insert(found->first).second, "Missing or repeated selected provider binding.");
            CE_CHECK(Validate(phase, *found->second));
            CE_REQUIRE(phase.m_profileFingerprint == request.m_profileFingerprint && phase.m_capabilityId == request.m_capabilityId,
                "Cross-profile or cross-capability phase.");
            CE_REQUIRE(static_cast<AZ::u32>(phase.m_phase) > static_cast<AZ::u32>(previous), "Phases are not in declared spine order.");
            previous = phase.m_phase; phases.insert(phase.m_phase);
            for (const auto& input : phase.m_inputs) { CE_REQUIRE(resolves(input), "Input is missing, changed or consumed before production."); }
            for (const auto& output : phase.m_expectedOutputs)
            {
                CE_REQUIRE(output.m_ownerPackId == request.m_packId && !supplied.count(output.m_id)
                    && produced.emplace(output.m_id, &output).second, "Duplicate/cross-owner expected output.");
                AZStd::string path = output.m_relativePath;
                for (auto& c : path) { if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; } }
                CE_REQUIRE(locations.insert(output.m_storageRootId + "/" + path).second, "Expected output location collides with an input or another output.");
            }
            for (const auto& mutation : phase.m_mutations)
            {
                CE_REQUIRE(mutation.m_ownerPackId == request.m_packId, "Target mutation belongs to another pack.");
                AZStd::string path = mutation.m_relativePath;
                for (auto& c : path) { if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; } }
                CE_REQUIRE(targets.insert(mutation.m_targetRootId + "/" + path).second, "Plan mutates the same target more than once.");
                if (mutation.m_desiredArtifact) { CE_REQUIRE(resolves(*mutation.m_desiredArtifact), "Desired artifact is not an exact available input/output."); }
                if (mutation.m_backupArtifact)
                {
                    const auto backup = supplied.find(mutation.m_backupArtifact->m_id);
                    CE_REQUIRE(backup != supplied.end() && Same(*backup->second, *mutation.m_backupArtifact),
                        "Immutable backup must be a supplied, exact preimage artifact.");
                }
            }
        }
        CE_REQUIRE(previous == request.m_terminalPhase, "Requested terminal phase is unreachable or not last.");
        for (const auto required : descriptor.m_requiredPhases) { CE_REQUIRE(phases.count(required), "Required phase is missing."); }
        for (const auto phase : phases)
        { CE_REQUIRE(Contains(descriptor.m_requiredPhases, phase) || Contains(descriptor.m_optionalPhases, phase), "Undeclared phase is present."); }
        for (const auto& preferred : request.m_preferredBindings)
        {
            const auto binding = byBinding.find(preferred.m_id);
            CE_REQUIRE(binding != byBinding.end() && Matches(preferred, *binding->second), "Explicit preferred binding was changed or ignored.");
        }
        return AZ::Success();
    }
    ContractValidation Validate(const CapabilityPhaseReceiptV1& v, const CapabilityExecutionPlanV1& plan,
        const CapabilityPhasePlanV1& phase, const AZStd::optional<PhaseExtensionReferenceV1>& expectedExtension)
    {
        CE_CHECK(Validate(v)); CE_CHECK(Validate(plan)); CE_CHECK(Validate(phase));
        CE_REQUIRE(Matches(v.m_plan, plan) && Matches(v.m_phasePlan, phase), "Phase receipt plan binding mismatch.");
        const auto planned = AZStd::find_if(plan.m_phases.begin(), plan.m_phases.end(), [&](const auto& row) { return row.m_id == phase.m_id; });
        CE_REQUIRE(planned != plan.m_phases.end() && Same(*planned, phase), "Phase is not the exact member of the supplied plan.");
        CE_REQUIRE(v.m_providerId == phase.m_providerId && v.m_providerVersion == phase.m_providerVersion
            && v.m_commandId == phase.m_commandId && v.m_providerFingerprint == phase.m_providerFingerprint
            && v.m_configurationFingerprint == phase.m_configurationFingerprint && v.m_environmentFingerprint == phase.m_environmentFingerprint,
            "Phase receipt provider/configuration/environment drift.");
        CE_REQUIRE(v.m_extension.has_value() == expectedExtension.has_value(), "Extension observation coverage mismatch.");
        if (expectedExtension)
        { CE_CHECK(Validate(*expectedExtension)); CE_REQUIRE(Same(*v.m_extension, *expectedExtension), "Phase extension owner bytes or identity changed."); }
        AZStd::map<AZStd::string, const ExpectedArtifactV1*> expected;
        for (const auto& output : phase.m_expectedOutputs) { expected.emplace(output.m_id, &output); }
        Ids outputIds;
        for (const auto& output : v.m_outputs)
        {
            const auto found = expected.find(output.m_artifact.m_id);
            CE_REQUIRE(found != expected.end() && outputIds.insert(found->first).second
                && ArtifactMatchesExpected(output.m_artifact, *found->second, false), "Unexpected, duplicate or changed output artifact.");
            CE_REQUIRE(output.m_producerExecutionId == v.m_executionId && output.m_producerPhaseId == phase.m_id
                && output.m_producerId == phase.m_providerId && output.m_role == found->second->m_role
                && output.m_mediaType == found->second->m_mediaType && output.m_redistribution == found->second->m_redistribution,
                "Output ownership, producer or media declaration changed.");
        }
        if (v.m_outcome == Outcome::SUCCEEDED) { CE_REQUIRE(outputIds.size() == expected.size(), "Successful phase is missing expected outputs."); }
        AZStd::map<AZStd::string, const TargetMutationClaimV1*> mutations;
        for (const auto& mutation : phase.m_mutations) { mutations.emplace(mutation.m_id, &mutation); }
        Ids observed;
        for (const auto& observation : v.m_observations)
        {
            const auto found = mutations.find(observation.m_mutationId);
            CE_REQUIRE(found != mutations.end() && observed.insert(found->first).second, "Unexpected or duplicate target observation.");
            const auto& mutation = *found->second;
            CE_REQUIRE(observation.m_targetRootId == mutation.m_targetRootId && observation.m_relativePath == mutation.m_relativePath,
                "Target observation root/path differs.");
            if (v.m_outcome == Outcome::SUCCEEDED) { CE_REQUIRE(observation.m_verification == VerificationState::PASSED, "Successful mutation lacks verification."); }
            if (observation.m_verification == VerificationState::PASSED)
            {
                if (mutation.m_operation == MutationOperation::REMOVE)
                { CE_REQUIRE(observation.m_presence == PreimagePresence::ABSENT, "Removed target is still present."); }
                else
                {
                    CE_REQUIRE(observation.m_presence == PreimagePresence::PRESENT && observation.m_ownerPackId == mutation.m_ownerPackId
                        && observation.m_contentFingerprint == mutation.m_desiredArtifact->m_digest, "Verified target differs from desired content/owner.");
                }
            }
        }
        if (v.m_outcome == Outcome::SUCCEEDED) { CE_REQUIRE(observed.size() == mutations.size(), "Successful phase is missing target observations."); }
        return AZ::Success();
    }
    ContractValidation Validate(const RollbackReceiptV1& v, const CapabilityExecutionPlanV1& plan, const CapabilityPhasePlanV1& phase)
    {
        CE_CHECK(Validate(v)); CE_CHECK(Validate(plan)); CE_CHECK(Validate(phase));
        CE_REQUIRE(Matches(v.m_plan, plan) && Matches(v.m_rollbackPlan, phase.m_rollback), "Rollback receipt plan mismatch.");
        const auto member = AZStd::find_if(plan.m_phases.begin(), plan.m_phases.end(), [&](const auto& row) { return row.m_id == phase.m_id; });
        CE_REQUIRE(member != plan.m_phases.end() && Same(*member, phase), "Rollback phase is not in the exact plan.");
        CE_REQUIRE(v.m_steps.size() <= phase.m_rollback.m_steps.size(), "Rollback receipt has extra steps.");
        if (v.m_state == RollbackState::SUCCEEDED)
        { CE_REQUIRE(v.m_steps.size() == phase.m_rollback.m_steps.size(), "Rollback success is missing inverse steps."); }
        for (size_t i = 0; i < v.m_steps.size(); ++i)
        {
            const auto& actual = v.m_steps[i]; const auto& expected = phase.m_rollback.m_steps[i];
            CE_REQUIRE(actual.m_stepId == expected.m_id, "Rollback receipt is not in inverse-plan order.");
            if (actual.m_outcome == Outcome::SUCCEEDED)
            {
                CE_REQUIRE(actual.m_observedFingerprint == expected.m_restoreFingerprint, "Rollback restored content fingerprint differs.");
                CE_REQUIRE(actual.m_observedOwnerId == (expected.m_action == RollbackAction::REMOVE_CREATED ? AZStd::string{} : expected.m_ownerPackId),
                    "Rollback restored target owner differs.");
            }
        }
        return AZ::Success();
    }
    ContractValidation Validate(const CapabilityExecutionReceiptV1& v, const CapabilityExecutionPlanV1& plan,
        const CapabilityAuthorizationReceiptV1& authorization, const AZStd::vector<PhaseExtensionReferenceV1>& expectedExtensions)
    {
        CE_REQUIRE(expectedExtensions.size() <= MaximumCollection, "Extension cardinality exceeded.");
        CE_CHECK(Validate(v)); CE_CHECK(Validate(plan)); CE_CHECK(Validate(authorization));
        CE_REQUIRE(Matches(v.m_plan, plan) && Same(v.m_authorization, authorization) && Matches(authorization.m_scope, plan),
            "Execution receipt plan or authorization binding mismatch.");
        AZStd::map<AZStd::string, const PhaseExtensionReferenceV1*> extensions;
        for (const auto& extension : expectedExtensions)
        { CE_CHECK(Validate(extension)); CE_REQUIRE(extensions.emplace(extension.m_id, &extension).second, "Duplicate expected extension."); }
        AZStd::map<AZStd::string, size_t> phaseIndexes;
        for (size_t i = 0; i < plan.m_phases.size(); ++i) { phaseIndexes.emplace(plan.m_phases[i].m_id, i); }
        AZStd::map<size_t, const CapabilityPhaseReceiptV1*> lastReceipts;
        size_t previous = 0; bool first = true; Ids usedExtensions;
        for (const auto& receipt : v.m_phaseReceipts)
        {
            CE_REQUIRE(receipt.m_startedAt.empty() || (!v.m_startedAt.empty() && receipt.m_startedAt >= v.m_startedAt),
                "Phase observation predates its execution.");
            CE_REQUIRE(v.m_finishedAt.empty() || (!receipt.m_finishedAt.empty() && receipt.m_finishedAt <= v.m_finishedAt)
                || receipt.m_startedAt.empty(), "Phase observation exceeds its completed execution.");
            const auto found = phaseIndexes.find(receipt.m_phasePlan.m_id);
            CE_REQUIRE(found != phaseIndexes.end() && (first || found->second >= previous), "Phase receipt sequence differs from plan order.");
            previous = found->second; first = false;
            const auto earlier = lastReceipts.find(previous);
            if (earlier != lastReceipts.end())
            { CE_REQUIRE(receipt.m_attempt > earlier->second->m_attempt, "Phase attempts must be strictly increasing."); }
            CE_REQUIRE(receipt.m_executionId == v.m_id, "Phase receipt belongs to another execution.");
            AZStd::optional<PhaseExtensionReferenceV1> expected;
            if (receipt.m_extension)
            {
                const auto extension = extensions.find(receipt.m_extension->m_id);
                CE_REQUIRE(extension != extensions.end(), "Phase extension lacks a supplied exact owner value.");
                expected = *extension->second; usedExtensions.insert(extension->first);
            }
            CE_CHECK(Validate(receipt, plan, plan.m_phases[previous], expected));
            lastReceipts[previous] = &receipt;
        }
        CE_REQUIRE(usedExtensions.size() == extensions.size(), "Supplied extension was not consumed.");
        if (v.m_outcome == Outcome::SUCCEEDED)
        {
            CE_REQUIRE(lastReceipts.size() == plan.m_phases.size(), "Successful execution is missing phase results.");
            for (const auto& pair : lastReceipts) { CE_REQUIRE(pair.second->m_outcome == Outcome::SUCCEEDED, "A required phase did not succeed."); }
            const bool verifies = AZStd::any_of(plan.m_phases.begin(), plan.m_phases.end(), [](const auto& phase) { return phase.m_phase == Phase::VERIFY; });
            CE_REQUIRE(!verifies || v.m_verification == VerificationState::PASSED, "Successful verification phase requires explicit verification observation.");
        }
        size_t priorRollback = plan.m_phases.size(); Ids rolledBack;
        bool rollbackFailed = false;
        for (const auto& rollback : v.m_rollbackReceipts)
        {
            auto found = AZStd::find_if(plan.m_phases.begin(), plan.m_phases.end(),
                [&](const auto& phase) { return phase.m_rollback.m_id == rollback.m_rollbackPlan.m_id; });
            CE_REQUIRE(found != plan.m_phases.end(), "Rollback receipt references an unknown phase rollback plan.");
            const auto index = static_cast<size_t>(found - plan.m_phases.begin());
            CE_REQUIRE(index < priorRollback && rolledBack.insert(rollback.m_rollbackPlan.m_id).second, "Rollback phase receipts must be in inverse order.");
            priorRollback = index; CE_CHECK(Validate(rollback, plan, *found));
            rollbackFailed = rollbackFailed || rollback.m_state == RollbackState::FAILED || rollback.m_state == RollbackState::PARTIAL;
            if (v.m_state == ExecutionState::ROLLED_BACK) { CE_REQUIRE(rollback.m_state == RollbackState::SUCCEEDED, "Rolled-back state includes unsuccessful rollback."); }
        }
        if (v.m_state == ExecutionState::ROLLED_BACK || v.m_state == ExecutionState::ROLLBACK_FAILED)
        { CE_REQUIRE(!v.m_rollbackReceipts.empty(), "Rollback execution state requires actual receipt observations."); }
        if (v.m_state == ExecutionState::ROLLED_BACK)
        {
            for (const auto& pair : lastReceipts)
            {
                const auto& phase = plan.m_phases[pair.first];
                if (pair.second->m_attempt && !phase.m_mutations.empty())
                { CE_REQUIRE(rolledBack.count(phase.m_rollback.m_id), "Rolled-back state omits an attempted mutation phase."); }
            }
        }
        if (v.m_state == ExecutionState::ROLLBACK_FAILED) { CE_REQUIRE(rollbackFailed, "Rollback-failed state lacks failed observations."); }
        return AZ::Success();
    }
#undef CE_CHECK
#undef CE_REQUIRE
}
