/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK::CapabilityExecution
{
    inline constexpr char ContractId[] = "foa-capability-execution-v1";
    inline constexpr char CanonicalProfile[] = "foa-capability-execution-canonical-json-v1";
    inline constexpr AZ::u32 ContractVersion = 1;
    inline constexpr size_t MaximumStringBytes = 4096;
    inline constexpr size_t MaximumIdBytes = 128;
    inline constexpr size_t MaximumPathBytes = 240;
    inline constexpr size_t MaximumCollection = 64;
    inline constexpr size_t MaximumPhases = 9;
    inline constexpr size_t MaximumNodes = 4096;
    inline constexpr size_t MaximumNesting = 12;
    inline constexpr size_t MaximumEmbeddedBytes = 1024 * 1024;
    inline constexpr size_t MaximumCanonicalBytes = 2 * 1024 * 1024;
    inline constexpr AZ::u64 MaximumArtifactBytes = AZ::u64{1} << 40;

    //! Checked multiplication/addition, usable before traversing or allocating.
    struct SizeBudget
    {
        size_t m_used = 0;
        size_t m_limit = MaximumCanonicalBytes;
        bool Add(size_t count, size_t width = 1);
    };

    bool IsStableId(AZStd::string_view value);
    bool IsDigest(AZStd::string_view value);
    bool IsRelativeLocator(AZStd::string_view value);
    bool IsSafeText(AZStd::string_view value);
    bool IsUtcTimestamp(AZStd::string_view value);
    bool IsExactVersion(AZStd::string_view value);

    enum class Phase : AZ::u32
    {
        INVALID,
        MATERIALIZE,
        BUILD,
        PACKAGE,
        DEPLOY,
        LAUNCH,
        VERIFY,
        ASSESS,
        RECONCILE,
        ROLLBACK,
    };
    AZStd::string_view Token(Phase value);
    bool ParseToken(AZStd::string_view token, Phase& output);

    enum class SideEffect : AZ::u32
    {
        INVALID,
        READ_ONLY,
        WORKSPACE_WRITE,
        STAGING_WRITE,
        PROCESS_LAUNCH,
        INSTALLATION_MUTATION,
        RUNTIME_MUTATION,
        SAVE_MUTATION,
        SECRET_USE,
        NETWORK_PUBLICATION,
        DESTRUCTIVE_DELETE,
    };
    AZStd::string_view Token(SideEffect value);
    bool ParseToken(AZStd::string_view token, SideEffect& output);

    enum class RollbackSupport : AZ::u32
    {
        INVALID,
        NONE,
        CLEANUP_ONLY,
        COMPENSATING,
        EXACT_RESTORE,
    };
    AZStd::string_view Token(RollbackSupport value);
    bool ParseToken(AZStd::string_view token, RollbackSupport& output);

    enum class SupportState : AZ::u32
    {
        INVALID,
        SUPPORTED,
        UNSUPPORTED,
    };
    AZStd::string_view Token(SupportState value);
    bool ParseToken(AZStd::string_view token, SupportState& output);

    enum class QualificationState : AZ::u32
    {
        INVALID,
        QUALIFIED,
        UNQUALIFIED,
        STALE,
        UNKNOWN,
    };
    AZStd::string_view Token(QualificationState value);
    bool ParseToken(AZStd::string_view token, QualificationState& output);

    enum class EnvironmentState : AZ::u32
    {
        INVALID,
        AVAILABLE,
        UNAVAILABLE,
        DRIFTED,
        UNKNOWN,
    };
    AZStd::string_view Token(EnvironmentState value);
    bool ParseToken(AZStd::string_view token, EnvironmentState& output);

    enum class PolicyState : AZ::u32
    {
        INVALID,
        ALLOWED,
        CONFIRMATION_REQUIRED,
        DENIED,
    };
    AZStd::string_view Token(PolicyState value);
    bool ParseToken(AZStd::string_view token, PolicyState& output);

    enum class AuthorizationState : AZ::u32
    {
        INVALID,
        NOT_REQUIRED,
        PENDING,
        GRANTED,
        EXPIRED,
        REVOKED,
        SCOPE_MISMATCH,
    };
    AZStd::string_view Token(AuthorizationState value);
    bool ParseToken(AZStd::string_view token, AuthorizationState& output);

    enum class Outcome : AZ::u32
    {
        INVALID,
        NOT_ATTEMPTED,
        RUNNING,
        SUCCEEDED,
        FAILED,
        SKIPPED,
        BLOCKED,
        CANCELLED,
        PARTIAL,
    };
    AZStd::string_view Token(Outcome value);
    bool ParseToken(AZStd::string_view token, Outcome& output);

    enum class VerificationState : AZ::u32
    {
        INVALID,
        NOT_CHECKED,
        PASSED,
        FAILED,
        UNKNOWN,
    };
    AZStd::string_view Token(VerificationState value);
    bool ParseToken(AZStd::string_view token, VerificationState& output);

    enum class AssessmentState : AZ::u32
    {
        INVALID,
        NOT_ASSESSED,
        ACCEPTED,
        REJECTED,
        NEEDS_REVIEW,
    };
    AZStd::string_view Token(AssessmentState value);
    bool ParseToken(AZStd::string_view token, AssessmentState& output);

    enum class PromotionState : AZ::u32
    {
        INVALID,
        NOT_PROMOTED,
        CANDIDATE,
        PROMOTED,
        REJECTED,
    };
    AZStd::string_view Token(PromotionState value);
    bool ParseToken(AZStd::string_view token, PromotionState& output);

    enum class ReleaseDecisionState : AZ::u32
    {
        INVALID,
        NOT_DECIDED,
        APPROVED,
        REJECTED,
    };
    AZStd::string_view Token(ReleaseDecisionState value);
    bool ParseToken(AZStd::string_view token, ReleaseDecisionState& output);

    enum class ArtifactLifecycle : AZ::u32
    {
        INVALID,
        DECLARED,
        PRODUCED,
        VERIFIED,
        STAGED,
        DEPLOYED,
        RETIRED,
        BACKUP,
    };
    AZStd::string_view Token(ArtifactLifecycle value);
    bool ParseToken(AZStd::string_view token, ArtifactLifecycle& output);

    enum class RedistributionState : AZ::u32
    {
        INVALID,
        UNKNOWN,
        PERMITTED,
        FORBIDDEN,
    };
    AZStd::string_view Token(RedistributionState value);
    bool ParseToken(AZStd::string_view token, RedistributionState& output);

    enum class PreimagePresence : AZ::u32
    {
        INVALID,
        ABSENT,
        PRESENT,
    };
    AZStd::string_view Token(PreimagePresence value);
    bool ParseToken(AZStd::string_view token, PreimagePresence& output);

    enum class MutationOperation : AZ::u32
    {
        INVALID,
        CREATE,
        REPLACE,
        REMOVE,
    };
    AZStd::string_view Token(MutationOperation value);
    bool ParseToken(AZStd::string_view token, MutationOperation& output);

    enum class RollbackAction : AZ::u32
    {
        INVALID,
        REMOVE_CREATED,
        RESTORE_BACKUP,
        COMPENSATE,
    };
    AZStd::string_view Token(RollbackAction value);
    bool ParseToken(AZStd::string_view token, RollbackAction& output);

    enum class CleanupState : AZ::u32
    {
        INVALID,
        NOT_REQUIRED,
        PENDING,
        SUCCEEDED,
        FAILED,
        SKIPPED,
    };
    AZStd::string_view Token(CleanupState value);
    bool ParseToken(AZStd::string_view token, CleanupState& output);

    enum class RollbackState : AZ::u32
    {
        INVALID,
        NOT_REQUIRED,
        NOT_ATTEMPTED,
        SUCCEEDED,
        FAILED,
        PARTIAL,
        SKIPPED,
        CANCELLED,
    };
    AZStd::string_view Token(RollbackState value);
    bool ParseToken(AZStd::string_view token, RollbackState& output);

    enum class ExecutionState : AZ::u32
    {
        INVALID,
        DRAFT,
        VALIDATED,
        RESOLVED,
        QUALIFIED,
        PLANNED,
        AWAITING_AUTHORIZATION,
        READY,
        EXECUTING,
        VERIFYING,
        SUCCEEDED,
        REJECTED,
        RESOLUTION_FAILED,
        QUALIFICATION_FAILED,
        POLICY_DENIED,
        AUTHORIZATION_EXPIRED,
        ENVIRONMENT_DRIFTED,
        FAILED,
        PARTIAL,
        CANCELLATION_REQUESTED,
        CANCELLED,
        ROLLBACK_REQUIRED,
        ROLLING_BACK,
        ROLLED_BACK,
        ROLLBACK_FAILED,
        SUPERSEDED,
        ARCHIVED,
    };
    AZStd::string_view Token(ExecutionState value);
    bool ParseToken(AZStd::string_view token, ExecutionState& output);

    enum class PhaseState : AZ::u32
    {
        INVALID,
        NOT_PLANNED,
        PENDING,
        READY,
        RUNNING,
        SUCCEEDED,
        FAILED,
        SKIPPED,
        BLOCKED,
        CANCELLED,
        ROLLBACK_PENDING,
        ROLLED_BACK,
        ROLLBACK_FAILED,
    };
    AZStd::string_view Token(PhaseState value);
    bool ParseToken(AZStd::string_view token, PhaseState& output);

    enum class ContractKind : AZ::u32
    {
        INVALID,
        CAPABILITY_DESCRIPTOR,
        CAPABILITY_PROVIDER_BINDING,
        ARTIFACT_REFERENCE,
        EXPECTED_ARTIFACT,
        ARTIFACT_RECORD,
        OPTION,
        CAPABILITY_EXECUTION_REQUEST,
        CAPABILITY_SUPPORT_DECISION,
        CAPABILITY_QUALIFICATION_DECISION,
        CAPABILITY_ENVIRONMENT_DECISION,
        CAPABILITY_POLICY_DECISION,
        CAPABILITY_AUTHORIZATION_RECEIPT,
        TARGET_MUTATION_CLAIM,
        ROLLBACK_STEP,
        ROLLBACK_PLAN,
        CAPABILITY_PHASE_PLAN,
        CAPABILITY_EXECUTION_PLAN,
        FAILURE_RECORD,
        DIAGNOSTIC_REFERENCE,
        TARGET_OBSERVATION,
        PHASE_EXTENSION_REFERENCE,
        CAPABILITY_PHASE_RECEIPT,
        ROLLBACK_STEP_RECEIPT,
        ROLLBACK_RECEIPT,
        CAPABILITY_EXECUTION_RECEIPT,
    };
    AZStd::string_view Token(ContractKind value);
    bool ParseToken(AZStd::string_view token, ContractKind& output);

    struct ContractHeaderV1
    {
        AZStd::string m_contractId = ContractId;
        AZStd::string m_canonicalProfile = CanonicalProfile;
        AZ::u32 m_version = ContractVersion;
    };
    struct ContractValueV1
    {
        ContractHeaderV1 m_header;
        AZStd::string m_id;
        AZStd::string m_fingerprint; //!< Excluded from this object's own projection.
    };
    struct CaptureMetadataV1
    {
        AZStd::string m_label;
        AZStd::string m_capturedAt;
        AZStd::string m_diagnosticLocator;
    };
    //! An upstream owner's canonical bytes, checked against the supplied typed value.
    struct UpstreamReferenceV1
    {
        AZStd::string m_id;
        ContractKind m_kind = ContractKind::INVALID;
        AZStd::string m_canonicalJson;
        AZStd::string m_fingerprint;
    };

    struct CapabilityDescriptorV1 : ContractValueV1
    {
        AZStd::string m_capabilityId;
        AZStd::vector<AZStd::string> m_inputContracts;
        AZStd::vector<AZStd::string> m_outputContracts;
        AZStd::vector<Phase> m_requiredPhases;
        AZStd::vector<Phase> m_optionalPhases;
        Phase m_terminalPhase = Phase::INVALID;
        AZStd::vector<SideEffect> m_sideEffects;
        bool m_exactProfileRequired = false;
        bool m_runtimeRequired = false;
        SideEffect m_saveImpact = SideEffect::INVALID;
        RollbackSupport m_rollbackRequired = RollbackSupport::INVALID;
    };
    constexpr ContractKind Kind(const CapabilityDescriptorV1&) { return ContractKind::CAPABILITY_DESCRIPTOR; }

    struct CapabilityProviderBindingV1 : ContractValueV1
    {
        AZStd::string m_capabilityId;
        Phase m_phase = Phase::INVALID;
        AZStd::string m_providerId;
        AZStd::string m_providerVersion;
        AZStd::string m_versionConstraint;
        AZStd::string m_commandId;
        AZStd::string m_providerFingerprint;
        AZStd::vector<AZStd::string> m_inputContracts;
        AZStd::vector<AZStd::string> m_outputContracts;
        AZStd::string m_profileFingerprint;
        AZStd::vector<AZStd::string> m_qualificationEvidence;
        AZStd::vector<SideEffect> m_sideEffects;
        bool m_cancellationSupported = false;
        bool m_resumeSupported = false;
        RollbackSupport m_rollbackSupport = RollbackSupport::INVALID;
    };
    constexpr ContractKind Kind(const CapabilityProviderBindingV1&) { return ContractKind::CAPABILITY_PROVIDER_BINDING; }

    struct ArtifactReferenceV1 : ContractValueV1
    {
        AZStd::string m_payloadContractId;
        AZStd::string m_ownerPackId;
        AZStd::string m_storageRootId;
        AZStd::string m_relativePath;
        AZStd::string m_digest;
        AZ::u64 m_byteSize = 0;
        AZStd::string m_custodianId;
        ArtifactLifecycle m_lifecycle = ArtifactLifecycle::INVALID;
    };
    constexpr ContractKind Kind(const ArtifactReferenceV1&) { return ContractKind::ARTIFACT_REFERENCE; }

    struct ExpectedArtifactV1 : ContractValueV1
    {
        AZStd::string m_payloadContractId;
        AZStd::string m_ownerPackId;
        AZStd::string m_storageRootId;
        AZStd::string m_relativePath;
        AZStd::string m_expectedDigest;
        AZ::u64 m_maximumByteSize = 0;
        AZStd::string m_custodianId;
        AZStd::string m_producerPhaseId;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        RedistributionState m_redistribution = RedistributionState::INVALID;
    };
    constexpr ContractKind Kind(const ExpectedArtifactV1&) { return ContractKind::EXPECTED_ARTIFACT; }

    struct ArtifactRecordV1 : ContractValueV1
    {
        ArtifactReferenceV1 m_artifact;
        AZStd::string m_role;
        AZStd::string m_mediaType;
        AZStd::string m_producerExecutionId;
        AZStd::string m_producerPhaseId;
        AZStd::string m_producerId;
        UpstreamReferenceV1 m_sourceManifest;
        RedistributionState m_redistribution = RedistributionState::INVALID;
    };
    constexpr ContractKind Kind(const ArtifactRecordV1&) { return ContractKind::ARTIFACT_RECORD; }

    struct OptionV1 : ContractValueV1
    {
        AZStd::string m_value;
    };
    constexpr ContractKind Kind(const OptionV1&) { return ContractKind::OPTION; }

    struct CapabilityExecutionRequestV1 : ContractValueV1
    {
        AZStd::string m_workspaceId;
        AZStd::string m_packId;
        AZStd::string m_profileFingerprint;
        AZStd::string m_capabilityId;
        Phase m_terminalPhase = Phase::INVALID;
        AZStd::vector<ArtifactReferenceV1> m_inputs;
        AZStd::vector<UpstreamReferenceV1> m_preferredBindings;
        AZStd::vector<OptionV1> m_options;
        CaptureMetadataV1 m_capture;
    };
    constexpr ContractKind Kind(const CapabilityExecutionRequestV1&) { return ContractKind::CAPABILITY_EXECUTION_REQUEST; }

    struct CapabilitySupportDecisionV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_request;
        SupportState m_state = SupportState::INVALID;
        AZStd::string m_reason;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_bindingFingerprints;
    };
    constexpr ContractKind Kind(const CapabilitySupportDecisionV1&) { return ContractKind::CAPABILITY_SUPPORT_DECISION; }

    struct CapabilityQualificationDecisionV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_request;
        QualificationState m_state = QualificationState::INVALID;
        AZStd::string m_reason;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_bindingFingerprints;
    };
    constexpr ContractKind Kind(const CapabilityQualificationDecisionV1&) { return ContractKind::CAPABILITY_QUALIFICATION_DECISION; }

    struct CapabilityEnvironmentDecisionV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_request;
        EnvironmentState m_state = EnvironmentState::INVALID;
        AZStd::string m_reason;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_bindingFingerprints;
    };
    constexpr ContractKind Kind(const CapabilityEnvironmentDecisionV1&) { return ContractKind::CAPABILITY_ENVIRONMENT_DECISION; }

    struct CapabilityPolicyDecisionV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_request;
        PolicyState m_state = PolicyState::INVALID;
        AZStd::string m_reason;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_bindingFingerprints;
    };
    constexpr ContractKind Kind(const CapabilityPolicyDecisionV1&) { return ContractKind::CAPABILITY_POLICY_DECISION; }

    struct CapabilityAuthorizationReceiptV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_scope;
        AuthorizationState m_state = AuthorizationState::INVALID;
        AZStd::string m_actorId;
        AZStd::string m_reason;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::string m_issuedAt;
        AZStd::string m_expiresAt;
    };
    constexpr ContractKind Kind(const CapabilityAuthorizationReceiptV1&) { return ContractKind::CAPABILITY_AUTHORIZATION_RECEIPT; }

    struct TargetMutationClaimV1 : ContractValueV1
    {
        AZStd::string m_targetRootId;
        AZStd::string m_relativePath;
        MutationOperation m_operation = MutationOperation::INVALID;
        AZStd::string m_ownerPackId;
        PreimagePresence m_preimagePresence = PreimagePresence::INVALID;
        AZStd::string m_preimageFingerprint;
        AZStd::string m_preimageOwnerId;
        AZStd::optional<ArtifactReferenceV1> m_desiredArtifact;
        AZStd::optional<ArtifactReferenceV1> m_backupArtifact;
        AZStd::string m_rollbackStepId;
    };
    constexpr ContractKind Kind(const TargetMutationClaimV1&) { return ContractKind::TARGET_MUTATION_CLAIM; }

    struct RollbackStepV1 : ContractValueV1
    {
        AZStd::string m_mutationId;
        RollbackAction m_action = RollbackAction::INVALID;
        AZStd::string m_targetRootId;
        AZStd::string m_relativePath;
        AZStd::string m_ownerPackId;
        AZStd::string m_expectedCurrentFingerprint;
        AZStd::string m_restoreFingerprint;
        AZStd::optional<ArtifactReferenceV1> m_backupArtifact;
        AZStd::string m_compensationCommandId;
    };
    constexpr ContractKind Kind(const RollbackStepV1&) { return ContractKind::ROLLBACK_STEP; }

    struct RollbackPlanV1 : ContractValueV1
    {
        RollbackSupport m_support = RollbackSupport::INVALID;
        AZStd::vector<RollbackStepV1> m_steps;
    };
    constexpr ContractKind Kind(const RollbackPlanV1&) { return ContractKind::ROLLBACK_PLAN; }

    struct CapabilityPhasePlanV1 : ContractValueV1
    {
        Phase m_phase = Phase::INVALID;
        AZStd::string m_capabilityId;
        AZStd::string m_profileFingerprint;
        UpstreamReferenceV1 m_binding;
        AZStd::string m_providerId;
        AZStd::string m_providerVersion;
        AZStd::string m_commandId;
        AZStd::string m_providerFingerprint;
        AZStd::vector<ArtifactReferenceV1> m_inputs;
        AZStd::vector<ExpectedArtifactV1> m_expectedOutputs;
        AZStd::vector<TargetMutationClaimV1> m_mutations;
        AZStd::string m_configurationFingerprint;
        AZStd::string m_environmentFingerprint;
        AZStd::string m_targetInventoryFingerprint;
        RollbackPlanV1 m_rollback;
        CaptureMetadataV1 m_capture;
    };
    constexpr ContractKind Kind(const CapabilityPhasePlanV1&) { return ContractKind::CAPABILITY_PHASE_PLAN; }

    struct CapabilityExecutionPlanV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_descriptor;
        UpstreamReferenceV1 m_request;
        CapabilitySupportDecisionV1 m_support;
        CapabilityQualificationDecisionV1 m_qualification;
        CapabilityEnvironmentDecisionV1 m_environment;
        CapabilityPolicyDecisionV1 m_policy;
        CapabilityAuthorizationReceiptV1 m_authorizationIntent;
        AZStd::vector<CapabilityPhasePlanV1> m_phases;
        CaptureMetadataV1 m_capture;
    };
    constexpr ContractKind Kind(const CapabilityExecutionPlanV1&) { return ContractKind::CAPABILITY_EXECUTION_PLAN; }

    struct FailureRecordV1 : ContractValueV1
    {
        AZStd::string m_code;
        AZStd::string m_message;
        bool m_retryable = false;
    };
    constexpr ContractKind Kind(const FailureRecordV1&) { return ContractKind::FAILURE_RECORD; }

    struct DiagnosticReferenceV1 : ContractValueV1
    {
        AZStd::string m_storageRootId;
        AZStd::string m_relativePath;
        AZStd::string m_digest;
        bool m_redacted = false;
    };
    constexpr ContractKind Kind(const DiagnosticReferenceV1&) { return ContractKind::DIAGNOSTIC_REFERENCE; }

    struct TargetObservationV1 : ContractValueV1
    {
        AZStd::string m_mutationId;
        AZStd::string m_targetRootId;
        AZStd::string m_relativePath;
        PreimagePresence m_presence = PreimagePresence::INVALID;
        AZStd::string m_contentFingerprint;
        AZStd::string m_ownerPackId;
        VerificationState m_verification = VerificationState::INVALID;
    };
    constexpr ContractKind Kind(const TargetObservationV1&) { return ContractKind::TARGET_OBSERVATION; }

    struct PhaseExtensionReferenceV1 : ContractValueV1
    {
        AZStd::string m_extensionContractId;
        AZStd::string m_canonicalJson;
        AZStd::string m_extensionFingerprint;
    };
    constexpr ContractKind Kind(const PhaseExtensionReferenceV1&) { return ContractKind::PHASE_EXTENSION_REFERENCE; }

    struct CapabilityPhaseReceiptV1 : ContractValueV1
    {
        AZStd::string m_executionId;
        UpstreamReferenceV1 m_plan;
        UpstreamReferenceV1 m_phasePlan;
        AZStd::string m_providerId;
        AZStd::string m_providerVersion;
        AZStd::string m_commandId;
        AZStd::string m_providerFingerprint;
        AZStd::string m_configurationFingerprint;
        AZStd::string m_environmentFingerprint;
        AZ::u64 m_attempt = 0;
        Outcome m_outcome = Outcome::INVALID;
        PhaseState m_phaseState = PhaseState::INVALID;
        AZStd::string m_startedAt;
        AZStd::string m_finishedAt;
        AZStd::optional<AZ::s64> m_exitCode;
        AZStd::vector<ArtifactRecordV1> m_outputs;
        AZStd::vector<TargetObservationV1> m_observations;
        AZStd::vector<FailureRecordV1> m_failures;
        AZStd::vector<DiagnosticReferenceV1> m_diagnostics;
        CleanupState m_cleanup = CleanupState::INVALID;
        AZStd::optional<PhaseExtensionReferenceV1> m_extension;
    };
    constexpr ContractKind Kind(const CapabilityPhaseReceiptV1&) { return ContractKind::CAPABILITY_PHASE_RECEIPT; }

    struct RollbackStepReceiptV1 : ContractValueV1
    {
        AZStd::string m_stepId;
        Outcome m_outcome = Outcome::INVALID;
        AZStd::string m_observedFingerprint;
        AZStd::string m_observedOwnerId;
        AZStd::string m_startedAt;
        AZStd::string m_finishedAt;
        AZStd::vector<FailureRecordV1> m_failures;
    };
    constexpr ContractKind Kind(const RollbackStepReceiptV1&) { return ContractKind::ROLLBACK_STEP_RECEIPT; }

    struct RollbackReceiptV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_plan;
        UpstreamReferenceV1 m_rollbackPlan;
        RollbackState m_state = RollbackState::INVALID;
        AZStd::vector<RollbackStepReceiptV1> m_steps;
        AZStd::vector<FailureRecordV1> m_failures;
        AZStd::vector<DiagnosticReferenceV1> m_diagnostics;
    };
    constexpr ContractKind Kind(const RollbackReceiptV1&) { return ContractKind::ROLLBACK_RECEIPT; }

    struct CapabilityExecutionReceiptV1 : ContractValueV1
    {
        UpstreamReferenceV1 m_plan;
        CapabilityAuthorizationReceiptV1 m_authorization;
        Outcome m_outcome = Outcome::INVALID;
        ExecutionState m_state = ExecutionState::INVALID;
        VerificationState m_verification = VerificationState::INVALID;
        AssessmentState m_assessment = AssessmentState::INVALID;
        PromotionState m_promotion = PromotionState::INVALID;
        ReleaseDecisionState m_releaseDecision = ReleaseDecisionState::INVALID;
        AZStd::optional<UpstreamReferenceV1> m_assessmentReference;
        AZStd::string m_startedAt;
        AZStd::string m_finishedAt;
        AZStd::vector<FailureRecordV1> m_failures;
        AZStd::vector<DiagnosticReferenceV1> m_diagnostics;
        AZStd::vector<CapabilityPhaseReceiptV1> m_phaseReceipts;
        AZStd::vector<RollbackReceiptV1> m_rollbackReceipts;
    };
    constexpr ContractKind Kind(const CapabilityExecutionReceiptV1&) { return ContractKind::CAPABILITY_EXECUTION_RECEIPT; }

}
