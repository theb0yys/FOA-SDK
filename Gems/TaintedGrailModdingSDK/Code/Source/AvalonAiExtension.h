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

namespace TaintedGrailModdingSDK::AvalonAiExtension
{
    enum class ActorExecutionMode
    {
        BlazeOwned,
        NativeAssisted,
        ObservationOnly,
        Disabled,
    };

    enum class BlackboardAccess
    {
        Authoritative,
        Derived,
        PackageLocal,
    };

    enum class BlackboardScope
    {
        Actor,
        World,
    };

    enum class PlanningComparison
    {
        Equal,
        NotEqual,
        GreaterThan,
        GreaterThanOrEqual,
        LessThan,
        LessThanOrEqual,
    };

    enum class InterruptPolicy
    {
        Never,
        OnInvalidTarget,
        OnThreatIncrease,
        OnDamage,
        OnGoalChange,
        Always,
    };

    struct SourceBinding
    {
        AZStd::string m_path;
        AZStd::string m_gitBlobSha1;
    };

    struct KnowledgePack
    {
        AZStd::string m_extensionId;
        AZStd::string m_version;
        int m_apiMajor = 2;
        int m_apiMinor = 0;
        AZStd::string m_upstreamRepository;
        AZStd::string m_upstreamCommit;
        AZStd::string m_licenseExpression;
        AZStd::vector<SourceBinding> m_sources;
        AZStd::vector<AZStd::string> m_portedSurfaces;
        AZStd::vector<AZStd::string> m_excludedRuntimeSurfaces;
    };

    struct ApiVersion
    {
        int m_major = 2;
        int m_minor = 0;
    };

    struct BlackboardKey
    {
        AZStd::string m_namespace;
        AZStd::string m_name;
        int m_schemaVersion = 1;
        BlackboardAccess m_access = BlackboardAccess::PackageLocal;
        BlackboardScope m_scope = BlackboardScope::Actor;
        AZStd::string m_valueType;
        bool m_persistent = false;
    };

    struct PlanningCondition
    {
        AZStd::string m_factId;
        PlanningComparison m_comparison = PlanningComparison::Equal;
        int m_value = 0;
    };

    struct PlanningEffect
    {
        AZStd::string m_factId;
        int m_assignedValue = 0;
    };

    struct GoalDefinition
    {
        AZStd::string m_goalId;
        int m_priority = 0;
        AZStd::vector<PlanningCondition> m_conditions;
    };

    struct ActionDefinition
    {
        AZStd::string m_actionId;
        AZStd::vector<PlanningCondition> m_conditions;
        AZStd::vector<PlanningEffect> m_effects;
        AZStd::string m_targetKeyId;
        AZStd::string m_requiredCapability;
        InterruptPolicy m_interruptPolicy = InterruptPolicy::Never;
        AZ::u64 m_timeoutMilliseconds = 0;
    };

    struct PackageManifest
    {
        AZ::u32 m_schemaVersion = 1;
        AZStd::string m_packageId;
        AZStd::string m_displayName;
        AZStd::string m_packageVersion;
        ApiVersion m_requiredRuntimeApi;
        AZStd::string m_blackboardNamespace;
        int m_blackboardSchemaVersion = 1;
        AZ::u64 m_maximumPolicyCadenceMilliseconds = 0;
        AZStd::vector<AZStd::string> m_supportedActorRoles;
        AZStd::vector<AZStd::string> m_requiredCapabilities;
        AZStd::vector<BlackboardKey> m_blackboardKeys;
        AZStd::vector<GoalDefinition> m_goals;
        AZStd::vector<ActionDefinition> m_actions;
        bool m_executionEnabled = false;
        bool m_hostLinked = false;
    };

    struct ValidationIssue
    {
        AZStd::string m_subjectId;
        AZStd::string m_code;
        AZStd::string m_message;
    };

    struct AuthoringPlan
    {
        AZStd::string m_packageId;
        AZStd::vector<AZStd::string> m_goalIds;
        AZStd::vector<AZStd::string> m_actionIds;
        AZStd::vector<AZStd::string> m_requiredCapabilities;
        AZStd::string m_canonicalFingerprint;
        bool m_executionAllowed = false;
        bool m_runtimeMutationAllowed = false;
    };

    struct ValidationResult
    {
        bool m_accepted = false;
        AZStd::vector<ValidationIssue> m_issues;
        AuthoringPlan m_plan;
    };

    const KnowledgePack& GetCanonicalKnowledgePack();

    ValidationResult ValidateAndPlan(const PackageManifest& manifest);
    AZStd::string BuildCanonicalPackage(const PackageManifest& manifest);
    AZStd::string CalculatePackageFingerprint(const PackageManifest& manifest);
} // namespace TaintedGrailModdingSDK::AvalonAiExtension
