/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace TaintedGrailModdingSDK
{
    inline constexpr AZ::u32 QuestDefinitionSchemaVersionV1 = 1;
    inline constexpr const char* QuestDefinitionSchemaIdV1 = "foa.quest-definition";

    enum class QuestDefinitionIssueSeverityV1 : AZ::u8
    {
        Error,
        Blocker,
    };

    struct QuestDefinitionIssueV1
    {
        QuestDefinitionIssueSeverityV1 m_severity =
            QuestDefinitionIssueSeverityV1::Error;
        AZStd::string m_code;
        AZStd::string m_subjectId;
        AZStd::string m_propertyPath;
    };

    struct QuestDefinitionValidationResultV1
    {
        AZStd::vector<QuestDefinitionIssueV1> m_issues;

        bool IsValid() const;
        bool IsBlocked() const;
    };

    struct QuestDefinitionDisplayV1
    {
        AZStd::string m_nameTextKey;
        AZStd::string m_summaryTextKey;
        AZStd::string m_fallbackName;
        AZStd::string m_fallbackSummary;
    };

    struct QuestDefinitionRoleV1
    {
        AZStd::string m_roleId;
        AZStd::string m_displayTextKey;
        bool m_required = true;
    };

    struct QuestDefinitionPhaseV1
    {
        AZStd::string m_phaseId;
        AZStd::string m_displayTextKey;
        bool m_entryPhase = false;
        bool m_terminalPhase = false;
        AZStd::vector<AZStd::string> m_entryActionIds;
        AZStd::vector<AZStd::string> m_objectiveIds;
    };

    struct QuestDefinitionObjectiveV1
    {
        AZStd::string m_objectiveId;
        AZStd::string m_phaseId;
        AZStd::string m_displayTextKey;
        AZStd::vector<AZStd::string> m_conditionIds;
        AZStd::vector<AZStd::string> m_completionActionIds;
    };

    struct QuestDefinitionTransitionV1
    {
        AZStd::string m_transitionId;
        AZStd::string m_fromPhaseId;
        AZStd::string m_toPhaseId;
        AZStd::string m_triggerId;
        AZ::u32 m_priority = 0;
        AZStd::vector<AZStd::string> m_conditionIds;
        AZStd::vector<AZStd::string> m_actionIds;
        bool m_repeatAllowed = false;
    };

    struct QuestDefinitionConditionV1
    {
        AZStd::string m_conditionId;
        AZStd::string m_conditionTypeId;
        AZStd::string m_subjectId;
    };

    struct QuestDefinitionActionV1
    {
        AZStd::string m_actionId;
        AZStd::string m_actionTypeId;
        AZStd::string m_subjectId;
        AZStd::string m_idempotencyKey;
    };

    struct QuestDefinitionOutcomeV1
    {
        AZStd::string m_outcomeId;
        AZStd::string m_phaseId;
        AZStd::string m_textKey;
    };

    struct QuestDefinitionBindingRequirementV1
    {
        AZStd::string m_requirementId;
        AZStd::string m_roleId;
        AZStd::string m_subjectKind;
        AZStd::string m_usage;
    };

    struct QuestDefinitionAuthorityV1
    {
        bool m_runtimeExecutionAllowed = false;
        bool m_editorMutationAllowed = false;
        bool m_saveMutationAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_assetExtractionAllowed = false;
    };

    struct QuestDefinitionV1
    {
        AZStd::string m_schema = QuestDefinitionSchemaIdV1;
        AZ::u32 m_schemaVersion = QuestDefinitionSchemaVersionV1;
        AZStd::string m_questId;
        AZStd::string m_contentVersion;
        AZStd::string m_ownerPackId;
        AZStd::string m_ownerModuleId;
        QuestDefinitionDisplayV1 m_display;
        AZStd::string m_lifecycle;
        AZStd::vector<QuestDefinitionRoleV1> m_roles;
        AZStd::vector<QuestDefinitionPhaseV1> m_phases;
        AZStd::vector<QuestDefinitionObjectiveV1> m_objectives;
        AZStd::vector<QuestDefinitionTransitionV1> m_transitions;
        AZStd::vector<QuestDefinitionConditionV1> m_conditions;
        AZStd::vector<QuestDefinitionActionV1> m_actions;
        AZStd::vector<QuestDefinitionOutcomeV1> m_outcomes;
        AZStd::vector<QuestDefinitionBindingRequirementV1> m_bindingRequirements;
        AZStd::string m_minimumSdkVersion;
        AZStd::vector<AZStd::string> m_compatibilityTags;
        QuestDefinitionAuthorityV1 m_authority;
        AZStd::string m_questFingerprint;
    };

    bool IsQuestDefinitionStableIdV1(const AZStd::string& value);

    QuestDefinitionValidationResultV1 ValidateQuestDefinitionV1(
        const QuestDefinitionV1& definition);

    QuestDefinitionValidationResultV1 ParseQuestDefinitionJsonV1(
        AZStd::string_view json,
        QuestDefinitionV1& definition);

    AZStd::string SerializeCanonicalQuestDefinitionV1(
        const QuestDefinitionV1& definition);

    AZStd::string CalculateQuestDefinitionFingerprintV1(
        const QuestDefinitionV1& definition);

    bool QuestDefinitionFingerprintMatchesV1(
        const QuestDefinitionV1& definition);
} // namespace TaintedGrailModdingSDK
