/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
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
        const AZStd::string& runtime,
        const AZStd::string& bepInExVersion) const
    {
        const auto& pack = TaintedFrameworkKnowledge::GetCanonicalKnowledgePack();

        CompatibilityDecision result;
        result.m_frameworkVersion = pack.m_frameworkVersion;
        result.m_gameVersion = gameVersion;
        result.m_branch = branch;
        result.m_runtime = runtime;
        result.m_bepInExVersion = bepInExVersion;

        const TaintedFrameworkKnowledge::CompatibilityObservation* blocked = nullptr;
        const TaintedFrameworkKnowledge::CompatibilityObservation* exact = nullptr;
        const TaintedFrameworkKnowledge::CompatibilityObservation* exactGame = nullptr;
        const TaintedFrameworkKnowledge::CompatibilityObservation* live = nullptr;
        const TaintedFrameworkKnowledge::CompatibilityObservation* nonActivatable = nullptr;
        size_t exactMatchCount = 0;

        for (const auto& row : pack.m_compatibility)
        {
            if (row.m_branch != branch || row.m_runtimeTarget != runtime)
            {
                continue;
            }

            if (row.m_status == "blocked")
            {
                blocked = &row;
                continue;
            }

            if (row.m_status == "live_load_validated")
            {
                live = &row;
                if (row.m_gameVersion != gameVersion)
                {
                    continue;
                }

                exactGame = &row;
                if (row.m_bepInExVersion != bepInExVersion)
                {
                    continue;
                }

                exact = &row;
                ++exactMatchCount;
                continue;
            }

            nonActivatable = &row;
        }

        if (blocked)
        {
            result.m_status = ReadinessStatus::Blocked;
            result.m_evidencePath = blocked->m_evidencePath;
            result.m_blockers.push_back("upstream_branch_blocked");
            return result;
        }

        if (exactMatchCount > 1)
        {
            result.m_status = ReadinessStatus::Unsupported;
            result.m_evidencePath = exact->m_evidencePath;
            result.m_blockers.push_back("ambiguous_exact_compatibility_observations");
            return result;
        }

        if (exact)
        {
            result.m_status = ReadinessStatus::Ready;
            result.m_evidencePath = exact->m_evidencePath;
            return result;
        }

        if (exactGame)
        {
            result.m_status = ReadinessStatus::Unsupported;
            result.m_evidencePath = exactGame->m_evidencePath;
            result.m_blockers.push_back("exact_bepinex_version_not_evidence_backed");
            return result;
        }

        if (live)
        {
            result.m_status = ReadinessStatus::Unsupported;
            result.m_evidencePath = live->m_evidencePath;
            result.m_blockers.push_back("exact_game_version_not_evidence_backed");
            return result;
        }

        if (nonActivatable)
        {
            result.m_status = ReadinessStatus::Unsupported;
            result.m_evidencePath = nonActivatable->m_evidencePath;
            result.m_blockers.push_back("compatibility_observation_not_activatable");
            return result;
        }

        result.m_status = ReadinessStatus::Unsupported;
        result.m_blockers.push_back("branch_runtime_pair_not_in_canonical_knowledge");
        return result;
    }

    CompatibilityDecision Service::EvaluateCompatibility(
        const ExtensionAPI::ProfileView& profile) const
    {
        return EvaluateCompatibility(
            profile.m_gameVersion,
            profile.m_branch,
            profile.m_runtimeTarget,
            profile.m_bepInExVersion);
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
            decision.m_consumerReady =
                surface.m_consumerReady
                && surface.m_status == "candidate"
                && !surface.m_consumerBinding.empty()
                && surface.m_consumerBinding != "disabled";
            if (surface.m_status == "blocked")
            {
                decision.m_blockers.push_back("surface_status_blocked");
            }
            else if (!decision.m_consumerReady)
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
        const AZStd::string& runtime,
        const AZStd::string& bepInExVersion) const
    {
        const auto& pack = TaintedFrameworkKnowledge::GetCanonicalKnowledgePack();

        ActivationPlan plan;
        plan.m_planId = "tainted-framework-editor-plan:";
        plan.m_planId += pack.m_frameworkVersion;
        plan.m_planId += ":";
        plan.m_planId += branch;
        plan.m_planId += ":";
        plan.m_planId += runtime;
        plan.m_planId += ":";
        plan.m_planId += gameVersion;
        plan.m_planId += ":";
        plan.m_planId += bepInExVersion;
        plan.m_extensionId = "extension.tainted-framework";
        plan.m_compatibility = EvaluateCompatibility(
            gameVersion,
            branch,
            runtime,
            bepInExVersion);
        plan.m_surfaces = GetApiSurfaceDecisions();
        plan.m_configuration = GetConfigurationDefaults();
        plan.m_diagnostics = GetDiagnosticVocabulary();
        const bool hasConsumerReadySurface = AZStd::any_of(
            plan.m_surfaces.begin(),
            plan.m_surfaces.end(),
            [](const ApiSurfaceDecision& surface)
            {
                return surface.m_consumerReady;
            });
        plan.m_candidateEvidenceSubmissionEligible =
            plan.m_compatibility.m_status == ReadinessStatus::Ready
            && hasConsumerReadySurface;
        plan.m_runtimeInvocationAllowed = false;
        plan.m_fileWriteAllowed = false;
        plan.m_catalogMutationAllowed = false;
        return plan;
    }

    ActivationPlan Service::BuildActivationPlan(
        const ExtensionAPI::ProfileView& profile) const
    {
        return BuildActivationPlan(
            profile.m_gameVersion,
            profile.m_branch,
            profile.m_runtimeTarget,
            profile.m_bepInExVersion);
    }
} // namespace TaintedGrailModdingSDK::TaintedFrameworkEditorServices
