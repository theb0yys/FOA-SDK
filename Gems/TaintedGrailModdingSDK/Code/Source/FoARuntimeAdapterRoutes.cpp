/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoARuntimeAdapterRoutes.h"

#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::FoARuntimeAdapterRoutes
{
    namespace
    {
        constexpr const char* UpstreamCommit =
            "d7e740e7f167b73152b53409e483dab07d80d048";

        void SetError(AZStd::string* error, AZStd::string message)
        {
            if (error)
            {
                *error = AZStd::move(message);
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
            for (char character : value)
            {
                if (!((character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f')))
                {
                    return false;
                }
            }
            return true;
        }

        bool IsSafeSourceBinding(const SourceBinding& source)
        {
            return IsBoundedText(source.m_path, 1024)
                && source.m_path.front() != '/'
                && source.m_path.find('\\') == AZStd::string::npos
                && source.m_path.find("../") == AZStd::string::npos
                && source.m_path.find("/..") == AZStd::string::npos
                && IsGitCommit(source.m_gitBlobSha1);
        }

        bool IsCapabilityId(const AZStd::string& capability)
        {
            return IsBoundedText(capability, 128)
                && AZStd::all_of(
                    capability.begin(), capability.end(),
                    [](char character)
                    {
                        return (character >= 'a' && character <= 'z')
                            || (character >= '0' && character <= '9')
                            || character == '.' || character == '-';
                    });
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

        void AppendString(AZStd::string& output, const AZStd::string& value)
        {
            output += AZStd::to_string(value.size());
            output.push_back(':');
            output += value;
            output.push_back('|');
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
            result.m_reasons.push_back("exact-profile-mismatch");
        }
        if (request.m_identityEvidenceIds.empty())
        {
            result.m_reasons.push_back("identity-evidence-required");
        }
        if (request.m_parityEvidenceIds.empty())
        {
            result.m_reasons.push_back("exact-install-parity-evidence-required");
        }
        if (HasDuplicates(request.m_identityEvidenceIds)
            || HasDuplicates(request.m_parityEvidenceIds))
        {
            result.m_reasons.push_back("duplicate-evidence-id");
        }
        if (AZStd::any_of(
                request.m_identityEvidenceIds.begin(),
                request.m_identityEvidenceIds.end(),
                [](const AZStd::string& evidenceId)
                {
                    return !IsStableContractId(evidenceId);
                })
            || AZStd::any_of(
                request.m_parityEvidenceIds.begin(),
                request.m_parityEvidenceIds.end(),
                [](const AZStd::string& evidenceId)
                {
                    return !IsStableContractId(evidenceId);
                }))
        {
            result.m_reasons.push_back("invalid-evidence-id");
        }
        if (request.m_requestBuild || request.m_requestDeployment
            || request.m_requestExecution || request.m_requestRuntimeMutation
            || request.m_requestSaveAccess)
        {
            result.m_reasons.push_back("requested-authority-not-granted-by-route-import");
        }
        result.m_planningCompatible = result.m_reasons.empty();
        return result;
    }

    AZStd::string BuildCanonicalRoute(const RouteDescriptor& route)
    {
        AZStd::string output = "foa-runtime-adapter-route-v1|";
        AppendString(output, route.m_adapterId);
        AppendString(output, route.m_displayName);
        AppendString(output, route.m_contractVersion);
        output += AZStd::to_string(static_cast<int>(route.m_kind));
        output.push_back('|');
        output += AZStd::to_string(static_cast<int>(route.m_evidenceState));
        output.push_back('|');
        AppendString(output, route.m_frameworkVersion);
        AppendString(output, route.m_gameVersion);
        AppendString(output, route.m_branch);
        AppendString(output, route.m_runtimeTarget);
        AppendString(output, route.m_unityVersion);
        AppendString(output, route.m_bepInExVersion);
        AppendString(output, route.m_upstreamRepository);
        AppendString(output, route.m_upstreamCommit);
        AppendString(output, route.m_licenseExpression);
        AZStd::vector<SourceBinding> sources = route.m_sources;
        AZStd::sort(
            sources.begin(), sources.end(),
            [](const SourceBinding& left, const SourceBinding& right)
            {
                return left.m_path < right.m_path;
            });
        for (const SourceBinding& source : sources)
        {
            AppendString(output, source.m_path);
            AppendString(output, source.m_gitBlobSha1);
        }
        AZStd::vector<AZStd::string> proven = route.m_provenCapabilities;
        AZStd::vector<AZStd::string> blocked = route.m_blockedCapabilities;
        AZStd::sort(proven.begin(), proven.end());
        AZStd::sort(blocked.begin(), blocked.end());
        for (const AZStd::string& capability : proven)
        {
            AppendString(output, "proven:" + capability);
        }
        for (const AZStd::string& capability : blocked)
        {
            AppendString(output, "blocked:" + capability);
        }
        output += "build=false|deploy=false|execute=false|mutation=false|save=false|";
        return output;
    }

    AZStd::string CalculateRouteFingerprint(const RouteDescriptor& route)
    {
        return CalculateCanonicalSha256(BuildCanonicalRoute(route));
    }
} // namespace TaintedGrailModdingSDK::FoARuntimeAdapterRoutes
