/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "QuestBindingManifestContract.h"
#include "QuestDefinitionContract.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string HashOf(char character)
        {
            AZStd::string value = "sha256:";
            for (int index = 0; index < 64; ++index)
            {
                value.push_back(character);
            }
            return value;
        }

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
            definition.m_roles.push_back(QuestDefinitionRoleV1{ "role.fixture.companion", "loc.role.fixture.companion", true });
            definition.m_phases.push_back(QuestDefinitionPhaseV1{ "phase.fixture.start", "loc.phase.fixture.start", true, false, {}, { "objective.fixture.inspect" } });
            definition.m_phases.push_back(QuestDefinitionPhaseV1{ "phase.fixture.done", "loc.phase.fixture.done", false, true, {}, {} });
            definition.m_objectives.push_back(QuestDefinitionObjectiveV1{ "objective.fixture.inspect", "phase.fixture.start", "loc.objective.fixture.inspect", { "condition.fixture.near-cart" }, { "action.fixture.complete-inspect" } });
            definition.m_conditions.push_back(QuestDefinitionConditionV1{ "condition.fixture.near-cart", "location.presence", "subject.fixture.cart" });
            definition.m_actions.push_back(QuestDefinitionActionV1{ "action.fixture.complete-inspect", "objective.complete", "objective.fixture.inspect", "idempotency.fixture.complete-inspect" });
            definition.m_transitions.push_back(QuestDefinitionTransitionV1{ "transition.fixture.finish", "phase.fixture.start", "phase.fixture.done", "trigger.fixture.objective-completed", 0, { "condition.fixture.near-cart" }, { "action.fixture.complete-inspect" }, false });
            definition.m_outcomes.push_back(QuestDefinitionOutcomeV1{ "outcome.fixture.resolved", "phase.fixture.done", "loc.outcome.fixture.resolved" });
            definition.m_bindingRequirements.push_back(QuestDefinitionBindingRequirementV1{ "binding.fixture.cart", "role.fixture.companion", "subject-kind.fixture.actor", "usage.fixture.required-companion" });
            definition.m_minimumSdkVersion = "1.0.0";
            definition.m_compatibilityTags.push_back("compat.fixture.contract-only");
            return definition;
        }

        QuestBindingManifestBindingV1 MakeBinding(
            const char* bindingKind,
            const char* bindingId,
            const char* catalogRecordId,
            const char* subjectRef)
        {
            QuestBindingManifestBindingV1 binding;
            binding.m_bindingId = bindingId;
            binding.m_bindingKind = bindingKind;
            binding.m_requirementId = "binding.fixture.cart";
            binding.m_roleId = "role.fixture.companion";
            binding.m_subjectKind = "subject-kind.fixture.actor";
            binding.m_usage = "usage.fixture.required-companion";
            binding.m_catalogRef.m_catalogRecordId = catalogRecordId;
            binding.m_catalogRef.m_domain = "catalog.domain.actor";
            binding.m_catalogRef.m_recordKind = "catalog.record.actor";
            binding.m_catalogRef.m_subjectRef = subjectRef;
            binding.m_catalogRef.m_catalogFingerprint = HashOf('1');
            binding.m_catalogRef.m_profileId = "profile.fixture.mono";
            binding.m_catalogRef.m_gameVersion = "1.0.0";
            binding.m_catalogRef.m_branch = "branch.fixture.main";
            binding.m_catalogRef.m_runtimeTarget = "Mono";
            binding.m_evidenceRefs.push_back(QuestBindingManifestEvidenceReferenceV1{
                "evidence.fixture.cart",
                "source.fixture.synthetic-catalog",
                HashOf('2'),
                "profile.fixture.mono",
                "1.0.0",
                "branch.fixture.main" });
            binding.m_permissionRefs.push_back(QuestBindingManifestPermissionReferenceV1{
                "permission.fixture.cart",
                "subject-kind.fixture.actor",
                subjectRef,
                "usage.fixture.required-companion",
                "allowed",
                "validation.fixture.permission" });
            binding.m_fallbackPolicy = "fail_closed";
            binding.m_unique = true;
            return binding;
        }

        QuestBindingManifestV1 MakeValidManifest(const QuestDefinitionV1& definition)
        {
            QuestBindingManifestV1 manifest;
            manifest.m_manifestId = "manifest.fixture.cart.binding";
            manifest.m_questId = definition.m_questId;
            manifest.m_contentVersion = "1.0.0";
            manifest.m_ownerPackId = "pack.fixture";
            manifest.m_ownerModuleId = "module.fixture.quests";
            manifest.m_questDefinitionFingerprint = CalculateQuestDefinitionFingerprintV1(definition);
            manifest.m_catalogId = "catalog.fixture.synthetic";
            manifest.m_catalogFingerprint = HashOf('1');
            manifest.m_profileId = "profile.fixture.mono";
            manifest.m_gameVersion = "1.0.0";
            manifest.m_branch = "branch.fixture.main";
            manifest.m_runtimeTarget = "Mono";
            manifest.m_roleBindings.push_back(MakeBinding("role", "binding.fixture.cart.role", "catalog.fixture.cart", "subject.fixture.cart"));
            manifest.m_minimumSdkVersion = "1.0.0";
            manifest.m_compatibilityTags.push_back("compat.fixture.contract-only");
            return manifest;
        }

        bool HasIssue(const QuestBindingManifestValidationResultV1& result, const AZStd::string& code)
        {
            for (const QuestBindingManifestIssueV1& issue : result.m_issues)
            {
                if (issue.m_code == code)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    TEST(QuestBindingManifestContractTests, ValidManifestHasCanonicalFingerprintAndNoMutationAuthority)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        ASSERT_TRUE(ValidateQuestDefinitionV1(definition).IsValid());

        QuestBindingManifestV1 manifest = MakeValidManifest(definition);
        EXPECT_TRUE(ValidateQuestBindingManifestV1(manifest, &definition).IsValid());

        manifest.m_manifestFingerprint = CalculateQuestBindingManifestFingerprintV1(manifest);
        EXPECT_TRUE(ValidateQuestBindingManifestV1(manifest, &definition).IsValid());
        EXPECT_TRUE(QuestBindingManifestFingerprintMatchesV1(manifest));

        const AZStd::string canonical = SerializeCanonicalQuestBindingManifestV1(manifest);
        EXPECT_NE(canonical.find("\"runtime_execution_allowed\":false"), AZStd::string::npos);
        EXPECT_NE(canonical.find("\"editor_mutation_allowed\":false"), AZStd::string::npos);
        EXPECT_NE(canonical.find("\"save_mutation_allowed\":false"), AZStd::string::npos);
        EXPECT_NE(canonical.find("\"deployment_allowed\":false"), AZStd::string::npos);
        EXPECT_NE(canonical.find("\"catalog_mutation_allowed\":false"), AZStd::string::npos);
        EXPECT_NE(canonical.find("\"evidence_promotion_allowed\":false"), AZStd::string::npos);
        EXPECT_NE(canonical.find("\"permission_grant_allowed\":false"), AZStd::string::npos);
        EXPECT_EQ(canonical.find("manifest_fingerprint"), AZStd::string::npos);
    }

    TEST(QuestBindingManifestContractTests, CanonicalSerializationIsOrderIndependent)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 left = MakeValidManifest(definition);
        QuestBindingManifestV1 right = MakeValidManifest(definition);

        QuestBindingManifestBindingV1 extra = MakeBinding(
            "role",
            "binding.fixture.cart.role-alt",
            "catalog.fixture.cart-alt",
            "subject.fixture.cart-alt");
        extra.m_unique = false;
        extra.m_evidenceRefs.push_back(QuestBindingManifestEvidenceReferenceV1{
            "evidence.fixture.cart-alt",
            "source.fixture.synthetic-catalog-alt",
            HashOf('3'),
            "profile.fixture.mono",
            "1.0.0",
            "branch.fixture.main" });
        extra.m_permissionRefs.push_back(QuestBindingManifestPermissionReferenceV1{
            "permission.fixture.cart-alt",
            "subject-kind.fixture.actor",
            "subject.fixture.cart-alt",
            "usage.fixture.required-companion",
            "allowed",
            "validation.fixture.permission-alt" });

        left.m_roleBindings.push_back(extra);
        left.m_compatibilityTags.push_back("compat.fixture.extra");
        right.m_roleBindings.insert(right.m_roleBindings.begin(), extra);
        right.m_compatibilityTags.insert(right.m_compatibilityTags.begin(), "compat.fixture.extra");
        AZStd::reverse(right.m_roleBindings.front().m_evidenceRefs.begin(), right.m_roleBindings.front().m_evidenceRefs.end());
        AZStd::reverse(right.m_roleBindings.front().m_permissionRefs.begin(), right.m_roleBindings.front().m_permissionRefs.end());

        EXPECT_EQ(
            SerializeCanonicalQuestBindingManifestV1(left),
            SerializeCanonicalQuestBindingManifestV1(right));

        right.m_roleBindings.front().m_permissionRefs.front().m_decision = "denied";
        EXPECT_NE(
            SerializeCanonicalQuestBindingManifestV1(left),
            SerializeCanonicalQuestBindingManifestV1(right));
    }

    TEST(QuestBindingManifestContractTests, MalformedJsonSchemaVersionAndUnknownFieldsFailClosed)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 parsed;
        EXPECT_TRUE(HasIssue(
            ParseQuestBindingManifestJsonV1("{", parsed, &definition),
            "quest.binding.schema.invalid-json"));

        QuestBindingManifestV1 manifest = MakeValidManifest(definition);
        AZStd::string unknownFieldJson = SerializeCanonicalQuestBindingManifestV1(manifest);
        unknownFieldJson.insert(unknownFieldJson.size() - 1, ",\"unexpected_public_field\":true");
        EXPECT_TRUE(HasIssue(
            ParseQuestBindingManifestJsonV1(unknownFieldJson, parsed, &definition),
            "quest.binding.schema.unknown-field"));

        const char* excessiveDepthJson =
            "{\"schema\":\"foa.quest-binding-manifest\",\"schema_version\":1,"
            "\"deep\":[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]}";
        EXPECT_TRUE(HasIssue(
            ParseQuestBindingManifestJsonV1(excessiveDepthJson, parsed, &definition),
            "quest.binding.bounds.exceeded"));

        manifest.m_schemaVersion = 2;
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.schema.unsupported-version"));
    }

    TEST(QuestBindingManifestContractTests, DuplicateIdsAndUnsafeReferencesFailClosed)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 manifest = MakeValidManifest(definition);
        manifest.m_locationBindings.push_back(MakeBinding("location", "binding.fixture.cart.role", "catalog.fixture.cart-location", "subject.fixture.cart-location"));
        manifest.m_locationBindings.back().m_unique = false;
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.identity.duplicate"));

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_catalogRef.m_subjectRef = "C:/private/game/cart.prefab";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.path.forbidden"));

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_catalogRef.m_subjectRef =
            "AZ::EntityId(01234567-89ab-cdef-0123-456789abcdef)";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.native-ref.forbidden"));
    }

    TEST(QuestBindingManifestContractTests, QuestDefinitionReferenceMismatchesFailClosed)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 manifest = MakeValidManifest(definition);

        manifest.m_questId = "quest.fixture.other";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.reference.mismatch"));

        manifest = MakeValidManifest(definition);
        manifest.m_questDefinitionFingerprint = HashOf('4');
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.fingerprint.mismatch"));

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_requirementId = "binding.fixture.missing";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.reference.missing"));

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_subjectKind = "subject-kind.fixture.item";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.reference.mismatch"));
    }

    TEST(QuestBindingManifestContractTests, CatalogEvidenceAndPermissionReferencesFailClosed)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_evidenceRefs.clear();
        const QuestBindingManifestValidationResultV1 missingEvidence =
            ValidateQuestBindingManifestV1(manifest, &definition);
        EXPECT_TRUE(HasIssue(missingEvidence, "quest.binding.evidence.invalid"));
        EXPECT_TRUE(missingEvidence.IsBlocked());

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_evidenceRefs.front().m_profileId = "profile.fixture.il2cpp";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.reference.mismatch"));

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_permissionRefs.front().m_decision = "denied";
        const QuestBindingManifestValidationResultV1 deniedPermission =
            ValidateQuestBindingManifestV1(manifest, &definition);
        EXPECT_TRUE(HasIssue(deniedPermission, "quest.binding.permission.blocked"));
        EXPECT_TRUE(deniedPermission.IsBlocked());

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_permissionRefs.front().m_usage = "usage.fixture.optional";
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.permission.mismatch"));

        manifest = MakeValidManifest(definition);
        manifest.m_roleBindings.front().m_catalogRef.m_catalogFingerprint = HashOf('5');
        EXPECT_TRUE(HasIssue(
            ValidateQuestBindingManifestV1(manifest, &definition),
            "quest.binding.reference.mismatch"));
    }

    TEST(QuestBindingManifestContractTests, AuthorityFlagsAreRejectedAndBlocked)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 manifest = MakeValidManifest(definition);
        manifest.m_authority.m_runtimeExecutionAllowed = true;
        manifest.m_authority.m_editorMutationAllowed = true;
        manifest.m_authority.m_saveMutationAllowed = true;
        manifest.m_authority.m_deploymentAllowed = true;
        manifest.m_authority.m_catalogMutationAllowed = true;
        manifest.m_authority.m_evidencePromotionAllowed = true;
        manifest.m_authority.m_permissionGrantAllowed = true;

        const QuestBindingManifestValidationResultV1 result =
            ValidateQuestBindingManifestV1(manifest, &definition);
        EXPECT_TRUE(HasIssue(result, "quest.binding.authority.forbidden"));
        EXPECT_TRUE(result.IsBlocked());
    }

    TEST(QuestBindingManifestContractTests, ParserAcceptsSyntheticFixture)
    {
        QuestDefinitionV1 definition = MakeValidQuestDefinition();
        QuestBindingManifestV1 expected = MakeValidManifest(definition);
        const AZStd::string json = SerializeCanonicalQuestBindingManifestV1(expected);

        QuestBindingManifestV1 parsed;
        const QuestBindingManifestValidationResultV1 result =
            ParseQuestBindingManifestJsonV1(json, parsed, &definition);
        EXPECT_TRUE(result.IsValid());
        EXPECT_EQ(parsed.m_manifestId, "manifest.fixture.cart.binding");
        EXPECT_EQ(parsed.m_roleBindings.size(), 1);
        EXPECT_EQ(
            SerializeCanonicalQuestBindingManifestV1(parsed),
            SerializeCanonicalQuestBindingManifestV1(expected));
    }
} // namespace TaintedGrailModdingSDK
