/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ActorAppearanceBindingService.h"

#include "FoundationModels.h"
#include "PopulationModels.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        const char* RoleName(ActorAppearanceBindingRole role)
        {
            return role == ActorAppearanceBindingRole::Portrait ? "portrait" : "model";
        }

        const char* RelationshipKind(ActorAppearanceBindingRole role)
        {
            return role == ActorAppearanceBindingRole::Portrait
                ? "actor_uses_portrait_preview"
                : "actor_uses_model_preview";
        }

        AZStd::string RelationshipId(
            const AZStd::string& actorRecordId,
            ActorAppearanceBindingRole role)
        {
            return AZStd::string("population.appearance.") + actorRecordId + "." + RoleName(role);
        }

        bool ContainsDuplicate(const AZStd::vector<AZStd::string>& values)
        {
            AZStd::vector<AZStd::string> sorted = values;
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
        }
    } // namespace

    AZ::Outcome<ActorAppearanceBindingResult, AZStd::string>
    ActorAppearanceBindingService::BuildCandidate(
        const ActorAppearanceBindingRequest& request,
        const GameProfile& activeProfile,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& currentCatalog)
    {
        if (request.m_actorRecordId.empty() || request.m_productAssetId.empty()
            || request.m_sourceAssetSubjectRef.empty())
        {
            return AZ::Failure(AZStd::string(
                "Actor appearance binding requires actor ID, product AssetId, and source asset subject."));
        }
        if (request.m_productEvidenceIds.empty())
        {
            return AZ::Failure(AZStd::string(
                "Actor appearance binding requires product evidence IDs."));
        }
        if (ContainsDuplicate(request.m_productEvidenceIds))
        {
            return AZ::Failure(AZStd::string(
                "Actor appearance binding evidence IDs must be unique."));
        }

        const CatalogRecord* actorRecord = currentCatalog.FindByRecordId(
            request.m_actorRecordId);
        if (!actorRecord || actorRecord->m_domain != "population"
            || actorRecord->m_recordKind != "actor")
        {
            return AZ::Failure(AZStd::string(
                "Actor appearance binding target is not a canonical population actor."));
        }
        const PopulationActorProfile* currentProfile =
            currentCatalog.FindPopulationActorProfile(request.m_actorRecordId);
        if (!currentProfile)
        {
            return AZ::Failure(AZStd::string(
                "Actor appearance binding requires an existing typed actor profile."));
        }

        AZStd::string evidenceTimestamp;
        for (const AZStd::string& evidenceId : request.m_productEvidenceIds)
        {
            const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
            if (!evidence)
            {
                return AZ::Failure(
                    AZStd::string("Actor appearance product evidence does not exist: ") + evidenceId);
            }
            if (evidence->m_profileId != activeProfile.m_profileId
                || evidence->m_gameVersion != activeProfile.m_gameVersion
                || evidence->m_branch != activeProfile.m_branch)
            {
                return AZ::Failure(
                    AZStd::string("Actor appearance product evidence is bound to a different active profile: ")
                    + evidenceId);
            }
            if (evidence->m_subjectRef != request.m_sourceAssetSubjectRef)
            {
                return AZ::Failure(
                    AZStd::string("Actor appearance product evidence subject does not match the selected source asset: ")
                    + evidenceId);
            }
            const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
            if (!source || source->m_fingerprint != evidence->m_sourceFingerprint)
            {
                return AZ::Failure(
                    AZStd::string("Actor appearance product evidence source fingerprint is unavailable or mismatched: ")
                    + evidenceId);
            }
            if (source->m_profileId != activeProfile.m_profileId
                || source->m_gameVersion != activeProfile.m_gameVersion
                || source->m_branch != activeProfile.m_branch
                || source->m_runtimeTarget != activeProfile.m_runtimeTarget)
            {
                return AZ::Failure(
                    AZStd::string("Actor appearance product source is bound to a different active profile: ")
                    + evidenceId);
            }
            if (evidenceTimestamp.empty())
            {
                evidenceTimestamp = !evidence->m_extractedAt.empty()
                    ? evidence->m_extractedAt
                    : source->m_importedAt;
            }
        }
        if (evidenceTimestamp.empty())
        {
            return AZ::Failure(AZStd::string(
                "Actor appearance product evidence requires a deterministic extraction or import timestamp."));
        }

        CatalogDatabase candidate = currentCatalog;
        PopulationActorProfile updatedProfile = *currentProfile;
        if (request.m_role == ActorAppearanceBindingRole::Portrait)
        {
            updatedProfile.m_portraitAssetRef = request.m_productAssetId;
        }
        else
        {
            updatedProfile.m_modelAssetRef = request.m_productAssetId;
        }

        AZStd::string error;
        if (!candidate.UpsertPopulationActorProfile(updatedProfile, &error))
        {
            return AZ::Failure(AZStd::string("Actor appearance profile candidate failed: ") + error);
        }

        CatalogRelationship relationship;
        relationship.m_relationshipId = RelationshipId(
            request.m_actorRecordId,
            request.m_role);
        relationship.m_fromRecordId = request.m_actorRecordId;
        relationship.m_targetSubjectRef = request.m_sourceAssetSubjectRef;
        relationship.m_relationshipKind = RelationshipKind(request.m_role);
        relationship.m_evidenceIds = request.m_productEvidenceIds;
        relationship.m_researchStage = "S2";
        relationship.m_confidence = "inferred";
        relationship.m_operationalRisk = "unknown";
        relationship.m_validationState = "unvalidated";
        relationship.m_stalenessState = "current";
        relationship.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
        relationship.m_createdAt = evidenceTimestamp;
        relationship.m_updatedAt = evidenceTimestamp;

        if (const CatalogRelationship* existing =
                currentCatalog.FindRelationshipById(relationship.m_relationshipId))
        {
            relationship.m_createdAt = existing->m_createdAt;
        }
        if (!candidate.UpsertRelationship(relationship, &error))
        {
            return AZ::Failure(AZStd::string("Actor appearance provenance candidate failed: ") + error);
        }

        ActorAppearanceBindingResult result;
        result.m_catalog = AZStd::move(candidate);
        result.m_provenanceRelationship = AZStd::move(relationship);
        return AZ::Success(AZStd::move(result));
    }
} // namespace TaintedGrailModdingSDK
