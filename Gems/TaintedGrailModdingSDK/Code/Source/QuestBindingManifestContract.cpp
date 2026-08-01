/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "QuestBindingManifestContract.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include "QuestDefinitionContract.h"
#include "ResearchContractValidation.h"

#include <AzCore/JSON/document.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr size_t MaxQuestBindingManifestJsonBytes = 1024 * 1024;
        constexpr size_t MaxQuestBindingManifestJsonDepth = 16;
        constexpr size_t MaxQuestBindingManifestIdLength = 160;
        constexpr size_t MaxQuestBindingManifestStringLength = 512;
        constexpr size_t MaxQuestBindingManifestItems = 256;
        constexpr size_t MaxQuestBindingManifestTotalBindings = 512;

        constexpr const char* IssueInvalidJson = "quest.binding.schema.invalid-json";
        constexpr const char* IssueUnsupportedVersion = "quest.binding.schema.unsupported-version";
        constexpr const char* IssueUnknownField = "quest.binding.schema.unknown-field";
        constexpr const char* IssueRequired = "quest.binding.schema.required";
        constexpr const char* IssueInvalidIdentity = "quest.binding.identity.invalid";
        constexpr const char* IssueDuplicateIdentity = "quest.binding.identity.duplicate";
        constexpr const char* IssueMissingReference = "quest.binding.reference.missing";
        constexpr const char* IssueReferenceMismatch = "quest.binding.reference.mismatch";
        constexpr const char* IssueEvidenceInvalid = "quest.binding.evidence.invalid";
        constexpr const char* IssuePermissionBlocked = "quest.binding.permission.blocked";
        constexpr const char* IssuePermissionMismatch = "quest.binding.permission.mismatch";
        constexpr const char* IssueBoundsExceeded = "quest.binding.bounds.exceeded";
        constexpr const char* IssueForbiddenPath = "quest.binding.path.forbidden";
        constexpr const char* IssueNativeReference = "quest.binding.native-ref.forbidden";
        constexpr const char* IssueAuthority = "quest.binding.authority.forbidden";
        constexpr const char* IssueFingerprint = "quest.binding.fingerprint.mismatch";

        struct IdRecord
        {
            AZStd::string m_id;
            AZStd::string m_namespace;
            AZStd::string m_path;
        };

        bool IsIssueLess(
            const QuestBindingManifestIssueV1& left,
            const QuestBindingManifestIssueV1& right)
        {
            if (left.m_code != right.m_code)
            {
                return left.m_code < right.m_code;
            }
            if (left.m_subjectId != right.m_subjectId)
            {
                return left.m_subjectId < right.m_subjectId;
            }
            if (left.m_propertyPath != right.m_propertyPath)
            {
                return left.m_propertyPath < right.m_propertyPath;
            }
            return static_cast<AZ::u8>(left.m_severity)
                < static_cast<AZ::u8>(right.m_severity);
        }

        void SortIssues(QuestBindingManifestValidationResultV1& result)
        {
            AZStd::sort(result.m_issues.begin(), result.m_issues.end(), IsIssueLess);
        }

        void AddIssue(
            QuestBindingManifestValidationResultV1& result,
            QuestBindingManifestIssueSeverityV1 severity,
            const char* code,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath)
        {
            result.m_issues.push_back(
                QuestBindingManifestIssueV1{ severity, code, subjectId, propertyPath });
        }

        bool IsAsciiControl(char value)
        {
            return static_cast<unsigned char>(value) < 0x20;
        }

        AZStd::string ToLowerAscii(const AZStd::string& value)
        {
            AZStd::string lower;
            lower.reserve(value.size());
            for (char character : value)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    lower.push_back(static_cast<char>(character - 'A' + 'a'));
                }
                else
                {
                    lower.push_back(character);
                }
            }
            return lower;
        }

        bool ContainsDriveOrRootedPath(const AZStd::string& value)
        {
            if (value.empty())
            {
                return false;
            }
            if (value[0] == '/' || value[0] == '\\')
            {
                return true;
            }
            if (value.size() >= 2
                && ((value[0] >= 'A' && value[0] <= 'Z')
                    || (value[0] >= 'a' && value[0] <= 'z'))
                && value[1] == ':')
            {
                return true;
            }
            return value.find("://") != AZStd::string::npos
                || value.find("../") != AZStd::string::npos
                || value.find("..\\") != AZStd::string::npos;
        }

        bool IsHex(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'a' && value <= 'f')
                || (value >= 'A' && value <= 'F');
        }

        bool ContainsGuidLikeValue(const AZStd::string& value)
        {
            constexpr size_t GuidLength = 36;
            if (value.size() < GuidLength)
            {
                return false;
            }
            for (size_t offset = 0; offset + GuidLength <= value.size(); ++offset)
            {
                bool match = true;
                for (size_t index = 0; index < GuidLength && match; ++index)
                {
                    const char character = value[offset + index];
                    const bool dash = index == 8 || index == 13
                        || index == 18 || index == 23;
                    match = dash ? character == '-' : IsHex(character);
                }
                if (match)
                {
                    return true;
                }
            }
            return false;
        }

        bool ContainsNativeReference(const AZStd::string& value)
        {
            const AZStd::string lower = ToLowerAscii(value);
            return lower.find("az::entityid") != AZStd::string::npos
                || lower.find("az::data::assetid") != AZStd::string::npos
                || lower.find("unityengine.") != AZStd::string::npos
                || lower.find("gameobject") != AZStd::string::npos
                || lower.find("scriptableobject") != AZStd::string::npos
                || lower.find("instanceid") != AZStd::string::npos
                || lower.find("0x") != AZStd::string::npos
                || ContainsGuidLikeValue(value);
        }

        void ValidatePublicString(
            QuestBindingManifestValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath,
            size_t maximumLength = MaxQuestBindingManifestStringLength)
        {
            if (value.size() > maximumLength)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, subjectId, propertyPath);
            }
            for (char character : value)
            {
                if (IsAsciiControl(character))
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidIdentity, subjectId, propertyPath);
                    break;
                }
            }
            if (ContainsDriveOrRootedPath(value))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssueForbiddenPath, subjectId, propertyPath);
            }
            if (ContainsNativeReference(value))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssueNativeReference, subjectId, propertyPath);
            }
        }

        void ValidateRequiredPublicString(
            QuestBindingManifestValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath)
        {
            if (value.empty())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, subjectId, propertyPath);
            }
            ValidatePublicString(result, value, subjectId, propertyPath);
        }

        void ValidateStableId(
            QuestBindingManifestValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath)
        {
            ValidateRequiredPublicString(result, value, subjectId, propertyPath);
            if (!IsQuestBindingManifestStableIdV1(value))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidIdentity, subjectId, propertyPath);
            }
        }

        void ValidateSemanticVersion(
            QuestBindingManifestValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath)
        {
            ValidateRequiredPublicString(result, value, subjectId, propertyPath);
            if (!IsStrictSemanticVersion(value))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidIdentity, subjectId, propertyPath);
            }
        }

        void ValidateFingerprint(
            QuestBindingManifestValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath)
        {
            ValidateRequiredPublicString(result, value, subjectId, propertyPath);
            if (!IsSha256Fingerprint(value))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueFingerprint, subjectId, propertyPath);
            }
        }

        bool ContainsString(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void AppendIdRecord(
            AZStd::vector<IdRecord>& records,
            const AZStd::string& id,
            const AZStd::string& idNamespace,
            const AZStd::string& path)
        {
            if (!id.empty())
            {
                records.push_back(IdRecord{ id, idNamespace, path });
            }
        }

        void ValidateIdRecords(
            QuestBindingManifestValidationResultV1& result,
            AZStd::vector<IdRecord> records)
        {
            AZStd::sort(
                records.begin(),
                records.end(),
                [](const IdRecord& left, const IdRecord& right)
                {
                    if (left.m_id != right.m_id)
                    {
                        return left.m_id < right.m_id;
                    }
                    return left.m_namespace < right.m_namespace;
                });
            for (size_t index = 1; index < records.size(); ++index)
            {
                if (records[index].m_id == records[index - 1].m_id)
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueDuplicateIdentity, records[index].m_id, records[index].m_path);
                }
            }
        }

        bool IsAllowedFallbackPolicy(const AZStd::string& value)
        {
            return value == "fail_closed"
                || value == "optional_unresolved"
                || value == "review_required";
        }

        bool IsAllowedPermissionDecision(const AZStd::string& value)
        {
            return value == "allowed";
        }

        size_t CountBindings(const QuestBindingManifestV1& manifest)
        {
            return manifest.m_roleBindings.size()
                + manifest.m_locationBindings.size()
                + manifest.m_anchorBindings.size()
                + manifest.m_itemBindings.size()
                + manifest.m_rewardBindings.size()
                + manifest.m_dialogueBindings.size()
                + manifest.m_journalBindings.size();
        }

        const QuestDefinitionRoleV1* FindRole(const QuestDefinitionV1& definition, const AZStd::string& roleId)
        {
            for (const QuestDefinitionRoleV1& role : definition.m_roles)
            {
                if (role.m_roleId == roleId)
                {
                    return &role;
                }
            }
            return nullptr;
        }

        const QuestDefinitionBindingRequirementV1* FindRequirement(
            const QuestDefinitionV1& definition,
            const AZStd::string& requirementId)
        {
            for (const QuestDefinitionBindingRequirementV1& requirement : definition.m_bindingRequirements)
            {
                if (requirement.m_requirementId == requirementId)
                {
                    return &requirement;
                }
            }
            return nullptr;
        }

        void ValidateCatalogReference(
            QuestBindingManifestValidationResultV1& result,
            const QuestBindingManifestV1& manifest,
            const QuestBindingManifestBindingV1& binding,
            const AZStd::string& propertyPath)
        {
            const QuestBindingManifestCatalogReferenceV1& catalog = binding.m_catalogRef;
            ValidateStableId(result, catalog.m_catalogRecordId, binding.m_bindingId, propertyPath + ".catalog_record_id");
            ValidateStableId(result, catalog.m_domain, binding.m_bindingId, propertyPath + ".domain");
            ValidateStableId(result, catalog.m_recordKind, binding.m_bindingId, propertyPath + ".record_kind");
            ValidateStableId(result, catalog.m_subjectRef, binding.m_bindingId, propertyPath + ".subject_ref");
            ValidateFingerprint(result, catalog.m_catalogFingerprint, binding.m_bindingId, propertyPath + ".catalog_fingerprint");
            ValidateStableId(result, catalog.m_profileId, binding.m_bindingId, propertyPath + ".profile_id");
            ValidateRequiredPublicString(result, catalog.m_gameVersion, binding.m_bindingId, propertyPath + ".game_version");
            ValidateRequiredPublicString(result, catalog.m_branch, binding.m_bindingId, propertyPath + ".branch");
            ValidateRequiredPublicString(result, catalog.m_runtimeTarget, binding.m_bindingId, propertyPath + ".runtime_target");

            if (!IsSupportedRuntimeTarget(catalog.m_runtimeTarget))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidIdentity, binding.m_bindingId, propertyPath + ".runtime_target");
            }
            if (catalog.m_catalogFingerprint != manifest.m_catalogFingerprint
                || catalog.m_profileId != manifest.m_profileId
                || catalog.m_gameVersion != manifest.m_gameVersion
                || catalog.m_branch != manifest.m_branch
                || catalog.m_runtimeTarget != manifest.m_runtimeTarget)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueReferenceMismatch, binding.m_bindingId, propertyPath);
            }
        }

        void ValidateEvidenceReferences(
            QuestBindingManifestValidationResultV1& result,
            const QuestBindingManifestV1& manifest,
            const QuestBindingManifestBindingV1& binding,
            const AZStd::string& propertyPath)
        {
            if (binding.m_evidenceRefs.empty())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssueEvidenceInvalid, binding.m_bindingId, propertyPath);
            }
            if (binding.m_evidenceRefs.size() > MaxQuestBindingManifestItems)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, binding.m_bindingId, propertyPath);
            }

            AZStd::vector<IdRecord> ids;
            for (const QuestBindingManifestEvidenceReferenceV1& evidence : binding.m_evidenceRefs)
            {
                ValidateStableId(result, evidence.m_evidenceId, binding.m_bindingId, propertyPath + ".evidence_id");
                ValidateStableId(result, evidence.m_sourceId, binding.m_bindingId, propertyPath + ".source_id");
                ValidateFingerprint(result, evidence.m_sourceFingerprint, binding.m_bindingId, propertyPath + ".source_fingerprint");
                ValidateStableId(result, evidence.m_profileId, binding.m_bindingId, propertyPath + ".profile_id");
                ValidateRequiredPublicString(result, evidence.m_gameVersion, binding.m_bindingId, propertyPath + ".game_version");
                ValidateRequiredPublicString(result, evidence.m_branch, binding.m_bindingId, propertyPath + ".branch");
                if (evidence.m_profileId != manifest.m_profileId
                    || evidence.m_gameVersion != manifest.m_gameVersion
                    || evidence.m_branch != manifest.m_branch)
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueReferenceMismatch, binding.m_bindingId, propertyPath);
                }
                AppendIdRecord(ids, evidence.m_evidenceId, "evidence", propertyPath + ".evidence_id");
            }
            ValidateIdRecords(result, ids);
        }

        void ValidatePermissionReferences(
            QuestBindingManifestValidationResultV1& result,
            const QuestBindingManifestBindingV1& binding,
            const AZStd::string& propertyPath)
        {
            if (binding.m_permissionRefs.empty())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssuePermissionBlocked, binding.m_bindingId, propertyPath);
            }
            if (binding.m_permissionRefs.size() > MaxQuestBindingManifestItems)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, binding.m_bindingId, propertyPath);
            }

            AZStd::vector<IdRecord> ids;
            for (const QuestBindingManifestPermissionReferenceV1& permission : binding.m_permissionRefs)
            {
                ValidateStableId(result, permission.m_permissionId, binding.m_bindingId, propertyPath + ".permission_id");
                ValidateStableId(result, permission.m_subjectKind, binding.m_bindingId, propertyPath + ".subject_kind");
                ValidateStableId(result, permission.m_subjectId, binding.m_bindingId, propertyPath + ".subject_id");
                ValidateStableId(result, permission.m_usage, binding.m_bindingId, propertyPath + ".usage");
                ValidateRequiredPublicString(result, permission.m_decision, binding.m_bindingId, propertyPath + ".decision");
                ValidateStableId(result, permission.m_validationId, binding.m_bindingId, propertyPath + ".validation_id");

                if (!IsAllowedPermissionDecision(permission.m_decision))
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssuePermissionBlocked, binding.m_bindingId, propertyPath + ".decision");
                }
                if (permission.m_subjectKind != binding.m_subjectKind
                    || permission.m_usage != binding.m_usage
                    || (permission.m_subjectId != binding.m_catalogRef.m_subjectRef
                        && permission.m_subjectId != binding.m_catalogRef.m_catalogRecordId))
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssuePermissionMismatch, binding.m_bindingId, propertyPath);
                }
                AppendIdRecord(ids, permission.m_permissionId, "permission", propertyPath + ".permission_id");
            }
            ValidateIdRecords(result, ids);
        }

        void ValidateBindingAgainstDefinition(
            QuestBindingManifestValidationResultV1& result,
            const QuestBindingManifestBindingV1& binding,
            const QuestDefinitionV1* definition,
            const AZStd::string& propertyPath)
        {
            if (definition == nullptr)
            {
                return;
            }
            const QuestDefinitionBindingRequirementV1* requirement = FindRequirement(*definition, binding.m_requirementId);
            if (requirement == nullptr)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueMissingReference, binding.m_bindingId, propertyPath + ".requirement_id");
                return;
            }
            if (requirement->m_roleId != binding.m_roleId
                || requirement->m_subjectKind != binding.m_subjectKind
                || requirement->m_usage != binding.m_usage)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueReferenceMismatch, binding.m_bindingId, propertyPath + ".requirement_id");
            }
            if (FindRole(*definition, binding.m_roleId) == nullptr)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueMissingReference, binding.m_bindingId, propertyPath + ".role_id");
            }
        }

        void ValidateBindingList(
            QuestBindingManifestValidationResultV1& result,
            const QuestBindingManifestV1& manifest,
            const AZStd::vector<QuestBindingManifestBindingV1>& bindings,
            const char* bindingKind,
            const char* propertyPath,
            AZStd::vector<IdRecord>& bindingIds,
            AZStd::vector<IdRecord>& uniqueRoleIds,
            const QuestDefinitionV1* definition)
        {
            if (bindings.size() > MaxQuestBindingManifestItems)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, manifest.m_manifestId, propertyPath);
            }

            for (const QuestBindingManifestBindingV1& binding : bindings)
            {
                ValidateStableId(result, binding.m_bindingId, binding.m_bindingId, AZStd::string(propertyPath) + ".binding_id");
                ValidateRequiredPublicString(result, binding.m_bindingKind, binding.m_bindingId, AZStd::string(propertyPath) + ".binding_kind");
                ValidateStableId(result, binding.m_requirementId, binding.m_bindingId, AZStd::string(propertyPath) + ".requirement_id");
                ValidateStableId(result, binding.m_roleId, binding.m_bindingId, AZStd::string(propertyPath) + ".role_id");
                ValidateStableId(result, binding.m_subjectKind, binding.m_bindingId, AZStd::string(propertyPath) + ".subject_kind");
                ValidateStableId(result, binding.m_usage, binding.m_bindingId, AZStd::string(propertyPath) + ".usage");
                ValidateRequiredPublicString(result, binding.m_fallbackPolicy, binding.m_bindingId, AZStd::string(propertyPath) + ".fallback_policy");

                if (binding.m_bindingKind != bindingKind)
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueReferenceMismatch, binding.m_bindingId, AZStd::string(propertyPath) + ".binding_kind");
                }
                if (!IsAllowedFallbackPolicy(binding.m_fallbackPolicy))
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssueReferenceMismatch, binding.m_bindingId, AZStd::string(propertyPath) + ".fallback_policy");
                }

                ValidateCatalogReference(result, manifest, binding, AZStd::string(propertyPath) + ".catalog_ref");
                ValidateEvidenceReferences(result, manifest, binding, AZStd::string(propertyPath) + ".evidence_refs");
                ValidatePermissionReferences(result, binding, AZStd::string(propertyPath) + ".permission_refs");
                ValidateBindingAgainstDefinition(result, binding, definition, propertyPath);
                AppendIdRecord(bindingIds, binding.m_bindingId, bindingKind, AZStd::string(propertyPath) + ".binding_id");
                if (binding.m_unique)
                {
                    AppendIdRecord(uniqueRoleIds, binding.m_roleId, bindingKind, AZStd::string(propertyPath) + ".role_id");
                }
            }
        }

        bool ExceedsJsonDepth(const rapidjson::Value& value, size_t depth)
        {
            if (depth > MaxQuestBindingManifestJsonDepth)
            {
                return true;
            }
            if (value.IsObject())
            {
                for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                {
                    if (ExceedsJsonDepth(member->value, depth + 1))
                    {
                        return true;
                    }
                }
            }
            else if (value.IsArray())
            {
                for (const rapidjson::Value& element : value.GetArray())
                {
                    if (ExceedsJsonDepth(element, depth + 1))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        AZStd::string JsonName(const rapidjson::Value& name)
        {
            return AZStd::string(name.GetString(), name.GetStringLength());
        }

        template<size_t FieldCount>
        bool IsKnownField(const AZStd::string& value, const char* const (&fields)[FieldCount])
        {
            for (const char* field : fields)
            {
                if (value == field)
                {
                    return true;
                }
            }
            return false;
        }

        const rapidjson::Value* FindMember(const rapidjson::Value& object, const char* name)
        {
            const auto member = object.FindMember(name);
            return member == object.MemberEnd() ? nullptr : &member->value;
        }

        template<size_t FieldCount>
        void ValidateKnownFields(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* const (&fields)[FieldCount],
            const char* path)
        {
            AZStd::vector<AZStd::string> seen;
            for (auto member = object.MemberBegin(); member != object.MemberEnd(); ++member)
            {
                const AZStd::string name = JsonName(member->name);
                if (!IsKnownField(name, fields))
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueUnknownField, name, path);
                }
                if (ContainsString(seen, name))
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueDuplicateIdentity, name, path);
                }
                seen.push_back(name);
            }
        }

        bool ReadRequiredString(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            AZStd::string& output,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr || !value->IsString())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, name, path);
                return false;
            }
            output = AZStd::string(value->GetString(), value->GetStringLength());
            return true;
        }

        bool ReadRequiredUInt(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            AZ::u32& output,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr || !value->IsUint())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, name, path);
                return false;
            }
            output = value->GetUint();
            return true;
        }

        bool ReadRequiredBool(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            bool& output,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr || !value->IsBool())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, name, path);
                return false;
            }
            output = value->GetBool();
            return true;
        }

        const rapidjson::Value* ReadRequiredArray(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr || !value->IsArray())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, name, path);
                return nullptr;
            }
            return value;
        }

        const rapidjson::Value* ReadRequiredObject(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr || !value->IsObject())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, name, path);
                return nullptr;
            }
            return value;
        }

        template<size_t FieldCount>
        const rapidjson::Value* ValidateObjectElement(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& value,
            const char* const (&fields)[FieldCount],
            const char* path)
        {
            if (!value.IsObject())
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueRequired, path, path);
                return nullptr;
            }
            ValidateKnownFields(result, value, fields, path);
            return &value;
        }

        void ReadStringArray(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            AZStd::vector<AZStd::string>& output,
            const char* path)
        {
            const rapidjson::Value* array = ReadRequiredArray(result, object, name, path);
            if (array == nullptr)
            {
                return;
            }
            for (const rapidjson::Value& element : array->GetArray())
            {
                if (!element.IsString())
                {
                    AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidIdentity, name, path);
                    continue;
                }
                output.push_back(AZStd::string(element.GetString(), element.GetStringLength()));
            }
        }

        void ParseCatalogReference(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            QuestBindingManifestCatalogReferenceV1& catalog,
            const char* propertyPath)
        {
            const rapidjson::Value* catalogObject = ReadRequiredObject(result, object, "catalog_ref", propertyPath);
            if (catalogObject == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "catalog_record_id", "domain", "record_kind", "subject_ref",
                "catalog_fingerprint", "profile_id", "game_version", "branch", "runtime_target",
            };
            ValidateKnownFields(result, *catalogObject, Fields, propertyPath);
            ReadRequiredString(result, *catalogObject, "catalog_record_id", catalog.m_catalogRecordId, "catalog_ref.catalog_record_id");
            ReadRequiredString(result, *catalogObject, "domain", catalog.m_domain, "catalog_ref.domain");
            ReadRequiredString(result, *catalogObject, "record_kind", catalog.m_recordKind, "catalog_ref.record_kind");
            ReadRequiredString(result, *catalogObject, "subject_ref", catalog.m_subjectRef, "catalog_ref.subject_ref");
            ReadRequiredString(result, *catalogObject, "catalog_fingerprint", catalog.m_catalogFingerprint, "catalog_ref.catalog_fingerprint");
            ReadRequiredString(result, *catalogObject, "profile_id", catalog.m_profileId, "catalog_ref.profile_id");
            ReadRequiredString(result, *catalogObject, "game_version", catalog.m_gameVersion, "catalog_ref.game_version");
            ReadRequiredString(result, *catalogObject, "branch", catalog.m_branch, "catalog_ref.branch");
            ReadRequiredString(result, *catalogObject, "runtime_target", catalog.m_runtimeTarget, "catalog_ref.runtime_target");
        }

        void ParseEvidenceReferences(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            AZStd::vector<QuestBindingManifestEvidenceReferenceV1>& output,
            const char* propertyPath)
        {
            const rapidjson::Value* array = ReadRequiredArray(result, object, "evidence_refs", propertyPath);
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = { "evidence_id", "source_id", "source_fingerprint", "profile_id", "game_version", "branch" };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestBindingManifestEvidenceReferenceV1 evidence;
                if (ValidateObjectElement(result, element, Fields, propertyPath) == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "evidence_id", evidence.m_evidenceId, "evidence_refs.evidence_id");
                ReadRequiredString(result, element, "source_id", evidence.m_sourceId, "evidence_refs.source_id");
                ReadRequiredString(result, element, "source_fingerprint", evidence.m_sourceFingerprint, "evidence_refs.source_fingerprint");
                ReadRequiredString(result, element, "profile_id", evidence.m_profileId, "evidence_refs.profile_id");
                ReadRequiredString(result, element, "game_version", evidence.m_gameVersion, "evidence_refs.game_version");
                ReadRequiredString(result, element, "branch", evidence.m_branch, "evidence_refs.branch");
                output.push_back(AZStd::move(evidence));
            }
        }

        void ParsePermissionReferences(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& object,
            AZStd::vector<QuestBindingManifestPermissionReferenceV1>& output,
            const char* propertyPath)
        {
            const rapidjson::Value* array = ReadRequiredArray(result, object, "permission_refs", propertyPath);
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = { "permission_id", "subject_kind", "subject_id", "usage", "decision", "validation_id" };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestBindingManifestPermissionReferenceV1 permission;
                if (ValidateObjectElement(result, element, Fields, propertyPath) == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "permission_id", permission.m_permissionId, "permission_refs.permission_id");
                ReadRequiredString(result, element, "subject_kind", permission.m_subjectKind, "permission_refs.subject_kind");
                ReadRequiredString(result, element, "subject_id", permission.m_subjectId, "permission_refs.subject_id");
                ReadRequiredString(result, element, "usage", permission.m_usage, "permission_refs.usage");
                ReadRequiredString(result, element, "decision", permission.m_decision, "permission_refs.decision");
                ReadRequiredString(result, element, "validation_id", permission.m_validationId, "permission_refs.validation_id");
                output.push_back(AZStd::move(permission));
            }
        }

        void ParseBindingList(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& document,
            const char* name,
            AZStd::vector<QuestBindingManifestBindingV1>& output)
        {
            const rapidjson::Value* array = ReadRequiredArray(result, document, name, name);
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "binding_id", "binding_kind", "requirement_id", "role_id", "subject_kind", "usage",
                "catalog_ref", "evidence_refs", "permission_refs", "fallback_policy", "unique",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestBindingManifestBindingV1 binding;
                if (ValidateObjectElement(result, element, Fields, name) == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "binding_id", binding.m_bindingId, "bindings.binding_id");
                ReadRequiredString(result, element, "binding_kind", binding.m_bindingKind, "bindings.binding_kind");
                ReadRequiredString(result, element, "requirement_id", binding.m_requirementId, "bindings.requirement_id");
                ReadRequiredString(result, element, "role_id", binding.m_roleId, "bindings.role_id");
                ReadRequiredString(result, element, "subject_kind", binding.m_subjectKind, "bindings.subject_kind");
                ReadRequiredString(result, element, "usage", binding.m_usage, "bindings.usage");
                ParseCatalogReference(result, element, binding.m_catalogRef, "bindings.catalog_ref");
                ParseEvidenceReferences(result, element, binding.m_evidenceRefs, "bindings.evidence_refs");
                ParsePermissionReferences(result, element, binding.m_permissionRefs, "bindings.permission_refs");
                ReadRequiredString(result, element, "fallback_policy", binding.m_fallbackPolicy, "bindings.fallback_policy");
                ReadRequiredBool(result, element, "unique", binding.m_unique, "bindings.unique");
                output.push_back(AZStd::move(binding));
            }
        }

        void ParseAuthority(
            QuestBindingManifestValidationResultV1& result,
            const rapidjson::Value& document,
            QuestBindingManifestV1& manifest)
        {
            const rapidjson::Value* authority = ReadRequiredObject(result, document, "authority", "authority");
            if (authority == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "runtime_execution_allowed", "editor_mutation_allowed", "save_mutation_allowed", "deployment_allowed",
                "catalog_mutation_allowed", "evidence_promotion_allowed", "permission_grant_allowed", "asset_extraction_allowed",
            };
            ValidateKnownFields(result, *authority, Fields, "authority");
            ReadRequiredBool(result, *authority, "runtime_execution_allowed", manifest.m_authority.m_runtimeExecutionAllowed, "authority.runtime_execution_allowed");
            ReadRequiredBool(result, *authority, "editor_mutation_allowed", manifest.m_authority.m_editorMutationAllowed, "authority.editor_mutation_allowed");
            ReadRequiredBool(result, *authority, "save_mutation_allowed", manifest.m_authority.m_saveMutationAllowed, "authority.save_mutation_allowed");
            ReadRequiredBool(result, *authority, "deployment_allowed", manifest.m_authority.m_deploymentAllowed, "authority.deployment_allowed");
            ReadRequiredBool(result, *authority, "catalog_mutation_allowed", manifest.m_authority.m_catalogMutationAllowed, "authority.catalog_mutation_allowed");
            ReadRequiredBool(result, *authority, "evidence_promotion_allowed", manifest.m_authority.m_evidencePromotionAllowed, "authority.evidence_promotion_allowed");
            ReadRequiredBool(result, *authority, "permission_grant_allowed", manifest.m_authority.m_permissionGrantAllowed, "authority.permission_grant_allowed");
            ReadRequiredBool(result, *authority, "asset_extraction_allowed", manifest.m_authority.m_assetExtractionAllowed, "authority.asset_extraction_allowed");
        }

        template<class ValueType, class KeyGetter, class Appender>
        void AppendSortedObjectArray(
            AZStd::string& output,
            const char* name,
            AZStd::vector<ValueType> values,
            KeyGetter getKey,
            Appender append)
        {
            AZStd::sort(values.begin(), values.end(), [&getKey](const ValueType& left, const ValueType& right)
            {
                return getKey(left) < getKey(right);
            });
            DeterministicContractJson::AppendName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                append(output, values[index]);
            }
            output += "],";
        }

        void AppendCatalogReference(AZStd::string& output, const QuestBindingManifestCatalogReferenceV1& catalog)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "branch", catalog.m_branch);
            DeterministicContractJson::AppendString(output, "catalog_fingerprint", catalog.m_catalogFingerprint);
            DeterministicContractJson::AppendString(output, "catalog_record_id", catalog.m_catalogRecordId);
            DeterministicContractJson::AppendString(output, "domain", catalog.m_domain);
            DeterministicContractJson::AppendString(output, "game_version", catalog.m_gameVersion);
            DeterministicContractJson::AppendString(output, "profile_id", catalog.m_profileId);
            DeterministicContractJson::AppendString(output, "record_kind", catalog.m_recordKind);
            DeterministicContractJson::AppendString(output, "runtime_target", catalog.m_runtimeTarget);
            DeterministicContractJson::AppendString(output, "subject_ref", catalog.m_subjectRef);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendEvidenceReference(AZStd::string& output, const QuestBindingManifestEvidenceReferenceV1& evidence)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "branch", evidence.m_branch);
            DeterministicContractJson::AppendString(output, "evidence_id", evidence.m_evidenceId);
            DeterministicContractJson::AppendString(output, "game_version", evidence.m_gameVersion);
            DeterministicContractJson::AppendString(output, "profile_id", evidence.m_profileId);
            DeterministicContractJson::AppendString(output, "source_fingerprint", evidence.m_sourceFingerprint);
            DeterministicContractJson::AppendString(output, "source_id", evidence.m_sourceId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendPermissionReference(AZStd::string& output, const QuestBindingManifestPermissionReferenceV1& permission)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "decision", permission.m_decision);
            DeterministicContractJson::AppendString(output, "permission_id", permission.m_permissionId);
            DeterministicContractJson::AppendString(output, "subject_id", permission.m_subjectId);
            DeterministicContractJson::AppendString(output, "subject_kind", permission.m_subjectKind);
            DeterministicContractJson::AppendString(output, "usage", permission.m_usage);
            DeterministicContractJson::AppendString(output, "validation_id", permission.m_validationId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendBinding(AZStd::string& output, const QuestBindingManifestBindingV1& binding)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "binding_id", binding.m_bindingId);
            DeterministicContractJson::AppendString(output, "binding_kind", binding.m_bindingKind);
            DeterministicContractJson::AppendName(output, "catalog_ref");
            AppendCatalogReference(output, binding.m_catalogRef);
            output.push_back(',');
            AppendSortedObjectArray(output, "evidence_refs", binding.m_evidenceRefs,
                [](const QuestBindingManifestEvidenceReferenceV1& value) -> const AZStd::string& { return value.m_evidenceId; },
                AppendEvidenceReference);
            DeterministicContractJson::AppendString(output, "fallback_policy", binding.m_fallbackPolicy);
            AppendSortedObjectArray(output, "permission_refs", binding.m_permissionRefs,
                [](const QuestBindingManifestPermissionReferenceV1& value) -> const AZStd::string& { return value.m_permissionId; },
                AppendPermissionReference);
            DeterministicContractJson::AppendString(output, "requirement_id", binding.m_requirementId);
            DeterministicContractJson::AppendString(output, "role_id", binding.m_roleId);
            DeterministicContractJson::AppendString(output, "subject_kind", binding.m_subjectKind);
            DeterministicContractJson::AppendBool(output, "unique", binding.m_unique);
            DeterministicContractJson::AppendString(output, "usage", binding.m_usage);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendBindingArray(AZStd::string& output, const char* name, const AZStd::vector<QuestBindingManifestBindingV1>& bindings)
        {
            AppendSortedObjectArray(output, name, bindings,
                [](const QuestBindingManifestBindingV1& value) -> const AZStd::string& { return value.m_bindingId; },
                AppendBinding);
        }
    } // namespace

    bool QuestBindingManifestValidationResultV1::IsValid() const
    {
        return m_issues.empty();
    }

    bool QuestBindingManifestValidationResultV1::IsBlocked() const
    {
        for (const QuestBindingManifestIssueV1& issue : m_issues)
        {
            if (issue.m_severity == QuestBindingManifestIssueSeverityV1::Blocker)
            {
                return true;
            }
        }
        return false;
    }

    bool IsQuestBindingManifestStableIdV1(const AZStd::string& value)
    {
        return IsStableContractId(value, MaxQuestBindingManifestIdLength);
    }

    QuestBindingManifestValidationResultV1 ValidateQuestBindingManifestV1(
        const QuestBindingManifestV1& manifest,
        const QuestDefinitionV1* definition)
    {
        QuestBindingManifestValidationResultV1 result;

        if (manifest.m_schema != QuestBindingManifestSchemaIdV1
            || manifest.m_schemaVersion != QuestBindingManifestSchemaVersionV1)
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueUnsupportedVersion, manifest.m_manifestId, "schema_version");
        }

        ValidateStableId(result, manifest.m_manifestId, manifest.m_manifestId, "manifest_id");
        ValidateStableId(result, manifest.m_questId, manifest.m_manifestId, "quest_id");
        ValidateStableId(result, manifest.m_contentVersion, manifest.m_manifestId, "content_version");
        ValidateStableId(result, manifest.m_ownerPackId, manifest.m_manifestId, "owner_pack_id");
        ValidateStableId(result, manifest.m_ownerModuleId, manifest.m_manifestId, "owner_module_id");
        ValidateFingerprint(result, manifest.m_questDefinitionFingerprint, manifest.m_manifestId, "quest_definition_fingerprint");
        ValidateStableId(result, manifest.m_catalogId, manifest.m_manifestId, "catalog_id");
        ValidateFingerprint(result, manifest.m_catalogFingerprint, manifest.m_manifestId, "catalog_fingerprint");
        ValidateStableId(result, manifest.m_profileId, manifest.m_manifestId, "profile_id");
        ValidateRequiredPublicString(result, manifest.m_gameVersion, manifest.m_manifestId, "game_version");
        ValidateRequiredPublicString(result, manifest.m_branch, manifest.m_manifestId, "branch");
        ValidateRequiredPublicString(result, manifest.m_runtimeTarget, manifest.m_manifestId, "runtime_target");
        if (!IsSupportedRuntimeTarget(manifest.m_runtimeTarget))
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidIdentity, manifest.m_manifestId, "runtime_target");
        }
        ValidateSemanticVersion(result, manifest.m_minimumSdkVersion, manifest.m_manifestId, "minimum_sdk_version");

        if (manifest.m_compatibilityTags.size() > MaxQuestBindingManifestItems)
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, manifest.m_manifestId, "compatibility_tags");
        }
        AZStd::vector<IdRecord> compatibilityIds;
        for (const AZStd::string& tag : manifest.m_compatibilityTags)
        {
            ValidateStableId(result, tag, manifest.m_manifestId, "compatibility_tags");
            AppendIdRecord(compatibilityIds, tag, "compatibility_tag", "compatibility_tags");
        }
        ValidateIdRecords(result, compatibilityIds);

        const size_t bindingCount = CountBindings(manifest);
        if (bindingCount == 0 || bindingCount > MaxQuestBindingManifestTotalBindings)
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, bindingCount == 0 ? IssueRequired : IssueBoundsExceeded, manifest.m_manifestId, "bindings");
        }

        AZStd::vector<IdRecord> bindingIds;
        AZStd::vector<IdRecord> uniqueRoleIds;
        ValidateBindingList(result, manifest, manifest.m_roleBindings, "role", "role_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateBindingList(result, manifest, manifest.m_locationBindings, "location", "location_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateBindingList(result, manifest, manifest.m_anchorBindings, "anchor", "anchor_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateBindingList(result, manifest, manifest.m_itemBindings, "item", "item_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateBindingList(result, manifest, manifest.m_rewardBindings, "reward", "reward_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateBindingList(result, manifest, manifest.m_dialogueBindings, "dialogue", "dialogue_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateBindingList(result, manifest, manifest.m_journalBindings, "journal", "journal_bindings", bindingIds, uniqueRoleIds, definition);
        ValidateIdRecords(result, bindingIds);
        ValidateIdRecords(result, uniqueRoleIds);

        if (definition != nullptr)
        {
            if (manifest.m_questId != definition->m_questId)
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueReferenceMismatch, manifest.m_manifestId, "quest_id");
            }
            if (manifest.m_questDefinitionFingerprint != CalculateQuestDefinitionFingerprintV1(*definition))
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueFingerprint, manifest.m_manifestId, "quest_definition_fingerprint");
            }
        }

        if (manifest.m_authority.m_runtimeExecutionAllowed
            || manifest.m_authority.m_editorMutationAllowed
            || manifest.m_authority.m_saveMutationAllowed
            || manifest.m_authority.m_deploymentAllowed
            || manifest.m_authority.m_catalogMutationAllowed
            || manifest.m_authority.m_evidencePromotionAllowed
            || manifest.m_authority.m_permissionGrantAllowed
            || manifest.m_authority.m_assetExtractionAllowed)
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Blocker, IssueAuthority, manifest.m_manifestId, "authority");
        }
        if (!manifest.m_manifestFingerprint.empty()
            && !QuestBindingManifestFingerprintMatchesV1(manifest))
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueFingerprint, manifest.m_manifestId, "manifest_fingerprint");
        }

        SortIssues(result);
        return result;
    }

    QuestBindingManifestValidationResultV1 ParseQuestBindingManifestJsonV1(
        AZStd::string_view json,
        QuestBindingManifestV1& manifest,
        const QuestDefinitionV1* definition)
    {
        manifest = QuestBindingManifestV1{};
        QuestBindingManifestValidationResultV1 result;
        if (json.empty() || json.size() > MaxQuestBindingManifestJsonBytes)
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, {}, "quest_binding_manifest");
            SortIssues(result);
            return result;
        }

        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject())
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueInvalidJson, {}, "quest_binding_manifest");
            SortIssues(result);
            return result;
        }
        if (ExceedsJsonDepth(document, 0))
        {
            AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueBoundsExceeded, {}, "quest_binding_manifest");
        }

        constexpr const char* TopLevelFields[] = {
            "schema", "schema_version", "manifest_id", "quest_id", "content_version", "owner_pack_id",
            "owner_module_id", "quest_definition_fingerprint", "catalog_id", "catalog_fingerprint", "profile_id",
            "game_version", "branch", "runtime_target", "role_bindings", "location_bindings", "anchor_bindings",
            "item_bindings", "reward_bindings", "dialogue_bindings", "journal_bindings", "minimum_sdk_version",
            "compatibility_tags", "authority", "manifest_fingerprint",
        };
        ValidateKnownFields(result, document, TopLevelFields, "quest_binding_manifest");

        ReadRequiredString(result, document, "schema", manifest.m_schema, "schema");
        ReadRequiredUInt(result, document, "schema_version", manifest.m_schemaVersion, "schema_version");
        ReadRequiredString(result, document, "manifest_id", manifest.m_manifestId, "manifest_id");
        ReadRequiredString(result, document, "quest_id", manifest.m_questId, "quest_id");
        ReadRequiredString(result, document, "content_version", manifest.m_contentVersion, "content_version");
        ReadRequiredString(result, document, "owner_pack_id", manifest.m_ownerPackId, "owner_pack_id");
        ReadRequiredString(result, document, "owner_module_id", manifest.m_ownerModuleId, "owner_module_id");
        ReadRequiredString(result, document, "quest_definition_fingerprint", manifest.m_questDefinitionFingerprint, "quest_definition_fingerprint");
        ReadRequiredString(result, document, "catalog_id", manifest.m_catalogId, "catalog_id");
        ReadRequiredString(result, document, "catalog_fingerprint", manifest.m_catalogFingerprint, "catalog_fingerprint");
        ReadRequiredString(result, document, "profile_id", manifest.m_profileId, "profile_id");
        ReadRequiredString(result, document, "game_version", manifest.m_gameVersion, "game_version");
        ReadRequiredString(result, document, "branch", manifest.m_branch, "branch");
        ReadRequiredString(result, document, "runtime_target", manifest.m_runtimeTarget, "runtime_target");
        ParseBindingList(result, document, "role_bindings", manifest.m_roleBindings);
        ParseBindingList(result, document, "location_bindings", manifest.m_locationBindings);
        ParseBindingList(result, document, "anchor_bindings", manifest.m_anchorBindings);
        ParseBindingList(result, document, "item_bindings", manifest.m_itemBindings);
        ParseBindingList(result, document, "reward_bindings", manifest.m_rewardBindings);
        ParseBindingList(result, document, "dialogue_bindings", manifest.m_dialogueBindings);
        ParseBindingList(result, document, "journal_bindings", manifest.m_journalBindings);
        ReadRequiredString(result, document, "minimum_sdk_version", manifest.m_minimumSdkVersion, "minimum_sdk_version");
        ReadStringArray(result, document, "compatibility_tags", manifest.m_compatibilityTags, "compatibility_tags");
        ParseAuthority(result, document, manifest);
        if (const rapidjson::Value* fingerprint = FindMember(document, "manifest_fingerprint"); fingerprint != nullptr)
        {
            if (fingerprint->IsString())
            {
                manifest.m_manifestFingerprint = AZStd::string(fingerprint->GetString(), fingerprint->GetStringLength());
            }
            else
            {
                AddIssue(result, QuestBindingManifestIssueSeverityV1::Error, IssueFingerprint, "manifest_fingerprint", "manifest_fingerprint");
            }
        }

        QuestBindingManifestValidationResultV1 semantic = ValidateQuestBindingManifestV1(manifest, definition);
        for (QuestBindingManifestIssueV1& issue : semantic.m_issues)
        {
            result.m_issues.push_back(AZStd::move(issue));
        }
        SortIssues(result);
        return result;
    }

    AZStd::string SerializeCanonicalQuestBindingManifestV1(const QuestBindingManifestV1& manifest)
    {
        AZStd::string output;
        output.reserve(4096);
        output.push_back('{');
        AppendBindingArray(output, "anchor_bindings", manifest.m_anchorBindings);
        DeterministicContractJson::AppendName(output, "authority");
        output.push_back('{');
        DeterministicContractJson::AppendBool(output, "asset_extraction_allowed", manifest.m_authority.m_assetExtractionAllowed);
        DeterministicContractJson::AppendBool(output, "catalog_mutation_allowed", manifest.m_authority.m_catalogMutationAllowed);
        DeterministicContractJson::AppendBool(output, "deployment_allowed", manifest.m_authority.m_deploymentAllowed);
        DeterministicContractJson::AppendBool(output, "editor_mutation_allowed", manifest.m_authority.m_editorMutationAllowed);
        DeterministicContractJson::AppendBool(output, "evidence_promotion_allowed", manifest.m_authority.m_evidencePromotionAllowed);
        DeterministicContractJson::AppendBool(output, "permission_grant_allowed", manifest.m_authority.m_permissionGrantAllowed);
        DeterministicContractJson::AppendBool(output, "runtime_execution_allowed", manifest.m_authority.m_runtimeExecutionAllowed);
        DeterministicContractJson::AppendBool(output, "save_mutation_allowed", manifest.m_authority.m_saveMutationAllowed);
        DeterministicContractJson::TrimTrailingComma(output);
        output += "},";
        DeterministicContractJson::AppendString(output, "branch", manifest.m_branch);
        DeterministicContractJson::AppendString(output, "catalog_fingerprint", manifest.m_catalogFingerprint);
        DeterministicContractJson::AppendString(output, "catalog_id", manifest.m_catalogId);
        DeterministicContractJson::AppendSortedStringArray(output, "compatibility_tags", manifest.m_compatibilityTags);
        DeterministicContractJson::AppendString(output, "content_version", manifest.m_contentVersion);
        AppendBindingArray(output, "dialogue_bindings", manifest.m_dialogueBindings);
        DeterministicContractJson::AppendString(output, "game_version", manifest.m_gameVersion);
        AppendBindingArray(output, "item_bindings", manifest.m_itemBindings);
        AppendBindingArray(output, "journal_bindings", manifest.m_journalBindings);
        AppendBindingArray(output, "location_bindings", manifest.m_locationBindings);
        DeterministicContractJson::AppendString(output, "manifest_id", manifest.m_manifestId);
        DeterministicContractJson::AppendString(output, "minimum_sdk_version", manifest.m_minimumSdkVersion);
        DeterministicContractJson::AppendString(output, "owner_module_id", manifest.m_ownerModuleId);
        DeterministicContractJson::AppendString(output, "owner_pack_id", manifest.m_ownerPackId);
        DeterministicContractJson::AppendString(output, "profile_id", manifest.m_profileId);
        DeterministicContractJson::AppendString(output, "quest_definition_fingerprint", manifest.m_questDefinitionFingerprint);
        DeterministicContractJson::AppendString(output, "quest_id", manifest.m_questId);
        AppendBindingArray(output, "reward_bindings", manifest.m_rewardBindings);
        AppendBindingArray(output, "role_bindings", manifest.m_roleBindings);
        DeterministicContractJson::AppendString(output, "runtime_target", manifest.m_runtimeTarget);
        DeterministicContractJson::AppendString(output, "schema", manifest.m_schema);
        DeterministicContractJson::AppendUnsigned(output, "schema_version", manifest.m_schemaVersion);
        DeterministicContractJson::TrimTrailingComma(output);
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateQuestBindingManifestFingerprintV1(const QuestBindingManifestV1& manifest)
    {
        return CalculateCanonicalSha256(SerializeCanonicalQuestBindingManifestV1(manifest));
    }

    bool QuestBindingManifestFingerprintMatchesV1(const QuestBindingManifestV1& manifest)
    {
        return manifest.m_manifestFingerprint == CalculateQuestBindingManifestFingerprintV1(manifest);
    }
} // namespace TaintedGrailModdingSDK
