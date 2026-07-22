/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::FoARuntimeAdapterRoutes
{
    enum class RouteKind
    {
        Mono,
        Il2Cpp,
    };

    enum class EvidenceState
    {
        ContractOnly,
        HostLiveLoadValidated,
        PackageInstallValidated,
    };

    struct SourceBinding
    {
        AZStd::string m_path;
        AZStd::string m_gitBlobSha1;
    };

    struct RouteDescriptor
    {
        AZStd::string m_adapterId;
        AZStd::string m_displayName;
        AZStd::string m_contractVersion;
        RouteKind m_kind = RouteKind::Mono;
        EvidenceState m_evidenceState = EvidenceState::ContractOnly;
        AZStd::string m_frameworkVersion;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_unityVersion;
        AZStd::string m_bepInExVersion;
        AZStd::string m_upstreamRepository;
        AZStd::string m_upstreamCommit;
        AZStd::string m_licenseExpression;
        AZStd::vector<SourceBinding> m_sources;
        AZStd::vector<AZStd::string> m_provenCapabilities;
        AZStd::vector<AZStd::string> m_blockedCapabilities;
        bool m_buildAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_executionAllowed = false;
        bool m_runtimeMutationAllowed = false;
        bool m_saveAccessAllowed = false;
    };

    struct QualificationRequest
    {
        AZStd::string m_adapterId;
        GameProfile m_profile;
        AZStd::vector<AZStd::string> m_identityEvidenceIds;
        AZStd::vector<AZStd::string> m_parityEvidenceIds;
        bool m_requestBuild = false;
        bool m_requestDeployment = false;
        bool m_requestExecution = false;
        bool m_requestRuntimeMutation = false;
        bool m_requestSaveAccess = false;
    };

    struct QualificationResult
    {
        AZStd::string m_adapterId;
        bool m_exactProfileMatch = false;
        bool m_planningCompatible = false;
        AZStd::vector<AZStd::string> m_reasons;
        AZStd::string m_routeFingerprint;
        bool m_buildAllowed = false;
        bool m_deploymentAllowed = false;
        bool m_executionAllowed = false;
        bool m_runtimeMutationAllowed = false;
        bool m_saveAccessAllowed = false;
    };

    const AZStd::vector<RouteDescriptor>& GetCanonicalRoutes();
    const RouteDescriptor* FindRoute(const AZStd::string& adapterId);

    bool ValidateRoute(
        const RouteDescriptor& route,
        AZStd::string* error = nullptr);

    QualificationResult Qualify(const QualificationRequest& request);

    AZStd::string BuildCanonicalRoute(const RouteDescriptor& route);
    AZStd::string CalculateRouteFingerprint(const RouteDescriptor& route);
} // namespace TaintedGrailModdingSDK::FoARuntimeAdapterRoutes
