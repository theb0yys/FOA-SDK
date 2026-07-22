/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AvalonAiExtension.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::AvalonAiExtension
{
    namespace
    {
        constexpr size_t MaximumKeys = 512;
        constexpr size_t MaximumGoals = 512;
        constexpr size_t MaximumActions = 1024;

        bool IsBoundedText(
            const AZStd::string& value,
            size_t maximumLength,
            bool allowEmpty = false)
        {
            if ((!allowEmpty && value.empty()) || value.size() > maximumLength)
            {
                return false;
            }
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte < 0x20 || byte == 0x7f)
                {
                    return false;
                }
            }
            return true;
        }

        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool HasDuplicates(AZStd::vector<AZStd::string> values)
        {
            AZStd::sort(values.begin(), values.end());
            return AZStd::adjacent_find(values.begin(), values.end()) != values.end();
        }

        void AddIssue(
            ValidationResult& result,
            const AZStd::string& subjectId,
            AZStd::string code,
            AZStd::string message)
        {
            result.m_issues.push_back(
                ValidationIssue{ subjectId, AZStd::move(code), AZStd::move(message) });
        }

        KnowledgePack BuildKnowledgePack()
        {
            KnowledgePack pack;
            pack.m_extensionId = "extension.avalon-ai-authoring";
            pack.m_version = "0.8.0";
            pack.m_apiMajor = 2;
            pack.m_apiMinor = 0;
            pack.m_upstreamRepository =
                "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods";
            pack.m_upstreamCommit =
                "d7e740e7f167b73152b53409e483dab07d80d048";
            pack.m_licenseExpression = "NOASSERTION";
            pack.m_sources = {
                { "mods/avalon-ai-runtime/src/AvalonAI.Contracts.V2/AvalonAiApiVersion.cs",
                  "77425609857e61eb60600fa87db2c94f08e01ec4" },
                { "mods/avalon-ai-runtime/src/AvalonAI.Contracts.V2/StableIds.cs",
                  "dfd83d6039c52505bb411db71c135918b80b5f84" },
                { "mods/avalon-ai-runtime/src/AvalonAI.Contracts.V2/PackageContracts.cs",
                  "bbc3ad12ed4fcdf5d398f23110f797a722e93f38" },
                { "mods/avalon-ai-runtime/src/AvalonAI.Contracts.V2/PlanningContracts.cs",
                  "385a6b7cba1c2d6c29a6faa4ba1714bac7b60199" },
                { "mods/avalon-ai-runtime/src/AvalonAI.Contracts.V2/BlackboardContracts.cs",
                  "2758fb8bfc176c54da01e3b85a430fa3a4bbbb96" },
                { "mods/avalon-ai-runtime/docs/architecture.md",
                  "b2b80b537a2d3c84bee13a1938a6d9f42698fd53" },
            };
            pack.m_portedSurfaces = {
                "api-2.0-versioning",
                "stable-identities",
                "package-manifests",
                "package-local-blackboards",
                "goal-and-action-authoring",
                "deterministic-planning-inputs",
            };
            pack.m_excludedRuntimeSurfaces = {
                "foa-mono-host",
                "blaze-execution",
                "rabbit-provider",
                "unity-lifecycle",
                "action-dispatch",
                "runtime-ownership-migration",
            };
            return pack;
        }

        bool IsValidComparison(PlanningComparison value)
        {
            return value >= PlanningComparison::Equal
                && value <= PlanningComparison::LessThanOrEqual;
        }

        bool IsValidInterruptPolicy(InterruptPolicy value)
        {
            return value >= InterruptPolicy::Never
                && value <= InterruptPolicy::Always;
        }

        bool ValidateCondition(const PlanningCondition& condition)
        {
            return IsStableContractId(condition.m_factId)
                && IsValidComparison(condition.m_comparison);
        }

        bool ValidateEffect(const PlanningEffect& effect)
        {
            return IsStableContractId(effect.m_factId);
        }

        bool ConditionLess(
            const PlanningCondition& left,
            const PlanningCondition& right)
        {
            if (left.m_factId != right.m_factId)
            {
                return left.m_factId < right.m_factId;
            }
            if (left.m_comparison != right.m_comparison)
            {
                return static_cast<int>(left.m_comparison)
                    < static_cast<int>(right.m_comparison);
            }
            return left.m_value < right.m_value;
        }

        bool EffectLess(
            const PlanningEffect& left,
            const PlanningEffect& right)
        {
            return left.m_factId != right.m_factId
                ? left.m_factId < right.m_factId
                : left.m_assignedValue < right.m_assignedValue;
        }

        void AppendConditions(
            AZStd::string& output,
            const char* name,
            AZStd::vector<PlanningCondition> conditions)
        {
            using namespace DeterministicContractJson;
            AZStd::sort(conditions.begin(), conditions.end(), ConditionLess);
            AppendName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < conditions.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const PlanningCondition& condition = conditions[index];
                output.push_back('{');
                AppendString(output, "fact_id", condition.m_factId);
                AppendSigned(
                    output,
                    "comparison",
                    static_cast<AZ::s64>(condition.m_comparison));
                AppendSigned(output, "value", condition.m_value, false);
                output.push_back('}');
            }
            output += "],";
        }

        void AppendEffects(
            AZStd::string& output,
            const char* name,
            AZStd::vector<PlanningEffect> effects)
        {
            using namespace DeterministicContractJson;
            AZStd::sort(effects.begin(), effects.end(), EffectLess);
            AppendName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < effects.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const PlanningEffect& effect = effects[index];
                output.push_back('{');
                AppendString(output, "fact_id", effect.m_factId);
                AppendSigned(
                    output,
                    "assigned_value",
                    effect.m_assignedValue,
                    false);
                output.push_back('}');
            }
            output += "],";
        }
    } // namespace

    const KnowledgePack& GetCanonicalKnowledgePack()
    {
        static const KnowledgePack pack = BuildKnowledgePack();
        return pack;
    }

    ValidationResult ValidateAndPlan(const PackageManifest& manifest)
    {
        ValidationResult result;
        const KnowledgePack& knowledge = GetCanonicalKnowledgePack();
        if (manifest.m_schemaVersion != 1
            || !IsStableContractId(manifest.m_packageId)
            || !IsBoundedText(manifest.m_displayName, 512)
            || !IsStrictSemanticVersion(manifest.m_packageVersion)
            || manifest.m_requiredRuntimeApi.m_major != knowledge.m_apiMajor
            || manifest.m_requiredRuntimeApi.m_minor < 0
            || manifest.m_requiredRuntimeApi.m_minor > knowledge.m_apiMinor
            || !IsStableContractId(manifest.m_blackboardNamespace)
            || manifest.m_blackboardSchemaVersion < 1
            || manifest.m_maximumPolicyCadenceMilliseconds == 0
            || manifest.m_executionEnabled
            || manifest.m_hostLinked)
        {
            AddIssue(
                result, manifest.m_packageId, "manifest.invalid",
                "Avalon AI manifest identity, supported API, cadence, schema, or inert authority is invalid.");
        }
        if (manifest.m_blackboardKeys.size() > MaximumKeys
            || manifest.m_goals.empty() || manifest.m_goals.size() > MaximumGoals
            || manifest.m_actions.empty() || manifest.m_actions.size() > MaximumActions
            || manifest.m_supportedActorRoles.empty()
            || manifest.m_requiredCapabilities.empty())
        {
            AddIssue(
                result, manifest.m_packageId, "manifest.collection-bounds",
                "Avalon AI manifest collections are empty or exceed contract bounds.");
        }
        if (HasDuplicates(manifest.m_supportedActorRoles)
            || HasDuplicates(manifest.m_requiredCapabilities))
        {
            AddIssue(
                result, manifest.m_packageId, "manifest.duplicate-values",
                "Avalon AI roles and capabilities must be unique.");
        }
        for (const AZStd::string& role : manifest.m_supportedActorRoles)
        {
            if (!IsStableContractId(role))
            {
                AddIssue(result, role, "role.invalid", "Avalon AI actor role ID is invalid.");
            }
        }
        for (const AZStd::string& capability : manifest.m_requiredCapabilities)
        {
            if (!IsStableContractId(capability))
            {
                AddIssue(
                    result, capability, "capability.invalid",
                    "Avalon AI capability ID is invalid.");
            }
        }

        AZStd::vector<AZStd::string> keyIds;
        for (const BlackboardKey& key : manifest.m_blackboardKeys)
        {
            keyIds.push_back(key.m_keyId);
            if (!IsStableContractId(key.m_keyId)
                || key.m_namespace != manifest.m_blackboardNamespace
                || !IsStableContractId(key.m_namespace)
                || !IsBoundedText(key.m_name, 128)
                || key.m_schemaVersion < 1
                || key.m_schemaVersion > manifest.m_blackboardSchemaVersion
                || key.m_access != BlackboardAccess::PackageLocal
                || !IsBoundedText(key.m_valueType, 128))
            {
                AddIssue(
                    result, key.m_keyId, "blackboard-key.invalid",
                    "Package keys must have stable IDs and remain package-local, namespaced, typed, and schema-compatible.");
            }
        }
        if (HasDuplicates(keyIds))
        {
            AddIssue(
                result, manifest.m_packageId, "blackboard-key.duplicate",
                "Avalon AI blackboard key IDs must be unique.");
        }

        AZStd::vector<AZStd::string> goalIds;
        for (const GoalDefinition& goal : manifest.m_goals)
        {
            goalIds.push_back(goal.m_goalId);
            if (!IsStableContractId(goal.m_goalId) || goal.m_priority < 0)
            {
                AddIssue(
                    result, goal.m_goalId, "goal.invalid",
                    "Avalon AI goal identity or priority is invalid.");
            }
            for (const PlanningCondition& condition : goal.m_conditions)
            {
                if (!ValidateCondition(condition))
                {
                    AddIssue(
                        result, goal.m_goalId, "goal.condition-invalid",
                        "Avalon AI goal contains an invalid planning condition.");
                }
            }
        }
        if (HasDuplicates(goalIds))
        {
            AddIssue(
                result, manifest.m_packageId, "goal.duplicate",
                "Avalon AI goal IDs must be unique.");
        }

        AZStd::vector<AZStd::string> actionIds;
        for (const ActionDefinition& action : manifest.m_actions)
        {
            actionIds.push_back(action.m_actionId);
            if (!IsStableContractId(action.m_actionId)
                || !IsStableContractId(action.m_requiredCapability)
                || !Contains(manifest.m_requiredCapabilities, action.m_requiredCapability)
                || !IsValidInterruptPolicy(action.m_interruptPolicy)
                || action.m_timeoutMilliseconds == 0)
            {
                AddIssue(
                    result, action.m_actionId, "action.invalid",
                    "Avalon AI action identity, capability, interrupt policy, or timeout is invalid.");
            }
            if (!action.m_targetKeyId.empty()
                && (!IsStableContractId(action.m_targetKeyId)
                    || !Contains(keyIds, action.m_targetKeyId)))
            {
                AddIssue(
                    result, action.m_actionId, "action.target-key-invalid",
                    "Avalon AI action target-key ID must resolve to one declared package key.");
            }
            for (const PlanningCondition& condition : action.m_conditions)
            {
                if (!ValidateCondition(condition))
                {
                    AddIssue(
                        result, action.m_actionId, "action.condition-invalid",
                        "Avalon AI action contains an invalid planning condition.");
                }
            }
            for (const PlanningEffect& effect : action.m_effects)
            {
                if (!ValidateEffect(effect))
                {
                    AddIssue(
                        result, action.m_actionId, "action.effect-invalid",
                        "Avalon AI action contains an invalid planning effect.");
                }
            }
        }
        if (HasDuplicates(actionIds))
        {
            AddIssue(
                result, manifest.m_packageId, "action.duplicate",
                "Avalon AI action IDs must be unique.");
        }

        result.m_accepted = result.m_issues.empty();
        if (!result.m_accepted)
        {
            return result;
        }
        AZStd::sort(goalIds.begin(), goalIds.end());
        AZStd::sort(actionIds.begin(), actionIds.end());
        AZStd::vector<AZStd::string> capabilities = manifest.m_requiredCapabilities;
        AZStd::sort(capabilities.begin(), capabilities.end());
        result.m_plan.m_packageId = manifest.m_packageId;
        result.m_plan.m_goalIds = AZStd::move(goalIds);
        result.m_plan.m_actionIds = AZStd::move(actionIds);
        result.m_plan.m_requiredCapabilities = AZStd::move(capabilities);
        result.m_plan.m_canonicalFingerprint = CalculatePackageFingerprint(manifest);
        result.m_plan.m_executionAllowed = false;
        result.m_plan.m_runtimeMutationAllowed = false;
        return result;
    }

    AZStd::string BuildCanonicalPackage(const PackageManifest& manifest)
    {
        using namespace DeterministicContractJson;

        AZStd::string output = "{";
        AppendString(output, "schema", "avalon-ai-package-v1");
        AppendUnsigned(output, "schema_version", manifest.m_schemaVersion);
        AppendString(output, "package_id", manifest.m_packageId);
        AppendString(output, "display_name", manifest.m_displayName);
        AppendString(output, "package_version", manifest.m_packageVersion);
        AppendName(output, "required_runtime_api");
        output.push_back('{');
        AppendSigned(output, "major", manifest.m_requiredRuntimeApi.m_major);
        AppendSigned(
            output,
            "minor",
            manifest.m_requiredRuntimeApi.m_minor,
            false);
        output += "},";
        AppendString(output, "blackboard_namespace", manifest.m_blackboardNamespace);
        AppendSigned(
            output,
            "blackboard_schema_version",
            manifest.m_blackboardSchemaVersion);
        AppendUnsigned(
            output,
            "maximum_policy_cadence_ms",
            manifest.m_maximumPolicyCadenceMilliseconds);
        AppendSortedStringArray(
            output,
            "supported_actor_roles",
            manifest.m_supportedActorRoles);
        AppendSortedStringArray(
            output,
            "required_capabilities",
            manifest.m_requiredCapabilities);

        AZStd::vector<const BlackboardKey*> keys;
        for (const BlackboardKey& key : manifest.m_blackboardKeys)
        {
            keys.push_back(&key);
        }
        AZStd::sort(
            keys.begin(), keys.end(),
            [](const BlackboardKey* left, const BlackboardKey* right)
            {
                return left->m_keyId < right->m_keyId;
            });
        AppendName(output, "blackboard_keys");
        output.push_back('[');
        for (size_t index = 0; index < keys.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const BlackboardKey& key = *keys[index];
            output.push_back('{');
            AppendString(output, "key_id", key.m_keyId);
            AppendString(output, "namespace", key.m_namespace);
            AppendString(output, "name", key.m_name);
            AppendSigned(output, "schema_version", key.m_schemaVersion);
            AppendSigned(output, "access", static_cast<AZ::s64>(key.m_access));
            AppendSigned(output, "scope", static_cast<AZ::s64>(key.m_scope));
            AppendString(output, "value_type", key.m_valueType);
            AppendBool(output, "persistent", key.m_persistent, false);
            output.push_back('}');
        }
        output += "],";

        AZStd::vector<const GoalDefinition*> goals;
        for (const GoalDefinition& goal : manifest.m_goals)
        {
            goals.push_back(&goal);
        }
        AZStd::sort(
            goals.begin(), goals.end(),
            [](const GoalDefinition* left, const GoalDefinition* right)
            {
                return left->m_goalId < right->m_goalId;
            });
        AppendName(output, "goals");
        output.push_back('[');
        for (size_t index = 0; index < goals.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const GoalDefinition& goal = *goals[index];
            output.push_back('{');
            AppendString(output, "goal_id", goal.m_goalId);
            AppendSigned(output, "priority", goal.m_priority);
            AppendConditions(output, "conditions", goal.m_conditions);
            TrimTrailingComma(output);
            output.push_back('}');
        }
        output += "],";

        AZStd::vector<const ActionDefinition*> actions;
        for (const ActionDefinition& action : manifest.m_actions)
        {
            actions.push_back(&action);
        }
        AZStd::sort(
            actions.begin(), actions.end(),
            [](const ActionDefinition* left, const ActionDefinition* right)
            {
                return left->m_actionId < right->m_actionId;
            });
        AppendName(output, "actions");
        output.push_back('[');
        for (size_t index = 0; index < actions.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            const ActionDefinition& action = *actions[index];
            output.push_back('{');
            AppendString(output, "action_id", action.m_actionId);
            AppendString(output, "target_key_id", action.m_targetKeyId);
            AppendString(
                output,
                "required_capability",
                action.m_requiredCapability);
            AppendSigned(
                output,
                "interrupt_policy",
                static_cast<AZ::s64>(action.m_interruptPolicy));
            AppendUnsigned(
                output,
                "timeout_ms",
                action.m_timeoutMilliseconds);
            AppendConditions(output, "conditions", action.m_conditions);
            AppendEffects(output, "effects", action.m_effects);
            TrimTrailingComma(output);
            output.push_back('}');
        }
        output += "],";
        AppendBool(output, "execution_enabled", manifest.m_executionEnabled);
        AppendBool(output, "host_linked", manifest.m_hostLinked, false);
        output.push_back('}');
        return output;
    }

    AZStd::string CalculatePackageFingerprint(const PackageManifest& manifest)
    {
        return CalculateCanonicalSha256(BuildCanonicalPackage(manifest));
    }
} // namespace TaintedGrailModdingSDK::AvalonAiExtension
