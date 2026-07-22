/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "TaintedFrameworkKnowledge.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::TaintedFrameworkEditorServices
{
    enum class ReadinessStatus
    {
        Ready,
        Blocked,
        Unsupported,
    };

    struct CompatibilityDecision
    {
        ReadinessStatus m_status = ReadinessStatus::Unsupported;
        AZStd::string m_frameworkVersion;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtime;
        AZStd::string m_bepInExVersion;
        AZStd::string m_evidencePath;
        AZStd::vector<AZStd::string> m_blockers;
    };

    struct ApiSurfaceDecision
    {
        AZStd::string m_surfaceId;
        AZStd::string m_status;
        AZStd::string m_consumerBinding;
        bool m_consumerReady = false;
        AZStd::vector<AZStd::string> m_blockers;
    };

    struct ConfigurationDefault
    {
        AZStd::string m_section;
        AZStd::string m_key;
        AZStd::string m_value;
    };

    struct ActivationPlan
    {
        AZStd::string m_planId;
        AZStd::string m_extensionId;
        CompatibilityDecision m_compatibility;
        AZStd::vector<ApiSurfaceDecision> m_surfaces;
        AZStd::vector<ConfigurationDefault> m_configuration;
        AZStd::vector<AZStd::string> m_diagnostics;
        bool m_runtimeInvocationAllowed = false;
        bool m_fileWriteAllowed = false;
        bool m_catalogMutationAllowed = false;
        bool m_candidateEvidenceSubmissionEligible = false;
    };

    class Service final
    {
    public:
        CompatibilityDecision EvaluateCompatibility(
            const AZStd::string& gameVersion,
            const AZStd::string& branch,
            const AZStd::string& runtime,
            const AZStd::string& bepInExVersion) const;

        CompatibilityDecision EvaluateCompatibility(
            const ExtensionAPI::ProfileView& profile) const;

        AZStd::vector<ApiSurfaceDecision> GetApiSurfaceDecisions() const;
        AZStd::vector<ConfigurationDefault> GetConfigurationDefaults() const;
        AZStd::vector<AZStd::string> GetDiagnosticVocabulary() const;

        ActivationPlan BuildActivationPlan(
            const AZStd::string& gameVersion,
            const AZStd::string& branch,
            const AZStd::string& runtime,
            const AZStd::string& bepInExVersion) const;

        ActivationPlan BuildActivationPlan(
            const ExtensionAPI::ProfileView& profile) const;
    };
} // namespace TaintedGrailModdingSDK::TaintedFrameworkEditorServices
