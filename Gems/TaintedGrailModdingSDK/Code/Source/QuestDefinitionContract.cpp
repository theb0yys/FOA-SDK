/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "QuestDefinitionContract.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
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
        constexpr size_t MaxQuestDefinitionJsonBytes = 1024 * 1024;
        constexpr size_t MaxQuestDefinitionJsonDepth = 16;
        constexpr size_t MaxQuestDefinitionIdLength = 128;
        constexpr size_t MaxQuestDefinitionStringLength = 512;
        constexpr size_t MaxQuestDefinitionSummaryLength = 2048;
        constexpr size_t MaxQuestDefinitionItems = 256;

        constexpr const char* IssueInvalidJson = "quest.schema.invalid-json";
        constexpr const char* IssueUnsupportedVersion = "quest.schema.unsupported-version";
        constexpr const char* IssueUnknownField = "quest.schema.unknown-field";
        constexpr const char* IssueRequired = "quest.schema.required";
        constexpr const char* IssueInvalidIdentity = "quest.identity.invalid";
        constexpr const char* IssueDuplicateIdentity = "quest.identity.duplicate";
        constexpr const char* IssueDisplayNameAsId = "quest.identity.display-name-as-id";
        constexpr const char* IssueMissingReference = "quest.reference.missing";
        constexpr const char* IssueAmbiguousTransition = "quest.transition.ambiguous";
        constexpr const char* IssueMissingEntry = "quest.lifecycle.missing-entry";
        constexpr const char* IssueMissingTerminal = "quest.lifecycle.missing-terminal";
        constexpr const char* IssueCycleWithoutRepeat = "quest.graph.cycle-without-repeat";
        constexpr const char* IssueUnknownCondition = "quest.registry.unknown-condition";
        constexpr const char* IssueUnknownAction = "quest.registry.unknown-action";
        constexpr const char* IssueBoundsExceeded = "quest.bounds.exceeded";
        constexpr const char* IssueForbiddenPath = "quest.path.forbidden";
        constexpr const char* IssueNativeReference = "quest.native-ref.forbidden";
        constexpr const char* IssueAuthority = "quest.authority.forbidden";
        constexpr const char* IssueFingerprint = "quest.fingerprint.mismatch";

        struct IdRecord
        {
            AZStd::string m_id;
            AZStd::string m_namespace;
            AZStd::string m_path;
        };

        struct EdgeRecord
        {
            AZStd::string m_from;
            AZStd::string m_to;
            bool m_repeatAllowed = false;
            AZStd::string m_transitionId;
        };

        bool IsIssueLess(
            const QuestDefinitionIssueV1& left,
            const QuestDefinitionIssueV1& right)
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

        void SortIssues(QuestDefinitionValidationResultV1& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                IsIssueLess);
        }

        void AddIssue(
            QuestDefinitionValidationResultV1& result,
            QuestDefinitionIssueSeverityV1 severity,
            const char* code,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath)
        {
            result.m_issues.push_back(
                QuestDefinitionIssueV1{ severity, code, subjectId, propertyPath });
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
            QuestDefinitionValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath,
            size_t maximumLength = MaxQuestDefinitionStringLength)
        {
            if (value.size() > maximumLength)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueBoundsExceeded,
                    subjectId,
                    propertyPath);
            }
            for (char character : value)
            {
                if (IsAsciiControl(character))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueInvalidIdentity,
                        subjectId,
                        propertyPath);
                    break;
                }
            }
            if (ContainsDriveOrRootedPath(value))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Blocker,
                    IssueForbiddenPath,
                    subjectId,
                    propertyPath);
            }
            if (ContainsNativeReference(value))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Blocker,
                    IssueNativeReference,
                    subjectId,
                    propertyPath);
            }
        }

        void ValidateStableId(
            QuestDefinitionValidationResultV1& result,
            const AZStd::string& value,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath,
            const AZStd::string& fallbackName)
        {
            ValidatePublicString(result, value, subjectId, propertyPath);
            if (!IsQuestDefinitionStableIdV1(value))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    subjectId,
                    propertyPath);
            }
            if (!fallbackName.empty() && value == fallbackName)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueDisplayNameAsId,
                    subjectId,
                    propertyPath);
            }
        }

        bool ContainsString(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool ExceedsJsonDepth(const rapidjson::Value& value, size_t depth)
        {
            if (depth > MaxQuestDefinitionJsonDepth)
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

        void ValidateStringIdArray(
            QuestDefinitionValidationResultV1& result,
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& subjectId,
            const AZStd::string& propertyPath,
            const AZStd::vector<AZStd::string>& knownIds,
            bool requireKnownReferences)
        {
            if (values.size() > MaxQuestDefinitionItems)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueBoundsExceeded,
                    subjectId,
                    propertyPath);
            }

            AZStd::vector<AZStd::string> sorted = values;
            AZStd::sort(sorted.begin(), sorted.end());
            for (size_t index = 0; index < sorted.size(); ++index)
            {
                ValidateStableId(result, sorted[index], subjectId, propertyPath, {});
                if (index > 0 && sorted[index] == sorted[index - 1])
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueDuplicateIdentity,
                        sorted[index],
                        propertyPath);
                }
                if (requireKnownReferences && !ContainsString(knownIds, sorted[index]))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueMissingReference,
                        sorted[index],
                        propertyPath);
                }
            }
        }

        void AppendIdRecord(
            AZStd::vector<IdRecord>& records,
            const AZStd::string& id,
            const AZStd::string& idNamespace,
            const AZStd::string& path)
        {
            records.push_back(IdRecord{ id, idNamespace, path });
        }

        void ValidateIdRecords(
            QuestDefinitionValidationResultV1& result,
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
                if (records[index].m_id != records[index - 1].m_id)
                {
                    continue;
                }
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueDuplicateIdentity,
                    records[index].m_id,
                    records[index].m_path);
            }
        }

        void AppendRoleIds(
            AZStd::vector<AZStd::string>& values,
            const QuestDefinitionV1& definition)
        {
            for (const QuestDefinitionRoleV1& role : definition.m_roles)
            {
                values.push_back(role.m_roleId);
            }
            AZStd::sort(values.begin(), values.end());
        }

        void AppendPhaseIds(
            AZStd::vector<AZStd::string>& values,
            const QuestDefinitionV1& definition)
        {
            for (const QuestDefinitionPhaseV1& phase : definition.m_phases)
            {
                values.push_back(phase.m_phaseId);
            }
            AZStd::sort(values.begin(), values.end());
        }

        void AppendObjectiveIds(
            AZStd::vector<AZStd::string>& values,
            const QuestDefinitionV1& definition)
        {
            for (const QuestDefinitionObjectiveV1& objective : definition.m_objectives)
            {
                values.push_back(objective.m_objectiveId);
            }
            AZStd::sort(values.begin(), values.end());
        }

        void AppendConditionIds(
            AZStd::vector<AZStd::string>& values,
            const QuestDefinitionV1& definition)
        {
            for (const QuestDefinitionConditionV1& condition : definition.m_conditions)
            {
                values.push_back(condition.m_conditionId);
            }
            AZStd::sort(values.begin(), values.end());
        }

        void AppendActionIds(
            AZStd::vector<AZStd::string>& values,
            const QuestDefinitionV1& definition)
        {
            for (const QuestDefinitionActionV1& action : definition.m_actions)
            {
                values.push_back(action.m_actionId);
            }
            AZStd::sort(values.begin(), values.end());
        }

        bool IsKnownConditionTypeId(const AZStd::string& value)
        {
            return value == "adapter.capability"
                || value == "counter.compare"
                || value == "decision.equals"
                || value == "fact.equals"
                || value == "location.presence"
                || value == "objective.status"
                || value == "phase.reached"
                || value == "quest.status"
                || value == "role.available";
        }

        bool IsKnownActionTypeId(const AZStd::string& value)
        {
            return value == "counter.increment"
                || value == "decision.set"
                || value == "fact.set"
                || value == "journal.update"
                || value == "marker.set"
                || value == "objective.activate"
                || value == "objective.complete"
                || value == "quest.archive"
                || value == "quest.resolve";
        }

        bool IsAllowedLifecycle(const AZStd::string& value)
        {
            return value == "registered"
                || value == "available"
                || value == "offered"
                || value == "accepted"
                || value == "active"
                || value == "suspended"
                || value == "resolved"
                || value == "archived";
        }

        bool HasPhase(
            const AZStd::vector<AZStd::string>& phaseIds,
            const AZStd::string& phaseId)
        {
            return ContainsString(phaseIds, phaseId);
        }

        bool IsTerminalPhase(
            const QuestDefinitionV1& definition,
            const AZStd::string& phaseId)
        {
            for (const QuestDefinitionPhaseV1& phase : definition.m_phases)
            {
                if (phase.m_phaseId == phaseId)
                {
                    return phase.m_terminalPhase;
                }
            }
            return false;
        }

        bool HasPathToPhase(
            const AZStd::vector<EdgeRecord>& edges,
            const AZStd::string& current,
            const AZStd::string& target,
            AZStd::vector<AZStd::string>& visiting)
        {
            if (current == target)
            {
                return true;
            }
            if (ContainsString(visiting, current))
            {
                return false;
            }
            visiting.push_back(current);
            for (const EdgeRecord& edge : edges)
            {
                if (edge.m_from == current
                    && HasPathToPhase(edges, edge.m_to, target, visiting))
                {
                    return true;
                }
            }
            return false;
        }

        void ValidateTransitions(
            QuestDefinitionValidationResultV1& result,
            const QuestDefinitionV1& definition,
            const AZStd::vector<AZStd::string>& phaseIds,
            const AZStd::vector<AZStd::string>& conditionIds,
            const AZStd::vector<AZStd::string>& actionIds)
        {
            AZStd::vector<AZStd::string> transitionKeys;
            AZStd::vector<EdgeRecord> edges;
            for (const QuestDefinitionTransitionV1& transition : definition.m_transitions)
            {
                if (!HasPhase(phaseIds, transition.m_fromPhaseId))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueMissingReference,
                        transition.m_transitionId,
                        "transitions.from_phase_id");
                }
                if (!HasPhase(phaseIds, transition.m_toPhaseId))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueMissingReference,
                        transition.m_transitionId,
                        "transitions.to_phase_id");
                }

                ValidateStringIdArray(
                    result,
                    transition.m_conditionIds,
                    transition.m_transitionId,
                    "transitions.condition_ids",
                    conditionIds,
                    true);
                ValidateStringIdArray(
                    result,
                    transition.m_actionIds,
                    transition.m_transitionId,
                    "transitions.action_ids",
                    actionIds,
                    true);

                AZStd::vector<AZStd::string> sortedConditions = transition.m_conditionIds;
                AZStd::sort(sortedConditions.begin(), sortedConditions.end());
                AZStd::string key = transition.m_fromPhaseId
                    + "|" + transition.m_triggerId
                    + "|" + DeterministicContractJson::UnsignedString(transition.m_priority);
                for (const AZStd::string& conditionId : sortedConditions)
                {
                    key += "|" + conditionId;
                }
                transitionKeys.push_back(key);
                edges.push_back(
                    EdgeRecord{
                        transition.m_fromPhaseId,
                        transition.m_toPhaseId,
                        transition.m_repeatAllowed,
                        transition.m_transitionId });
            }

            AZStd::sort(transitionKeys.begin(), transitionKeys.end());
            for (size_t index = 1; index < transitionKeys.size(); ++index)
            {
                if (transitionKeys[index] == transitionKeys[index - 1])
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueAmbiguousTransition,
                        {},
                        "transitions");
                }
            }

            for (const EdgeRecord& edge : edges)
            {
                if (edge.m_repeatAllowed)
                {
                    continue;
                }
                AZStd::vector<AZStd::string> visiting;
                if (HasPathToPhase(edges, edge.m_to, edge.m_from, visiting))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueCycleWithoutRepeat,
                        edge.m_transitionId,
                        "transitions");
                }
            }
        }

        bool IsKnownField(
            const AZStd::string& name,
            const char* const* knownFields,
            size_t knownFieldCount)
        {
            for (size_t index = 0; index < knownFieldCount; ++index)
            {
                if (name == knownFields[index])
                {
                    return true;
                }
            }
            return false;
        }

        AZStd::string JsonName(const rapidjson::Value& value)
        {
            return AZStd::string(value.GetString(), value.GetStringLength());
        }

        const rapidjson::Value* FindMember(
            const rapidjson::Value& object,
            const char* name)
        {
            const auto member = object.FindMember(name);
            return member == object.MemberEnd() ? nullptr : &member->value;
        }

        template<size_t FieldCount>
        void ValidateKnownFields(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* const (&fields)[FieldCount],
            const char* path)
        {
            AZStd::vector<AZStd::string> seen;
            for (auto member = object.MemberBegin(); member != object.MemberEnd(); ++member)
            {
                const AZStd::string name = JsonName(member->name);
                if (!IsKnownField(name, fields, FieldCount))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueUnknownField,
                        name,
                        path);
                }
                if (ContainsString(seen, name))
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueDuplicateIdentity,
                        name,
                        path);
                }
                seen.push_back(name);
            }
        }

        bool ReadRequiredString(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            AZStd::string& output,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueRequired,
                    name,
                    path);
                return false;
            }
            if (!value->IsString())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    name,
                    path);
                return false;
            }
            output = AZStd::string(value->GetString(), value->GetStringLength());
            return true;
        }

        bool ReadRequiredUInt(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            AZ::u32& output,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueRequired,
                    name,
                    path);
                return false;
            }
            if (!value->IsUint())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    name,
                    path);
                return false;
            }
            output = value->GetUint();
            return true;
        }

        bool ReadBoolMember(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            bool& output,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueRequired,
                    name,
                    path);
                return false;
            }
            if (!value->IsBool())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    name,
                    path);
                return false;
            }
            output = value->GetBool();
            return true;
        }

        bool ReadStringArray(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            AZStd::vector<AZStd::string>& output,
            const char* path)
        {
            const rapidjson::Value* array = FindMember(object, name);
            if (array == nullptr)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueRequired,
                    name,
                    path);
                return false;
            }
            if (!array->IsArray())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    name,
                    path);
                return false;
            }
            for (const rapidjson::Value& value : array->GetArray())
            {
                if (!value.IsString())
                {
                    AddIssue(
                        result,
                        QuestDefinitionIssueSeverityV1::Error,
                        IssueInvalidIdentity,
                        name,
                        path);
                    continue;
                }
                output.push_back(
                    AZStd::string(value.GetString(), value.GetStringLength()));
            }
            return true;
        }

        const rapidjson::Value* ReadRequiredObject(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueRequired,
                    name,
                    path);
                return nullptr;
            }
            if (!value->IsObject())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    name,
                    path);
                return nullptr;
            }
            return value;
        }

        const rapidjson::Value* ReadRequiredArray(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& object,
            const char* name,
            const char* path)
        {
            const rapidjson::Value* value = FindMember(object, name);
            if (value == nullptr)
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueRequired,
                    name,
                    path);
                return nullptr;
            }
            if (!value->IsArray())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    name,
                    path);
                return nullptr;
            }
            return value;
        }

        void ParseDisplay(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* display =
                ReadRequiredObject(result, document, "display", "display");
            if (display == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "name_text_key",
                "summary_text_key",
                "fallback_name",
                "fallback_summary",
            };
            ValidateKnownFields(result, *display, Fields, "display");
            ReadRequiredString(
                result,
                *display,
                "name_text_key",
                definition.m_display.m_nameTextKey,
                "display.name_text_key");
            ReadRequiredString(
                result,
                *display,
                "summary_text_key",
                definition.m_display.m_summaryTextKey,
                "display.summary_text_key");
            ReadRequiredString(
                result,
                *display,
                "fallback_name",
                definition.m_display.m_fallbackName,
                "display.fallback_name");
            ReadRequiredString(
                result,
                *display,
                "fallback_summary",
                definition.m_display.m_fallbackSummary,
                "display.fallback_summary");
        }

        template<size_t FieldCount>
        const rapidjson::Value* ValidateObjectElement(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& element,
            const char* const (&fields)[FieldCount],
            const char* path)
        {
            if (!element.IsObject())
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    {},
                    path);
                return nullptr;
            }
            ValidateKnownFields(result, element, fields, path);
            return &element;
        }

        void ParseRoles(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "roles", "roles");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = { "role_id", "display_text_key", "required" };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionRoleV1 role;
                if (ValidateObjectElement(result, element, Fields, "roles") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "role_id", role.m_roleId, "roles.role_id");
                ReadRequiredString(
                    result,
                    element,
                    "display_text_key",
                    role.m_displayTextKey,
                    "roles.display_text_key");
                ReadBoolMember(result, element, "required", role.m_required, "roles.required");
                definition.m_roles.push_back(AZStd::move(role));
            }
        }

        void ParsePhases(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "phases", "phases");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "phase_id",
                "display_text_key",
                "entry_phase",
                "terminal_phase",
                "entry_action_ids",
                "objective_ids",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionPhaseV1 phase;
                if (ValidateObjectElement(result, element, Fields, "phases") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "phase_id", phase.m_phaseId, "phases.phase_id");
                ReadRequiredString(
                    result,
                    element,
                    "display_text_key",
                    phase.m_displayTextKey,
                    "phases.display_text_key");
                ReadBoolMember(result, element, "entry_phase", phase.m_entryPhase, "phases.entry_phase");
                ReadBoolMember(result, element, "terminal_phase", phase.m_terminalPhase, "phases.terminal_phase");
                ReadStringArray(
                    result,
                    element,
                    "entry_action_ids",
                    phase.m_entryActionIds,
                    "phases.entry_action_ids");
                ReadStringArray(
                    result,
                    element,
                    "objective_ids",
                    phase.m_objectiveIds,
                    "phases.objective_ids");
                definition.m_phases.push_back(AZStd::move(phase));
            }
        }

        void ParseObjectives(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "objectives", "objectives");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "objective_id",
                "phase_id",
                "display_text_key",
                "condition_ids",
                "completion_action_ids",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionObjectiveV1 objective;
                if (ValidateObjectElement(result, element, Fields, "objectives") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(
                    result,
                    element,
                    "objective_id",
                    objective.m_objectiveId,
                    "objectives.objective_id");
                ReadRequiredString(result, element, "phase_id", objective.m_phaseId, "objectives.phase_id");
                ReadRequiredString(
                    result,
                    element,
                    "display_text_key",
                    objective.m_displayTextKey,
                    "objectives.display_text_key");
                ReadStringArray(
                    result,
                    element,
                    "condition_ids",
                    objective.m_conditionIds,
                    "objectives.condition_ids");
                ReadStringArray(
                    result,
                    element,
                    "completion_action_ids",
                    objective.m_completionActionIds,
                    "objectives.completion_action_ids");
                definition.m_objectives.push_back(AZStd::move(objective));
            }
        }

        void ParseTransitions(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "transitions", "transitions");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "transition_id",
                "from_phase_id",
                "to_phase_id",
                "trigger_id",
                "priority",
                "condition_ids",
                "action_ids",
                "repeat_allowed",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionTransitionV1 transition;
                if (ValidateObjectElement(result, element, Fields, "transitions") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(
                    result,
                    element,
                    "transition_id",
                    transition.m_transitionId,
                    "transitions.transition_id");
                ReadRequiredString(
                    result,
                    element,
                    "from_phase_id",
                    transition.m_fromPhaseId,
                    "transitions.from_phase_id");
                ReadRequiredString(
                    result,
                    element,
                    "to_phase_id",
                    transition.m_toPhaseId,
                    "transitions.to_phase_id");
                ReadRequiredString(
                    result,
                    element,
                    "trigger_id",
                    transition.m_triggerId,
                    "transitions.trigger_id");
                ReadRequiredUInt(result, element, "priority", transition.m_priority, "transitions.priority");
                ReadStringArray(
                    result,
                    element,
                    "condition_ids",
                    transition.m_conditionIds,
                    "transitions.condition_ids");
                ReadStringArray(
                    result,
                    element,
                    "action_ids",
                    transition.m_actionIds,
                    "transitions.action_ids");
                ReadBoolMember(
                    result,
                    element,
                    "repeat_allowed",
                    transition.m_repeatAllowed,
                    "transitions.repeat_allowed");
                definition.m_transitions.push_back(AZStd::move(transition));
            }
        }

        void ParseConditions(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "conditions", "conditions");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "condition_id",
                "condition_type_id",
                "subject_id",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionConditionV1 condition;
                if (ValidateObjectElement(result, element, Fields, "conditions") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(
                    result,
                    element,
                    "condition_id",
                    condition.m_conditionId,
                    "conditions.condition_id");
                ReadRequiredString(
                    result,
                    element,
                    "condition_type_id",
                    condition.m_conditionTypeId,
                    "conditions.condition_type_id");
                ReadRequiredString(
                    result,
                    element,
                    "subject_id",
                    condition.m_subjectId,
                    "conditions.subject_id");
                definition.m_conditions.push_back(AZStd::move(condition));
            }
        }

        void ParseActions(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "actions", "actions");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "action_id",
                "action_type_id",
                "subject_id",
                "idempotency_key",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionActionV1 action;
                if (ValidateObjectElement(result, element, Fields, "actions") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "action_id", action.m_actionId, "actions.action_id");
                ReadRequiredString(
                    result,
                    element,
                    "action_type_id",
                    action.m_actionTypeId,
                    "actions.action_type_id");
                ReadRequiredString(result, element, "subject_id", action.m_subjectId, "actions.subject_id");
                ReadRequiredString(
                    result,
                    element,
                    "idempotency_key",
                    action.m_idempotencyKey,
                    "actions.idempotency_key");
                definition.m_actions.push_back(AZStd::move(action));
            }
        }

        void ParseOutcomes(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "outcomes", "outcomes");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = { "outcome_id", "phase_id", "text_key" };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionOutcomeV1 outcome;
                if (ValidateObjectElement(result, element, Fields, "outcomes") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(result, element, "outcome_id", outcome.m_outcomeId, "outcomes.outcome_id");
                ReadRequiredString(result, element, "phase_id", outcome.m_phaseId, "outcomes.phase_id");
                ReadRequiredString(result, element, "text_key", outcome.m_textKey, "outcomes.text_key");
                definition.m_outcomes.push_back(AZStd::move(outcome));
            }
        }

        void ParseBindingRequirements(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* array =
                ReadRequiredArray(result, document, "binding_requirements", "binding_requirements");
            if (array == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "requirement_id",
                "role_id",
                "subject_kind",
                "usage",
            };
            for (const rapidjson::Value& element : array->GetArray())
            {
                QuestDefinitionBindingRequirementV1 requirement;
                if (ValidateObjectElement(result, element, Fields, "binding_requirements") == nullptr)
                {
                    continue;
                }
                ReadRequiredString(
                    result,
                    element,
                    "requirement_id",
                    requirement.m_requirementId,
                    "binding_requirements.requirement_id");
                ReadRequiredString(
                    result,
                    element,
                    "role_id",
                    requirement.m_roleId,
                    "binding_requirements.role_id");
                ReadRequiredString(
                    result,
                    element,
                    "subject_kind",
                    requirement.m_subjectKind,
                    "binding_requirements.subject_kind");
                ReadRequiredString(
                    result,
                    element,
                    "usage",
                    requirement.m_usage,
                    "binding_requirements.usage");
                definition.m_bindingRequirements.push_back(AZStd::move(requirement));
            }
        }

        void ParseAuthority(
            QuestDefinitionValidationResultV1& result,
            const rapidjson::Value& document,
            QuestDefinitionV1& definition)
        {
            const rapidjson::Value* authority =
                ReadRequiredObject(result, document, "authority", "authority");
            if (authority == nullptr)
            {
                return;
            }
            constexpr const char* Fields[] = {
                "runtime_execution_allowed",
                "editor_mutation_allowed",
                "save_mutation_allowed",
                "deployment_allowed",
                "asset_extraction_allowed",
            };
            ValidateKnownFields(result, *authority, Fields, "authority");
            ReadBoolMember(
                result,
                *authority,
                "runtime_execution_allowed",
                definition.m_authority.m_runtimeExecutionAllowed,
                "authority.runtime_execution_allowed");
            ReadBoolMember(
                result,
                *authority,
                "editor_mutation_allowed",
                definition.m_authority.m_editorMutationAllowed,
                "authority.editor_mutation_allowed");
            ReadBoolMember(
                result,
                *authority,
                "save_mutation_allowed",
                definition.m_authority.m_saveMutationAllowed,
                "authority.save_mutation_allowed");
            ReadBoolMember(
                result,
                *authority,
                "deployment_allowed",
                definition.m_authority.m_deploymentAllowed,
                "authority.deployment_allowed");
            ReadBoolMember(
                result,
                *authority,
                "asset_extraction_allowed",
                definition.m_authority.m_assetExtractionAllowed,
                "authority.asset_extraction_allowed");
        }

        template<class T, class Getter>
        void AppendSortedObjectArray(
            AZStd::string& output,
            const char* name,
            AZStd::vector<T> values,
            Getter getId,
            void (*appendObject)(AZStd::string&, const T&),
            bool comma = true)
        {
            AZStd::sort(
                values.begin(),
                values.end(),
                [getId](const T& left, const T& right)
                {
                    return getId(left) < getId(right);
                });
            DeterministicContractJson::AppendName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                appendObject(output, values[index]);
            }
            output.push_back(']');
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendRole(AZStd::string& output, const QuestDefinitionRoleV1& role)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "display_text_key", role.m_displayTextKey);
            DeterministicContractJson::AppendBool(output, "required", role.m_required);
            DeterministicContractJson::AppendString(output, "role_id", role.m_roleId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendPhase(AZStd::string& output, const QuestDefinitionPhaseV1& phase)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "display_text_key", phase.m_displayTextKey);
            DeterministicContractJson::AppendSortedStringArray(output, "entry_action_ids", phase.m_entryActionIds);
            DeterministicContractJson::AppendBool(output, "entry_phase", phase.m_entryPhase);
            DeterministicContractJson::AppendSortedStringArray(output, "objective_ids", phase.m_objectiveIds);
            DeterministicContractJson::AppendString(output, "phase_id", phase.m_phaseId);
            DeterministicContractJson::AppendBool(output, "terminal_phase", phase.m_terminalPhase);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendObjective(AZStd::string& output, const QuestDefinitionObjectiveV1& objective)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "display_text_key", objective.m_displayTextKey);
            DeterministicContractJson::AppendSortedStringArray(output, "completion_action_ids", objective.m_completionActionIds);
            DeterministicContractJson::AppendSortedStringArray(output, "condition_ids", objective.m_conditionIds);
            DeterministicContractJson::AppendString(output, "objective_id", objective.m_objectiveId);
            DeterministicContractJson::AppendString(output, "phase_id", objective.m_phaseId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendTransition(AZStd::string& output, const QuestDefinitionTransitionV1& transition)
        {
            output.push_back('{');
            DeterministicContractJson::AppendSortedStringArray(output, "action_ids", transition.m_actionIds);
            DeterministicContractJson::AppendSortedStringArray(output, "condition_ids", transition.m_conditionIds);
            DeterministicContractJson::AppendString(output, "from_phase_id", transition.m_fromPhaseId);
            DeterministicContractJson::AppendUnsigned(output, "priority", transition.m_priority);
            DeterministicContractJson::AppendBool(output, "repeat_allowed", transition.m_repeatAllowed);
            DeterministicContractJson::AppendString(output, "to_phase_id", transition.m_toPhaseId);
            DeterministicContractJson::AppendString(output, "transition_id", transition.m_transitionId);
            DeterministicContractJson::AppendString(output, "trigger_id", transition.m_triggerId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendCondition(AZStd::string& output, const QuestDefinitionConditionV1& condition)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "condition_id", condition.m_conditionId);
            DeterministicContractJson::AppendString(output, "condition_type_id", condition.m_conditionTypeId);
            DeterministicContractJson::AppendString(output, "subject_id", condition.m_subjectId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendAction(AZStd::string& output, const QuestDefinitionActionV1& action)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "action_id", action.m_actionId);
            DeterministicContractJson::AppendString(output, "action_type_id", action.m_actionTypeId);
            DeterministicContractJson::AppendString(output, "idempotency_key", action.m_idempotencyKey);
            DeterministicContractJson::AppendString(output, "subject_id", action.m_subjectId);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendOutcome(AZStd::string& output, const QuestDefinitionOutcomeV1& outcome)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "outcome_id", outcome.m_outcomeId);
            DeterministicContractJson::AppendString(output, "phase_id", outcome.m_phaseId);
            DeterministicContractJson::AppendString(output, "text_key", outcome.m_textKey);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }

        void AppendBindingRequirement(
            AZStd::string& output,
            const QuestDefinitionBindingRequirementV1& requirement)
        {
            output.push_back('{');
            DeterministicContractJson::AppendString(output, "requirement_id", requirement.m_requirementId);
            DeterministicContractJson::AppendString(output, "role_id", requirement.m_roleId);
            DeterministicContractJson::AppendString(output, "subject_kind", requirement.m_subjectKind);
            DeterministicContractJson::AppendString(output, "usage", requirement.m_usage);
            DeterministicContractJson::TrimTrailingComma(output);
            output.push_back('}');
        }
    } // namespace

    bool QuestDefinitionValidationResultV1::IsValid() const
    {
        return m_issues.empty();
    }

    bool QuestDefinitionValidationResultV1::IsBlocked() const
    {
        for (const QuestDefinitionIssueV1& issue : m_issues)
        {
            if (issue.m_severity == QuestDefinitionIssueSeverityV1::Blocker)
            {
                return true;
            }
        }
        return false;
    }

    bool IsQuestDefinitionStableIdV1(const AZStd::string& value)
    {
        return IsStableContractId(value, MaxQuestDefinitionIdLength)
            && value.find('/') == AZStd::string::npos
            && value.find('\\') == AZStd::string::npos
            && value.find("..") == AZStd::string::npos
            && !ContainsDriveOrRootedPath(value)
            && !ContainsNativeReference(value);
    }

    QuestDefinitionValidationResultV1 ValidateQuestDefinitionV1(
        const QuestDefinitionV1& definition)
    {
        QuestDefinitionValidationResultV1 result;
        ValidateStableId(
            result,
            definition.m_questId,
            definition.m_questId,
            "quest_id",
            definition.m_display.m_fallbackName);
        ValidateStableId(
            result,
            definition.m_ownerPackId,
            definition.m_questId,
            "owner_pack_id",
            definition.m_display.m_fallbackName);
        ValidateStableId(
            result,
            definition.m_ownerModuleId,
            definition.m_questId,
            "owner_module_id",
            definition.m_display.m_fallbackName);
        ValidateStableId(
            result,
            definition.m_display.m_nameTextKey,
            definition.m_questId,
            "display.name_text_key",
            definition.m_display.m_fallbackName);
        ValidateStableId(
            result,
            definition.m_display.m_summaryTextKey,
            definition.m_questId,
            "display.summary_text_key",
            definition.m_display.m_fallbackName);
        ValidatePublicString(
            result,
            definition.m_display.m_fallbackName,
            definition.m_questId,
            "display.fallback_name");
        ValidatePublicString(
            result,
            definition.m_display.m_fallbackSummary,
            definition.m_questId,
            "display.fallback_summary",
            MaxQuestDefinitionSummaryLength);
        ValidatePublicString(result, definition.m_lifecycle, definition.m_questId, "lifecycle");

        if (definition.m_schema != QuestDefinitionSchemaIdV1
            || definition.m_schemaVersion != QuestDefinitionSchemaVersionV1)
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueUnsupportedVersion,
                definition.m_questId,
                "schema_version");
        }
        if (!IsStrictSemanticVersion(definition.m_contentVersion))
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueInvalidIdentity,
                definition.m_questId,
                "content_version");
        }
        if (!IsStrictSemanticVersion(definition.m_minimumSdkVersion))
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueInvalidIdentity,
                definition.m_questId,
                "minimum_sdk_version");
        }
        if (!IsAllowedLifecycle(definition.m_lifecycle))
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueInvalidIdentity,
                definition.m_questId,
                "lifecycle");
        }

        if (definition.m_roles.empty() || definition.m_roles.size() > MaxQuestDefinitionItems
            || definition.m_phases.empty() || definition.m_phases.size() > MaxQuestDefinitionItems
            || definition.m_objectives.empty() || definition.m_objectives.size() > MaxQuestDefinitionItems
            || definition.m_transitions.size() > MaxQuestDefinitionItems
            || definition.m_conditions.size() > MaxQuestDefinitionItems
            || definition.m_actions.size() > MaxQuestDefinitionItems
            || definition.m_outcomes.empty() || definition.m_outcomes.size() > MaxQuestDefinitionItems
            || definition.m_compatibilityTags.size() > MaxQuestDefinitionItems
            || definition.m_bindingRequirements.empty()
            || definition.m_bindingRequirements.size() > MaxQuestDefinitionItems)
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueBoundsExceeded,
                definition.m_questId,
                "quest_definition");
        }

        AZStd::vector<IdRecord> records;
        AZStd::vector<AZStd::string> roleIds;
        AZStd::vector<AZStd::string> phaseIds;
        AZStd::vector<AZStd::string> objectiveIds;
        AZStd::vector<AZStd::string> conditionIds;
        AZStd::vector<AZStd::string> actionIds;
        AppendRoleIds(roleIds, definition);
        AppendPhaseIds(phaseIds, definition);
        AppendObjectiveIds(objectiveIds, definition);
        AppendConditionIds(conditionIds, definition);
        AppendActionIds(actionIds, definition);

        AppendIdRecord(records, definition.m_questId, "quest", "quest_id");
        for (const QuestDefinitionRoleV1& role : definition.m_roles)
        {
            ValidateStableId(result, role.m_roleId, role.m_roleId, "roles.role_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, role.m_displayTextKey, role.m_roleId, "roles.display_text_key", definition.m_display.m_fallbackName);
            AppendIdRecord(records, role.m_roleId, "role", "roles.role_id");
        }
        for (const QuestDefinitionPhaseV1& phase : definition.m_phases)
        {
            ValidateStableId(result, phase.m_phaseId, phase.m_phaseId, "phases.phase_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, phase.m_displayTextKey, phase.m_phaseId, "phases.display_text_key", definition.m_display.m_fallbackName);
            ValidateStringIdArray(
                result,
                phase.m_entryActionIds,
                phase.m_phaseId,
                "phases.entry_action_ids",
                actionIds,
                true);
            ValidateStringIdArray(
                result,
                phase.m_objectiveIds,
                phase.m_phaseId,
                "phases.objective_ids",
                objectiveIds,
                true);
            AppendIdRecord(records, phase.m_phaseId, "phase", "phases.phase_id");
        }
        for (const QuestDefinitionObjectiveV1& objective : definition.m_objectives)
        {
            ValidateStableId(result, objective.m_objectiveId, objective.m_objectiveId, "objectives.objective_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, objective.m_displayTextKey, objective.m_objectiveId, "objectives.display_text_key", definition.m_display.m_fallbackName);
            if (!HasPhase(phaseIds, objective.m_phaseId))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueMissingReference,
                    objective.m_objectiveId,
                    "objectives.phase_id");
            }
            ValidateStringIdArray(
                result,
                objective.m_conditionIds,
                objective.m_objectiveId,
                "objectives.condition_ids",
                conditionIds,
                true);
            ValidateStringIdArray(
                result,
                objective.m_completionActionIds,
                objective.m_objectiveId,
                "objectives.completion_action_ids",
                actionIds,
                true);
            AppendIdRecord(records, objective.m_objectiveId, "objective", "objectives.objective_id");
        }
        for (const QuestDefinitionTransitionV1& transition : definition.m_transitions)
        {
            ValidateStableId(result, transition.m_transitionId, transition.m_transitionId, "transitions.transition_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, transition.m_triggerId, transition.m_transitionId, "transitions.trigger_id", definition.m_display.m_fallbackName);
            AppendIdRecord(records, transition.m_transitionId, "transition", "transitions.transition_id");
        }
        for (const QuestDefinitionConditionV1& condition : definition.m_conditions)
        {
            ValidateStableId(result, condition.m_conditionId, condition.m_conditionId, "conditions.condition_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, condition.m_conditionTypeId, condition.m_conditionId, "conditions.condition_type_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, condition.m_subjectId, condition.m_conditionId, "conditions.subject_id", definition.m_display.m_fallbackName);
            if (!IsKnownConditionTypeId(condition.m_conditionTypeId))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueUnknownCondition,
                    condition.m_conditionId,
                    "conditions.condition_type_id");
            }
            AppendIdRecord(records, condition.m_conditionId, "condition", "conditions.condition_id");
        }
        for (const QuestDefinitionActionV1& action : definition.m_actions)
        {
            ValidateStableId(result, action.m_actionId, action.m_actionId, "actions.action_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, action.m_actionTypeId, action.m_actionId, "actions.action_type_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, action.m_subjectId, action.m_actionId, "actions.subject_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, action.m_idempotencyKey, action.m_actionId, "actions.idempotency_key", definition.m_display.m_fallbackName);
            if (!IsKnownActionTypeId(action.m_actionTypeId))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueUnknownAction,
                    action.m_actionId,
                    "actions.action_type_id");
            }
            AppendIdRecord(records, action.m_actionId, "action", "actions.action_id");
        }
        for (const QuestDefinitionOutcomeV1& outcome : definition.m_outcomes)
        {
            ValidateStableId(result, outcome.m_outcomeId, outcome.m_outcomeId, "outcomes.outcome_id", definition.m_display.m_fallbackName);
            ValidateStableId(result, outcome.m_textKey, outcome.m_outcomeId, "outcomes.text_key", definition.m_display.m_fallbackName);
            if (!HasPhase(phaseIds, outcome.m_phaseId))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueMissingReference,
                    outcome.m_outcomeId,
                    "outcomes.phase_id");
            }
            else if (!IsTerminalPhase(definition, outcome.m_phaseId))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueMissingTerminal,
                    outcome.m_outcomeId,
                    "outcomes.phase_id");
            }
            AppendIdRecord(records, outcome.m_outcomeId, "outcome", "outcomes.outcome_id");
        }
        for (const QuestDefinitionBindingRequirementV1& requirement : definition.m_bindingRequirements)
        {
            ValidateStableId(result, requirement.m_requirementId, requirement.m_requirementId, "binding_requirements.requirement_id", definition.m_display.m_fallbackName);
            if (!ContainsString(roleIds, requirement.m_roleId))
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueMissingReference,
                    requirement.m_requirementId,
                    "binding_requirements.role_id");
            }
            ValidateStableId(result, requirement.m_subjectKind, requirement.m_requirementId, "binding_requirements.subject_kind", definition.m_display.m_fallbackName);
            ValidateStableId(result, requirement.m_usage, requirement.m_requirementId, "binding_requirements.usage", definition.m_display.m_fallbackName);
            AppendIdRecord(
                records,
                requirement.m_requirementId,
                "binding_requirement",
                "binding_requirements.requirement_id");
        }
        for (const AZStd::string& compatibilityTag : definition.m_compatibilityTags)
        {
            ValidateStableId(
                result,
                compatibilityTag,
                definition.m_questId,
                "compatibility_tags",
                definition.m_display.m_fallbackName);
        }

        ValidateIdRecords(result, records);
        ValidateTransitions(result, definition, phaseIds, conditionIds, actionIds);

        size_t entryPhaseCount = 0;
        for (const QuestDefinitionPhaseV1& phase : definition.m_phases)
        {
            entryPhaseCount += phase.m_entryPhase ? 1 : 0;
        }
        if (entryPhaseCount != 1)
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueMissingEntry,
                definition.m_questId,
                "phases.entry_phase");
        }
        bool hasTerminalOutcome = false;
        for (const QuestDefinitionOutcomeV1& outcome : definition.m_outcomes)
        {
            hasTerminalOutcome = hasTerminalOutcome || IsTerminalPhase(definition, outcome.m_phaseId);
        }
        if (!hasTerminalOutcome)
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueMissingTerminal,
                definition.m_questId,
                "outcomes");
        }

        if (definition.m_authority.m_runtimeExecutionAllowed
            || definition.m_authority.m_editorMutationAllowed
            || definition.m_authority.m_saveMutationAllowed
            || definition.m_authority.m_deploymentAllowed
            || definition.m_authority.m_assetExtractionAllowed)
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Blocker,
                IssueAuthority,
                definition.m_questId,
                "authority");
        }
        if (!definition.m_questFingerprint.empty()
            && !QuestDefinitionFingerprintMatchesV1(definition))
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueFingerprint,
                definition.m_questId,
                "quest_fingerprint");
        }

        SortIssues(result);
        return result;
    }

    QuestDefinitionValidationResultV1 ParseQuestDefinitionJsonV1(
        AZStd::string_view json,
        QuestDefinitionV1& definition)
    {
        definition = QuestDefinitionV1{};
        QuestDefinitionValidationResultV1 result;
        if (json.empty() || json.size() > MaxQuestDefinitionJsonBytes)
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueBoundsExceeded,
                {},
                "quest_definition");
            SortIssues(result);
            return result;
        }

        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject())
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueInvalidJson,
                {},
                "quest_definition");
            SortIssues(result);
            return result;
        }
        if (ExceedsJsonDepth(document, 0))
        {
            AddIssue(
                result,
                QuestDefinitionIssueSeverityV1::Error,
                IssueBoundsExceeded,
                {},
                "quest_definition");
        }

        constexpr const char* TopLevelFields[] = {
            "schema",
            "schema_version",
            "quest_id",
            "content_version",
            "owner_pack_id",
            "owner_module_id",
            "display",
            "lifecycle",
            "roles",
            "phases",
            "objectives",
            "transitions",
            "conditions",
            "actions",
            "outcomes",
            "binding_requirements",
            "minimum_sdk_version",
            "compatibility_tags",
            "authority",
            "quest_fingerprint",
        };
        ValidateKnownFields(result, document, TopLevelFields, "quest_definition");

        ReadRequiredString(result, document, "schema", definition.m_schema, "schema");
        ReadRequiredUInt(result, document, "schema_version", definition.m_schemaVersion, "schema_version");
        ReadRequiredString(result, document, "quest_id", definition.m_questId, "quest_id");
        ReadRequiredString(result, document, "content_version", definition.m_contentVersion, "content_version");
        ReadRequiredString(result, document, "owner_pack_id", definition.m_ownerPackId, "owner_pack_id");
        ReadRequiredString(result, document, "owner_module_id", definition.m_ownerModuleId, "owner_module_id");
        ParseDisplay(result, document, definition);
        ReadRequiredString(result, document, "lifecycle", definition.m_lifecycle, "lifecycle");
        ParseRoles(result, document, definition);
        ParsePhases(result, document, definition);
        ParseObjectives(result, document, definition);
        ParseTransitions(result, document, definition);
        ParseConditions(result, document, definition);
        ParseActions(result, document, definition);
        ParseOutcomes(result, document, definition);
        ParseBindingRequirements(result, document, definition);
        ReadRequiredString(result, document, "minimum_sdk_version", definition.m_minimumSdkVersion, "minimum_sdk_version");
        ReadStringArray(
            result,
            document,
            "compatibility_tags",
            definition.m_compatibilityTags,
            "compatibility_tags");
        ParseAuthority(result, document, definition);
        if (const rapidjson::Value* fingerprint = FindMember(document, "quest_fingerprint");
            fingerprint != nullptr)
        {
            if (fingerprint->IsString())
            {
                definition.m_questFingerprint =
                    AZStd::string(fingerprint->GetString(), fingerprint->GetStringLength());
            }
            else
            {
                AddIssue(
                    result,
                    QuestDefinitionIssueSeverityV1::Error,
                    IssueInvalidIdentity,
                    "quest_fingerprint",
                    "quest_fingerprint");
            }
        }

        QuestDefinitionValidationResultV1 semantic = ValidateQuestDefinitionV1(definition);
        for (QuestDefinitionIssueV1& issue : semantic.m_issues)
        {
            result.m_issues.push_back(AZStd::move(issue));
        }
        SortIssues(result);
        return result;
    }

    AZStd::string SerializeCanonicalQuestDefinitionV1(
        const QuestDefinitionV1& definition)
    {
        AZStd::string output;
        output.reserve(4096);
        output.push_back('{');
        DeterministicContractJson::AppendName(output, "actions");
        output.push_back('[');
        AZStd::vector<QuestDefinitionActionV1> actions = definition.m_actions;
        AZStd::sort(
            actions.begin(),
            actions.end(),
            [](const QuestDefinitionActionV1& left, const QuestDefinitionActionV1& right)
            {
                return left.m_actionId < right.m_actionId;
            });
        for (size_t index = 0; index < actions.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendAction(output, actions[index]);
        }
        output += "],";
        DeterministicContractJson::AppendName(output, "authority");
        output.push_back('{');
        DeterministicContractJson::AppendBool(
            output,
            "asset_extraction_allowed",
            definition.m_authority.m_assetExtractionAllowed);
        DeterministicContractJson::AppendBool(
            output,
            "deployment_allowed",
            definition.m_authority.m_deploymentAllowed);
        DeterministicContractJson::AppendBool(
            output,
            "editor_mutation_allowed",
            definition.m_authority.m_editorMutationAllowed);
        DeterministicContractJson::AppendBool(
            output,
            "runtime_execution_allowed",
            definition.m_authority.m_runtimeExecutionAllowed);
        DeterministicContractJson::AppendBool(
            output,
            "save_mutation_allowed",
            definition.m_authority.m_saveMutationAllowed);
        DeterministicContractJson::TrimTrailingComma(output);
        output += "},";
        AppendSortedObjectArray(
            output,
            "binding_requirements",
            definition.m_bindingRequirements,
            [](const QuestDefinitionBindingRequirementV1& value) -> const AZStd::string&
            {
                return value.m_requirementId;
            },
            AppendBindingRequirement);
        DeterministicContractJson::AppendSortedStringArray(output, "compatibility_tags", definition.m_compatibilityTags);
        DeterministicContractJson::AppendName(output, "conditions");
        output.push_back('[');
        AZStd::vector<QuestDefinitionConditionV1> conditions = definition.m_conditions;
        AZStd::sort(
            conditions.begin(),
            conditions.end(),
            [](const QuestDefinitionConditionV1& left, const QuestDefinitionConditionV1& right)
            {
                return left.m_conditionId < right.m_conditionId;
            });
        for (size_t index = 0; index < conditions.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendCondition(output, conditions[index]);
        }
        output += "],";
        DeterministicContractJson::AppendString(output, "content_version", definition.m_contentVersion);
        DeterministicContractJson::AppendName(output, "display");
        output.push_back('{');
        DeterministicContractJson::AppendString(output, "fallback_name", definition.m_display.m_fallbackName);
        DeterministicContractJson::AppendString(output, "fallback_summary", definition.m_display.m_fallbackSummary);
        DeterministicContractJson::AppendString(output, "name_text_key", definition.m_display.m_nameTextKey);
        DeterministicContractJson::AppendString(output, "summary_text_key", definition.m_display.m_summaryTextKey);
        DeterministicContractJson::TrimTrailingComma(output);
        output += "},";
        DeterministicContractJson::AppendString(output, "lifecycle", definition.m_lifecycle);
        DeterministicContractJson::AppendString(output, "minimum_sdk_version", definition.m_minimumSdkVersion);
        AppendSortedObjectArray(
            output,
            "objectives",
            definition.m_objectives,
            [](const QuestDefinitionObjectiveV1& value) -> const AZStd::string&
            {
                return value.m_objectiveId;
            },
            AppendObjective);
        DeterministicContractJson::AppendName(output, "outcomes");
        output.push_back('[');
        AZStd::vector<QuestDefinitionOutcomeV1> outcomes = definition.m_outcomes;
        AZStd::sort(
            outcomes.begin(),
            outcomes.end(),
            [](const QuestDefinitionOutcomeV1& left, const QuestDefinitionOutcomeV1& right)
            {
                return left.m_outcomeId < right.m_outcomeId;
            });
        for (size_t index = 0; index < outcomes.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendOutcome(output, outcomes[index]);
        }
        output += "],";
        DeterministicContractJson::AppendString(output, "owner_module_id", definition.m_ownerModuleId);
        DeterministicContractJson::AppendString(output, "owner_pack_id", definition.m_ownerPackId);
        AppendSortedObjectArray(
            output,
            "phases",
            definition.m_phases,
            [](const QuestDefinitionPhaseV1& value) -> const AZStd::string&
            {
                return value.m_phaseId;
            },
            AppendPhase);
        DeterministicContractJson::AppendString(output, "quest_id", definition.m_questId);
        AppendSortedObjectArray(
            output,
            "roles",
            definition.m_roles,
            [](const QuestDefinitionRoleV1& value) -> const AZStd::string&
            {
                return value.m_roleId;
            },
            AppendRole);
        DeterministicContractJson::AppendString(output, "schema", definition.m_schema);
        DeterministicContractJson::AppendUnsigned(output, "schema_version", definition.m_schemaVersion);
        DeterministicContractJson::AppendName(output, "transitions");
        output.push_back('[');
        AZStd::vector<QuestDefinitionTransitionV1> transitions = definition.m_transitions;
        AZStd::sort(
            transitions.begin(),
            transitions.end(),
            [](const QuestDefinitionTransitionV1& left, const QuestDefinitionTransitionV1& right)
            {
                return left.m_transitionId < right.m_transitionId;
            });
        for (size_t index = 0; index < transitions.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendTransition(output, transitions[index]);
        }
        output.push_back(']');
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateQuestDefinitionFingerprintV1(
        const QuestDefinitionV1& definition)
    {
        return CalculateCanonicalSha256(
            SerializeCanonicalQuestDefinitionV1(definition));
    }

    bool QuestDefinitionFingerprintMatchesV1(
        const QuestDefinitionV1& definition)
    {
        return definition.m_questFingerprint
            == CalculateQuestDefinitionFingerprintV1(definition);
    }
} // namespace TaintedGrailModdingSDK
