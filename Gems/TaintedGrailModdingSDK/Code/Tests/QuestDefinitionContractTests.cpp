/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "QuestDefinitionContract.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        QuestDefinitionV1 MakeValidQuestDefinition()
        {
            QuestDefinitionV1 definition;
            definition.m_questId = "quest.fixture.cart";
            definition.m_contentVersion = "1.0.0";
            definition.m_ownerPackId = "pack.fixture";
            definition.m_ownerModuleId = "module.fixture.quests";
            definition.m_display.m_nameTextKey = "loc.quest.fixture.cart.name";
            definition.m_display.m_summaryTextKey = "loc.quest.fixture.cart.summary";
            definition.m_display.m_fallbackName = "Fixture Cart";
            definition.m_display.m_fallbackSummary = "Synthetic fixture quest.";
            definition.m_lifecycle = "registered";
            definition.m_roles.push_back(
                QuestDefinitionRoleV1{
                    "role.fixture.companion",
                    "loc.role.fixture.companion",
                    true });
            definition.m_phases.push_back(
                QuestDefinitionPhaseV1{
                    "phase.fixture.start",
                    "loc.phase.fixture.start",
                    true,
                    false,
                    {},
                    { "objective.fixture.inspect" } });
            definition.m_phases.push_back(
                QuestDefinitionPhaseV1{
                    "phase.fixture.done",
                    "loc.phase.fixture.done",
                    false,
                    true,
                    {},
                    {} });
            definition.m_objectives.push_back(
                QuestDefinitionObjectiveV1{
                    "objective.fixture.inspect",
                    "phase.fixture.start",
                    "loc.objective.fixture.inspect",
                    { "condition.fixture.near-cart" },
                    { "action.fixture.complete-inspect" } });
            definition.m_conditions.push_back(
                QuestDefinitionConditionV1{
                    "condition.fixture.near-cart",
                    "location.presence",
                    "subject.fixture.cart" });
            definition.m_actions.push_back(
                QuestDefinitionActionV1{
                    "action.fixture.complete-inspect",
                    "objective.complete",
                    "objective.fixture.inspect",
                    "idempotency.fixture.complete-inspect" });
            definition.m_transitions.push_back(
                QuestDefinitionTransitionV1{
                    "transition.fixture.finish",
                    "phase.fixture.start",
                    "phase.fixture.done",
                    "trigger.fixture.objective-completed",
                    0,
                    { "condition.fixture.near-cart" },
                    { "action.fixture.complete-inspect" },
                    false });
            definition.m_outcomes.push_back(
                QuestDefinitionOutcomeV1{
                    "outcome.fixture.resolved",
                    "phase.fixture.done",
                    "loc.outcome.fixture.resolved" });
            definition.m_bindingRequirements.push_back(
                QuestDefinitionBindingRequirementV1{
                    "binding.fixture.cart",
                    "role.fixture.companion",
                    "subject-kind.fixture.actor",
                    "usage.fixture.required-companion" });
            definition.m_minimumSdkVersion = "1.0.0";
            definition.m_compatibilityTags.push_back("compat.fixture.contract-only");
            return definition;
        }

        bool HasIssue(
            const QuestDefinitionValidationResultV1& result,
            const AZStd::string& code)
        {
            for (const QuestDefinitionIssueV1& issue : result.m_issues)
            {
                if (issue.m_code == code)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    TEST(QuestDefinitionContractTests, ValidDefinitionHasCanonicalFingerprintAndNoRuntimeAuthority)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        EXPECT_TRUE(ValidateQuestDefinitionV1(definition).IsValid());

        const AZStd::string fingerprint =
            CalculateQuestDefinitionFingerprintV1(definition);
        definition.m_questFingerprint = fingerprint;

        EXPECT_TRUE(ValidateQuestDefinitionV1(definition).IsValid());
        EXPECT_TRUE(QuestDefinitionFingerprintMatchesV1(definition));
        EXPECT_NE(
            SerializeCanonicalQuestDefinitionV1(definition).find(
                "\"runtime_execution_allowed\":false"),
            AZStd::string::npos);
        EXPECT_EQ(
            SerializeCanonicalQuestDefinitionV1(definition).find("quest_fingerprint"),
            AZStd::string::npos);
    }

    TEST(QuestDefinitionContractTests, CanonicalSerializationIsOrderIndependent)
    {
        QuestDefinitionV1 left = MakeValidQuestDefinition();
        QuestDefinitionV1 right = MakeValidQuestDefinition();
        right.m_phases[0].m_objectiveIds.push_back("objective.fixture.extra");
        right.m_objectives.push_back(
            QuestDefinitionObjectiveV1{
                "objective.fixture.extra",
                "phase.fixture.start",
                "loc.objective.fixture.extra",
                { "condition.fixture.near-cart" },
                { "action.fixture.complete-inspect" } });

        QuestDefinitionV1 leftWithSameSet = left;
        leftWithSameSet.m_objectives.push_back(right.m_objectives.back());
        leftWithSameSet.m_phases[0].m_objectiveIds.push_back("objective.fixture.extra");
        AZStd::reverse(leftWithSameSet.m_objectives.begin(), leftWithSameSet.m_objectives.end());
        AZStd::reverse(leftWithSameSet.m_phases.begin(), leftWithSameSet.m_phases.end());

        EXPECT_EQ(
            SerializeCanonicalQuestDefinitionV1(right),
            SerializeCanonicalQuestDefinitionV1(leftWithSameSet));

        right.m_actions[0].m_actionTypeId = "journal.update";
        EXPECT_NE(
            SerializeCanonicalQuestDefinitionV1(right),
            SerializeCanonicalQuestDefinitionV1(leftWithSameSet));
    }

    TEST(QuestDefinitionContractTests, MalformedJsonSchemaVersionAndUnknownFieldsFailClosed)
    {
        QuestDefinitionV1 parsed;
        EXPECT_TRUE(HasIssue(
            ParseQuestDefinitionJsonV1("{", parsed),
            "quest.schema.invalid-json"));

        const char* unknownFieldJson =
            "{\"schema\":\"foa.quest-definition\",\"schema_version\":1,"
            "\"quest_id\":\"quest.fixture.cart\",\"content_version\":\"1.0.0\","
            "\"owner_pack_id\":\"pack.fixture\",\"owner_module_id\":\"module.fixture.quests\","
            "\"display\":{\"name_text_key\":\"loc.quest.fixture.cart.name\","
            "\"summary_text_key\":\"loc.quest.fixture.cart.summary\","
            "\"fallback_name\":\"Fixture Cart\",\"fallback_summary\":\"Synthetic fixture quest.\"},"
            "\"lifecycle\":\"registered\",\"roles\":[],\"phases\":[],\"objectives\":[],"
            "\"transitions\":[],\"conditions\":[],\"actions\":[],\"outcomes\":[],"
            "\"binding_requirements\":[],\"minimum_sdk_version\":\"1.0.0\","
            "\"compatibility_tags\":[],\"authority\":{\"runtime_execution_allowed\":false,"
            "\"editor_mutation_allowed\":false,\"save_mutation_allowed\":false,"
            "\"deployment_allowed\":false,\"asset_extraction_allowed\":false},"
            "\"unexpected_public_field\":true}";
        EXPECT_TRUE(HasIssue(
            ParseQuestDefinitionJsonV1(unknownFieldJson, parsed),
            "quest.schema.unknown-field"));

        const char* excessiveDepthJson =
            "{\"schema\":\"foa.quest-definition\",\"schema_version\":1,"
            "\"deep\":[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]}";
        EXPECT_TRUE(HasIssue(
            ParseQuestDefinitionJsonV1(excessiveDepthJson, parsed),
            "quest.bounds.exceeded"));

        QuestDefinitionV1 unsupported = MakeValidQuestDefinition();
        unsupported.m_schemaVersion = 2;
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(unsupported),
            "quest.schema.unsupported-version"));
    }

    TEST(QuestDefinitionContractTests, DuplicateIdsAndDisplayNamesAsIdsFailClosed)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        definition.m_questId = "Fixture Cart";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.identity.invalid"));
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.identity.display-name-as-id"));

        QuestDefinitionV1 duplicates = MakeValidQuestDefinition();
        QuestDefinitionActionV1 duplicateAction = duplicates.m_actions.front();
        duplicates.m_actions.push_back(duplicateAction);
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(duplicates),
            "quest.identity.duplicate"));

        QuestDefinitionV1 tooMany = MakeValidQuestDefinition();
        for (int index = 0; index < 300; ++index)
        {
            tooMany.m_roles.push_back(tooMany.m_roles.front());
        }
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(tooMany),
            "quest.bounds.exceeded"));

        QuestDefinitionV1 tooManyCompatibilityTags = MakeValidQuestDefinition();
        for (int index = 0; index < 300; ++index)
        {
            tooManyCompatibilityTags.m_compatibilityTags.push_back(
                "compat.fixture.contract-only");
        }
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(tooManyCompatibilityTags),
            "quest.bounds.exceeded"));
    }

    TEST(QuestDefinitionContractTests, ReferencesAmbiguityCyclesAndUnknownRegistriesFailClosed)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        definition.m_transitions.front().m_toPhaseId = "phase.fixture.missing";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.reference.missing"));

        definition = MakeValidQuestDefinition();
        definition.m_outcomes.front().m_phaseId = "phase.fixture.missing";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.reference.missing"));

        definition = MakeValidQuestDefinition();
        definition.m_actions.front().m_actionTypeId = "adapter.unknown-action";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.registry.unknown-action"));

        definition = MakeValidQuestDefinition();
        QuestDefinitionTransitionV1 duplicateTransition =
            definition.m_transitions.front();
        definition.m_transitions.push_back(duplicateTransition);
        definition.m_transitions.back().m_transitionId = "transition.fixture.finish-alt";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.transition.ambiguous"));

        definition = MakeValidQuestDefinition();
        definition.m_transitions.push_back(
            QuestDefinitionTransitionV1{
                "transition.fixture.loop",
                "phase.fixture.done",
                "phase.fixture.start",
                "trigger.fixture.reset",
                0,
                {},
                {},
                false });
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.graph.cycle-without-repeat"));
    }

    TEST(QuestDefinitionContractTests, RuntimeEditorMutationAuthorityAndUnsafeReferencesAreRejected)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        definition.m_authority.m_runtimeExecutionAllowed = true;
        definition.m_authority.m_editorMutationAllowed = true;
        const QuestDefinitionValidationResultV1 authority =
            ValidateQuestDefinitionV1(definition);
        EXPECT_TRUE(HasIssue(authority, "quest.authority.forbidden"));
        EXPECT_TRUE(authority.IsBlocked());

        definition = MakeValidQuestDefinition();
        definition.m_actions.front().m_subjectId = "C:/private/game/object.prefab";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.path.forbidden"));

        definition = MakeValidQuestDefinition();
        definition.m_conditions.front().m_subjectId =
            "AZ::EntityId(01234567-89ab-cdef-0123-456789abcdef)";
        EXPECT_TRUE(HasIssue(
            ValidateQuestDefinitionV1(definition),
            "quest.native-ref.forbidden"));
    }

    TEST(QuestDefinitionContractTests, ParserAcceptsSyntheticFixture)
    {
        const char* json =
            "{\"schema\":\"foa.quest-definition\",\"schema_version\":1,"
            "\"quest_id\":\"quest.fixture.cart\",\"content_version\":\"1.0.0\","
            "\"owner_pack_id\":\"pack.fixture\",\"owner_module_id\":\"module.fixture.quests\","
            "\"display\":{\"name_text_key\":\"loc.quest.fixture.cart.name\","
            "\"summary_text_key\":\"loc.quest.fixture.cart.summary\","
            "\"fallback_name\":\"Fixture Cart\",\"fallback_summary\":\"Synthetic fixture quest.\"},"
            "\"lifecycle\":\"registered\","
            "\"roles\":[{\"role_id\":\"role.fixture.companion\","
            "\"display_text_key\":\"loc.role.fixture.companion\",\"required\":true}],"
            "\"phases\":[{\"phase_id\":\"phase.fixture.start\","
            "\"display_text_key\":\"loc.phase.fixture.start\",\"entry_phase\":true,"
            "\"terminal_phase\":false,\"entry_action_ids\":[],"
            "\"objective_ids\":[\"objective.fixture.inspect\"]},"
            "{\"phase_id\":\"phase.fixture.done\","
            "\"display_text_key\":\"loc.phase.fixture.done\",\"entry_phase\":false,"
            "\"terminal_phase\":true,\"entry_action_ids\":[],\"objective_ids\":[]}],"
            "\"objectives\":[{\"objective_id\":\"objective.fixture.inspect\","
            "\"phase_id\":\"phase.fixture.start\","
            "\"display_text_key\":\"loc.objective.fixture.inspect\","
            "\"condition_ids\":[\"condition.fixture.near-cart\"],"
            "\"completion_action_ids\":[\"action.fixture.complete-inspect\"]}],"
            "\"transitions\":[{\"transition_id\":\"transition.fixture.finish\","
            "\"from_phase_id\":\"phase.fixture.start\",\"to_phase_id\":\"phase.fixture.done\","
            "\"trigger_id\":\"trigger.fixture.objective-completed\",\"priority\":0,"
            "\"condition_ids\":[\"condition.fixture.near-cart\"],"
            "\"action_ids\":[\"action.fixture.complete-inspect\"],"
            "\"repeat_allowed\":false}],"
            "\"conditions\":[{\"condition_id\":\"condition.fixture.near-cart\","
            "\"condition_type_id\":\"location.presence\","
            "\"subject_id\":\"subject.fixture.cart\"}],"
            "\"actions\":[{\"action_id\":\"action.fixture.complete-inspect\","
            "\"action_type_id\":\"objective.complete\","
            "\"subject_id\":\"objective.fixture.inspect\","
            "\"idempotency_key\":\"idempotency.fixture.complete-inspect\"}],"
            "\"outcomes\":[{\"outcome_id\":\"outcome.fixture.resolved\","
            "\"phase_id\":\"phase.fixture.done\","
            "\"text_key\":\"loc.outcome.fixture.resolved\"}],"
            "\"binding_requirements\":[{\"requirement_id\":\"binding.fixture.cart\","
            "\"role_id\":\"role.fixture.companion\","
            "\"subject_kind\":\"subject-kind.fixture.actor\","
            "\"usage\":\"usage.fixture.required-companion\"}],"
            "\"minimum_sdk_version\":\"1.0.0\","
            "\"compatibility_tags\":[\"compat.fixture.contract-only\"],"
            "\"authority\":{\"runtime_execution_allowed\":false,"
            "\"editor_mutation_allowed\":false,\"save_mutation_allowed\":false,"
            "\"deployment_allowed\":false,\"asset_extraction_allowed\":false}}";

        QuestDefinitionV1 parsed;
        const QuestDefinitionValidationResultV1 result =
            ParseQuestDefinitionJsonV1(json, parsed);
        EXPECT_TRUE(result.IsValid());
        EXPECT_EQ(parsed.m_questId, "quest.fixture.cart");
        EXPECT_EQ(
            SerializeCanonicalQuestDefinitionV1(parsed),
            SerializeCanonicalQuestDefinitionV1(MakeValidQuestDefinition()));
    }
} // namespace TaintedGrailModdingSDK
