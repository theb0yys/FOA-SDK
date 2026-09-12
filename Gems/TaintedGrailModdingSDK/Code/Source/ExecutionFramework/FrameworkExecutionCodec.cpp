/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionCodec.h"
#include <AzCore/JSON/document.h>
#include <AzCore/std/containers/set.h>
#include <type_traits>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        using Json = rapidjson::Value;
        thread_local size_t s_depth = 0;
        thread_local size_t s_bytes = 0;
        thread_local AZStd::vector<CE::UpstreamReferenceV1> s_verifiedReferences;
        thread_local size_t s_referenceBytes = 0;
        thread_local AZStd::string s_lastCanonical;
        struct DecodeBudget
        {
            bool m_ok = false;
            explicit DecodeBudget(size_t bytes)
            {
                if (!s_depth)
                {
                    s_bytes = 0;
                    s_referenceBytes = 0;
                    s_verifiedReferences.clear();
                }
                ++s_depth;
                m_ok = s_depth <= CE::MaximumNesting && bytes <= CE::MaximumCanonicalBytes &&
                    s_bytes <= 64 * CE::MaximumCanonicalBytes - bytes;
                if (m_ok)
                {
                    s_bytes += bytes;
                }
            }
            ~DecodeBudget()
            {
                --s_depth;
            }
        };
        // Bound parser recursion before allocating its DOM, including adversarial unknown fields.
        bool Bounded(const AZStd::string& text)
        {
            size_t depth = 0, nodes = 0;
            bool quoted = false, escaped = false;
            for (char ch : text)
            {
                if (quoted)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (ch == '\\')
                    {
                        escaped = true;
                    }
                    else if (ch == '"')
                    {
                        quoted = false;
                    }
                }
                else if (ch == '"')
                {
                    quoted = true;
                }
                else if (ch == '{' || ch == '[')
                {
                    if (++depth > CE::MaximumNesting || ++nodes > CE::MaximumNodes)
                    {
                        return false;
                    }
                }
                else if (ch == '}' || ch == ']')
                {
                    if (!depth)
                    {
                        return false;
                    }
                    --depth;
                }
            }
            return !quoted && !escaped && !depth;
        }
        bool Object(const Json& j, size_t members)
        {
            if (!j.IsObject() || j.MemberCount() != members)
            {
                return false;
            }
            AZStd::set<AZStd::string> names;
            for (auto it = j.MemberBegin(); it != j.MemberEnd(); ++it)
            {
                if (!names.insert(AZStd::string(it->name.GetString(), it->name.GetStringLength())).second)
                {
                    return false;
                }
            }
            return true;
        }
        bool Read(const Json& j, AZStd::string& value)
        {
            if (!j.IsString() || j.GetStringLength() > CE::MaximumEmbeddedBytes)
            {
                return false;
            }
            value.assign(j.GetString(), j.GetStringLength());
            return true;
        }
        bool Read(const Json& j, bool& v)
        {
            if (!j.IsBool())
            {
                return false;
            }
            v = j.GetBool();
            return true;
        }
        bool Read(const Json& j, AZ::u64& v)
        {
            if (!j.IsUint64())
            {
                return false;
            }
            v = j.GetUint64();
            return true;
        }
        bool Read(const Json& j, AZ::s64& v)
        {
            if (!j.IsInt64())
            {
                return false;
            }
            v = j.GetInt64();
            return true;
        }
        template<class T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
        bool Read(const Json& j, T& v)
        {
            return j.IsString() && CE::ParseToken(AZStd::string_view(j.GetString(), j.GetStringLength()), v);
        }
        bool Read(const Json&, CE::UpstreamReferenceV1&);
        bool Read(const Json&, CE::CapabilityDescriptorV1&);
        bool Read(const Json&, CE::CapabilityProviderBindingV1&);
        bool Read(const Json&, CE::ArtifactReferenceV1&);
        bool Read(const Json&, CE::ExpectedArtifactV1&);
        bool Read(const Json&, CE::ArtifactRecordV1&);
        bool Read(const Json&, CE::OptionV1&);
        bool Read(const Json&, CE::CapabilityExecutionRequestV1&);
        bool Read(const Json&, CE::CapabilitySupportDecisionV1&);
        bool Read(const Json&, CE::CapabilityQualificationDecisionV1&);
        bool Read(const Json&, CE::CapabilityEnvironmentDecisionV1&);
        bool Read(const Json&, CE::CapabilityPolicyDecisionV1&);
        bool Read(const Json&, CE::CapabilityAuthorizationReceiptV1&);
        bool Read(const Json&, CE::TargetMutationClaimV1&);
        bool Read(const Json&, CE::RollbackStepV1&);
        bool Read(const Json&, CE::RollbackPlanV1&);
        bool Read(const Json&, CE::CapabilityPhasePlanV1&);
        bool Read(const Json&, CE::CapabilityExecutionPlanV1&);
        bool Read(const Json&, CE::FailureRecordV1&);
        bool Read(const Json&, CE::DiagnosticReferenceV1&);
        bool Read(const Json&, CE::TargetObservationV1&);
        bool Read(const Json&, CE::PhaseExtensionReferenceV1&);
        bool Read(const Json&, CE::CapabilityPhaseReceiptV1&);
        bool Read(const Json&, CE::RollbackStepReceiptV1&);
        bool Read(const Json&, CE::RollbackReceiptV1&);
        bool Read(const Json&, CE::CapabilityExecutionReceiptV1&);
        template<class T>
        bool Read(const Json& j, AZStd::vector<T>& values)
        {
            if (!j.IsArray() || j.Size() > CE::MaximumCollection)
            {
                return false;
            }
            for (const auto& element : j.GetArray())
            {
                T value{};
                if (!Read(element, value))
                {
                    return false;
                }
                values.push_back(AZStd::move(value));
            }
            return true;
        }
        template<class T>
        bool Read(const Json& j, AZStd::optional<T>& output)
        {
            if (j.IsNull())
            {
                output.reset();
                return true;
            }
            T value{};
            if (!Read(j, value))
            {
                return false;
            }
            output = AZStd::move(value);
            return true;
        }
        template<class T>
        bool Field(const Json& j, const char* key, T& output)
        {
            auto found = j.FindMember(key);
            return found != j.MemberEnd() && Read(found->value, output);
        }
        template<class T>
        bool Header(const Json& j, T& value)
        {
            CE::ContractKind kind{};
            return Field(j, "contract_id", value.m_header.m_contractId) &&
                Field(j, "canonical_profile", value.m_header.m_canonicalProfile) && j.HasMember("version") && j["version"].IsUint() &&
                j["version"].GetUint() == 1 && Field(j, "kind", kind) && kind == CE::Kind(value) && Field(j, "id", value.m_id);
        }
        template<class T>
        bool VerifyReference(const CE::UpstreamReferenceV1& reference, T& typed)
        {
            if (!Decode(reference.m_canonicalJson, typed) || typed.m_id != reference.m_id || typed.m_fingerprint != reference.m_fingerprint)
            {
                return false;
            }
            if (s_verifiedReferences.size() < CE::MaximumCollection &&
                s_referenceBytes + reference.m_canonicalJson.size() <= 4 * CE::MaximumCanonicalBytes)
            {
                s_verifiedReferences.push_back(reference);
                s_referenceBytes += reference.m_canonicalJson.size();
            }
            return true;
        }
        bool Read(const Json& j, CE::UpstreamReferenceV1& value)
        {
            if (!Object(j, 4) || !Field(j, "id", value.m_id) || !Field(j, "kind", value.m_kind) ||
                !Field(j, "canonical_json", value.m_canonicalJson) || !Field(j, "fingerprint", value.m_fingerprint))
            {
                return false;
            }
            for (const auto& checked : s_verifiedReferences)
            {
                if (checked.m_kind == value.m_kind && checked.m_id == value.m_id && checked.m_fingerprint == value.m_fingerprint &&
                    checked.m_canonicalJson == value.m_canonicalJson)
                {
                    return true;
                }
            }
            switch (value.m_kind)
            {
            case CE::ContractKind::CAPABILITY_DESCRIPTOR:
                {
                    CE::CapabilityDescriptorV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_PROVIDER_BINDING:
                {
                    CE::CapabilityProviderBindingV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::ARTIFACT_REFERENCE:
                {
                    CE::ArtifactReferenceV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::EXPECTED_ARTIFACT:
                {
                    CE::ExpectedArtifactV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::ARTIFACT_RECORD:
                {
                    CE::ArtifactRecordV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::OPTION:
                {
                    CE::OptionV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_EXECUTION_REQUEST:
                {
                    CE::CapabilityExecutionRequestV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_SUPPORT_DECISION:
                {
                    CE::CapabilitySupportDecisionV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_QUALIFICATION_DECISION:
                {
                    CE::CapabilityQualificationDecisionV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_ENVIRONMENT_DECISION:
                {
                    CE::CapabilityEnvironmentDecisionV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_POLICY_DECISION:
                {
                    CE::CapabilityPolicyDecisionV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_AUTHORIZATION_RECEIPT:
                {
                    CE::CapabilityAuthorizationReceiptV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::TARGET_MUTATION_CLAIM:
                {
                    CE::TargetMutationClaimV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::ROLLBACK_STEP:
                {
                    CE::RollbackStepV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::ROLLBACK_PLAN:
                {
                    CE::RollbackPlanV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_PHASE_PLAN:
                {
                    CE::CapabilityPhasePlanV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_EXECUTION_PLAN:
                {
                    CE::CapabilityExecutionPlanV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::FAILURE_RECORD:
                {
                    CE::FailureRecordV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::DIAGNOSTIC_REFERENCE:
                {
                    CE::DiagnosticReferenceV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::TARGET_OBSERVATION:
                {
                    CE::TargetObservationV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::PHASE_EXTENSION_REFERENCE:
                {
                    CE::PhaseExtensionReferenceV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_PHASE_RECEIPT:
                {
                    CE::CapabilityPhaseReceiptV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::ROLLBACK_STEP_RECEIPT:
                {
                    CE::RollbackStepReceiptV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::ROLLBACK_RECEIPT:
                {
                    CE::RollbackReceiptV1 typed;
                    return VerifyReference(value, typed);
                }
            case CE::ContractKind::CAPABILITY_EXECUTION_RECEIPT:
                {
                    CE::CapabilityExecutionReceiptV1 typed;
                    return VerifyReference(value, typed);
                }
            default:
                return false;
            }
        }
        template<class T>
        bool Fingerprint(T& value)
        {
            auto encoded = CE::Canonicalize(value);
            if (!encoded.IsSuccess())
            {
                return false;
            }
            value.m_fingerprint = encoded.GetValue().m_fingerprint;
            s_lastCanonical = AZStd::move(encoded.GetValue().m_json);
            return true;
        }
        bool Read(const Json& j, CE::CapabilityDescriptorV1& value)
        {
            return Object(j, 16) && Header(j, value) && Field(j, "capabilityId", value.m_capabilityId) &&
                Field(j, "inputContracts", value.m_inputContracts) && Field(j, "outputContracts", value.m_outputContracts) &&
                Field(j, "requiredPhases", value.m_requiredPhases) && Field(j, "optionalPhases", value.m_optionalPhases) &&
                Field(j, "terminalPhase", value.m_terminalPhase) && Field(j, "sideEffects", value.m_sideEffects) &&
                Field(j, "exactProfileRequired", value.m_exactProfileRequired) && Field(j, "runtimeRequired", value.m_runtimeRequired) &&
                Field(j, "saveImpact", value.m_saveImpact) && Field(j, "rollbackRequired", value.m_rollbackRequired) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityProviderBindingV1& value)
        {
            return Object(j, 20) && Header(j, value) && Field(j, "capabilityId", value.m_capabilityId) &&
                Field(j, "phase", value.m_phase) && Field(j, "providerId", value.m_providerId) &&
                Field(j, "providerVersion", value.m_providerVersion) && Field(j, "versionConstraint", value.m_versionConstraint) &&
                Field(j, "commandId", value.m_commandId) && Field(j, "providerFingerprint", value.m_providerFingerprint) &&
                Field(j, "inputContracts", value.m_inputContracts) && Field(j, "outputContracts", value.m_outputContracts) &&
                Field(j, "profileFingerprint", value.m_profileFingerprint) &&
                Field(j, "qualificationEvidence", value.m_qualificationEvidence) && Field(j, "sideEffects", value.m_sideEffects) &&
                Field(j, "cancellationSupported", value.m_cancellationSupported) && Field(j, "resumeSupported", value.m_resumeSupported) &&
                Field(j, "rollbackSupport", value.m_rollbackSupport) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::ArtifactReferenceV1& value)
        {
            return Object(j, 13) && Header(j, value) && Field(j, "payloadContractId", value.m_payloadContractId) &&
                Field(j, "ownerPackId", value.m_ownerPackId) && Field(j, "storageRootId", value.m_storageRootId) &&
                Field(j, "relativePath", value.m_relativePath) && Field(j, "digest", value.m_digest) &&
                Field(j, "byteSize", value.m_byteSize) && Field(j, "custodianId", value.m_custodianId) &&
                Field(j, "lifecycle", value.m_lifecycle) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::ExpectedArtifactV1& value)
        {
            return Object(j, 16) && Header(j, value) && Field(j, "payloadContractId", value.m_payloadContractId) &&
                Field(j, "ownerPackId", value.m_ownerPackId) && Field(j, "storageRootId", value.m_storageRootId) &&
                Field(j, "relativePath", value.m_relativePath) && Field(j, "expectedDigest", value.m_expectedDigest) &&
                Field(j, "maximumByteSize", value.m_maximumByteSize) && Field(j, "custodianId", value.m_custodianId) &&
                Field(j, "producerPhaseId", value.m_producerPhaseId) && Field(j, "role", value.m_role) &&
                Field(j, "mediaType", value.m_mediaType) && Field(j, "redistribution", value.m_redistribution) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::ArtifactRecordV1& value)
        {
            return Object(j, 13) && Header(j, value) && Field(j, "artifact", value.m_artifact) && Field(j, "role", value.m_role) &&
                Field(j, "mediaType", value.m_mediaType) && Field(j, "producerExecutionId", value.m_producerExecutionId) &&
                Field(j, "producerPhaseId", value.m_producerPhaseId) && Field(j, "producerId", value.m_producerId) &&
                Field(j, "sourceManifest", value.m_sourceManifest) && Field(j, "redistribution", value.m_redistribution) &&
                Fingerprint(value);
        }
        bool Read(const Json& j, CE::OptionV1& value)
        {
            return Object(j, 6) && Header(j, value) && Field(j, "value", value.m_value) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityExecutionRequestV1& value)
        {
            return Object(j, 13) && Header(j, value) && Field(j, "workspaceId", value.m_workspaceId) &&
                Field(j, "packId", value.m_packId) && Field(j, "profileFingerprint", value.m_profileFingerprint) &&
                Field(j, "capabilityId", value.m_capabilityId) && Field(j, "terminalPhase", value.m_terminalPhase) &&
                Field(j, "inputs", value.m_inputs) && Field(j, "preferredBindings", value.m_preferredBindings) &&
                Field(j, "options", value.m_options) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilitySupportDecisionV1& value)
        {
            return Object(j, 10) && Header(j, value) && Field(j, "request", value.m_request) && Field(j, "state", value.m_state) &&
                Field(j, "reason", value.m_reason) && Field(j, "evidenceIds", value.m_evidenceIds) &&
                Field(j, "bindingFingerprints", value.m_bindingFingerprints) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityQualificationDecisionV1& value)
        {
            return Object(j, 10) && Header(j, value) && Field(j, "request", value.m_request) && Field(j, "state", value.m_state) &&
                Field(j, "reason", value.m_reason) && Field(j, "evidenceIds", value.m_evidenceIds) &&
                Field(j, "bindingFingerprints", value.m_bindingFingerprints) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityEnvironmentDecisionV1& value)
        {
            return Object(j, 10) && Header(j, value) && Field(j, "request", value.m_request) && Field(j, "state", value.m_state) &&
                Field(j, "reason", value.m_reason) && Field(j, "evidenceIds", value.m_evidenceIds) &&
                Field(j, "bindingFingerprints", value.m_bindingFingerprints) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityPolicyDecisionV1& value)
        {
            return Object(j, 10) && Header(j, value) && Field(j, "request", value.m_request) && Field(j, "state", value.m_state) &&
                Field(j, "reason", value.m_reason) && Field(j, "evidenceIds", value.m_evidenceIds) &&
                Field(j, "bindingFingerprints", value.m_bindingFingerprints) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityAuthorizationReceiptV1& value)
        {
            return Object(j, 12) && Header(j, value) && Field(j, "scope", value.m_scope) && Field(j, "state", value.m_state) &&
                Field(j, "actorId", value.m_actorId) && Field(j, "reason", value.m_reason) &&
                Field(j, "evidenceIds", value.m_evidenceIds) && Field(j, "issuedAt", value.m_issuedAt) &&
                Field(j, "expiresAt", value.m_expiresAt) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::TargetMutationClaimV1& value)
        {
            return Object(j, 15) && Header(j, value) && Field(j, "targetRootId", value.m_targetRootId) &&
                Field(j, "relativePath", value.m_relativePath) && Field(j, "operation", value.m_operation) &&
                Field(j, "ownerPackId", value.m_ownerPackId) && Field(j, "preimagePresence", value.m_preimagePresence) &&
                Field(j, "preimageFingerprint", value.m_preimageFingerprint) && Field(j, "preimageOwnerId", value.m_preimageOwnerId) &&
                Field(j, "desiredArtifact", value.m_desiredArtifact) && Field(j, "backupArtifact", value.m_backupArtifact) &&
                Field(j, "rollbackStepId", value.m_rollbackStepId) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::RollbackStepV1& value)
        {
            return Object(j, 14) && Header(j, value) && Field(j, "mutationId", value.m_mutationId) && Field(j, "action", value.m_action) &&
                Field(j, "targetRootId", value.m_targetRootId) && Field(j, "relativePath", value.m_relativePath) &&
                Field(j, "ownerPackId", value.m_ownerPackId) &&
                Field(j, "expectedCurrentFingerprint", value.m_expectedCurrentFingerprint) &&
                Field(j, "restoreFingerprint", value.m_restoreFingerprint) && Field(j, "backupArtifact", value.m_backupArtifact) &&
                Field(j, "compensationCommandId", value.m_compensationCommandId) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::RollbackPlanV1& value)
        {
            return Object(j, 7) && Header(j, value) && Field(j, "support", value.m_support) && Field(j, "steps", value.m_steps) &&
                Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityPhasePlanV1& value)
        {
            return Object(j, 20) && Header(j, value) && Field(j, "phase", value.m_phase) &&
                Field(j, "capabilityId", value.m_capabilityId) && Field(j, "profileFingerprint", value.m_profileFingerprint) &&
                Field(j, "binding", value.m_binding) && Field(j, "providerId", value.m_providerId) &&
                Field(j, "providerVersion", value.m_providerVersion) && Field(j, "commandId", value.m_commandId) &&
                Field(j, "providerFingerprint", value.m_providerFingerprint) && Field(j, "inputs", value.m_inputs) &&
                Field(j, "expectedOutputs", value.m_expectedOutputs) && Field(j, "mutations", value.m_mutations) &&
                Field(j, "configurationFingerprint", value.m_configurationFingerprint) &&
                Field(j, "environmentFingerprint", value.m_environmentFingerprint) &&
                Field(j, "targetInventoryFingerprint", value.m_targetInventoryFingerprint) && Field(j, "rollback", value.m_rollback) &&
                Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityExecutionPlanV1& value)
        {
            return Object(j, 13) && Header(j, value) && Field(j, "descriptor", value.m_descriptor) &&
                Field(j, "request", value.m_request) && Field(j, "support", value.m_support) &&
                Field(j, "qualification", value.m_qualification) && Field(j, "environment", value.m_environment) &&
                Field(j, "policy", value.m_policy) && Field(j, "authorizationIntent", value.m_authorizationIntent) &&
                Field(j, "phases", value.m_phases) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::FailureRecordV1& value)
        {
            return Object(j, 8) && Header(j, value) && Field(j, "code", value.m_code) && Field(j, "message", value.m_message) &&
                Field(j, "retryable", value.m_retryable) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::DiagnosticReferenceV1& value)
        {
            return Object(j, 9) && Header(j, value) && Field(j, "storageRootId", value.m_storageRootId) &&
                Field(j, "relativePath", value.m_relativePath) && Field(j, "digest", value.m_digest) &&
                Field(j, "redacted", value.m_redacted) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::TargetObservationV1& value)
        {
            return Object(j, 12) && Header(j, value) && Field(j, "mutationId", value.m_mutationId) &&
                Field(j, "targetRootId", value.m_targetRootId) && Field(j, "relativePath", value.m_relativePath) &&
                Field(j, "presence", value.m_presence) && Field(j, "contentFingerprint", value.m_contentFingerprint) &&
                Field(j, "ownerPackId", value.m_ownerPackId) && Field(j, "verification", value.m_verification) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::PhaseExtensionReferenceV1& value)
        {
            return Object(j, 8) && Header(j, value) && Field(j, "extensionContractId", value.m_extensionContractId) &&
                Field(j, "canonicalJson", value.m_canonicalJson) && Field(j, "extensionFingerprint", value.m_extensionFingerprint) &&
                Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityPhaseReceiptV1& value)
        {
            return Object(j, 26) && Header(j, value) && Field(j, "executionId", value.m_executionId) && Field(j, "plan", value.m_plan) &&
                Field(j, "phasePlan", value.m_phasePlan) && Field(j, "providerId", value.m_providerId) &&
                Field(j, "providerVersion", value.m_providerVersion) && Field(j, "commandId", value.m_commandId) &&
                Field(j, "providerFingerprint", value.m_providerFingerprint) &&
                Field(j, "configurationFingerprint", value.m_configurationFingerprint) &&
                Field(j, "environmentFingerprint", value.m_environmentFingerprint) && Field(j, "attempt", value.m_attempt) &&
                Field(j, "outcome", value.m_outcome) && Field(j, "phaseState", value.m_phaseState) &&
                Field(j, "startedAt", value.m_startedAt) && Field(j, "finishedAt", value.m_finishedAt) &&
                Field(j, "exitCode", value.m_exitCode) && Field(j, "outputs", value.m_outputs) &&
                Field(j, "observations", value.m_observations) && Field(j, "failures", value.m_failures) &&
                Field(j, "diagnostics", value.m_diagnostics) && Field(j, "cleanup", value.m_cleanup) &&
                Field(j, "extension", value.m_extension) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::RollbackStepReceiptV1& value)
        {
            return Object(j, 12) && Header(j, value) && Field(j, "stepId", value.m_stepId) && Field(j, "outcome", value.m_outcome) &&
                Field(j, "observedFingerprint", value.m_observedFingerprint) && Field(j, "observedOwnerId", value.m_observedOwnerId) &&
                Field(j, "startedAt", value.m_startedAt) && Field(j, "finishedAt", value.m_finishedAt) &&
                Field(j, "failures", value.m_failures) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::RollbackReceiptV1& value)
        {
            return Object(j, 11) && Header(j, value) && Field(j, "plan", value.m_plan) && Field(j, "rollbackPlan", value.m_rollbackPlan) &&
                Field(j, "state", value.m_state) && Field(j, "steps", value.m_steps) && Field(j, "failures", value.m_failures) &&
                Field(j, "diagnostics", value.m_diagnostics) && Fingerprint(value);
        }
        bool Read(const Json& j, CE::CapabilityExecutionReceiptV1& value)
        {
            return Object(j, 20) && Header(j, value) && Field(j, "plan", value.m_plan) &&
                Field(j, "authorization", value.m_authorization) && Field(j, "outcome", value.m_outcome) &&
                Field(j, "state", value.m_state) && Field(j, "verification", value.m_verification) &&
                Field(j, "assessment", value.m_assessment) && Field(j, "promotion", value.m_promotion) &&
                Field(j, "releaseDecision", value.m_releaseDecision) && Field(j, "assessmentReference", value.m_assessmentReference) &&
                Field(j, "startedAt", value.m_startedAt) && Field(j, "finishedAt", value.m_finishedAt) &&
                Field(j, "failures", value.m_failures) && Field(j, "diagnostics", value.m_diagnostics) &&
                Field(j, "phaseReceipts", value.m_phaseReceipts) && Field(j, "rollbackReceipts", value.m_rollbackReceipts) &&
                Fingerprint(value);
        }
        template<class T>
        bool DecodeValue(const AZStd::string& json, T& output)
        {
            DecodeBudget budget(json.size());
            if (!budget.m_ok || !Bounded(json))
            {
                return false;
            }
            rapidjson::Document document;
            document.Parse<rapidjson::kParseValidateEncodingFlag>(json.data(), json.size());
            T value;
            if (document.HasParseError() || !Read(document, value))
            {
                return false;
            }
            // Read's final fingerprint step canonicalizes this exact root after its children.
            // Retain those bytes rather than regenerating the entire root for comparison.
            auto canonical = AZStd::move(s_lastCanonical);
            if (canonical != json)
            {
                return false;
            }
            if constexpr (std::is_same_v<T, CE::CapabilityExecutionPlanV1>)
            {
                CE::CapabilityDescriptorV1 descriptor;
                CE::CapabilityExecutionRequestV1 request;
                AZStd::vector<CE::CapabilityProviderBindingV1> bindings;
                if (!Decode(value.m_descriptor.m_canonicalJson, descriptor) || !Decode(value.m_request.m_canonicalJson, request))
                {
                    return false;
                }
                for (const auto& phase : value.m_phases)
                {
                    CE::CapabilityProviderBindingV1 binding;
                    if (!Decode(phase.m_binding.m_canonicalJson, binding))
                    {
                        return false;
                    }
                    bindings.push_back(AZStd::move(binding));
                }
                if (!CE::Validate(value, descriptor, request, bindings).IsSuccess())
                {
                    return false;
                }
            }
            else if (!CE::Validate(value).IsSuccess())
            {
                return false;
            }
            output = AZStd::move(value);
            return true;
        }

    } // namespace
    bool Decode(const AZStd::string& json, CE::CapabilityDescriptorV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityProviderBindingV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::ArtifactReferenceV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::ExpectedArtifactV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::ArtifactRecordV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::OptionV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityExecutionRequestV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilitySupportDecisionV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityQualificationDecisionV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityEnvironmentDecisionV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityPolicyDecisionV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityAuthorizationReceiptV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::TargetMutationClaimV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::RollbackStepV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::RollbackPlanV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityPhasePlanV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityExecutionPlanV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::FailureRecordV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::DiagnosticReferenceV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::TargetObservationV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::PhaseExtensionReferenceV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityPhaseReceiptV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::RollbackStepReceiptV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::RollbackReceiptV1& output)
    {
        return DecodeValue(json, output);
    }
    bool Decode(const AZStd::string& json, CE::CapabilityExecutionReceiptV1& output)
    {
        return DecodeValue(json, output);
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
