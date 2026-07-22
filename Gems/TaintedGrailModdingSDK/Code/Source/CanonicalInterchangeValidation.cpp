/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeValidation.h"

#include "CanonicalInterchangeCanonical.h"

#include <AzCore/JSON/document.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/sort.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>

namespace TaintedGrailModdingSDK::Interchange
{
    namespace
    {
        using JsonValue = rapidjson::Value;

        int IssuePrecedence(AZStd::string_view code)
        {
            if (code.starts_with("interchange.schema.")) { return 0; }
            if (code.starts_with("interchange.canonical.")) { return 1; }
            if (code.starts_with("interchange.identity.") || code == IssueCodeV1::RevisionMismatch) { return 2; }
            if (code.starts_with("interchange.path.")) { return 3; }
            if (code.starts_with("interchange.reference.") || code.starts_with("interchange.dependency.") || code.starts_with("interchange.payload.")) { return 4; }
            if (code.starts_with("interchange.binding.") || code == IssueCodeV1::MappingInvalid) { return 5; }
            if (code.starts_with("interchange.transform.") || code.starts_with("interchange.loss.")) { return 6; }
            if (code.starts_with("interchange.provenance.") || code.starts_with("interchange.licensing.") || code.starts_with("interchange.content.")) { return 7; }
            if (code.starts_with("interchange.lock.") || code.starts_with("interchange.extension.")) { return 8; }
            return 9;
        }

        IssueSeverityV1 DefaultSeverity(AZStd::string_view code)
        {
            if (code == IssueCodeV1::UnsupportedVersion || code == IssueCodeV1::UnsupportedProfile ||
                code == IssueCodeV1::ForbiddenField || code == IssueCodeV1::MappingInvalid ||
                code == IssueCodeV1::BindingUnresolvedRequired || code == IssueCodeV1::BindingConflict ||
                code == IssueCodeV1::TransformMatrixInvalid || code == IssueCodeV1::LossBlocker ||
                code == IssueCodeV1::ProvenanceIncomplete || code == IssueCodeV1::LicensingNoAssertionBlocked ||
                code == IssueCodeV1::ContentProhibited || code == IssueCodeV1::LockIncomplete ||
                code == IssueCodeV1::ExtensionUnapproved)
            {
                return IssueSeverityV1::Blocker;
            }
            return IssueSeverityV1::Error;
        }

        void AddIssue(
            CanonicalInterchangeValidationResultV1& result,
            AZStd::string_view code,
            AZStd::string_view subject = {},
            AZStd::string_view propertyPath = {},
            AZStd::vector<AZStd::string> relatedIds = {})
        {
            AZStd::sort(relatedIds.begin(), relatedIds.end());
            relatedIds.erase(AZStd::unique(relatedIds.begin(), relatedIds.end()), relatedIds.end());
            result.m_issues.push_back({
                DefaultSeverity(code), AZStd::string(code), AZStd::string(subject),
                AZStd::string(propertyPath), AZStd::move(relatedIds) });
        }

        AZStd::string ChildPath(AZStd::string_view parent, AZStd::string_view child)
        {
            AZStd::string result(parent);
            if (!result.empty()) { result.push_back('.'); }
            result.append(child.data(), child.size());
            return result;
        }

        AZStd::string IndexedPath(AZStd::string_view parent, size_t index)
        {
            char buffer[32] = {};
            std::snprintf(buffer, sizeof(buffer), "[%zu]", index);
            AZStd::string result(parent);
            result += buffer;
            return result;
        }

        bool ContainsForbiddenFieldTerm(AZStd::string_view name)
        {
            constexpr AZStd::array<AZStd::string_view, 13> Terms = {
                "authority", "permission", "credential", "password", "secret",
                "command", "runtime", "deployment", "execution", "process",
                "environment", "game_path", "path_to_game"
            };
            for (const auto term : Terms)
            {
                if (name.find(term) != AZStd::string_view::npos) { return true; }
            }
            return false;
        }

        bool IsAllowedName(AZStd::string_view name, std::initializer_list<const char*> allowed)
        {
            for (const char* candidate : allowed)
            {
                if (name == candidate) { return true; }
            }
            return false;
        }

        struct ParseContext
        {
            CanonicalInterchangeValidationResultV1 m_result;
            bool m_structurallyValid = true;

            void Fail(AZStd::string_view code, AZStd::string_view path)
            {
                AddIssue(m_result, code, {}, path);
                m_structurallyValid = false;
            }
        };

        void InspectJsonTree(const JsonValue& value, AZStd::string_view path, size_t depth, ParseContext& context)
        {
            if (depth > MaxCanonicalDepthV1)
            {
                context.Fail(IssueCodeV1::DepthExceeded, path);
                return;
            }
            if (value.IsObject())
            {
                AZStd::vector<AZStd::string> names;
                for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                {
                    AZStd::string name(member->name.GetString(), member->name.GetStringLength());
                    if (AZStd::find(names.begin(), names.end(), name) != names.end())
                    {
                        context.Fail(IssueCodeV1::DuplicateKey, ChildPath(path, name));
                    }
                    else
                    {
                        names.push_back(name);
                    }
                    InspectJsonTree(member->value, ChildPath(path, name), depth + 1, context);
                }
            }
            else if (value.IsArray())
            {
                for (rapidjson::SizeType index = 0; index < value.Size(); ++index)
                {
                    InspectJsonTree(value[index], IndexedPath(path, index), depth + 1, context);
                }
            }
        }

        bool CheckObject(
            const JsonValue& value,
            AZStd::string_view path,
            std::initializer_list<const char*> allowed,
            ParseContext& context)
        {
            if (!value.IsObject())
            {
                context.Fail(IssueCodeV1::InvalidJson, path);
                return false;
            }
            bool valid = true;
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
            {
                const AZStd::string_view name(member->name.GetString(), member->name.GetStringLength());
                if (!IsAllowedName(name, allowed))
                {
                    context.Fail(
                        ContainsForbiddenFieldTerm(name) ? IssueCodeV1::ForbiddenField : IssueCodeV1::UnknownField,
                        ChildPath(path, name));
                    valid = false;
                }
            }
            return valid;
        }

        const JsonValue* FindMember(const JsonValue& object, const char* name)
        {
            if (!object.IsObject()) { return nullptr; }
            const auto member = object.FindMember(name);
            return member == object.MemberEnd() ? nullptr : &member->value;
        }

        const JsonValue* RequireMember(
            const JsonValue& object,
            const char* name,
            AZStd::string_view path,
            ParseContext& context)
        {
            const JsonValue* value = FindMember(object, name);
            if (value == nullptr)
            {
                context.Fail(IssueCodeV1::InvalidJson, ChildPath(path, name));
            }
            return value;
        }

        bool ReadStringValue(
            const JsonValue& value,
            AZStd::string& output,
            AZStd::string_view path,
            ParseContext& context,
            size_t maxBytes = MaxPresentationStringBytesV1)
        {
            if (!value.IsString())
            {
                context.Fail(IssueCodeV1::InvalidJson, path);
                return false;
            }
            const AZStd::string_view text(value.GetString(), value.GetStringLength());
            if (text.size() > maxBytes)
            {
                context.Fail(IssueCodeV1::SizeExceeded, path);
                return false;
            }
            if (!IsValidUtf8V1(text))
            {
                context.Fail(IssueCodeV1::InvalidString, path);
                return false;
            }
            output.assign(text.data(), text.size());
            return true;
        }

        bool ReadString(
            const JsonValue& object,
            const char* name,
            AZStd::string& output,
            AZStd::string_view path,
            ParseContext& context,
            bool required = true,
            size_t maxBytes = MaxPresentationStringBytesV1)
        {
            const JsonValue* value = required ? RequireMember(object, name, path, context) : FindMember(object, name);
            if (value == nullptr) { return !required; }
            return ReadStringValue(*value, output, ChildPath(path, name), context, maxBytes);
        }

        bool ReadBool(const JsonValue& object, const char* name, bool& output, AZStd::string_view path, ParseContext& context)
        {
            const JsonValue* value = RequireMember(object, name, path, context);
            if (value == nullptr) { return false; }
            if (!value->IsBool()) { context.Fail(IssueCodeV1::InvalidJson, ChildPath(path, name)); return false; }
            output = value->GetBool();
            return true;
        }

        bool ReadU32(const JsonValue& object, const char* name, AZ::u32& output, AZStd::string_view path, ParseContext& context)
        {
            const JsonValue* value = RequireMember(object, name, path, context);
            if (value == nullptr) { return false; }
            if (!value->IsUint()) { context.Fail(IssueCodeV1::InvalidJson, ChildPath(path, name)); return false; }
            output = value->GetUint();
            return true;
        }

        bool ReadU64(const JsonValue& object, const char* name, AZ::u64& output, AZStd::string_view path, ParseContext& context)
        {
            const JsonValue* value = RequireMember(object, name, path, context);
            if (value == nullptr) { return false; }
            if (!value->IsUint64()) { context.Fail(IssueCodeV1::InvalidJson, ChildPath(path, name)); return false; }
            output = value->GetUint64();
            return true;
        }

        bool ReadS64(const JsonValue& object, const char* name, AZ::s64& output, AZStd::string_view path, ParseContext& context)
        {
            const JsonValue* value = RequireMember(object, name, path, context);
            if (value == nullptr) { return false; }
            if (!value->IsInt64()) { context.Fail(IssueCodeV1::InvalidJson, ChildPath(path, name)); return false; }
            output = value->GetInt64();
            return true;
        }

        bool ReadNumberValue(const JsonValue& value, double& output, AZStd::string_view path, ParseContext& context)
        {
            if (!value.IsNumber()) { context.Fail(IssueCodeV1::InvalidJson, path); return false; }
            output = value.GetDouble();
            if (!std::isfinite(output)) { context.Fail(IssueCodeV1::NonFiniteNumber, path); return false; }
            return true;
        }

        bool ReadNumber(const JsonValue& object, const char* name, double& output, AZStd::string_view path, ParseContext& context)
        {
            const JsonValue* value = RequireMember(object, name, path, context);
            return value != nullptr && ReadNumberValue(*value, output, ChildPath(path, name), context);
        }

        template<class Enum>
        bool ReadToken(const JsonValue& object, const char* name, Enum& output, AZStd::string_view path, ParseContext& context)
        {
            AZStd::string token;
            if (!ReadString(object, name, token, path, context)) { return false; }
            if (!TryParseToken(token, output))
            {
                context.Fail(IssueCodeV1::InvalidString, ChildPath(path, name));
                return false;
            }
            return true;
        }

        template<class Item, class Reader>
        bool ReadArray(
            const JsonValue& object,
            const char* name,
            AZStd::vector<Item>& output,
            AZStd::string_view path,
            ParseContext& context,
            size_t maxCount,
            Reader reader)
        {
            const JsonValue* value = RequireMember(object, name, path, context);
            if (value == nullptr) { return false; }
            const AZStd::string arrayPath = ChildPath(path, name);
            if (!value->IsArray()) { context.Fail(IssueCodeV1::InvalidJson, arrayPath); return false; }
            if (value->Size() > maxCount) { context.Fail(IssueCodeV1::SizeExceeded, arrayPath); return false; }
            output.clear();
            output.reserve(value->Size());
            bool valid = true;
            for (rapidjson::SizeType index = 0; index < value->Size(); ++index)
            {
                Item item{};
                if (!reader((*value)[index], item, IndexedPath(arrayPath, index), context)) { valid = false; }
                output.push_back(AZStd::move(item));
            }
            return valid;
        }

        bool ReadStringItem(const JsonValue& value, AZStd::string& output, AZStd::string_view path, ParseContext& context)
        {
            return ReadStringValue(value, output, path, context);
        }

        template<class Wrapper>
        bool ReadValueWrapperItem(const JsonValue& value, Wrapper& output, AZStd::string_view path, ParseContext& context)
        {
            return ReadStringValue(value, output.m_value, path, context);
        }

        bool ReadRevisionItem(const JsonValue& value, RevisionFingerprintV1& output, AZStd::string_view path, ParseContext& context)
        {
            return ReadStringValue(value, output.m_sha256, path, context);
        }

        bool ReadSpatialBasis(const JsonValue& value, CanonicalSpatialBasisV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "handedness", "right_axis", "forward_axis", "up_axis", "linear_unit", "angular_unit", "vector_convention", "multiplication_convention", "storage_order" }, context);
            return ReadString(value, "handedness", output.m_handedness, path, context) &&
                ReadString(value, "right_axis", output.m_rightAxis, path, context) &&
                ReadString(value, "forward_axis", output.m_forwardAxis, path, context) &&
                ReadString(value, "up_axis", output.m_upAxis, path, context) &&
                ReadString(value, "linear_unit", output.m_linearUnit, path, context) &&
                ReadString(value, "angular_unit", output.m_angularUnit, path, context) &&
                ReadString(value, "vector_convention", output.m_vectorConvention, path, context) &&
                ReadString(value, "multiplication_convention", output.m_multiplicationConvention, path, context) &&
                ReadString(value, "storage_order", output.m_storageOrder, path, context);
        }

        bool ReadTemporalBasis(const JsonValue& value, CanonicalTemporalBasisV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "time_unit" }, context);
            return ReadString(value, "time_unit", output.m_timeUnit, path, context);
        }

        bool ReadMatrix(const JsonValue& value, Matrix4x4V1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "column_major_values", "mapping_direction" }, context);
            const JsonValue* values = RequireMember(value, "column_major_values", path, context);
            bool valid = values != nullptr;
            if (values != nullptr)
            {
                if (!values->IsArray() || values->Size() != output.m_columnMajorValues.size())
                {
                    context.Fail(IssueCodeV1::InvalidJson, ChildPath(path, "column_major_values"));
                    valid = false;
                }
                else
                {
                    for (rapidjson::SizeType index = 0; index < values->Size(); ++index)
                    {
                        valid = ReadNumberValue((*values)[index], output.m_columnMajorValues[index], IndexedPath(ChildPath(path, "column_major_values"), index), context) && valid;
                    }
                }
            }
            return ReadString(value, "mapping_direction", output.m_mappingDirection, path, context) && valid;
        }

        bool ReadRate(const JsonValue& value, RationalRateV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "numerator", "denominator" }, context);
            return ReadU64(value, "numerator", output.m_numerator, path, context) &&
                ReadU64(value, "denominator", output.m_denominator, path, context);
        }

        bool ReadTolerance(const JsonValue& value, NumericToleranceV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "subject_property_path", "absolute", "relative", "angular_radians", "comparison_norm", "unit", "scope" }, context);
            return ReadString(value, "subject_property_path", output.m_subjectPropertyPath, path, context) &&
                ReadNumber(value, "absolute", output.m_absolute, path, context) &&
                ReadNumber(value, "relative", output.m_relative, path, context) &&
                ReadNumber(value, "angular_radians", output.m_angularRadians, path, context) &&
                ReadString(value, "comparison_norm", output.m_comparisonNorm, path, context) &&
                ReadString(value, "unit", output.m_unit, path, context) &&
                ReadString(value, "scope", output.m_scope, path, context);
        }

        bool ReadProducer(const JsonValue& value, ProducerV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "host_kind", "application_id", "application_version", "provider_id", "provider_version", "extension_id", "extension_version", "extension_revision", "extension_digest", "configuration_fingerprint", "workspace_id_observation", "profile_id_observation" }, context);
            return ReadToken(value, "host_kind", output.m_hostKind, path, context) &&
                ReadString(value, "application_id", output.m_applicationId, path, context) &&
                ReadString(value, "application_version", output.m_applicationVersion, path, context) &&
                ReadString(value, "provider_id", output.m_providerId, path, context) &&
                ReadString(value, "provider_version", output.m_providerVersion, path, context) &&
                ReadString(value, "extension_id", output.m_extensionId, path, context) &&
                ReadString(value, "extension_version", output.m_extensionVersion, path, context) &&
                ReadString(value, "extension_revision", output.m_extensionRevision, path, context) &&
                ReadString(value, "extension_digest", output.m_extensionDigest.m_hex, path, context) &&
                ReadString(value, "configuration_fingerprint", output.m_configurationFingerprint.m_hex, path, context) &&
                ReadString(value, "workspace_id_observation", output.m_workspaceIdObservation, path, context) &&
                ReadString(value, "profile_id_observation", output.m_profileIdObservation, path, context);
        }

        bool ReadConsumer(const JsonValue& value, ConsumerConstraintV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "host_kind", "consumer_id", "consumer_version", "profile_id", "required" }, context);
            return ReadToken(value, "host_kind", output.m_hostKind, path, context) &&
                ReadString(value, "consumer_id", output.m_consumerId, path, context) &&
                ReadString(value, "consumer_version", output.m_consumerVersion, path, context) &&
                ReadString(value, "profile_id", output.m_profileId, path, context) &&
                ReadBool(value, "required", output.m_required, path, context);
        }

        bool ReadComponent(const JsonValue& value, ToolchainComponentV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "component_kind", "component_id", "version", "revision", "digest" }, context);
            return ReadString(value, "component_kind", output.m_componentKind, path, context) &&
                ReadString(value, "component_id", output.m_componentId, path, context) &&
                ReadString(value, "version", output.m_version, path, context) &&
                ReadString(value, "revision", output.m_revision, path, context) &&
                ReadString(value, "digest", output.m_digest.m_hex, path, context);
        }

        bool ReadToolchainLock(const JsonValue& value, ToolchainLockV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "foa_sdk_commit", "schema_profile", "configuration_fingerprint", "fixture_revision", "components" }, context);
            return ReadString(value, "foa_sdk_commit", output.m_foaSdkCommit, path, context) &&
                ReadString(value, "schema_profile", output.m_schemaProfile, path, context) &&
                ReadString(value, "configuration_fingerprint", output.m_configurationFingerprint.m_hex, path, context) &&
                ReadString(value, "fixture_revision", output.m_fixtureRevision, path, context) &&
                ReadArray(value, "components", output.m_components, path, context, MaxToolchainComponentsV1, ReadComponent);
        }

        bool ReadDocument(const JsonValue& value, DomainDocumentRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "document_id", "document_kind", "document_schema_id", "document_schema_version", "payload_id", "revision_fingerprint", "subject_refs", "asset_ids", "provenance_ids", "licensing_ids" }, context);
            return ReadString(value, "document_id", output.m_documentId.m_value, path, context) &&
                ReadString(value, "document_kind", output.m_documentKind, path, context) &&
                ReadString(value, "document_schema_id", output.m_documentSchemaId, path, context) &&
                ReadU32(value, "document_schema_version", output.m_documentSchemaVersion, path, context) &&
                ReadString(value, "payload_id", output.m_payloadId.m_value, path, context) &&
                ReadString(value, "revision_fingerprint", output.m_revisionFingerprint.m_sha256, path, context) &&
                ReadArray(value, "subject_refs", output.m_subjectReferences, path, context, MaxReferencesPerRecordV1, ReadStringItem) &&
                ReadArray(value, "asset_ids", output.m_assetIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<AssetIdV1>) &&
                ReadArray(value, "provenance_ids", output.m_provenanceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<ProvenanceIdV1>) &&
                ReadArray(value, "licensing_ids", output.m_licensingIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<LicensingIdV1>);
        }

        bool ReadAsset(const JsonValue& value, AssetRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "asset_id", "asset_kind", "revision_fingerprint", "payload_ids", "document_ids", "subject_refs", "provenance_ids", "licensing_ids" }, context);
            return ReadString(value, "asset_id", output.m_assetId.m_value, path, context) &&
                ReadString(value, "asset_kind", output.m_assetKind, path, context) &&
                ReadString(value, "revision_fingerprint", output.m_revisionFingerprint.m_sha256, path, context) &&
                ReadArray(value, "payload_ids", output.m_payloadIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<PayloadIdV1>) &&
                ReadArray(value, "document_ids", output.m_documentIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<DocumentIdV1>) &&
                ReadArray(value, "subject_refs", output.m_subjectReferences, path, context, MaxReferencesPerRecordV1, ReadStringItem) &&
                ReadArray(value, "provenance_ids", output.m_provenanceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<ProvenanceIdV1>) &&
                ReadArray(value, "licensing_ids", output.m_licensingIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<LicensingIdV1>);
        }

        bool ReadMapping(const JsonValue& value, IdentityMappingRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "mapping_id", "operation", "source_asset_ids", "target_asset_ids", "source_revisions", "target_revisions", "transformation_ids", "loss_ids", "provenance_ids", "evidence_ref_ids", "completeness" }, context);
            return ReadString(value, "mapping_id", output.m_mappingId.m_value, path, context) &&
                ReadToken(value, "operation", output.m_operation, path, context) &&
                ReadArray(value, "source_asset_ids", output.m_sourceAssetIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<AssetIdV1>) &&
                ReadArray(value, "target_asset_ids", output.m_targetAssetIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<AssetIdV1>) &&
                ReadArray(value, "source_revisions", output.m_sourceRevisions, path, context, MaxReferencesPerRecordV1, ReadRevisionItem) &&
                ReadArray(value, "target_revisions", output.m_targetRevisions, path, context, MaxReferencesPerRecordV1, ReadRevisionItem) &&
                ReadArray(value, "transformation_ids", output.m_transformationIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<TransformationIdV1>) &&
                ReadArray(value, "loss_ids", output.m_lossIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<LossIdV1>) &&
                ReadArray(value, "provenance_ids", output.m_provenanceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<ProvenanceIdV1>) &&
                ReadArray(value, "evidence_ref_ids", output.m_evidenceReferenceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<EvidenceReferenceIdV1>) &&
                ReadToken(value, "completeness", output.m_completeness, path, context);
        }

        bool ReadBinding(const JsonValue& value, NativeBindingRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "binding_id", "subject_id", "host_kind", "consumer_profile", "native_id_kind", "native_id_value", "native_path_observation", "state", "supersedes_binding_id", "required" }, context);
            return ReadString(value, "binding_id", output.m_bindingId.m_value, path, context) &&
                ReadString(value, "subject_id", output.m_subjectId, path, context) &&
                ReadToken(value, "host_kind", output.m_hostKind, path, context) &&
                ReadString(value, "consumer_profile", output.m_consumerProfile, path, context) &&
                ReadString(value, "native_id_kind", output.m_nativeIdKind, path, context) &&
                ReadString(value, "native_id_value", output.m_nativeIdValue, path, context) &&
                ReadString(value, "native_path_observation", output.m_nativePathObservation, path, context) &&
                ReadToken(value, "state", output.m_state, path, context) &&
                ReadString(value, "supersedes_binding_id", output.m_supersedesBindingId.m_value, path, context) &&
                ReadBool(value, "required", output.m_required, path, context);
        }

        bool ReadPayload(const JsonValue& value, PayloadRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "payload_id", "relative_path", "role", "media_type", "byte_size", "digest", "dependency_ids", "subject_ids", "provenance_ids", "licensing_ids" }, context);
            return ReadString(value, "payload_id", output.m_payloadId.m_value, path, context) &&
                ReadString(value, "relative_path", output.m_relativePath, path, context, true, MaxRelativePathBytesV1) &&
                ReadToken(value, "role", output.m_role, path, context) &&
                ReadString(value, "media_type", output.m_mediaType, path, context) &&
                ReadU64(value, "byte_size", output.m_byteSize, path, context) &&
                ReadString(value, "digest", output.m_digest.m_hex, path, context) &&
                ReadArray(value, "dependency_ids", output.m_dependencyIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<PayloadIdV1>) &&
                ReadArray(value, "subject_ids", output.m_subjectIds, path, context, MaxReferencesPerRecordV1, ReadStringItem) &&
                ReadArray(value, "provenance_ids", output.m_provenanceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<ProvenanceIdV1>) &&
                ReadArray(value, "licensing_ids", output.m_licensingIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<LicensingIdV1>);
        }

        bool ReadParameter(const JsonValue& value, CanonicalParameterV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "name", "kind", "boolean_value", "signed_value", "unsigned_value", "number_value", "string_value", "matrix_value", "rate_value" }, context);
            const JsonValue* matrix = RequireMember(value, "matrix_value", path, context);
            const JsonValue* rate = RequireMember(value, "rate_value", path, context);
            return ReadString(value, "name", output.m_name, path, context) &&
                ReadToken(value, "kind", output.m_kind, path, context) &&
                ReadBool(value, "boolean_value", output.m_booleanValue, path, context) &&
                ReadS64(value, "signed_value", output.m_signedValue, path, context) &&
                ReadU64(value, "unsigned_value", output.m_unsignedValue, path, context) &&
                ReadNumber(value, "number_value", output.m_numberValue, path, context) &&
                ReadString(value, "string_value", output.m_stringValue, path, context) &&
                matrix != nullptr && ReadMatrix(*matrix, output.m_matrixValue, ChildPath(path, "matrix_value"), context) &&
                rate != nullptr && ReadRate(*rate, output.m_rateValue, ChildPath(path, "rate_value"), context);
        }

        bool ReadTransformation(const JsonValue& value, TransformationRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "transformation_id", "sequence", "phase", "operation_code", "subject_ids", "property_paths", "source_basis_or_profile", "destination_basis_or_profile", "parameters", "reversibility", "tolerances", "evidence_ref_ids" }, context);
            return ReadString(value, "transformation_id", output.m_transformationId.m_value, path, context) &&
                ReadU32(value, "sequence", output.m_sequence, path, context) &&
                ReadToken(value, "phase", output.m_phase, path, context) &&
                ReadString(value, "operation_code", output.m_operationCode, path, context) &&
                ReadArray(value, "subject_ids", output.m_subjectIds, path, context, MaxReferencesPerRecordV1, ReadStringItem) &&
                ReadArray(value, "property_paths", output.m_propertyPaths, path, context, MaxReferencesPerRecordV1, ReadStringItem) &&
                ReadString(value, "source_basis_or_profile", output.m_sourceBasisOrProfile, path, context) &&
                ReadString(value, "destination_basis_or_profile", output.m_destinationBasisOrProfile, path, context) &&
                ReadArray(value, "parameters", output.m_parameters, path, context, MaxReferencesPerRecordV1, ReadParameter) &&
                ReadToken(value, "reversibility", output.m_reversibility, path, context) &&
                ReadArray(value, "tolerances", output.m_tolerances, path, context, MaxReferencesPerRecordV1, ReadTolerance) &&
                ReadArray(value, "evidence_ref_ids", output.m_evidenceReferenceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<EvidenceReferenceIdV1>);
        }

        bool ReadLoss(const JsonValue& value, LossRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "loss_id", "sequence", "subject_id", "property_path", "source_profile", "destination_profile", "phase", "reason_code", "severity", "source_feature", "result_representation", "reversibility", "tolerances", "evidence_ref_ids" }, context);
            return ReadString(value, "loss_id", output.m_lossId.m_value, path, context) &&
                ReadU32(value, "sequence", output.m_sequence, path, context) &&
                ReadString(value, "subject_id", output.m_subjectId, path, context) &&
                ReadString(value, "property_path", output.m_propertyPath, path, context) &&
                ReadString(value, "source_profile", output.m_sourceProfile, path, context) &&
                ReadString(value, "destination_profile", output.m_destinationProfile, path, context) &&
                ReadToken(value, "phase", output.m_phase, path, context) &&
                ReadString(value, "reason_code", output.m_reasonCode, path, context) &&
                ReadToken(value, "severity", output.m_severity, path, context) &&
                ReadString(value, "source_feature", output.m_sourceFeature, path, context) &&
                ReadString(value, "result_representation", output.m_resultRepresentation, path, context) &&
                ReadToken(value, "reversibility", output.m_reversibility, path, context) &&
                ReadArray(value, "tolerances", output.m_tolerances, path, context, MaxReferencesPerRecordV1, ReadTolerance) &&
                ReadArray(value, "evidence_ref_ids", output.m_evidenceReferenceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<EvidenceReferenceIdV1>);
        }

        bool ReadProvenance(const JsonValue& value, ProvenanceRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "provenance_id", "subject_id", "source_kind", "source_identity", "source_revision", "source_digest", "authorship", "modification_state", "attribution", "permitted_use", "evidence_ref_ids" }, context);
            return ReadString(value, "provenance_id", output.m_provenanceId.m_value, path, context) &&
                ReadString(value, "subject_id", output.m_subjectId, path, context) &&
                ReadToken(value, "source_kind", output.m_sourceKind, path, context) &&
                ReadString(value, "source_identity", output.m_sourceIdentity, path, context, true, MaxSourceIdentityBytesV1) &&
                ReadString(value, "source_revision", output.m_sourceRevision, path, context) &&
                ReadString(value, "source_digest", output.m_sourceDigest.m_hex, path, context) &&
                ReadString(value, "authorship", output.m_authorship, path, context) &&
                ReadString(value, "modification_state", output.m_modificationState, path, context) &&
                ReadString(value, "attribution", output.m_attribution, path, context) &&
                ReadString(value, "permitted_use", output.m_permittedUse, path, context) &&
                ReadArray(value, "evidence_ref_ids", output.m_evidenceReferenceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<EvidenceReferenceIdV1>);
        }

        bool ReadLicensing(const JsonValue& value, LicensingRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "licensing_id", "subject_id", "spdx_expression_or_license_ref", "redistribution_state", "required_notices", "reviewer", "decision_at_utc", "evidence_ref_ids" }, context);
            return ReadString(value, "licensing_id", output.m_licensingId.m_value, path, context) &&
                ReadString(value, "subject_id", output.m_subjectId, path, context) &&
                ReadString(value, "spdx_expression_or_license_ref", output.m_spdxExpressionOrLicenseRef, path, context) &&
                ReadToken(value, "redistribution_state", output.m_redistributionState, path, context) &&
                ReadArray(value, "required_notices", output.m_requiredNotices, path, context, MaxReferencesPerRecordV1, ReadStringItem) &&
                ReadString(value, "reviewer", output.m_reviewer, path, context) &&
                ReadString(value, "decision_at_utc", output.m_decisionAtUtc, path, context) &&
                ReadArray(value, "evidence_ref_ids", output.m_evidenceReferenceIds, path, context, MaxReferencesPerRecordV1, ReadValueWrapperItem<EvidenceReferenceIdV1>);
        }

        bool ReadEvidence(const JsonValue& value, EvidenceReferenceV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "evidence_ref_id", "evidence_kind", "subject_id", "external_evidence_id", "external_evidence_fingerprint" }, context);
            return ReadString(value, "evidence_ref_id", output.m_evidenceReferenceId.m_value, path, context) &&
                ReadString(value, "evidence_kind", output.m_evidenceKind, path, context) &&
                ReadString(value, "subject_id", output.m_subjectId, path, context) &&
                ReadString(value, "external_evidence_id", output.m_externalEvidenceId, path, context) &&
                ReadString(value, "external_evidence_fingerprint", output.m_externalEvidenceFingerprint.m_hex, path, context);
        }

        bool ReadExtension(const JsonValue& value, ExtensionRecordV1& output, AZStd::string_view path, ParseContext& context)
        {
            CheckObject(value, path, { "namespace", "schema_version", "canonical_json", "digest" }, context);
            return ReadString(value, "namespace", output.m_namespace, path, context) &&
                ReadU32(value, "schema_version", output.m_schemaVersion, path, context) &&
                ReadString(value, "canonical_json", output.m_canonicalJson, path, context, true, MaxExtensionCanonicalBytesV1) &&
                ReadString(value, "digest", output.m_digest.m_hex, path, context);
        }

        bool ReadManifest(const JsonValue& value, CanonicalInterchangeManifestV1& output, ParseContext& context, bool& displayNamePresent)
        {
            constexpr AZStd::string_view Root = "$";
            CheckObject(value, Root, { "schema_version", "schema_profile", "canonical_profile", "package_kind", "package_id", "pack_id", "display_name", "producer", "intended_consumers", "toolchain_lock", "spatial_basis", "temporal_basis", "documents", "assets", "identity_mappings", "bindings", "payloads", "transformations", "losses", "provenance", "licensing", "evidence_refs", "extensions" }, context);
            displayNamePresent = FindMember(value, "display_name") != nullptr;
            const JsonValue* producer = RequireMember(value, "producer", Root, context);
            const JsonValue* lock = RequireMember(value, "toolchain_lock", Root, context);
            const JsonValue* spatial = RequireMember(value, "spatial_basis", Root, context);
            const JsonValue* temporal = RequireMember(value, "temporal_basis", Root, context);
            bool valid = ReadU32(value, "schema_version", output.m_schemaVersion, Root, context) &&
                ReadString(value, "schema_profile", output.m_schemaProfile, Root, context) &&
                ReadString(value, "canonical_profile", output.m_canonicalProfile, Root, context) &&
                ReadToken(value, "package_kind", output.m_packageKind, Root, context) &&
                ReadString(value, "package_id", output.m_packageId.m_value, Root, context) &&
                ReadString(value, "pack_id", output.m_packId, Root, context) &&
                ReadString(value, "display_name", output.m_displayName, Root, context, false) &&
                producer != nullptr && ReadProducer(*producer, output.m_producer, "$.producer", context) &&
                ReadArray(value, "intended_consumers", output.m_intendedConsumers, Root, context, MaxConsumersV1, ReadConsumer) &&
                lock != nullptr && ReadToolchainLock(*lock, output.m_toolchainLock, "$.toolchain_lock", context) &&
                spatial != nullptr && ReadSpatialBasis(*spatial, output.m_spatialBasis, "$.spatial_basis", context) &&
                temporal != nullptr && ReadTemporalBasis(*temporal, output.m_temporalBasis, "$.temporal_basis", context) &&
                ReadArray(value, "documents", output.m_documents, Root, context, MaxDocumentsV1, ReadDocument) &&
                ReadArray(value, "assets", output.m_assets, Root, context, MaxAssetsV1, ReadAsset) &&
                ReadArray(value, "identity_mappings", output.m_identityMappings, Root, context, MaxIdentityMappingsV1, ReadMapping) &&
                ReadArray(value, "bindings", output.m_bindings, Root, context, MaxBindingsV1, ReadBinding) &&
                ReadArray(value, "payloads", output.m_payloads, Root, context, MaxPayloadsV1, ReadPayload) &&
                ReadArray(value, "transformations", output.m_transformations, Root, context, MaxTransformationsV1, ReadTransformation) &&
                ReadArray(value, "losses", output.m_losses, Root, context, MaxLossesV1, ReadLoss) &&
                ReadArray(value, "provenance", output.m_provenance, Root, context, MaxProvenanceRecordsV1, ReadProvenance) &&
                ReadArray(value, "licensing", output.m_licensing, Root, context, MaxLicensingRecordsV1, ReadLicensing) &&
                ReadArray(value, "evidence_refs", output.m_evidenceReferences, Root, context, MaxEvidenceReferencesV1, ReadEvidence) &&
                ReadArray(value, "extensions", output.m_extensions, Root, context, MaxExtensionNamespacesV1, ReadExtension);
            if (displayNamePresent && output.m_displayName.empty())
            {
                context.Fail(IssueCodeV1::InvalidString, "$.display_name");
                valid = false;
            }
            return valid;
        }

        bool ContainsNegativeZeroNumber(AZStd::string_view bytes)
        {
            bool inString = false;
            bool escaped = false;
            size_t index = 0;
            while (index < bytes.size())
            {
                const char current = bytes[index];
                if (inString)
                {
                    if (escaped) { escaped = false; }
                    else if (current == '\\') { escaped = true; }
                    else if (current == '"') { inString = false; }
                    ++index;
                    continue;
                }
                if (current == '"') { inString = true; ++index; continue; }
                if (current == '-' && index + 1 < bytes.size() && bytes[index + 1] == '0')
                {
                    size_t end = index + 2;
                    while (end < bytes.size())
                    {
                        const char token = bytes[end];
                        if ((token >= '0' && token <= '9') || token == '.' || token == 'e' || token == 'E' || token == '+' || token == '-') { ++end; }
                        else { break; }
                    }
                    AZStd::string number(bytes.substr(index, end - index));
                    char* parsedEnd = nullptr;
                    const double parsed = std::strtod(number.c_str(), &parsedEnd);
                    if (parsedEnd == number.c_str() + number.size() && parsed == 0.0 && std::signbit(parsed)) { return true; }
                    index = end;
                    continue;
                }
                ++index;
            }
            return false;
        }

        template<class T>
        bool ContainsValue(const AZStd::vector<T>& values, const T& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        template<class T, class ValueGetter>
        void ValidateUniqueIds(
            const AZStd::vector<T>& values,
            ValueGetter getter,
            AZStd::string_view propertyPath,
            CanonicalInterchangeValidationResultV1& result)
        {
            AZStd::vector<AZStd::string> seen;
            for (const auto& value : values)
            {
                const AZStd::string& id = getter(value);
                if (!IsSemanticIdV1(id)) { AddIssue(result, IssueCodeV1::IdentityInvalid, id, propertyPath); }
                if (AZStd::find(seen.begin(), seen.end(), id) != seen.end()) { AddIssue(result, IssueCodeV1::IdentityDuplicate, id, propertyPath); }
                else { seen.push_back(id); }
            }
        }

        template<class T, class Less>
        bool IsStrictlySorted(const AZStd::vector<T>& values, Less less)
        {
            for (size_t index = 1; index < values.size(); ++index)
            {
                if (!less(values[index - 1], values[index])) { return false; }
            }
            return true;
        }

        template<class T>
        bool IsStrictlySorted(const AZStd::vector<T>& values)
        {
            return IsStrictlySorted(values, [](const T& left, const T& right) { return left < right; });
        }

        AZStd::string LowerAscii(AZStd::string_view value)
        {
            AZStd::string result(value);
            AZStd::transform(result.begin(), result.end(), result.begin(), [](char c)
            {
                return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
            });
            return result;
        }

        bool IsWindowsReservedStem(AZStd::string_view segment)
        {
            const AZStd::string lower = LowerAscii(segment);
            const size_t dot = lower.find('.');
            const AZStd::string_view stem = dot == AZStd::string::npos ? AZStd::string_view(lower) : AZStd::string_view(lower.data(), dot);
            if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") { return true; }
            return stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9';
        }

        void ValidatePath(const PayloadRecordV1& payload, CanonicalInterchangeValidationResultV1& result)
        {
            const AZStd::string_view path = payload.m_relativePath;
            if (path.empty() || path.front() == '/' || path.find(':') != AZStd::string_view::npos || path.find("//") != AZStd::string_view::npos)
            {
                AddIssue(result, IssueCodeV1::PathAbsolute, payload.m_payloadId.m_value, "payloads.relative_path");
                return;
            }
            if (path.find('\\') != AZStd::string_view::npos)
            {
                AddIssue(result, IssueCodeV1::PathTraversal, payload.m_payloadId.m_value, "payloads.relative_path");
                return;
            }
            size_t start = 0;
            while (start < path.size())
            {
                const size_t slash = path.find('/', start);
                const size_t end = slash == AZStd::string_view::npos ? path.size() : slash;
                const AZStd::string_view segment = path.substr(start, end - start);
                if (segment.empty() || segment == "." || segment == "..")
                {
                    AddIssue(result, IssueCodeV1::PathTraversal, payload.m_payloadId.m_value, "payloads.relative_path");
                    return;
                }
                if (segment.back() == '.' || segment.back() == ' ' || IsWindowsReservedStem(segment))
                {
                    AddIssue(result, IssueCodeV1::PathWindowsAlias, payload.m_payloadId.m_value, "payloads.relative_path");
                    return;
                }
                if (slash == AZStd::string_view::npos) { break; }
                start = slash + 1;
            }
            if (!IsRelativePackagePathV1(path)) { AddIssue(result, IssueCodeV1::InvalidString, payload.m_payloadId.m_value, "payloads.relative_path"); }
        }

        bool IsDefaultMatrix(const Matrix4x4V1& value)
        {
            if (!value.m_mappingDirection.empty()) { return false; }
            for (double number : value.m_columnMajorValues) { if (number != 0.0) { return false; } }
            return true;
        }

        bool IsDefaultRate(const RationalRateV1& value)
        {
            return value.m_numerator == 0 && value.m_denominator == 1;
        }

        void ValidateTolerance(const NumericToleranceV1& tolerance, AZStd::string_view subject, CanonicalInterchangeValidationResultV1& result)
        {
            if (tolerance.m_subjectPropertyPath.empty() || tolerance.m_comparisonNorm.empty() || tolerance.m_unit.empty() || tolerance.m_scope.empty() ||
                !std::isfinite(tolerance.m_absolute) || !std::isfinite(tolerance.m_relative) || !std::isfinite(tolerance.m_angularRadians) ||
                tolerance.m_absolute < 0.0 || tolerance.m_relative < 0.0 || tolerance.m_angularRadians < 0.0)
            {
                AddIssue(result, IssueCodeV1::TransformToleranceInvalid, subject, "tolerances");
            }
        }
    } // namespace

    bool CanonicalInterchangeValidationResultV1::IsValid() const
    {
        return m_issues.empty();
    }

    bool CanonicalInterchangeValidationResultV1::IsBlocked() const
    {
        for (const auto& issue : m_issues)
        {
            if (issue.m_severity == IssueSeverityV1::Blocker) { return true; }
        }
        return false;
    }

    void SortCanonicalInterchangeIssuesV1(CanonicalInterchangeValidationResultV1& result)
    {
        for (auto& issue : result.m_issues)
        {
            AZStd::sort(issue.m_relatedIds.begin(), issue.m_relatedIds.end());
            issue.m_relatedIds.erase(AZStd::unique(issue.m_relatedIds.begin(), issue.m_relatedIds.end()), issue.m_relatedIds.end());
        }
        AZStd::sort(result.m_issues.begin(), result.m_issues.end(), [](const auto& left, const auto& right)
        {
            const int leftPrecedence = IssuePrecedence(left.m_code);
            const int rightPrecedence = IssuePrecedence(right.m_code);
            if (leftPrecedence != rightPrecedence) { return leftPrecedence < rightPrecedence; }
            if (left.m_code != right.m_code) { return left.m_code < right.m_code; }
            if (left.m_subjectId != right.m_subjectId) { return left.m_subjectId < right.m_subjectId; }
            if (left.m_propertyPath != right.m_propertyPath) { return left.m_propertyPath < right.m_propertyPath; }
            return left.m_relatedIds < right.m_relatedIds;
        });
        result.m_issues.erase(AZStd::unique(result.m_issues.begin(), result.m_issues.end()), result.m_issues.end());
    }

    CanonicalInterchangeValidationResultV1 ValidateCanonicalManifestV1(const CanonicalInterchangeManifestV1& manifest)
    {
        CanonicalInterchangeValidationResultV1 result;
        if (manifest.m_schemaVersion != SchemaVersionV1) { AddIssue(result, IssueCodeV1::UnsupportedVersion, manifest.m_packageId.m_value, "schema_version"); }
        if (manifest.m_schemaProfile != SchemaProfileV1 || manifest.m_canonicalProfile != CanonicalProfileV1) { AddIssue(result, IssueCodeV1::UnsupportedProfile, manifest.m_packageId.m_value, "schema_profile"); }
        if (ToToken(manifest.m_packageKind).empty()) { AddIssue(result, IssueCodeV1::InvalidString, manifest.m_packageId.m_value, "package_kind"); }
        if (!IsSemanticIdV1(manifest.m_packageId.m_value)) { AddIssue(result, IssueCodeV1::IdentityInvalid, manifest.m_packageId.m_value, "package_id"); }
        if (!IsSemanticIdV1(manifest.m_packId)) { AddIssue(result, IssueCodeV1::IdentityInvalid, manifest.m_packId, "pack_id"); }
        if (!manifest.m_displayName.empty() && (!IsValidUtf8V1(manifest.m_displayName) || manifest.m_displayName.size() > MaxPresentationStringBytesV1)) { AddIssue(result, IssueCodeV1::InvalidString, manifest.m_packageId.m_value, "display_name"); }

        const auto& basis = manifest.m_spatialBasis;
        if (basis.m_handedness != "right-handed" || basis.m_rightAxis != "+x" || basis.m_forwardAxis != "+y" || basis.m_upAxis != "+z" ||
            basis.m_linearUnit != "metre" || basis.m_angularUnit != "radian" || basis.m_vectorConvention != "column-vector" ||
            basis.m_multiplicationConvention != "matrix-on-left" || basis.m_storageOrder != "column-major" || manifest.m_temporalBasis.m_timeUnit != "second")
        {
            AddIssue(result, IssueCodeV1::TransformMatrixInvalid, manifest.m_packageId.m_value, "spatial_basis");
        }

        if (manifest.m_intendedConsumers.size() > MaxConsumersV1 || manifest.m_documents.size() > MaxDocumentsV1 || manifest.m_assets.size() > MaxAssetsV1 ||
            manifest.m_identityMappings.size() > MaxIdentityMappingsV1 || manifest.m_bindings.size() > MaxBindingsV1 || manifest.m_payloads.size() > MaxPayloadsV1 ||
            manifest.m_transformations.size() > MaxTransformationsV1 || manifest.m_losses.size() > MaxLossesV1 || manifest.m_provenance.size() > MaxProvenanceRecordsV1 ||
            manifest.m_licensing.size() > MaxLicensingRecordsV1 || manifest.m_evidenceReferences.size() > MaxEvidenceReferencesV1 || manifest.m_extensions.size() > MaxExtensionNamespacesV1)
        {
            AddIssue(result, IssueCodeV1::SizeExceeded, manifest.m_packageId.m_value, "manifest");
        }

        ValidateUniqueIds(manifest.m_documents, [](const auto& value) -> const AZStd::string& { return value.m_documentId.m_value; }, "documents.document_id", result);
        ValidateUniqueIds(manifest.m_assets, [](const auto& value) -> const AZStd::string& { return value.m_assetId.m_value; }, "assets.asset_id", result);
        ValidateUniqueIds(manifest.m_identityMappings, [](const auto& value) -> const AZStd::string& { return value.m_mappingId.m_value; }, "identity_mappings.mapping_id", result);
        ValidateUniqueIds(manifest.m_bindings, [](const auto& value) -> const AZStd::string& { return value.m_bindingId.m_value; }, "bindings.binding_id", result);
        ValidateUniqueIds(manifest.m_payloads, [](const auto& value) -> const AZStd::string& { return value.m_payloadId.m_value; }, "payloads.payload_id", result);
        ValidateUniqueIds(manifest.m_transformations, [](const auto& value) -> const AZStd::string& { return value.m_transformationId.m_value; }, "transformations.transformation_id", result);
        ValidateUniqueIds(manifest.m_losses, [](const auto& value) -> const AZStd::string& { return value.m_lossId.m_value; }, "losses.loss_id", result);
        ValidateUniqueIds(manifest.m_provenance, [](const auto& value) -> const AZStd::string& { return value.m_provenanceId.m_value; }, "provenance.provenance_id", result);
        ValidateUniqueIds(manifest.m_licensing, [](const auto& value) -> const AZStd::string& { return value.m_licensingId.m_value; }, "licensing.licensing_id", result);
        ValidateUniqueIds(manifest.m_evidenceReferences, [](const auto& value) -> const AZStd::string& { return value.m_evidenceReferenceId.m_value; }, "evidence_refs.evidence_ref_id", result);

        if (!IsStrictlySorted(manifest.m_documents, [](const auto& left, const auto& right) { return left.m_documentId < right.m_documentId; }) ||
            !IsStrictlySorted(manifest.m_assets, [](const auto& left, const auto& right) { return left.m_assetId < right.m_assetId; }) ||
            !IsStrictlySorted(manifest.m_bindings, [](const auto& left, const auto& right) { return left.m_bindingId < right.m_bindingId; }) ||
            !IsStrictlySorted(manifest.m_payloads, [](const auto& left, const auto& right) { return left.m_payloadId < right.m_payloadId; }) ||
            !IsStrictlySorted(manifest.m_provenance, [](const auto& left, const auto& right) { return left.m_provenanceId < right.m_provenanceId; }) ||
            !IsStrictlySorted(manifest.m_licensing, [](const auto& left, const auto& right) { return left.m_licensingId < right.m_licensingId; }) ||
            !IsStrictlySorted(manifest.m_evidenceReferences, [](const auto& left, const auto& right) { return left.m_evidenceReferenceId < right.m_evidenceReferenceId; }))
        {
            AddIssue(result, IssueCodeV1::OrderInvalid, manifest.m_packageId.m_value, "manifest.collections");
        }

        AZ::u64 declaredBytes = 0;
        AZStd::vector<AZStd::string> pathKeys;
        for (const auto& payload : manifest.m_payloads)
        {
            ValidatePath(payload, result);
            if (payload.m_byteSize > MaxPayloadBytesV1 || declaredBytes > MaxDeclaredPackageBytesV1 - AZStd::min(payload.m_byteSize, MaxDeclaredPackageBytesV1))
            {
                AddIssue(result, IssueCodeV1::PayloadLimitExceeded, payload.m_payloadId.m_value, "payloads.byte_size");
            }
            else { declaredBytes += payload.m_byteSize; }
            if (!IsSha256HexV1(payload.m_digest.m_hex)) { AddIssue(result, IssueCodeV1::IdentityInvalid, payload.m_payloadId.m_value, "payloads.digest"); }
            const AZStd::string key = LowerAscii(payload.m_relativePath);
            if (AZStd::find(pathKeys.begin(), pathKeys.end(), key) != pathKeys.end()) { AddIssue(result, IssueCodeV1::PathCaseCollision, payload.m_payloadId.m_value, "payloads.relative_path"); }
            else { pathKeys.push_back(key); }
            for (const auto& dependency : payload.m_dependencyIds)
            {
                bool found = false;
                for (const auto& candidate : manifest.m_payloads) { if (candidate.m_payloadId == dependency) { found = true; break; } }
                if (!found) { AddIssue(result, IssueCodeV1::DependencyMissing, payload.m_payloadId.m_value, "payloads.dependency_ids", { dependency.m_value }); }
            }
            for (const auto& provenance : payload.m_provenanceIds)
            {
                if (!ContainsValue(manifest.m_provenance, ProvenanceRecordV1{ provenance }))
                {
                    bool found = false; for (const auto& candidate : manifest.m_provenance) { if (candidate.m_provenanceId == provenance) { found = true; break; } }
                    if (!found) { AddIssue(result, IssueCodeV1::ReferenceMissing, payload.m_payloadId.m_value, "payloads.provenance_ids", { provenance.m_value }); }
                }
            }
        }
        if (declaredBytes > MaxDeclaredPackageBytesV1) { AddIssue(result, IssueCodeV1::PayloadLimitExceeded, manifest.m_packageId.m_value, "payloads.total_byte_size"); }

        AZStd::vector<int> payloadState(manifest.m_payloads.size(), 0);
        std::function<void(size_t)> visitPayload = [&](size_t index)
        {
            payloadState[index] = 1;
            for (const auto& dependency : manifest.m_payloads[index].m_dependencyIds)
            {
                size_t target = manifest.m_payloads.size();
                for (size_t candidate = 0; candidate < manifest.m_payloads.size(); ++candidate) { if (manifest.m_payloads[candidate].m_payloadId == dependency) { target = candidate; break; } }
                if (target == manifest.m_payloads.size()) { continue; }
                if (payloadState[target] == 1) { AddIssue(result, IssueCodeV1::DependencyCycle, manifest.m_payloads[index].m_payloadId.m_value, "payloads.dependency_ids", { dependency.m_value }); }
                else if (payloadState[target] == 0) { visitPayload(target); }
            }
            payloadState[index] = 2;
        };
        for (size_t index = 0; index < manifest.m_payloads.size(); ++index) { if (payloadState[index] == 0) { visitPayload(index); } }

        for (const auto& document : manifest.m_documents)
        {
            bool payloadFound = false;
            for (const auto& payload : manifest.m_payloads) { if (payload.m_payloadId == document.m_payloadId) { payloadFound = true; break; } }
            if (!payloadFound) { AddIssue(result, IssueCodeV1::ReferenceMissing, document.m_documentId.m_value, "documents.payload_id", { document.m_payloadId.m_value }); }
            for (const auto& assetId : document.m_assetIds)
            {
                bool found = false; for (const auto& asset : manifest.m_assets) { if (asset.m_assetId == assetId) { found = true; break; } }
                if (!found) { AddIssue(result, IssueCodeV1::ReferenceMissing, document.m_documentId.m_value, "documents.asset_ids", { assetId.m_value }); }
            }
            if (!IsSha256HexV1(document.m_revisionFingerprint.m_sha256)) { AddIssue(result, IssueCodeV1::IdentityInvalid, document.m_documentId.m_value, "documents.revision_fingerprint"); }
            else
            {
                const auto calculated = CalculateDocumentRevisionFingerprintV1(manifest, document);
                if (IsSha256HexV1(calculated.m_sha256) && calculated != document.m_revisionFingerprint) { AddIssue(result, IssueCodeV1::RevisionMismatch, document.m_documentId.m_value, "documents.revision_fingerprint"); }
            }
        }

        for (const auto& asset : manifest.m_assets)
        {
            for (const auto& payloadId : asset.m_payloadIds)
            {
                bool found = false; for (const auto& payload : manifest.m_payloads) { if (payload.m_payloadId == payloadId) { found = true; break; } }
                if (!found) { AddIssue(result, IssueCodeV1::ReferenceMissing, asset.m_assetId.m_value, "assets.payload_ids", { payloadId.m_value }); }
            }
            for (const auto& documentId : asset.m_documentIds)
            {
                bool found = false; for (const auto& document : manifest.m_documents) { if (document.m_documentId == documentId) { found = true; break; } }
                if (!found) { AddIssue(result, IssueCodeV1::ReferenceMissing, asset.m_assetId.m_value, "assets.document_ids", { documentId.m_value }); }
            }
            if (!IsSha256HexV1(asset.m_revisionFingerprint.m_sha256)) { AddIssue(result, IssueCodeV1::IdentityInvalid, asset.m_assetId.m_value, "assets.revision_fingerprint"); }
            else
            {
                const auto calculated = CalculateAssetRevisionFingerprintV1(manifest, asset);
                if (IsSha256HexV1(calculated.m_sha256) && calculated != asset.m_revisionFingerprint) { AddIssue(result, IssueCodeV1::RevisionMismatch, asset.m_assetId.m_value, "assets.revision_fingerprint"); }
            }
        }

        for (const auto& mapping : manifest.m_identityMappings)
        {
            bool cardinalityValid = false;
            switch (mapping.m_operation)
            {
            case MappingOperationV1::Rename:
            case MappingOperationV1::Move:
            case MappingOperationV1::Replace: cardinalityValid = mapping.m_sourceAssetIds.size() == 1 && mapping.m_targetAssetIds.size() == 1; break;
            case MappingOperationV1::Duplicate:
            case MappingOperationV1::Fork: cardinalityValid = mapping.m_sourceAssetIds.size() == 1 && !mapping.m_targetAssetIds.empty(); break;
            case MappingOperationV1::Merge:
            case MappingOperationV1::Aggregate: cardinalityValid = mapping.m_sourceAssetIds.size() >= 2 && mapping.m_targetAssetIds.size() == 1; break;
            case MappingOperationV1::Split: cardinalityValid = mapping.m_sourceAssetIds.size() == 1 && mapping.m_targetAssetIds.size() >= 2; break;
            case MappingOperationV1::Tombstone: cardinalityValid = mapping.m_sourceAssetIds.size() == 1 && mapping.m_targetAssetIds.empty(); break;
            }
            if (!cardinalityValid || mapping.m_completeness != MappingCompletenessV1::Complete ||
                mapping.m_sourceRevisions.size() != mapping.m_sourceAssetIds.size() || mapping.m_targetRevisions.size() != mapping.m_targetAssetIds.size())
            {
                AddIssue(result, IssueCodeV1::MappingInvalid, mapping.m_mappingId.m_value, "identity_mappings");
            }
        }

        for (size_t index = 0; index < manifest.m_bindings.size(); ++index)
        {
            const auto& binding = manifest.m_bindings[index];
            if (binding.m_subjectId.empty() || binding.m_consumerProfile.empty() || binding.m_nativeIdKind.empty() || binding.m_nativeIdValue.empty() || ToToken(binding.m_hostKind).empty() || ToToken(binding.m_state).empty())
            {
                AddIssue(result, IssueCodeV1::BindingInvalid, binding.m_bindingId.m_value, "bindings");
            }
            if (binding.m_required && binding.m_state != BindingStateV1::Active) { AddIssue(result, IssueCodeV1::BindingUnresolvedRequired, binding.m_bindingId.m_value, "bindings.state"); }
            if (!binding.m_supersedesBindingId.m_value.empty())
            {
                bool found = false;
                for (const auto& candidate : manifest.m_bindings) { if (candidate.m_bindingId == binding.m_supersedesBindingId) { found = true; break; } }
                if (!found || binding.m_supersedesBindingId == binding.m_bindingId) { AddIssue(result, IssueCodeV1::BindingSupersessionInvalid, binding.m_bindingId.m_value, "bindings.supersedes_binding_id"); }
            }
            if (binding.m_state == BindingStateV1::Active)
            {
                for (size_t other = index + 1; other < manifest.m_bindings.size(); ++other)
                {
                    const auto& candidate = manifest.m_bindings[other];
                    if (candidate.m_state == BindingStateV1::Active && candidate.m_subjectId == binding.m_subjectId && candidate.m_hostKind == binding.m_hostKind && candidate.m_consumerProfile == binding.m_consumerProfile && candidate.m_nativeIdKind == binding.m_nativeIdKind)
                    {
                        AddIssue(result, IssueCodeV1::BindingConflict, binding.m_bindingId.m_value, "bindings", { candidate.m_bindingId.m_value });
                    }
                }
            }
        }

        for (size_t index = 0; index < manifest.m_transformations.size(); ++index)
        {
            const auto& transformation = manifest.m_transformations[index];
            if (transformation.m_sequence != index) { AddIssue(result, IssueCodeV1::TransformSequenceInvalid, transformation.m_transformationId.m_value, "transformations.sequence"); }
            if (transformation.m_operationCode.empty() || transformation.m_subjectIds.empty()) { AddIssue(result, IssueCodeV1::TransformParameterInvalid, transformation.m_transformationId.m_value, "transformations"); }
            AZStd::vector<AZStd::string> parameterNames;
            for (const auto& parameter : transformation.m_parameters)
            {
                if (parameter.m_name.empty() || AZStd::find(parameterNames.begin(), parameterNames.end(), parameter.m_name) != parameterNames.end()) { AddIssue(result, IssueCodeV1::TransformParameterInvalid, transformation.m_transformationId.m_value, "transformations.parameters"); }
                parameterNames.push_back(parameter.m_name);
                bool inactiveDefaults = true;
                switch (parameter.m_kind)
                {
                case ParameterKindV1::Boolean: inactiveDefaults = parameter.m_signedValue == 0 && parameter.m_unsignedValue == 0 && parameter.m_numberValue == 0.0 && parameter.m_stringValue.empty() && IsDefaultMatrix(parameter.m_matrixValue) && IsDefaultRate(parameter.m_rateValue); break;
                case ParameterKindV1::SignedInteger: inactiveDefaults = !parameter.m_booleanValue && parameter.m_unsignedValue == 0 && parameter.m_numberValue == 0.0 && parameter.m_stringValue.empty() && IsDefaultMatrix(parameter.m_matrixValue) && IsDefaultRate(parameter.m_rateValue); break;
                case ParameterKindV1::UnsignedInteger: inactiveDefaults = !parameter.m_booleanValue && parameter.m_signedValue == 0 && parameter.m_numberValue == 0.0 && parameter.m_stringValue.empty() && IsDefaultMatrix(parameter.m_matrixValue) && IsDefaultRate(parameter.m_rateValue); break;
                case ParameterKindV1::Number: inactiveDefaults = !parameter.m_booleanValue && parameter.m_signedValue == 0 && parameter.m_unsignedValue == 0 && parameter.m_stringValue.empty() && IsDefaultMatrix(parameter.m_matrixValue) && IsDefaultRate(parameter.m_rateValue) && std::isfinite(parameter.m_numberValue); break;
                case ParameterKindV1::String: inactiveDefaults = !parameter.m_booleanValue && parameter.m_signedValue == 0 && parameter.m_unsignedValue == 0 && parameter.m_numberValue == 0.0 && IsDefaultMatrix(parameter.m_matrixValue) && IsDefaultRate(parameter.m_rateValue); break;
                case ParameterKindV1::Matrix4x4:
                    inactiveDefaults = !parameter.m_booleanValue && parameter.m_signedValue == 0 && parameter.m_unsignedValue == 0 && parameter.m_numberValue == 0.0 && parameter.m_stringValue.empty() && IsDefaultRate(parameter.m_rateValue);
                    for (double number : parameter.m_matrixValue.m_columnMajorValues) { if (!std::isfinite(number)) { inactiveDefaults = false; } }
                    if (parameter.m_matrixValue.m_mappingDirection.empty()) { AddIssue(result, IssueCodeV1::TransformMatrixInvalid, transformation.m_transformationId.m_value, "transformations.parameters.matrix_value"); }
                    break;
                case ParameterKindV1::RationalRate: inactiveDefaults = !parameter.m_booleanValue && parameter.m_signedValue == 0 && parameter.m_unsignedValue == 0 && parameter.m_numberValue == 0.0 && parameter.m_stringValue.empty() && IsDefaultMatrix(parameter.m_matrixValue) && parameter.m_rateValue.m_denominator != 0; break;
                }
                if (!inactiveDefaults) { AddIssue(result, IssueCodeV1::TransformParameterInvalid, transformation.m_transformationId.m_value, "transformations.parameters"); }
            }
            for (const auto& tolerance : transformation.m_tolerances) { ValidateTolerance(tolerance, transformation.m_transformationId.m_value, result); }
        }

        for (size_t index = 0; index < manifest.m_losses.size(); ++index)
        {
            const auto& loss = manifest.m_losses[index];
            if (loss.m_sequence != index || loss.m_subjectId.empty() || loss.m_reasonCode.empty() || loss.m_resultRepresentation.empty() || ToToken(loss.m_reversibility).empty())
            {
                AddIssue(result, IssueCodeV1::LossIncomplete, loss.m_lossId.m_value, "losses");
            }
            if (loss.m_severity == LossSeverityV1::Blocker) { AddIssue(result, IssueCodeV1::LossBlocker, loss.m_lossId.m_value, "losses.severity"); }
            for (const auto& tolerance : loss.m_tolerances) { ValidateTolerance(tolerance, loss.m_lossId.m_value, result); }
        }

        for (const auto& provenance : manifest.m_provenance)
        {
            if (provenance.m_subjectId.empty() || provenance.m_sourceIdentity.empty() || provenance.m_sourceRevision.empty() || !IsSha256HexV1(provenance.m_sourceDigest.m_hex) || provenance.m_authorship.empty() || provenance.m_modificationState.empty() || provenance.m_permittedUse.empty())
            {
                AddIssue(result, IssueCodeV1::ProvenanceIncomplete, provenance.m_provenanceId.m_value, "provenance");
            }
            if (provenance.m_sourceKind == SourceKindV1::Prohibited) { AddIssue(result, IssueCodeV1::ContentProhibited, provenance.m_provenanceId.m_value, "provenance.source_kind"); }
        }

        for (const auto& licensing : manifest.m_licensing)
        {
            if (licensing.m_subjectId.empty() || licensing.m_spdxExpressionOrLicenseRef.empty()) { AddIssue(result, IssueCodeV1::LicensingInvalid, licensing.m_licensingId.m_value, "licensing"); }
            if (licensing.m_spdxExpressionOrLicenseRef == "NOASSERTION" || licensing.m_redistributionState == RedistributionStateV1::ReviewRequired || licensing.m_redistributionState == RedistributionStateV1::Blocked)
            {
                AddIssue(result, IssueCodeV1::LicensingNoAssertionBlocked, licensing.m_licensingId.m_value, "licensing.redistribution_state");
            }
        }

        if (manifest.m_toolchainLock.m_foaSdkCommit.empty() || manifest.m_toolchainLock.m_schemaProfile != SchemaProfileV1 || !IsSha256HexV1(manifest.m_toolchainLock.m_configurationFingerprint.m_hex) || manifest.m_toolchainLock.m_fixtureRevision.empty() || manifest.m_toolchainLock.m_components.empty())
        {
            AddIssue(result, IssueCodeV1::LockIncomplete, manifest.m_packageId.m_value, "toolchain_lock");
        }
        for (const auto& component : manifest.m_toolchainLock.m_components)
        {
            if (component.m_componentKind.empty() || component.m_componentId.empty() || component.m_version.empty() || component.m_revision.empty() || !IsSha256HexV1(component.m_digest.m_hex))
            {
                AddIssue(result, IssueCodeV1::LockIncomplete, component.m_componentId, "toolchain_lock.components");
            }
        }

        for (const auto& extension : manifest.m_extensions)
        {
            AddIssue(result, IssueCodeV1::ExtensionUnapproved, extension.m_namespace, "extensions.namespace");
            if (!CanonicalBytesDigestMatchesV1(extension.m_canonicalJson, extension.m_digest)) { AddIssue(result, IssueCodeV1::ExtensionDigestMismatch, extension.m_namespace, "extensions.digest"); }
        }

        if (SerializeDeclaredPackageFingerprintInputV1(manifest).empty()) { AddIssue(result, IssueCodeV1::PackageDeclarationMismatch, manifest.m_packageId.m_value, "package_fingerprint_input"); }
        SortCanonicalInterchangeIssuesV1(result);
        return result;
    }

    CanonicalInterchangeValidationResultV1 ParseCanonicalManifestV1(AZStd::string_view bytes, CanonicalInterchangeManifestV1& manifest)
    {
        ParseContext context;
        if (bytes.size() > MaxCanonicalManifestBytesV1)
        {
            context.Fail(IssueCodeV1::SizeExceeded, "$");
            SortCanonicalInterchangeIssuesV1(context.m_result);
            return context.m_result;
        }
        if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xef && static_cast<unsigned char>(bytes[1]) == 0xbb && static_cast<unsigned char>(bytes[2]) == 0xbf)
        {
            context.Fail(IssueCodeV1::BomForbidden, "$");
            SortCanonicalInterchangeIssuesV1(context.m_result);
            return context.m_result;
        }
        if (!IsValidUtf8V1(bytes))
        {
            context.Fail(IssueCodeV1::InvalidString, "$");
            SortCanonicalInterchangeIssuesV1(context.m_result);
            return context.m_result;
        }

        rapidjson::Document document;
        document.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseFullPrecisionFlag>(bytes.data(), bytes.size());
        if (document.HasParseError() || !document.IsObject())
        {
            context.Fail(IssueCodeV1::InvalidJson, "$");
            SortCanonicalInterchangeIssuesV1(context.m_result);
            return context.m_result;
        }

        InspectJsonTree(document, "$", 1, context);
        CanonicalInterchangeManifestV1 candidate;
        bool displayNamePresent = false;
        ReadManifest(document, candidate, context, displayNamePresent);
        if (!context.m_structurallyValid)
        {
            SortCanonicalInterchangeIssuesV1(context.m_result);
            return context.m_result;
        }

        manifest = candidate;
        auto semantic = ValidateCanonicalManifestV1(candidate);
        context.m_result.m_issues.insert(context.m_result.m_issues.end(), semantic.m_issues.begin(), semantic.m_issues.end());

        if (ContainsNegativeZeroNumber(bytes)) { AddIssue(context.m_result, IssueCodeV1::NegativeZero, candidate.m_packageId.m_value, "$"); }
        AZStd::string canonical = SerializeCanonicalManifestV1(candidate);
        if (!displayNamePresent && candidate.m_displayName.empty())
        {
            const AZStd::string marker = ",\"display_name\":\"\"";
            const size_t markerPosition = canonical.find(marker);
            if (markerPosition != AZStd::string::npos) { canonical.erase(markerPosition, marker.size()); }
        }
        if (canonical.empty()) { AddIssue(context.m_result, IssueCodeV1::InvalidString, candidate.m_packageId.m_value, "$"); }
        else if (canonical != bytes) { AddIssue(context.m_result, IssueCodeV1::OrderInvalid, candidate.m_packageId.m_value, "$"); }

        SortCanonicalInterchangeIssuesV1(context.m_result);
        return context.m_result;
    }
} // namespace TaintedGrailModdingSDK::Interchange
