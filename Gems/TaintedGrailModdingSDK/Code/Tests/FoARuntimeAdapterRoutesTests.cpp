/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>

#include "FoARuntimeAdapterRoutes.h"
#include "SourceEvidenceRegistry.h"

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

        SourceRecord MakeSource(bool il2Cpp)
        {
            SourceRecord source;
            source.m_sourceId = il2Cpp
                ? "source.adapter.il2cpp"
                : "source.adapter.mono";
            source.m_title = "Runtime adapter qualification fixture";
            source.m_sourceKind = "compiled_fixture";
            source.m_locator = "fixture://runtime-adapter";
            source.m_fingerprint = "sha256:" + AZStd::string(64, il2Cpp ? '2' : '1');
            source.m_profileId = MakeRouteProfile(il2Cpp).m_profileId;
            source.m_gameVersion = "1.23.401";
            source.m_branch = il2Cpp ? "il2cpp" : "mono";
            source.m_runtimeTarget = il2Cpp ? "IL2CPP" : "Mono";
            source.m_toolName = "fixture";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "importer.adapter.fixture";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-21T12:00:00Z";
            source.m_importedAt = "2026-07-21T12:05:00Z";
            source.m_mediaType = "application/json";
            source.m_importStatus = "imported";
            return source;
        }

        EvidenceRecord MakeRouteEvidence(
            bool il2Cpp,
            AZStd::string evidenceId,
            AZStd::string kind)
        {
            const SourceRecord source = MakeSource(il2Cpp);
            EvidenceRecord evidence;
            evidence.m_evidenceId = AZStd::move(evidenceId);
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = il2Cpp ? "adapter.foa.il2cpp" : "adapter.foa.mono";
            evidence.m_claim = "The exact runtime adapter identity or install parity was reviewed.";
            evidence.m_evidenceKind = AZStd::move(kind);
            evidence.m_confidence = "reviewed";
            evidence.m_locator = "fixture://runtime-adapter#evidence";
            evidence.m_recordPath = "records[0]";
            evidence.m_extractedAt = "2026-07-21T12:03:00Z";
            return evidence;
        }

        SourceEvidenceRegistry MakeRegistry(bool il2Cpp)
        {
            SourceEvidenceRegistry registry;
            EXPECT_TRUE(registry.RegisterSource(MakeSource(il2Cpp)));
            EXPECT_TRUE(registry.RegisterEvidence(MakeRouteEvidence(
                il2Cpp,
                "evidence.adapter.identity",
                "adapter-identity")));
            EXPECT_TRUE(registry.RegisterEvidence(MakeRouteEvidence(
                il2Cpp,
                "evidence.adapter.parity",
                "adapter-install-parity")));
            return registry;
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

    TEST(FoARuntimeAdapterRoutesTests, ExactProfilesRequireActiveTypedEvidenceForPlanning)
    {
        const SourceEvidenceRegistry monoRegistry = MakeRegistry(false);
        const auto mono = FoARuntimeAdapterRoutes::Qualify(
            MakeRequest(false), monoRegistry);
        EXPECT_TRUE(mono.m_exactProfileMatch);
        EXPECT_TRUE(mono.m_planningCompatible);
        EXPECT_FALSE(mono.m_executionAllowed);

        const SourceEvidenceRegistry il2CppRegistry = MakeRegistry(true);
        const auto il2Cpp = FoARuntimeAdapterRoutes::Qualify(
            MakeRequest(true), il2CppRegistry);
        EXPECT_TRUE(il2Cpp.m_exactProfileMatch);
        EXPECT_TRUE(il2Cpp.m_planningCompatible);
        EXPECT_FALSE(il2Cpp.m_executionAllowed);
    }

    TEST(FoARuntimeAdapterRoutesTests, RegistryFreeQualificationFailsClosed)
    {
        const auto result = FoARuntimeAdapterRoutes::Qualify(MakeRequest(false));
        EXPECT_FALSE(result.m_planningCompatible);
        ASSERT_EQ(result.m_reasons.size(), 1);
        EXPECT_EQ(result.m_reasons[0], "active-evidence-registry-required");
    }

    TEST(FoARuntimeAdapterRoutesTests, FabricatedOrPendingEvidenceCannotQualify)
    {
        SourceEvidenceRegistry registry;
        ASSERT_TRUE(registry.RegisterSource(MakeSource(false)));
        ASSERT_TRUE(registry.RegisterCandidateEvidence(MakeRouteEvidence(
            false,
            "evidence.adapter.identity",
            "adapter-identity")));
        ASSERT_TRUE(registry.RegisterCandidateEvidence(MakeRouteEvidence(
            false,
            "evidence.adapter.parity",
            "adapter-install-parity")));

        const auto pending = FoARuntimeAdapterRoutes::Qualify(
            MakeRequest(false), registry);
        EXPECT_FALSE(pending.m_planningCompatible);

        auto fabricated = MakeRequest(false);
        fabricated.m_identityEvidenceIds = { "evidence.adapter.fabricated" };
        const auto missing = FoARuntimeAdapterRoutes::Qualify(fabricated, registry);
        EXPECT_FALSE(missing.m_planningCompatible);
    }

    TEST(FoARuntimeAdapterRoutesTests, OneEvidenceRecordCannotSatisfyBothClasses)
    {
        SourceEvidenceRegistry registry = MakeRegistry(false);
        auto request = MakeRequest(false);
        request.m_parityEvidenceIds = request.m_identityEvidenceIds;
        const auto result = FoARuntimeAdapterRoutes::Qualify(request, registry);
        EXPECT_FALSE(result.m_planningCompatible);
        EXPECT_NE(
            AZStd::find(
                result.m_reasons.begin(),
                result.m_reasons.end(),
                "evidence-id-cannot-satisfy-multiple-classes"),
            result.m_reasons.end());
    }

    TEST(FoARuntimeAdapterRoutesTests, CrossBranchQualificationFailsClosed)
    {
        auto request = MakeRequest(false);
        request.m_profile = MakeRouteProfile(true);
        const SourceEvidenceRegistry registry = MakeRegistry(false);
        const auto result = FoARuntimeAdapterRoutes::Qualify(request, registry);
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
        const SourceEvidenceRegistry registry = MakeRegistry(true);
        const auto result = FoARuntimeAdapterRoutes::Qualify(request, registry);
        EXPECT_FALSE(result.m_planningCompatible);
        EXPECT_FALSE(result.m_buildAllowed);
        EXPECT_FALSE(result.m_executionAllowed);
        EXPECT_FALSE(result.m_saveAccessAllowed);
    }

    TEST(FoARuntimeAdapterRoutesTests, CanonicalFingerprintBindsActualAuthorityFields)
    {
        auto inert = *FoARuntimeAdapterRoutes::FindRoute("adapter.foa.mono");
        auto elevated = inert;
        elevated.m_buildAllowed = true;
        EXPECT_NE(
            FoARuntimeAdapterRoutes::CalculateRouteFingerprint(inert),
            FoARuntimeAdapterRoutes::CalculateRouteFingerprint(elevated));
        EXPECT_FALSE(FoARuntimeAdapterRoutes::ValidateRoute(elevated));
    }

    TEST(FoARuntimeAdapterRoutesTests, InvalidSourceAndEvidenceBindingsFailClosed)
    {
        auto route = *FoARuntimeAdapterRoutes::FindRoute("adapter.foa.mono");
        route.m_sources[0].m_path = "../outside/Plugin.cs";
        EXPECT_FALSE(FoARuntimeAdapterRoutes::ValidateRoute(route));

        auto request = MakeRequest(false);
        request.m_identityEvidenceIds = { "invalid evidence id" };
        const SourceEvidenceRegistry registry = MakeRegistry(false);
        const auto result = FoARuntimeAdapterRoutes::Qualify(request, registry);
        EXPECT_FALSE(result.m_planningCompatible);
    }
} // namespace TaintedGrailModdingSDK
