/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorAppearanceBindingService.h"
#include "CatalogDatabase.h"
#include "SourceEvidenceRegistry.h"

#include <AzTest/AzTest.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        constexpr const char* Fingerprint =
            "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

        GameProfile MakeProfile()
        {
            GameProfile profile;
            profile.m_profileId = "foa.stage8.test";
            profile.m_gameVersion = "1.0.0";
            profile.m_branch = "test";
            profile.m_runtimeTarget = "Mono";
            return profile;
        }

        SourceEvidenceRegistry MakeRegistry()
        {
            SourceEvidenceRegistry registry;
            SourceRecord source;
            source.m_sourceId = "source.stage8.preview";
            source.m_title = "Stage 8 synthetic preview";
            source.m_sourceKind = "synthetic-preview";
            source.m_locator = "$fixture/stage8-preview.json";
            source.m_fingerprint = Fingerprint;
            source.m_profileId = "foa.stage8.test";
            source.m_gameVersion = "1.0.0";
            source.m_branch = "test";
            source.m_runtimeTarget = "Mono";
            source.m_toolName = "stage8-test";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "foa.stage8.test-importer";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-08-05T00:00:00Z";
            source.m_importedAt = "2026-08-05T00:00:02Z";
            source.m_limitations = "Synthetic test only.";
            source.m_mediaType = "application/json";
            source.m_byteSize = 1;
            source.m_importStatus = "imported";
            AZStd::string error;
            EXPECT_TRUE(registry.RegisterSource(source, &error)) << error.c_str();

            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence.stage8.preview-product";
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = "visual.asset.stage8.actor-model";
            evidence.m_claim = "Synthetic O3DE preview product exists.";
            evidence.m_evidenceKind = "o3de-product";
            evidence.m_confidence = "documented";
            evidence.m_locator = "$fixture/stage8-preview.json";
            evidence.m_recordPath = "/PaneEntries/0";
            evidence.m_extractedAt = "2026-08-05T00:00:01Z";
            EXPECT_TRUE(registry.RegisterEvidence(evidence, &error)) << error.c_str();
            return registry;
        }

        CatalogDatabase MakeCatalog()
        {
            CatalogDatabase catalog;
            CatalogRecord actor;
            actor.m_recordId = "actor.stage8.binding";
            actor.m_ownerPackId = "pack.stage8.tests";
            actor.m_domain = "population";
            actor.m_recordKind = "actor";
            actor.m_subjectRef = "subject:actor:stage8-binding";
            actor.m_identityKind = "synthetic";
            actor.m_displayName = "Stage 8 Binding Actor";
            actor.m_researchStage = "S2";
            actor.m_confidence = "documented";
            actor.m_operationalRisk = "low";
            actor.m_validationState = "unvalidated";
            actor.m_stalenessState = "current";
            actor.m_evidenceIds = { "evidence.actor.stage8" };
            actor.m_createdAt = "2026-08-05T00:00:00Z";
            actor.m_updatedAt = "2026-08-05T00:00:00Z";
            AZStd::string error;
            EXPECT_TRUE(catalog.InsertNew(actor, &error)) << error.c_str();

            PopulationActorProfile profile;
            profile.m_recordId = actor.m_recordId;
            profile.m_actorKind = "npc";
            profile.m_archetype = "test";
            profile.m_minimumLevel = 1;
            profile.m_maximumLevel = 1;
            profile.m_portraitAssetRef = "old-portrait";
            profile.m_modelAssetRef = "old-model";
            profile.m_evidenceIds = { "evidence.actor.stage8" };
            EXPECT_TRUE(catalog.UpsertPopulationActorProfile(profile, &error)) << error.c_str();
            return catalog;
        }
    } // namespace

    TEST(ActorAppearanceBindingServiceTests, UpdatesProfileAndProvenanceInOneCandidate)
    {
        const CatalogDatabase current = MakeCatalog();
        const SourceEvidenceRegistry registry = MakeRegistry();
        ActorAppearanceBindingRequest request;
        request.m_actorRecordId = "actor.stage8.binding";
        request.m_role = ActorAppearanceBindingRole::Model;
        request.m_productAssetId = "{22222222-2222-2222-2222-222222222222}:2";
        request.m_sourceAssetSubjectRef = "visual.asset.stage8.actor-model";
        request.m_productEvidenceIds = { "evidence.stage8.preview-product" };

        auto result = ActorAppearanceBindingService::BuildCandidate(
            request, MakeProfile(), registry, current);
        ASSERT_TRUE(result.IsSuccess()) << result.GetError().c_str();
        const ActorAppearanceBindingResult& candidate = result.GetValue();
        ASSERT_NE(candidate.m_catalog.FindPopulationActorProfile(request.m_actorRecordId), nullptr);
        EXPECT_EQ(
            candidate.m_catalog.FindPopulationActorProfile(request.m_actorRecordId)->m_modelAssetRef,
            request.m_productAssetId);
        EXPECT_EQ(
            current.FindPopulationActorProfile(request.m_actorRecordId)->m_modelAssetRef,
            "old-model");
        const CatalogRelationship* relationship = candidate.m_catalog.FindRelationshipById(
            "population.appearance.actor.stage8.binding.model");
        ASSERT_NE(relationship, nullptr);
        EXPECT_EQ(relationship->m_targetSubjectRef, request.m_sourceAssetSubjectRef);
        EXPECT_EQ(relationship->m_relationshipKind, "actor_uses_model_preview");
    }

    TEST(ActorAppearanceBindingServiceTests, RejectsEvidenceFromDifferentProfile)
    {
        const CatalogDatabase current = MakeCatalog();
        const SourceEvidenceRegistry registry = MakeRegistry();
        GameProfile wrongProfile = MakeProfile();
        wrongProfile.m_gameVersion = "2.0.0";
        ActorAppearanceBindingRequest request;
        request.m_actorRecordId = "actor.stage8.binding";
        request.m_productAssetId = "{22222222-2222-2222-2222-222222222222}:2";
        request.m_sourceAssetSubjectRef = "visual.asset.stage8.actor-model";
        request.m_productEvidenceIds = { "evidence.stage8.preview-product" };

        auto result = ActorAppearanceBindingService::BuildCandidate(
            request, wrongProfile, registry, current);
        EXPECT_FALSE(result.IsSuccess());
        EXPECT_EQ(
            current.FindPopulationActorProfile(request.m_actorRecordId)->m_modelAssetRef,
            "old-model");
    }
} // namespace TaintedGrailModdingSDK
