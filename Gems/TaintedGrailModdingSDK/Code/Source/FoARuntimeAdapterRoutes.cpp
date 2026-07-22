/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoARuntimeAdapterRoutes.h"

#include "CanonicalFingerprint.h"
#include "DeterministicContractJson.h"
#include "ResearchContractValidation.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::FoARuntimeAdapterRoutes
{
    namespace
    {
        constexpr const char* UpstreamCommit =
            "d7e740e7f167b73152b53409e483dab07d80d048";
        constexpr const char* IdentityEvidenceKind = "adapter-identity";
        constexpr const char* ParityEvidenceKind = "adapter-install-parity";

        void SetError(AZStd::string* error, AZStd::string message)
        {
            if (error)
            {
                *error = AZStd::move(message);
            }
        }

        void AddReason(
            QualificationResult& result,
            const AZStd::string& reason)
        {
            if (AZStd::find(
                    result.m_reasons.begin(), result.m_reasons.end(), reason)
                == result.m_reasons.end())
            {
                result.m_reasons.push_back(reason);
            }
        }

        bool IsBoundedText(
            const AZStd::string& value,
            size_t maximumLength,
            bool allowEmpty = false)
        {
            if (value.size() > maximumLength || (!allowEmpty && value.empty()))
            {
                return false;
            }
            return AZStd::all_of(
                value.begin(), value.end(),
                [](char character)
                {
                    const unsigned char byte = static_cast<unsigned char>(character);
                    return byte >= 0x20 && byte != 0x7f;
                });
        }

        bool HasDuplicates(AZStd::vector<AZStd::string> values)
        {
            AZStd::sort(values.begin(), values.end());
            return AZStd::adjacent_find(values.begin(), values.end()) != values.end();
        }

        bool IsGitCommit(const AZStd::string& value)
        {
            if (value.size() != 40)
            {
                return false;
            }
            return AZStd::all_of(
                value.begin(), value.end(),
                [](char character)
                {
                    return (character >= '0' && character <= '9')
                        || (character >= 'a' && character <= 'f');
                });
        }

        bool IsCanonicalRelativePosixPath(const AZStd::string& value)
        {
            if (!IsBoundedText(value, 1024)
                || value.front() == '/'
                || value.back() == '/'
                || value.find('\\') != AZStd::string::npos)
            {
                return false;
            }

            size_t componentStart = 0;
            while (componentStart < value.size())
            {
                const size_t separator = value.find('/', componentStart);
                const size_t componentEnd = separator == AZStd::string::npos
                    ? value.size()
                    : separator;
                const AZStd::string component =
                    value.substr(componentStart, componentEnd - componentStart);
                if (component.empty() || component == "." || component == "..")
                {
                    return false;
                }
                if (separator == AZStd::string::npos)
                {
                    break;
                }
                componentStart = separator + 1;
            }
            return true;
        }

        bool IsSafeSourceBinding(const SourceBinding& source)
        {
            return IsCanonicalRelativePosixPath(source.m_path)
                && IsGitCommit(source.m_gitBlobSha1);
        }

        bool IsValidEvidenceState(EvidenceState state)
        {
            switch (state)
            {
            case EvidenceState::ContractOnly:
            case EvidenceState::HostLiveLoadValidated:
            case EvidenceState::PackageInstallValidated:
                return true;
            default:
                return false;
            }
        }

        bool IsCapabilityId(const AZStd::string& capability)
        {
            return IsStableContractId(capability) && capability.size() <= 128;
        }

        AZStd::vector<RouteDescriptor> BuildRoutes()
        {
            RouteDescriptor mono;
            mono.m_adapterId = "adapter.foa.mono";
            mono.m_displayName = "FoA Mono Runtime Adapter Route";
            mono.m_contractVersion = "1.0.0";
            mono.m_kind = RouteKind::Mono;
            mono.m_evidenceState = EvidenceState::HostLiveLoadValidated;
            mono.m_frameworkVersion = "0.1.33";
            mono.m_gameVersion = "1.23.401";
            mono.m_branch = "mono";
            mono.m_runtimeTarget = "Mono";
            mono.m_unityVersion = "6000.0.64f1";
            mono.m_bepInExVersion = "5.4.23.3";
            mono.m_upstreamRepository =
                "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods";
            mono.m_upstreamCommit = UpstreamCommit;
            mono.m_licenseExpression = "NOASSERTION";
            mono.m_sources = {
                { "mods/tainted-framework/src/Tainted.Host.Mono/Plugin.cs",
                  "f73491e1f6766ff5d328f686ea339c24f1548151" },
                { "mods/tainted-framework/docs/gates/TF-Mono-0.1.33-live-load.md",
                  "38776cbde96f0d6737b666914486caa0484fb046" },
                { "mods/avalon-ai-runtime/src/AvalonAI.FoAHost.Mono.V2/"
                  "AvalonFoACompanionHostIntegration.cs",
                  "2be955101407286802a9ad7cf928b81157cf5a8d" },
                { "mods/avalon-ai-runtime/src/AvalonAI.Execution.FoA.Mono/"
                  "FoACompanionCatchUpContract.cs",
                  "bd97cd3f13b6c336a5dddb9b1891d65bfadbdd37" },
            };
            mono.m_provenCapabilities = {
                "framework.host-live-load",
                "framework.report-only-diagnostics",
            };
            mono.m_blockedCapabilities = {
                "adapter.build",
                "adapter.deploy",
                "adapter.execute",
                "game-api-access",
                "runtime-mutation",
                "save-access",
            };

            RouteDescriptor il2Cpp;
            il2Cpp.m_adapterId = "adapter.foa.il2cpp";
            il2Cpp.m_displayName = "FoA IL2CPP Runtime Adapter Route";
            il2Cpp.m_contractVersion = "1.0.0";
            il2Cpp.m_kind = RouteKind::Il2Cpp;
            il2Cpp.m_evidenceState = EvidenceState::PackageInstallValidated;
            il2Cpp.m_frameworkVersion = "0.1.36";
            il2Cpp.m_gameVersion = "1.23.401";
            il2Cpp.m_branch = "il2cpp";
            il2Cpp.m_runtimeTarget = "IL2CPP";
            il2Cpp.m_unityVersion = "6000.0.64f1";
            il2Cpp.m_bepInExVersion = "6.0.0-be.735";
            il2Cpp.m_upstreamRepository =
                "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods";
            il2Cpp.m_upstreamCommit = UpstreamCommit;
            il2Cpp.m_licenseExpression = "NOASSERTION";
            il2Cpp.m_sources = {
                { "mods/tainted-framework/src/Tainted.Host.IL2CPP/Plugin.cs",
                  "b34239da98387261bdda56cc02c590c710fc4b22" },
                { "mods/tainted-framework/docs/gates/"
                  "TF-IL2CPP-0.1.36-full-package-install.md",
                  "c97001a72687080e666c88fc726e834985c75d2f" },
                { "mods/tainted-framework/docs/gates/"
                  "TF-IL2CPP-0.1.36-release-decision.md",
                  "7fd4216f64166aab25e21f2855227e76abbd52e3" },
            };
            il2Cpp.m_provenCapabilities = {
                "framework.host-live-load",
                "framework.package-install-load",
                "framework.report-only-diagnostics",
            };
            il2Cpp.m_blockedCapabilities = {
                "adapter.build",
                "adapter.deploy",
                "adapter.execute",
                "feature-tested-gameplay",
                "game-api-access",
                "runtime-mutation",
                "save-access",
            };

            return { AZStd::move(mono), AZStd::move(il2Cpp) };
        }

        bool EvidenceMatchesRoute(
            const EvidenceRecord& evidence,
            const SourceRecord& source,
            const RouteDescriptor& route,
            const GameProfile& profile,
            const char* requiredKind)
        {
            return evidence.m_evidenceKind == requiredKind
                && evidence.m_subjectRef == route.m_adapterId
                && evidence.m_profileId == profile.m_profileId
                && evidence.m_gameVersion == profile.m_gameVersion
                && evidence.m_branch == profile.m_branch
                && source.m_sourceId == evidence.m_sourceId
                && source.m_fingerprint == evidence.m_sourceFingerprint
                && source.m_profileId == profile.m_profileId
                && source.m_gameVersion == profile.m_gameVersion
                && source.m_branch == profile.m_branch
                && source.m_runtimeTarget == profile.m_runtimeTarget;
        }

        void ValidateEvidenceSet(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const char* requiredKind,
            const char* missingReason,
            const char* invalidReason,
            const RouteDescriptor& route,
            const GameProfile& profile,
            const SourceEvidenceRegistry& registry,
            QualificationResult& result)
        {
            if (evidenceIds.empty())
            {
                AddReason(result, missingReason);
                return;
            }
            if (HasDuplicates(evidenceIds))
            {
                AddReason(result, "duplicate-evidence-id");
            }
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                if (!IsStableContractId(evidenceId))
                {
                    AddReason(result, "invalid-evidence-id");
                    continue;
                }
                const EvidenceRecord* evidence = registry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    AddReason(result, invalidReason);
                    continue;
                }
                const SourceRecord* source = registry.FindSource(evidence->m_sourceId);
                if (!source
                    || !EvidenceMatchesRoute(
                        *evidence, *source, route, profile, requiredKind))
                {
                    AddReason(result, invalidReason);
                }
            }
        }
    } // namespace

    const AZStd::vector<RouteDescriptor>& GetCanonicalRoutes()
    {
        static const AZStd::vector<RouteDescriptor> routes = BuildRoutes();
        return routes;
    }

    const RouteDescriptor* FindRoute(const AZStd::string& adapterId)
    {
        const auto& routes = GetCanonicalRoutes();
        const auto found = AZStd::find_if(
            routes.begin(), routes.end(),
            [&adapterId](const RouteDescriptor& route)
            {
                return route.m_adapterId == adapterId;
            });
        return found == routes.end() ? nullptr : &*found;
    }

    bool ValidateRoute(const RouteDescriptor& route, AZStd::string* error)
    {
        const bool kindMatches =
            (route.m_kind == RouteKind::Mono
                && route.m_branch == "mono" && route.m_runtimeTarget == "Mono")
            || (route.m_kind == RouteKind::Il2Cpp
                && route.m_branch == "il2cpp" && route.m_runtimeTarget == "IL2CPP");
        if (!IsStableContractId(route.m_adapterId)
            || !IsBoundedText(route.m_displayName, 256)
            || !IsStrictSemanticVersion(route.m_contractVersion)
            || !IsValidEvidenceState(route.m_evidenceState)
            || !IsStrictSemanticVersion(route.m_frameworkVersion)
            || !IsBoundedText(route.m_gameVersion, 128)
            || !kindMatches
            || !IsBoundedText(route.m_unityVersion, 128)
            || !IsBoundedText(route.m_bepInExVersion, 128)
            || !IsBoundedText(route.m_upstreamRepository, 512)
            || !IsGitCommit(route.m_upstreamCommit)
            || !IsBoundedText(route.m_licenseExpression, 128)
            || route.m_sources.empty()
            || route.m_provenCapabilities.empty()
            || route.m_blockedCapabilities.empty()
            || HasDuplicates(route.m_provenCapabilities)
            || HasDuplicates(route.m_blockedCapabilities)
            || route.m_buildAllowed || route.m_deploymentAllowed
            || route.m_executionAllowed || route.m_runtimeMutationAllowed
            || route.m_saveAccessAllowed)
        {
            SetError(error, "Runtime adapter route identity, target, evidence, or inert authority is invalid.");
            return false;
        }

        AZStd::vector<AZStd::string> sourcePaths;
        for (const SourceBinding& source : route.m_sources)
        {
            sourcePaths.push_back(source.m_path);
            if (!IsSafeSourceBinding(source))
            {
                SetError(error, "Runtime adapter source binding is invalid.");
                return false;
            }
        }
        if (HasDuplicates(sourcePaths))
        {
            SetError(error, "Runtime adapter source paths must be unique.");
            return false;
        }
        for (const AZStd::string& capability : route.m_provenCapabilities)
        {
            if (!IsCapabilityId(capability)
                || AZStd::find(
                    route.m_blockedCapabilities.begin(),
                    route.m_blockedCapabilities.end(), capability)
                != route.m_blockedCapabilities.end())
            {
                SetError(error, "Runtime adapter capability cannot be both proven and blocked.");
                return false;
            }
        }
        for (const AZStd::string& capability : route.m_blockedCapabilities)
        {
            if (!IsCapabilityId(capability))
            {
                SetError(error, "Runtime adapter blocked capability ID is invalid.");
                return false;
            }
        }
        if (error)
        {
            error->clear();
        }
        return true;
    }

    QualificationResult Qualify(const QualificationRequest& request)
    {
        QualificationResult result;
        result.m_adapterId = request.m_adapterId;
        const RouteDescriptor* route = FindRoute(request.m_adapterId);
        if (route && ValidateRoute(*route))
        {
            result.m_routeFingerprint = CalculateRouteFingerprint(*route);
        }
        result.m_reasons.push_back("active-evidence-registry-required");
        return result;
    }

    QualificationResult Qualify(
        const QualificationRequest& request,
        const SourceEvidenceRegistry& evidenceRegistry)
    {
        QualificationResult result;
        result.m_adapterId = request.m_adapterId;
        const RouteDescriptor* route = FindRoute(request.m_adapterId);
        if (!route || !ValidateRoute(*route))
        {
            result.m_reasons.push_back("unknown-or-invalid-adapter-route");
            return result;
        }
        result.m_routeFingerprint = CalculateRouteFingerprint(*route);
        if (!request.m_profile.IsConfigured())
        {
            result.m_reasons.push_back("configured-exact-profile-required");
            return result;
        }
        result.m_exactProfileMatch =
            request.m_profile.m_gameVersion == route->m_gameVersion
            && request.m_profile.m_branch == route->m_branch
            && request.m_profile.m_runtimeTarget == route->m_runtimeTarget
            && request.m_profile.m_unityVersion == route->m_unityVersion
            && request.m_profile.m_bepInExVersion == route->m_bepInExVersion;
        if (!result.m_exactProfileMatch)
        {
            AddReason(result, "exact-profile-mismatch");
        }

        ValidateEvidenceSet(
            request.m_identityEvidenceIds,
            IdentityEvidenceKind,
            "identity-evidence-required",
            "identity-evidence-not-active-or-mismatched",
            *route,
            request.m_profile,
            evidenceRegistry,
            result);
        ValidateEvidenceSet(
            request.m_parityEvidenceIds,
            ParityEvidenceKind,
            "exact-install-parity-evidence-required",
            "parity-evidence-not-active-or-mismatched",
            *route,
            request.m_profile,
            evidenceRegistry,
            result);

        for (const AZStd::string& evidenceId : request.m_identityEvidenceIds)
        {
            if (AZStd::find(
                    request.m_parityEvidenceIds.begin(),
                    request.m_parityEvidenceIds.end(), evidenceId)
                != request.m_parityEvidenceIds.end())
            {
                AddReason(result, "evidence-id-cannot-satisfy-multiple-classes");
            }
        }
        if (request.m_requestBuild || request.m_requestDeployment
            || request.m_requestExecution || request.m_requestRuntimeMutation
            || request.m_requestSaveAccess)
        {
            AddReason(result, "requested-authority-not-granted-by-route-import");
        }
        AZStd::sort(result.m_reasons.begin(), result.m_reasons.end());
        result.m_planningCompatible = result.m_reasons.empty();
        return result;
    }

    AZStd::string BuildCanonicalRoute(const RouteDescriptor& route)
    {
        using namespace DeterministicContractJson;

        AZStd::string output = "{";
        AppendString(output, "schema", "foa-runtime-adapter-route-v1");
        AppendString(output, "adapter_id", route.m_adapterId);
        AppendString(output, "display_name", route.m_displayName);
        AppendString(output, "contract_version", route.m_contractVersion);
        AppendSigned(output, "kind", static_cast<AZ::s64>(route.m_kind));
        AppendSigned(
            output,
            "evidence_state",
            static_cast<AZ::s64>(route.m_evidenceState));
        AppendString(output, "framework_version", route.m_frameworkVersion);
        AppendString(output, "game_version", route.m_gameVersion);
        AppendString(output, "branch", route.m_branch);
        AppendString(output, "runtime_target", route.m_runtimeTarget);
        AppendString(output, "unity_version", route.m_unityVersion);
        AppendString(output, "bepinex_version", route.m_bepInExVersion);
        AppendString(output, "upstream_repository", route.m_upstreamRepository);
        AppendString(output, "upstream_commit", route.m_upstreamCommit);
        AppendString(output, "license_expression", route.m_licenseExpression);

        AZStd::vector<SourceBinding> sources = route.m_sources;
        AZStd::sort(
            sources.begin(), sources.end(),
            [](const SourceBinding& left, const SourceBinding& right)
            {
                return left.m_path < right.m_path;
            });
        AppendName(output, "sources");
        output.push_back('[');
        for (size_t index = 0; index < sources.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            output.push_back('{');
            AppendString(output, "path", sources[index].m_path);
            AppendString(
                output,
                "git_blob_sha1",
                sources[index].m_gitBlobSha1,
                false);
            output.push_back('}');
        }
        output += "],";
        AppendSortedStringArray(
            output, "proven_capabilities", route.m_provenCapabilities);
        AppendSortedStringArray(
            output, "blocked_capabilities", route.m_blockedCapabilities);
        AppendName(output, "authority");
        output.push_back('{');
        AppendBool(output, "build", route.m_buildAllowed);
        AppendBool(output, "deployment", route.m_deploymentAllowed);
        AppendBool(output, "execution", route.m_executionAllowed);
        AppendBool(output, "runtime_mutation", route.m_runtimeMutationAllowed);
        AppendBool(output, "save_access", route.m_saveAccessAllowed, false);
        output.push_back('}');
        output.push_back('}');
        return output;
    }

    AZStd::string CalculateRouteFingerprint(const RouteDescriptor& route)
    {
        return CalculateCanonicalSha256(BuildCanonicalRoute(route));
    }
} // namespace TaintedGrailModdingSDK::FoARuntimeAdapterRoutes
