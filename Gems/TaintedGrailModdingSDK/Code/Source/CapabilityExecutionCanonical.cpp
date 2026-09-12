/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CapabilityExecutionCanonical.h"
#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include <AzCore/std/containers/set.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK::CapabilityExecution
{
    namespace
    {
        class Sink;
        void Append(Sink&, const UpstreamReferenceV1&);
        void Append(Sink&, const CapabilityDescriptorV1&);
        void Append(Sink&, const CapabilityProviderBindingV1&);
        void Append(Sink&, const ArtifactReferenceV1&);
        void Append(Sink&, const ExpectedArtifactV1&);
        void Append(Sink&, const ArtifactRecordV1&);
        void Append(Sink&, const OptionV1&);
        void Append(Sink&, const CapabilityExecutionRequestV1&);
        void Append(Sink&, const CapabilitySupportDecisionV1&);
        void Append(Sink&, const CapabilityQualificationDecisionV1&);
        void Append(Sink&, const CapabilityEnvironmentDecisionV1&);
        void Append(Sink&, const CapabilityPolicyDecisionV1&);
        void Append(Sink&, const CapabilityAuthorizationReceiptV1&);
        void Append(Sink&, const TargetMutationClaimV1&);
        void Append(Sink&, const RollbackStepV1&);
        void Append(Sink&, const RollbackPlanV1&);
        void Append(Sink&, const CapabilityPhasePlanV1&);
        void Append(Sink&, const CapabilityExecutionPlanV1&);
        void Append(Sink&, const FailureRecordV1&);
        void Append(Sink&, const DiagnosticReferenceV1&);
        void Append(Sink&, const TargetObservationV1&);
        void Append(Sink&, const PhaseExtensionReferenceV1&);
        void Append(Sink&, const CapabilityPhaseReceiptV1&);
        void Append(Sink&, const RollbackStepReceiptV1&);
        void Append(Sink&, const RollbackReceiptV1&);
        void Append(Sink&, const CapabilityExecutionReceiptV1&);

        // This is a bounded lexical nesting audit of opaque upstream bytes, not an object reader.
        bool BoundedOpaqueJson(const AZStd::string& value)
        {
            if (value.size() < 2 || value.size() > MaximumEmbeddedBytes || value.front() != '{'
                || value.back() != '}') { return false; }
            // Nested upstream JSON is carried as exact escaped bytes. Quote escaping
            // is structural; remove only those escape runs for the conservative text audit.
            AZStd::string screened; screened.reserve(value.size());
            for (size_t i = 0; i < value.size();)
            {
                size_t end = i;
                while (end < value.size() && value[end] == '\\') { ++end; }
                if (end > i && end < value.size() && value[end] == '"') { i = end; }
                screened.push_back(value[i++]);
            }
            if (!IsSafeText(screened)) { return false; }
            char stack[MaximumNesting] = {}; size_t depth = 0; bool quoted = false, escaped = false;
            for (char c : value)
            {
                if (quoted)
                {
                    if (escaped) { escaped = false; }
                    else if (c == '\\') { escaped = true; }
                    else if (c == '"') { quoted = false; }
                    continue;
                }
                if (c == '"') { quoted = true; continue; }
                if (c == '{' || c == '[')
                { if (depth == MaximumNesting) { return false; } stack[depth++] = c; }
                else if (c == '}' || c == ']')
                { if (!depth || stack[--depth] != (c == '}' ? '{' : '[')) { return false; } }
            }
            return !quoted && !escaped && depth == 0;
        }
        class Sink
        {
        public:
            explicit Sink(bool dry) : m_dry(dry) {}
            AZStd::string m_output;
            AZStd::string m_error;
            bool Good() const { return m_error.empty(); }
            void Require(bool condition, const char* message)
            { if (Good() && !condition) { m_error = message; } }
            void Bytes(AZStd::string_view value)
            {
                if (!Good()) { return; }
                if (!m_bytes.Add(value.size())) { m_error = "Canonical byte budget exceeded."; return; }
                if (!m_dry) { m_output.append(value.data(), value.size()); }
            }
            void Quote(const AZStd::string& value)
            {
                Bytes("\"");
                for (char c : value)
                {
                    if (!Good()) { return; }
                    if (c == '"' || c == '\\') { Bytes("\\"); }
                    Bytes(AZStd::string_view(&c, 1));
                }
                Bytes("\"");
            }
            void Begin()
            {
                Require(m_depth < MaximumNesting && m_nodes < MaximumNodes, "Contract nesting or node budget exceeded.");
                if (!Good()) { return; }
                ++m_depth; ++m_nodes; m_first[m_depth - 1] = true; Bytes("{");
            }
            void End() { if (!Good()) { return; } Bytes("}"); --m_depth; }
            void Name(const char* name)
            {
                if (!Good()) { return; }
                if (!m_first[m_depth - 1]) { Bytes(","); } m_first[m_depth - 1] = false;
                Quote(name); Bytes(":");
            }
            void Header(const ContractValueV1& v, ContractKind kind)
            {
                Require(v.m_header.m_contractId == ContractId && v.m_header.m_canonicalProfile == CanonicalProfile
                    && v.m_header.m_version == ContractVersion, "Unsupported contract identity, profile or version.");
                Require(v.m_fingerprint.empty() || IsDigest(v.m_fingerprint), "Malformed own fingerprint.");
                Name("contract_id"); Quote(ContractId); Name("canonical_profile"); Quote(CanonicalProfile);
                Name("version"); Bytes("1"); Enum("kind", kind); Id("id", v.m_id);
            }
            void Id(const char* name, const AZStd::string& value, bool optional = false)
            { Require((optional && value.empty()) || IsStableId(value), "Invalid stable namespaced identity."); Name(name); if (Good()) { Quote(value); } }
            void Digest(const char* name, const AZStd::string& value, bool optional = false)
            { Require((optional && value.empty()) || IsDigest(value), "Invalid SHA-256 fingerprint."); Name(name); if (Good()) { Quote(value); } }
            void Path(const char* name, const AZStd::string& value, bool optional = false)
            { Require((optional && value.empty()) || IsRelativeLocator(value), "Unsafe relative locator."); Name(name); if (Good()) { Quote(value); } }
            void Time(const char* name, const AZStd::string& value, bool optional = false)
            { Require((optional && value.empty()) || IsUtcTimestamp(value), "Invalid UTC event timestamp."); Name(name); if (Good()) { Quote(value); } }
            void Text(const char* name, const AZStd::string& value)
            { Require(value.size() <= MaximumStringBytes && IsSafeText(value), "Text is excessive, malformed or contains forbidden data."); Name(name); if (Good()) { Quote(value); } }
            void Json(const char* name, const AZStd::string& value)
            { Require(BoundedOpaqueJson(value), "Upstream canonical bytes are excessive or unsafe."); Name(name); if (Good()) { Quote(value); } }
            void Version(const char* name, const AZStd::string& value)
            { Require(IsExactVersion(value), "Provider versions must be exact numeric semantic versions."); Name(name); if (Good()) { Quote(value); } }
            void Media(const char* name, const AZStd::string& value)
            {
                const auto slash = value.find('/');
                Require(value.size() <= 128 && slash > 0 && slash != value.npos && slash + 1 < value.size()
                    && value.find('/', slash + 1) == value.npos && AZStd::all_of(value.begin(), value.end(), [](char c)
                    { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                        || c == '/' || c == '.' || c == '-' || c == '+'; }), "Invalid media type.");
                Name(name); if (Good()) { Quote(value); }
            }
            void Number(const char* name, AZ::u64 value, AZ::u64 maximum = MaximumArtifactBytes)
            { Require(value <= maximum, "Numeric bound exceeded."); Name(name); if (Good()) { Bytes(DeterministicContractJson::UnsignedString(value)); } }
            void Integer(const char* name, const AZStd::optional<AZ::s64>& value)
            {
                Name(name); if (!value) { Bytes("null"); return; }
                Require(*value >= -2147483648LL && *value <= 2147483647LL, "Exit code exceeds signed 32-bit range.");
                if (Good()) { Bytes(DeterministicContractJson::SignedString(*value)); }
            }
            void Bool(const char* name, bool value) { Name(name); Bytes(value ? "true" : "false"); }
            template<class T> void Enum(const char* name, T value)
            { const auto token = Token(value); Require(!token.empty(), "Unknown enum value."); Name(name); if (Good()) { Quote(AZStd::string(token)); } }
            void Metadata(const CaptureMetadataV1& value)
            {
                Require(value.m_label.size() <= MaximumStringBytes && IsSafeText(value.m_label)
                    && (value.m_capturedAt.empty() || IsUtcTimestamp(value.m_capturedAt))
                    && (value.m_diagnosticLocator.empty() || IsRelativeLocator(value.m_diagnosticLocator)),
                    "Invalid excluded capture metadata.");
            }
            template<class T> void Object(const char* name, const T& value)
            { Name(name); if (Good()) { Append(*this, value); } }
            template<class T> void Optional(const char* name, const AZStd::optional<T>& value)
            { Name(name); if (!value) { Bytes("null"); } else if (Good()) { Append(*this, *value); } }
            template<class T> void Objects(const char* name, const AZStd::vector<T>& values, bool sorted, size_t maximum = MaximumCollection)
            {
                Require(values.size() <= maximum, "Collection bound exceeded.");
                if (!Good()) { return; }
                AZStd::vector<const T*> order; order.reserve(values.size()); AZStd::set<AZStd::string_view> ids;
                for (const auto& value : values)
                {
                    Require(IsStableId(value.m_id) && ids.insert(value.m_id).second, "Invalid or duplicate collection identity.");
                    if (!Good()) { return; } order.push_back(&value);
                }
                if (sorted) { AZStd::sort(order.begin(), order.end(), [](const T* a, const T* b) { return a->m_id < b->m_id; }); }
                Name(name); Bytes("["); bool first = true;
                for (const auto* value : order) { if (!Good()) { return; } if (!first) { Bytes(","); } first = false; Append(*this, *value); }
                Bytes("]");
            }
            void Strings(const char* name, const AZStd::vector<AZStd::string>& values, bool digests)
            {
                Require(values.size() <= MaximumCollection, "Set cardinality exceeded.");
                if (!Good()) { return; }
                AZStd::set<AZStd::string_view> sorted;
                for (const auto& value : values)
                {
                    Require((digests ? IsDigest(value) : IsStableId(value)) && sorted.insert(value).second,
                        "Invalid or duplicate set value."); if (!Good()) { return; }
                }
                Name(name); Bytes("["); bool first = true;
                for (const auto value : sorted) { if (!first) { Bytes(","); } first = false; Quote(AZStd::string(value)); } Bytes("]");
            }
            template<class T> void Enums(const char* name, const AZStd::vector<T>& values, bool sorted)
            {
                Require(values.size() <= MaximumCollection, "Phase/side-effect cardinality exceeded.");
                if (!Good()) { return; }
                AZStd::vector<AZStd::string_view> tokens; AZStd::set<AZStd::string_view> seen;
                for (const auto value : values)
                {
                    const auto token = Token(value);
                    Require(!token.empty() && seen.insert(token).second, "Unknown or duplicate enum set value.");
                    if (!Good()) { return; } tokens.push_back(token);
                }
                if (sorted) { AZStd::sort(tokens.begin(), tokens.end()); }
                Name(name); Bytes("["); bool first = true;
                for (const auto token : tokens) { if (!first) { Bytes(","); } first = false; Quote(AZStd::string(token)); } Bytes("]");
            }
        private:
            bool m_dry;
            SizeBudget m_bytes;
            size_t m_nodes = 0, m_depth = 0;
            bool m_first[MaximumNesting] = {};
        };

        void Append(Sink& out, const UpstreamReferenceV1& v)
        {
            out.Begin(); out.Id("id", v.m_id); out.Enum("kind", v.m_kind);
            out.Json("canonical_json", v.m_canonicalJson); out.Digest("fingerprint", v.m_fingerprint); out.End();
        }

        void Append(Sink& out, const CapabilityDescriptorV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("capabilityId", v.m_capabilityId);
            out.Strings("inputContracts", v.m_inputContracts, false);
            out.Strings("outputContracts", v.m_outputContracts, false);
            out.Enums("requiredPhases", v.m_requiredPhases, false);
            out.Enums("optionalPhases", v.m_optionalPhases, true);
            out.Enum("terminalPhase", v.m_terminalPhase);
            out.Enums("sideEffects", v.m_sideEffects, true);
            out.Bool("exactProfileRequired", v.m_exactProfileRequired);
            out.Bool("runtimeRequired", v.m_runtimeRequired);
            out.Enum("saveImpact", v.m_saveImpact);
            out.Enum("rollbackRequired", v.m_rollbackRequired);
            out.End();
        }

        void Append(Sink& out, const CapabilityProviderBindingV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("capabilityId", v.m_capabilityId);
            out.Enum("phase", v.m_phase);
            out.Id("providerId", v.m_providerId);
            out.Version("providerVersion", v.m_providerVersion);
            out.Version("versionConstraint", v.m_versionConstraint);
            out.Id("commandId", v.m_commandId);
            out.Digest("providerFingerprint", v.m_providerFingerprint);
            out.Strings("inputContracts", v.m_inputContracts, false);
            out.Strings("outputContracts", v.m_outputContracts, false);
            out.Digest("profileFingerprint", v.m_profileFingerprint);
            out.Strings("qualificationEvidence", v.m_qualificationEvidence, false);
            out.Enums("sideEffects", v.m_sideEffects, true);
            out.Bool("cancellationSupported", v.m_cancellationSupported);
            out.Bool("resumeSupported", v.m_resumeSupported);
            out.Enum("rollbackSupport", v.m_rollbackSupport);
            out.End();
        }

        void Append(Sink& out, const ArtifactReferenceV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("payloadContractId", v.m_payloadContractId);
            out.Id("ownerPackId", v.m_ownerPackId);
            out.Id("storageRootId", v.m_storageRootId);
            out.Path("relativePath", v.m_relativePath);
            out.Digest("digest", v.m_digest);
            out.Number("byteSize", v.m_byteSize);
            out.Id("custodianId", v.m_custodianId);
            out.Enum("lifecycle", v.m_lifecycle);
            out.End();
        }

        void Append(Sink& out, const ExpectedArtifactV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("payloadContractId", v.m_payloadContractId);
            out.Id("ownerPackId", v.m_ownerPackId);
            out.Id("storageRootId", v.m_storageRootId);
            out.Path("relativePath", v.m_relativePath);
            out.Digest("expectedDigest", v.m_expectedDigest, true);
            out.Number("maximumByteSize", v.m_maximumByteSize);
            out.Id("custodianId", v.m_custodianId);
            out.Id("producerPhaseId", v.m_producerPhaseId);
            out.Id("role", v.m_role);
            out.Media("mediaType", v.m_mediaType);
            out.Enum("redistribution", v.m_redistribution);
            out.End();
        }

        void Append(Sink& out, const ArtifactRecordV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("artifact", v.m_artifact);
            out.Id("role", v.m_role);
            out.Media("mediaType", v.m_mediaType);
            out.Id("producerExecutionId", v.m_producerExecutionId);
            out.Id("producerPhaseId", v.m_producerPhaseId);
            out.Id("producerId", v.m_producerId);
            out.Object("sourceManifest", v.m_sourceManifest);
            out.Enum("redistribution", v.m_redistribution);
            out.End();
        }

        void Append(Sink& out, const OptionV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Text("value", v.m_value);
            out.End();
        }

        void Append(Sink& out, const CapabilityExecutionRequestV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("workspaceId", v.m_workspaceId);
            out.Id("packId", v.m_packId);
            out.Digest("profileFingerprint", v.m_profileFingerprint);
            out.Id("capabilityId", v.m_capabilityId);
            out.Enum("terminalPhase", v.m_terminalPhase);
            out.Objects("inputs", v.m_inputs, false);
            out.Objects("preferredBindings", v.m_preferredBindings, true);
            out.Objects("options", v.m_options, true);
            out.Metadata(v.m_capture);
            out.End();
        }

        void Append(Sink& out, const CapabilitySupportDecisionV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("request", v.m_request);
            out.Enum("state", v.m_state);
            out.Id("reason", v.m_reason);
            out.Strings("evidenceIds", v.m_evidenceIds, false);
            out.Strings("bindingFingerprints", v.m_bindingFingerprints, true);
            out.End();
        }

        void Append(Sink& out, const CapabilityQualificationDecisionV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("request", v.m_request);
            out.Enum("state", v.m_state);
            out.Id("reason", v.m_reason);
            out.Strings("evidenceIds", v.m_evidenceIds, false);
            out.Strings("bindingFingerprints", v.m_bindingFingerprints, true);
            out.End();
        }

        void Append(Sink& out, const CapabilityEnvironmentDecisionV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("request", v.m_request);
            out.Enum("state", v.m_state);
            out.Id("reason", v.m_reason);
            out.Strings("evidenceIds", v.m_evidenceIds, false);
            out.Strings("bindingFingerprints", v.m_bindingFingerprints, true);
            out.End();
        }

        void Append(Sink& out, const CapabilityPolicyDecisionV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("request", v.m_request);
            out.Enum("state", v.m_state);
            out.Id("reason", v.m_reason);
            out.Strings("evidenceIds", v.m_evidenceIds, false);
            out.Strings("bindingFingerprints", v.m_bindingFingerprints, true);
            out.End();
        }

        void Append(Sink& out, const CapabilityAuthorizationReceiptV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("scope", v.m_scope);
            out.Enum("state", v.m_state);
            out.Id("actorId", v.m_actorId, true);
            out.Id("reason", v.m_reason);
            out.Strings("evidenceIds", v.m_evidenceIds, false);
            out.Time("issuedAt", v.m_issuedAt, true);
            out.Time("expiresAt", v.m_expiresAt, true);
            out.End();
        }

        void Append(Sink& out, const TargetMutationClaimV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("targetRootId", v.m_targetRootId);
            out.Path("relativePath", v.m_relativePath);
            out.Enum("operation", v.m_operation);
            out.Id("ownerPackId", v.m_ownerPackId);
            out.Enum("preimagePresence", v.m_preimagePresence);
            out.Digest("preimageFingerprint", v.m_preimageFingerprint, true);
            out.Id("preimageOwnerId", v.m_preimageOwnerId, true);
            out.Optional("desiredArtifact", v.m_desiredArtifact);
            out.Optional("backupArtifact", v.m_backupArtifact);
            out.Id("rollbackStepId", v.m_rollbackStepId);
            out.End();
        }

        void Append(Sink& out, const RollbackStepV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("mutationId", v.m_mutationId);
            out.Enum("action", v.m_action);
            out.Id("targetRootId", v.m_targetRootId);
            out.Path("relativePath", v.m_relativePath);
            out.Id("ownerPackId", v.m_ownerPackId);
            out.Digest("expectedCurrentFingerprint", v.m_expectedCurrentFingerprint, true);
            out.Digest("restoreFingerprint", v.m_restoreFingerprint, true);
            out.Optional("backupArtifact", v.m_backupArtifact);
            out.Id("compensationCommandId", v.m_compensationCommandId, true);
            out.End();
        }

        void Append(Sink& out, const RollbackPlanV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Enum("support", v.m_support);
            out.Objects("steps", v.m_steps, false);
            out.End();
        }

        void Append(Sink& out, const CapabilityPhasePlanV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Enum("phase", v.m_phase);
            out.Id("capabilityId", v.m_capabilityId);
            out.Digest("profileFingerprint", v.m_profileFingerprint);
            out.Object("binding", v.m_binding);
            out.Id("providerId", v.m_providerId);
            out.Version("providerVersion", v.m_providerVersion);
            out.Id("commandId", v.m_commandId);
            out.Digest("providerFingerprint", v.m_providerFingerprint);
            out.Objects("inputs", v.m_inputs, false);
            out.Objects("expectedOutputs", v.m_expectedOutputs, true);
            out.Objects("mutations", v.m_mutations, false);
            out.Digest("configurationFingerprint", v.m_configurationFingerprint);
            out.Digest("environmentFingerprint", v.m_environmentFingerprint);
            out.Digest("targetInventoryFingerprint", v.m_targetInventoryFingerprint);
            out.Object("rollback", v.m_rollback);
            out.Metadata(v.m_capture);
            out.End();
        }

        void Append(Sink& out, const CapabilityExecutionPlanV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("descriptor", v.m_descriptor);
            out.Object("request", v.m_request);
            out.Object("support", v.m_support);
            out.Object("qualification", v.m_qualification);
            out.Object("environment", v.m_environment);
            out.Object("policy", v.m_policy);
            out.Object("authorizationIntent", v.m_authorizationIntent);
            out.Objects("phases", v.m_phases, false, MaximumPhases);
            out.Metadata(v.m_capture);
            out.End();
        }

        void Append(Sink& out, const FailureRecordV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("code", v.m_code);
            out.Text("message", v.m_message);
            out.Bool("retryable", v.m_retryable);
            out.End();
        }

        void Append(Sink& out, const DiagnosticReferenceV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("storageRootId", v.m_storageRootId);
            out.Path("relativePath", v.m_relativePath);
            out.Digest("digest", v.m_digest);
            out.Bool("redacted", v.m_redacted);
            out.End();
        }

        void Append(Sink& out, const TargetObservationV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("mutationId", v.m_mutationId);
            out.Id("targetRootId", v.m_targetRootId);
            out.Path("relativePath", v.m_relativePath);
            out.Enum("presence", v.m_presence);
            out.Digest("contentFingerprint", v.m_contentFingerprint, true);
            out.Id("ownerPackId", v.m_ownerPackId, true);
            out.Enum("verification", v.m_verification);
            out.End();
        }

        void Append(Sink& out, const PhaseExtensionReferenceV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("extensionContractId", v.m_extensionContractId);
            out.Json("canonicalJson", v.m_canonicalJson);
            out.Digest("extensionFingerprint", v.m_extensionFingerprint);
            out.End();
        }

        void Append(Sink& out, const CapabilityPhaseReceiptV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("executionId", v.m_executionId);
            out.Object("plan", v.m_plan);
            out.Object("phasePlan", v.m_phasePlan);
            out.Id("providerId", v.m_providerId);
            out.Version("providerVersion", v.m_providerVersion);
            out.Id("commandId", v.m_commandId);
            out.Digest("providerFingerprint", v.m_providerFingerprint);
            out.Digest("configurationFingerprint", v.m_configurationFingerprint);
            out.Digest("environmentFingerprint", v.m_environmentFingerprint);
            out.Number("attempt", v.m_attempt, 1000000000);
            out.Enum("outcome", v.m_outcome);
            out.Enum("phaseState", v.m_phaseState);
            out.Time("startedAt", v.m_startedAt, true);
            out.Time("finishedAt", v.m_finishedAt, true);
            out.Integer("exitCode", v.m_exitCode);
            out.Objects("outputs", v.m_outputs, true);
            out.Objects("observations", v.m_observations, true);
            out.Objects("failures", v.m_failures, true);
            out.Objects("diagnostics", v.m_diagnostics, true);
            out.Enum("cleanup", v.m_cleanup);
            out.Optional("extension", v.m_extension);
            out.End();
        }

        void Append(Sink& out, const RollbackStepReceiptV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Id("stepId", v.m_stepId);
            out.Enum("outcome", v.m_outcome);
            out.Digest("observedFingerprint", v.m_observedFingerprint, true);
            out.Id("observedOwnerId", v.m_observedOwnerId, true);
            out.Time("startedAt", v.m_startedAt, true);
            out.Time("finishedAt", v.m_finishedAt, true);
            out.Objects("failures", v.m_failures, true);
            out.End();
        }

        void Append(Sink& out, const RollbackReceiptV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("plan", v.m_plan);
            out.Object("rollbackPlan", v.m_rollbackPlan);
            out.Enum("state", v.m_state);
            out.Objects("steps", v.m_steps, false);
            out.Objects("failures", v.m_failures, true);
            out.Objects("diagnostics", v.m_diagnostics, true);
            out.End();
        }

        void Append(Sink& out, const CapabilityExecutionReceiptV1& v)
        {
            out.Begin(); out.Header(v, Kind(v));
            out.Object("plan", v.m_plan);
            out.Object("authorization", v.m_authorization);
            out.Enum("outcome", v.m_outcome);
            out.Enum("state", v.m_state);
            out.Enum("verification", v.m_verification);
            out.Enum("assessment", v.m_assessment);
            out.Enum("promotion", v.m_promotion);
            out.Enum("releaseDecision", v.m_releaseDecision);
            out.Optional("assessmentReference", v.m_assessmentReference);
            out.Time("startedAt", v.m_startedAt, true);
            out.Time("finishedAt", v.m_finishedAt, true);
            out.Objects("failures", v.m_failures, true);
            out.Objects("diagnostics", v.m_diagnostics, true);
            out.Objects("phaseReceipts", v.m_phaseReceipts, false);
            out.Objects("rollbackReceipts", v.m_rollbackReceipts, false);
            out.End();
        }

        template<class T> AZ::Outcome<CanonicalValueV1, AZStd::string> Project(const T& value)
        {
            // Validate every bound before allocating the final canonical buffer.
            Sink audit(true); Append(audit, value);
            if (!audit.Good()) { return AZ::Failure(audit.m_error); }
            Sink writer(false); Append(writer, value);
            if (!writer.Good()) { return AZ::Failure(writer.m_error); }
            CanonicalValueV1 result;
            result.m_json = AZStd::move(writer.m_output);
            result.m_fingerprint = CalculateCanonicalSha256(result.m_json);
            return AZ::Success(AZStd::move(result));
        }
    }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityDescriptorV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityProviderBindingV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const ArtifactReferenceV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const ExpectedArtifactV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const ArtifactRecordV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const OptionV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityExecutionRequestV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilitySupportDecisionV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityQualificationDecisionV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityEnvironmentDecisionV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityPolicyDecisionV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityAuthorizationReceiptV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const TargetMutationClaimV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackStepV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackPlanV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityPhasePlanV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityExecutionPlanV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const FailureRecordV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const DiagnosticReferenceV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const TargetObservationV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const PhaseExtensionReferenceV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityPhaseReceiptV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackStepReceiptV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const RollbackReceiptV1& value) { return Project(value); }
    AZ::Outcome<CanonicalValueV1, AZStd::string> Canonicalize(const CapabilityExecutionReceiptV1& value) { return Project(value); }
}
