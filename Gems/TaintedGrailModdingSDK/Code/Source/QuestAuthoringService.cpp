/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "QuestAuthoringService.h"
#include "CatalogDatabase.h"
#include "CanonicalFingerprint.h"
#include "WorldPlanningService.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/sort.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IntegerDefault(const AZStd::string& text)
        {
            size_t i = !text.empty() && text[0] == '-' ? 1 : 0;
            if (i == text.size() || text.size() > 11) { return false; }
            AZ::u64 value = 0;
            for (; i < text.size(); ++i) { if (text[i] < '0' || text[i] > '9') { return false; } value = value * 10 + text[i] - '0'; }
            return value <= 1000000000;
        }
        bool Has(const AZStd::vector<AZStd::string>& ids, const AZStd::string& id)
        { return AZStd::find(ids.begin(), ids.end(), id) != ids.end(); }
    }
    AZStd::vector<AZStd::string> QuestAuthoringService::InternalIds(const QuestDefinitionV1& d)
    {
        AZStd::vector<AZStd::string> ids{d.m_questId};
        for (const auto& v : d.m_phases) { ids.push_back(v.m_phaseId); }
        for (const auto& v : d.m_objectives) { ids.push_back(v.m_objectiveId); }
        for (const auto& v : d.m_transitions) { ids.push_back(v.m_transitionId); }
        for (const auto& v : d.m_conditions) { ids.push_back(v.m_conditionId); }
        for (const auto& v : d.m_actions) { ids.push_back(v.m_actionId); }
        for (const auto& v : d.m_outcomes) { ids.push_back(v.m_outcomeId); }
        for (const auto& v : d.m_roles) { ids.push_back(v.m_roleId); }
        for (const auto& v : d.m_bindingRequirements) { ids.push_back(v.m_requirementId); }
        return ids;
    }
    bool QuestAuthoringService::IsReferencedSubject(const QuestDefinitionV1& d, const AZStd::string& id)
    {
        for (const auto& v : d.m_roles) { if (v.m_roleId == id) { return true; } }
        for (const auto& v : d.m_conditions) { if (v.m_subjectId == id) { return true; } }
        for (const auto& v : d.m_actions) { if (v.m_subjectId == id) { return true; } }
        return false;
    }
    AZStd::string QuestAuthoringService::Label(const QuestAuthoringDraft& draft, const AZStd::string& id)
    {
        for (const auto& v : draft.m_labels) { if (v.m_id == id) { return v.m_text; } }
        if (id == draft.m_definition.m_questId) { return draft.m_definition.m_display.m_fallbackName; }
        return id;
    }
    QuestAuthoringDraft QuestAuthoringService::Read(const QuestAuthoringProfile& p)
    {
        QuestAuthoringDraft draft;
        ParseQuestDefinitionJsonV1(p.m_definitionJson, draft.m_definition);
        draft.m_labels = p.m_labels; draft.m_stateKeys = p.m_stateKeys; draft.m_bindings = p.m_bindings;
        return draft;
    }
    QuestAuthoringProfile QuestAuthoringService::CanonicalProfile(const QuestAuthoringDraft& draft)
    {
        auto definition = draft.m_definition; definition.m_questFingerprint.clear();
        QuestAuthoringProfile p; p.m_recordId = definition.m_questId;
        p.m_definitionJson = SerializeCanonicalQuestDefinitionV1(definition);
        p.m_labels = draft.m_labels; p.m_stateKeys = draft.m_stateKeys; p.m_bindings = draft.m_bindings;
        AZStd::sort(p.m_labels.begin(), p.m_labels.end(), [](const auto& a, const auto& b) { return a.m_id < b.m_id; });
        AZStd::sort(p.m_stateKeys.begin(), p.m_stateKeys.end(), [](const auto& a, const auto& b) { return a.m_keyId < b.m_keyId; });
        AZStd::sort(p.m_bindings.begin(), p.m_bindings.end(), [](const auto& a, const auto& b) { return a.m_subjectId < b.m_subjectId; });
        return p;
    }
    AZStd::string QuestAuthoringService::Revision(const QuestAuthoringProfile& profile)
    {
        const auto p = CanonicalProfile(Read(profile));
        AZStd::string bytes;
        const auto add = [&bytes](const AZStd::string& v) { bytes += AZStd::string::format("%zu:", v.size()); bytes += v; };
        add(p.m_recordId); add(p.m_definitionJson);
        add("labels"); for (const auto& v : p.m_labels) { add(v.m_id); add(v.m_text); }
        add("states"); for (const auto& v : p.m_stateKeys) { add(v.m_keyId); add(v.m_type); add(v.m_defaultValue); add(v.m_description); }
        add("bindings"); for (const auto& v : p.m_bindings) { add(v.m_subjectId); add(v.m_recordId); }
        return CalculateCanonicalSha256(bytes);
    }
    QuestInspection QuestAuthoringService::Inspect(const QuestAuthoringDraft& draft, const CatalogDatabase& catalog)
    {
        QuestInspection result; const auto& d = draft.m_definition;
        const auto error = [&result](const AZStd::string& message) { result.m_errors.push_back(message); };
        const auto validation = ValidateQuestDefinitionV1(d);
        for (const auto& issue : validation.m_issues)
        { error(issue.m_code + ": " + issue.m_subjectId + " (" + issue.m_propertyPath + ")"); }
        if (draft.m_labels.size() > 256 || draft.m_stateKeys.size() > 256 || draft.m_bindings.size() > 256)
        { error("Quest authoring supports up to 256 labels, state keys and bindings each."); return result; }
        const auto internal = InternalIds(d); AZStd::unordered_set<AZStd::string> labels;
        for (const auto& v : draft.m_labels)
        {
            if (!Has(internal, v.m_id) || !labels.insert(v.m_id).second || !WorldPlanningService::IsSingleLine(v.m_text, 512, false))
            { error("A display label has a missing/duplicate identity or invalid text: " + v.m_id); }
        }
        AZStd::unordered_map<AZStd::string, const QuestStateKey*> states;
        for (const auto& v : draft.m_stateKeys)
        {
            if (!IsQuestDefinitionStableIdV1(v.m_keyId) || Has(internal, v.m_keyId) || !states.emplace(v.m_keyId, &v).second)
            { error("State keys require unique stable IDs distinct from quest elements: " + v.m_keyId); }
            if ((v.m_type != "boolean" && v.m_type != "integer" && v.m_type != "text")
                || (v.m_type == "boolean" && v.m_defaultValue != "true" && v.m_defaultValue != "false")
                || (v.m_type == "integer" && !IntegerDefault(v.m_defaultValue))
                || !WorldPlanningService::IsSingleLine(v.m_defaultValue, 2048)
                || !WorldPlanningService::IsSingleLine(v.m_description, 2048))
            { error("Invalid state type/default; use boolean, integer within +/-1000000000, or text: " + v.m_keyId); }
        }
        AZStd::unordered_map<AZStd::string, const CatalogRecord*> bindings;
        for (const auto& v : draft.m_bindings)
        {
            const auto* record = catalog.FindByRecordId(v.m_recordId);
            if (!IsQuestDefinitionStableIdV1(v.m_subjectId) || !IsReferencedSubject(d, v.m_subjectId)
                || states.count(v.m_subjectId)
                || (Has(internal, v.m_subjectId) && AZStd::none_of(d.m_roles.begin(), d.m_roles.end(), [&](const auto& role) { return role.m_roleId == v.m_subjectId; }))
                || !bindings.emplace(v.m_subjectId, record).second
                || (catalog.FindByRecordId(v.m_subjectId) && v.m_subjectId != v.m_recordId)
                || !record || (record->m_recordKind != "actor" && record->m_recordKind != "item"
                    && record->m_recordKind != "location" && record->m_recordKind != "quest"))
            { error("A subject binding is missing, duplicated, unused or targets an unsupported record: " + v.m_subjectId); }
        }
        const auto resolve = [&catalog, &bindings](const AZStd::string& id) -> const CatalogRecord*
        { const auto it = bindings.find(id); return it == bindings.end() ? catalog.FindByRecordId(id) : it->second; };
        const auto check = [&](const AZStd::string& type, const AZStd::string& subject, const AZStd::string& element)
        {
            if (type.find("counter.") == 0 || type.find("fact.") == 0 || type.find("decision.") == 0)
            {
                const auto it = states.find(subject);
                if (it == states.end() || (type.find("counter.") == 0 && it->second->m_type != "integer")
                    || (type.find("fact.") == 0 && it->second->m_type != "boolean")
                    || (type.find("decision.") == 0 && it->second->m_type != "text"))
                { error("Missing state key or incompatible state type for " + element + ": " + subject); }
            }
            else if (type == "location.presence")
            {
                const auto* record = resolve(subject);
                if (!record || record->m_domain != "world" || record->m_recordKind != "location")
                { error("A location condition needs an exact world location: " + element); }
            }
            else if (type.find("objective.") == 0)
            {
                if (AZStd::none_of(d.m_objectives.begin(), d.m_objectives.end(), [&](const auto& v) { return v.m_objectiveId == subject; }))
                { error("Objective reference is missing: " + element); }
            }
            else if (type == "phase.reached")
            {
                if (AZStd::none_of(d.m_phases.begin(), d.m_phases.end(), [&](const auto& v) { return v.m_phaseId == subject; }))
                { error("Phase reference is missing: " + element); }
            }
            else if (type == "role.available")
            {
                const auto* record = resolve(subject);
                if (!record || record->m_recordKind != "actor")
                { error("An actor role needs an exact actor binding: " + element); }
            }
            else if (type.find("quest.") == 0)
            {
                const auto* record = resolve(subject);
                if (subject != d.m_questId && (!record || record->m_recordKind != "quest"))
                { error("Quest reference is missing: " + element); }
            }
            else { result.m_warnings.push_back("External requirement remains unresolved for runtime: " + element + " (" + type + ")"); }
        };
        for (const auto& v : d.m_conditions) { check(v.m_conditionTypeId, v.m_subjectId, v.m_conditionId); }
        for (const auto& v : d.m_actions) { check(v.m_actionTypeId, v.m_subjectId, v.m_actionId); }
        for (const auto& role : d.m_roles)
        { if (role.m_required && !resolve(role.m_roleId)) { result.m_warnings.push_back("Required role has no local binding: " + Label(draft, role.m_roleId)); } }
        for (const auto& phase : d.m_phases)
        {
            if (!phase.m_entryPhase && AZStd::none_of(d.m_transitions.begin(), d.m_transitions.end(), [&](const auto& t) { return t.m_toPhaseId == phase.m_phaseId; }))
            { result.m_warnings.push_back("Phase has no incoming transition: " + Label(draft, phase.m_phaseId)); }
        }
        return result;
    }
}
