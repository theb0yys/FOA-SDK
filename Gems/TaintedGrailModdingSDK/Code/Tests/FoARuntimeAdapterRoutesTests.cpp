/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include "FoARuntimeAdapterRoutes.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        GameProfile MakeRouteProfile(bool il2Cpp)
        {
            GameProfile profile;
            profile.m_profileId = il2Cpp ? "profile.foa.il2cpp" : "profile.foa.mono";
            profile.m_displayName = il2Cpp ? "FoA IL2CPP" : "FoA Mono";
            profile.m_gameVersion = "1.23.401";
            profile.m_branch = il2Cpp ? "il2cpp" : "mono";
            profile.m_runtimeTarget = il2Cpp ? "IL2CPP" : "Mono";
            profile.m_unityVersion = "6000.0.64f1";
            profile.m_bepInExVersion = il2Cpp ? "6.0.0-be.735" : "5.4.23.3";
            profile.m_installPath = "C:/fixture/game";
            profile.m_managedAssembliesPath = "C:/fixture/game/Managed";
            profile.m_pluginPath = il2Cpp
                ? AZStd::string{}
                : AZStd::string("C:/fixture/game/BepInEx/plugins");
            profile.m_diagnosticsPath = "C:/fixture/workspace/diagnostics";
            profile.m_extractedDataPath = "C:/fixture/workspace/extracted";
            return profile;
        }

        FoARuntimeAdapterRoutes::QualificationRequest MakeRequest(bool il2Cpp)
        {
            FoARuntimeAdapterRoutes::QualificationRequest request;
            request.m_adapterId = il2Cpp ? "adapter.foa.il2cpp" : "adapter.foa.mono";
            request.m_profile = MakeRouteProfile(il2Cpp);
            request.m_identityEvidenceIds = { "evidence.adapter.identity" };
            request.m_parityEvidenceIds = { "evidence.adapter.parity" };
            return request;
        }
    } // namespace

    TEST(FoARuntimeAdapterRoutesTests, MonoAndIl2CppRoutesAreSeparateAndInert)
    {
        const auto& routes = FoARuntimeAdapterRoutes::GetCanonicalRoutes();
        ASSERT_EQ(routes.size(), 2);
        EXPECT_EQ(routes[0].m_adapterId, "adapter.foa.mono");
        EXPECT_EQ(routes[0].m_frameworkVersion, "0.1.33");
        EXPECT_EQ(routes[1].m_adapterId, "adapter.foa.il2cpp");
        EXPECT_EQ(routes[1].m_frameworkVersion, "0.1.36");
        for (const auto& route : routes)
        {
            EXPECT_TRUE(FoARuntimeAdapterRoutes::ValidateRoute(route));
            EXPECT_FALSE(route.m_buildAllowed);
            EXPECT_FALSE(route.m_deploymentAllowed);
            EXPECT_FALSE(route.m_executionAllowed);
            EXPECT_FALSE(route.m_runtimeMutationAllowed);
            EXPECT_FALSE(route.m_saveAccessAllowed);
        }
    }

    TEST(FoARuntimeAdapterRoutesTests, ExactProfilesCanQualifyForPlanningOnly)
    {
        const auto mono = FoARuntimeAdapterRoutes::Qualify(MakeRequest(false));
        EXPECT_TRUE(mono.m_exactProfileMatch);
        EXPECT_TRUE(mono.m_planningCompatible);
        EXPECT_FALSE(mono.m_executionAllowed);

        const auto il2Cpp = FoARuntimeAdapterRoutes::Qualify(MakeRequest(true));
        EXPECT_TRUE(il2Cpp.m_exactProfileMatch);
        EXPECT_TRUE(il2Cpp.m_planningCompatible);
        EXPECT_FALSE(il2Cpp.m_executionAllowed);
    }

    TEST(FoARuntimeAdapterRoutesTests, CrossBranchQualificationFailsClosed)
    {
        auto request = MakeRequest(false);
        request.m_profile = MakeRouteProfile(true);
        const auto result = FoARuntimeAdapterRoutes::Qualify(request);
        EXPECT_FALSE(result.m_exactProfileMatch);
        EXPECT_FALSE(result.m_planningCompatible);
    }

    TEST(FoARuntimeAdapterRoutesTests, AuthorityRequestIsNotGrantedByImportedEvidence)
    {
        auto request = MakeRequest(true);
        request.m_requestBuild = true;
        request.m_requestDeployment = true;
        request.m_requestExecution = true;
        request.m_requestRuntimeMutation = true;
        request.m_requestSaveAccess = true;
        const auto result = FoARuntimeAdapterRoutes::Qualify(request);
        EXPECT_FALSE(result.m_planningCompatible);
        EXPECT_FALSE(result.m_buildAllowed);
        EXPECT_FALSE(result.m_executionAllowed);
        EXPECT_FALSE(result.m_saveAccessAllowed);
    }

    TEST(FoARuntimeAdapterRoutesTests, InvalidSourceAndEvidenceBindingsFailClosed)
    {
        auto route = *FoARuntimeAdapterRoutes::FindRoute("adapter.foa.mono");
        route.m_sources[0].m_path = "../outside/Plugin.cs";
        EXPECT_FALSE(FoARuntimeAdapterRoutes::ValidateRoute(route));

        auto request = MakeRequest(false);
        request.m_identityEvidenceIds = { "invalid evidence id" };
        const auto result = FoARuntimeAdapterRoutes::Qualify(request);
        EXPECT_FALSE(result.m_planningCompatible);
    }
} // namespace TaintedGrailModdingSDK
