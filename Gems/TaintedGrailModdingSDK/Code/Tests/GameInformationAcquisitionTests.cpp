/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <AzTest/AzTest.h>

#include <AzCore/std/utility/move.h>

#include "GameInformationAcquisition.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        GameProfile MakeAcquisitionProfile()
        {
            GameProfile profile;
            profile.m_profileId = "profile.foa.mono";
            profile.m_displayName = "FoA Mono";
            profile.m_gameVersion = "1.23.401";
            profile.m_branch = "mono";
            profile.m_runtimeTarget = "Mono";
            profile.m_unityVersion = "6000.0.64f1";
            profile.m_bepInExVersion = "5.4.23.3";
            profile.m_installPath = "C:/fixture/game";
            profile.m_managedAssembliesPath = "C:/fixture/game/Managed";
            profile.m_pluginPath = "C:/fixture/game/BepInEx/plugins";
            profile.m_diagnosticsPath = "C:/fixture/workspace/diagnostics";
            profile.m_extractedDataPath = "C:/fixture/workspace/extracted";
            return profile;
        }

        GameInformationAcquisition::CandidateObservation MakeObservation(
            AZStd::string observationId,
            char valueDigit = '2')
        {
            GameInformationAcquisition::CandidateObservation observation;
            observation.m_observationId = AZStd::move(observationId);
            observation.m_providerId = "provider.tainted-grail-github";
            observation.m_providerVersion = "1.0.0";
            observation.m_sourceId = "source.tg.github.d7e740e7";
            observation.m_sourceFingerprint = "sha256:" + AZStd::string(64, '1');
            observation.m_sourceRevision =
                "d7e740e7f167b73152b53409e483dab07d80d048";
            observation.m_profileId = "profile.foa.mono";
            observation.m_gameVersion = "1.23.401";
            observation.m_branch = "mono";
            observation.m_runtimeTarget = "Mono";
            observation.m_subjectRef = "framework:tainted";
            observation.m_claim = "Tainted Framework exact runtime observation";
            observation.m_valueFingerprint =
                "sha256:" + AZStd::string(64, valueDigit);
            observation.m_evidenceKind = "pinned-source-observation";
            observation.m_confidence = "reviewed";
            observation.m_locator = "github:theb0yys/tainted-grail";
            observation.m_recordPath = "mods/tainted-framework/docs/compatibility.md";
            observation.m_capturedAtUtc = "2026-07-21T20:00:00Z";
            return observation;
        }
    } // namespace

    TEST(GameInformationAcquisitionTests, CanonicalProvidersPreserveOrderAndAuthorityBoundary)
    {
        const auto& providers = GameInformationAcquisition::GetCanonicalProviders();
        ASSERT_EQ(providers.size(), 3);
        EXPECT_EQ(providers[0].m_kind, GameInformationAcquisition::ProviderKind::LocalInstall);
        EXPECT_EQ(providers[1].m_kind, GameInformationAcquisition::ProviderKind::PinnedGitHub);
        EXPECT_EQ(providers[2].m_kind, GameInformationAcquisition::ProviderKind::MerlinWorkshop);
        EXPECT_TRUE(providers[2].m_optional);
        for (const auto& provider : providers)
        {
            EXPECT_FALSE(provider.m_autoPromotionAllowed);
            EXPECT_TRUE(GameInformationAcquisition::ValidateProvider(provider));
        }
    }

    TEST(GameInformationAcquisitionTests, ContractOnlyLocalAndMerlinProvidersCannotPublishObservations)
    {
        auto observation = MakeObservation("evidence.provider.contract-only");
        observation.m_providerId = "provider.foa-local-capture";
        observation.m_sourceRevision.clear();
        EXPECT_FALSE(GameInformationAcquisition::ValidateObservation(
            observation, MakeAcquisitionProfile()));

        observation.m_providerId = "provider.merlin-workshop";
        EXPECT_FALSE(GameInformationAcquisition::ValidateObservation(
            observation, MakeAcquisitionProfile()));
    }

    TEST(GameInformationAcquisitionTests, PinnedGitHubObservationBuildsCandidateEvidence)
    {
        const auto observation = MakeObservation("evidence.provider.github");
        EvidenceRecord evidence;
        AZStd::string error;
        ASSERT_TRUE(GameInformationAcquisition::BuildEvidenceCandidate(
            observation, MakeAcquisitionProfile(), evidence, &error)) << error.c_str();
        EXPECT_EQ(evidence.m_evidenceId, observation.m_observationId);
        EXPECT_EQ(evidence.m_sourceFingerprint, observation.m_sourceFingerprint);
        EXPECT_EQ(evidence.m_subjectRef, observation.m_subjectRef);
    }

    TEST(GameInformationAcquisitionTests, ReconciliationSurfacesConflictsWithoutOverriding)
    {
        auto first = MakeObservation("evidence.provider.github.first", '2');
        auto second = MakeObservation("evidence.provider.github.second", '3');
        const auto result = GameInformationAcquisition::Reconcile(
            { second, first }, MakeAcquisitionProfile());
        ASSERT_EQ(result.m_candidates.size(), 2);
        ASSERT_EQ(result.m_conflicts.size(), 1);
        EXPECT_EQ(result.m_conflicts[0].m_subjectRef, "framework:tainted");
        EXPECT_FALSE(result.m_canPromoteAutomatically);
        EXPECT_FALSE(result.m_grantsRuntimePermission);
    }

    TEST(GameInformationAcquisitionTests, ObservationAuthorityEscalationFailsClosed)
    {
        auto observation = MakeObservation("evidence.provider.github.escalation");
        observation.m_promoteAutomatically = true;
        EXPECT_FALSE(GameInformationAcquisition::ValidateObservation(
            observation, MakeAcquisitionProfile()));
        observation.m_promoteAutomatically = false;
        observation.m_grantsRuntimePermission = true;
        EXPECT_FALSE(GameInformationAcquisition::ValidateObservation(
            observation, MakeAcquisitionProfile()));
    }
} // namespace TaintedGrailModdingSDK
