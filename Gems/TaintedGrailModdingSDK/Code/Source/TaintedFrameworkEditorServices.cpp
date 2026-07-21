/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "TaintedFrameworkEditorServices.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::TaintedFrameworkEditorServices
{
    CompatibilityDecision Service::EvaluateCompatibility(
        const AZStd::string& gameVersion,
        const AZStd::string& branch,
        const AZStd::string& runtime) const
    {
        CompatibilityDecision result;
        result.m_frameworkVersion = "0.1.33";
        result.m_gameVersion = gameVersion;
        result.m_branch = branch;
        result.m_runtime = runtime;

        const auto& pack = TaintedFrameworkKnowledge::GetCanonicalKnowledgePack();
        for (const auto& row : pack.m_compatibility)
        {
            if (row.m_branch != branch || row.m_runtime != runtime)
            {
                continue;
            }

            result.m_evidencePath = row.m_evidencePath;
            if (row.m_status == "live_load_validated")
            {
                if (row.m_gameVersion == gameVersion)
                {
                    result.m_status = ReadinessStatus::Ready;
                    return result;
                }
                result.m_status = ReadinessStatus::Unsupported;
                result.m_blockers.push_back("exact_game_version_not_evidence_backed");
                return result;
            }
            if (row.m_status == "blocked")
            {
                result.m_status = ReadinessStatus::Blocked;
                result.m_blockers.push_back("upstream_branch_blocked");
                return result;
            }

            result.m_status = ReadinessStatus::Unsupported;
            result.m_blockers.push_back("compatibility_observation_not_activatable");
            return result;
        }

        result.m_status = ReadinessStatus::Unsupported;
        result.m_blockers.push_back("branch_runtime_pair_not_in_canonical_knowledge");
        return result;
    }

    AZStd::vector<ApiSurfaceDecision> Service::GetApiSurfaceDecisions() const
    {
        AZStd::vector<ApiSurfaceDecision> results;
        for (const auto& surface : TaintedFrameworkKnowledge::GetCanonicalKnowledgePack().m_apiSurfaces)
        {
            ApiSurfaceDecision decision;
            decision.m_surfaceId = surface.m_surfaceId;
            decision.m_status = surface.m_status;
            decision.m_consumerBinding = surface.m_consumerBinding;
            decision.m_consumerReady = surface.m_consumerReady;
            if (!decision.m_consumerReady)
            {
                decision.m_blockers.push_back("consumer_binding_not_approved");
            }
            results.push_back(AZStd::move(decision));
        }
        AZStd::sort(results.begin(), results.end(), [](const auto& left, const auto& right)
        {
            return left.m_surfaceId < right.m_surfaceId;
        });
        return results;
    }

    AZStd::vector<ConfigurationDefault> Service::GetConfigurationDefaults() const
    {
        return {
            { "General", "Enabled", "true" },
            { "Safety", "DryRunOnly", "true" },
            { "Safety", "ReportOnlyMode", "true" },
        };
    }

    AZStd::vector<AZStd::string> Service::GetDiagnosticVocabulary() const
    {
        auto values = TaintedFrameworkKnowledge::GetCanonicalKnowledgePack().m_diagnosticVocabulary;
        AZStd::sort(values.begin(), values.end());
        values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        return values;
    }

    ActivationPlan Service::BuildActivationPlan(
        const AZStd::string& gameVersion,
        const AZStd::string& branch,
        const AZStd::string& runtime) const
    {
        ActivationPlan plan;
        plan.m_planId = "tainted-framework-editor-plan:0.1.33:";
        plan.m_planId += branch;
        plan.m_planId += ":";
        plan.m_planId += runtime;
        plan.m_planId += ":";
        plan.m_planId += gameVersion;
        plan.m_extensionId = "extension.tainted-framework";
        plan.m_compatibility = EvaluateCompatibility(gameVersion, branch, runtime);
        plan.m_surfaces = GetApiSurfaceDecisions();
        plan.m_configuration = GetConfigurationDefaults();
        plan.m_diagnostics = GetDiagnosticVocabulary();
        plan.m_candidateEvidenceSubmissionEligible =
            plan.m_compatibility.m_status == ReadinessStatus::Ready;
        plan.m_runtimeInvocationAllowed = false;
        plan.m_fileWriteAllowed = false;
        plan.m_catalogMutationAllowed = false;
        return plan;
    }
} // namespace TaintedGrailModdingSDK::TaintedFrameworkEditorServices
