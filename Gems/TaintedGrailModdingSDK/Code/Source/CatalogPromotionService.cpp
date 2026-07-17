/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogPromotionService.h"

#include <AzCore/std/utility/move.h>

#include <QDateTime>

namespace TaintedGrailModdingSDK
{
    AZ::Outcome<CatalogRecord, AZStd::string> CatalogPromotionService::BuildReviewedRecord(
        const CatalogPromotionRequest& request,
        const WorkspaceModel& workspace,
        const AZStd::vector<PackManifest>& packs,
        const SourceEvidenceRegistry& sourceRegistry) const
    {
        const GameProfile* profile = workspace.FindActiveGameProfile();
        if (!profile || !profile->IsConfigured())
        {
            return AZ::Failure(AZStd::string("An exact active game profile is required before claim promotion."));
        }

        const EvidenceRecord* evidence = sourceRegistry.FindEvidence(request.m_evidenceId);
        if (!evidence)
        {
            return AZ::Failure(AZStd::string("Claim promotion references an unknown evidence ID."));
        }
        if (evidence->m_profileId != profile->m_profileId
            || evidence->m_gameVersion != profile->m_gameVersion
            || evidence->m_branch != profile->m_branch)
        {
            return AZ::Failure(AZStd::string("Evidence is outside the active catalog game-profile scope."));
        }
        if (request.m_recordId.empty() || request.m_domain.empty() || request.m_recordKind.empty()
            || request.m_subjectRef.empty() || request.m_identityKind.empty())
        {
            return AZ::Failure(AZStd::string(
                "Promotion requires record ID, domain, record kind, subject reference, and identity kind."));
        }
        if (request.m_subjectRef != evidence->m_subjectRef)
        {
            return AZ::Failure(AZStd::string(
                "Promotion subject reference must exactly match the selected evidence subject."));
        }
        if (!request.m_allowedUsages.empty())
        {
            return AZ::Failure(AZStd::string(
                "Claim promotion cannot grant usage permission; validation and permission are separate steps."));
        }

        if (request.m_identityKind == "native")
        {
            if (request.m_nativeRefExact.empty())
            {
                return AZ::Failure(AZStd::string("Native promotion requires an exact native reference."));
            }
            if (!request.m_ownerPackId.empty())
            {
                return AZ::Failure(AZStd::string("Native catalog records must not claim pack ownership."));
            }
        }
        else if (request.m_identityKind == "synthetic")
        {
            if (request.m_ownerPackId.empty() || !FindPack(packs, request.m_ownerPackId))
            {
                return AZ::Failure(AZStd::string(
                    "Synthetic promotion requires an existing owning pack ID."));
            }
            if (!request.m_nativeRefExact.empty())
            {
                return AZ::Failure(AZStd::string(
                    "Synthetic records must not borrow an exact native reference."));
            }
        }
        else if (request.m_identityKind != "composite" && request.m_identityKind != "source_scoped")
        {
            return AZ::Failure(AZStd::string(
                "Identity kind must be native, synthetic, composite, or source_scoped."));
        }

        CatalogRecord record;
        record.m_recordId = request.m_recordId;
        record.m_ownerPackId = request.m_ownerPackId;
        record.m_domain = request.m_domain;
        record.m_recordKind = request.m_recordKind;
        record.m_subjectRef = request.m_subjectRef;
        record.m_nativeRefExact = request.m_nativeRefExact;
        record.m_identityKind = request.m_identityKind;
        record.m_displayName = request.m_displayName;
        record.m_aliases = request.m_aliases;
        record.m_researchStage = request.m_researchStage.empty() ? "reviewed" : request.m_researchStage;
        record.m_confidence = request.m_confidence.empty() ? evidence->m_confidence : request.m_confidence;
        record.m_operationalRisk = request.m_operationalRisk.empty() ? "unknown" : request.m_operationalRisk;
        record.m_validationState = "unvalidated";
        record.m_forbiddenUsages = request.m_forbiddenUsages;
        if (AZStd::find(record.m_forbiddenUsages.begin(), record.m_forbiddenUsages.end(), "no_unvalidated_runtime_use")
            == record.m_forbiddenUsages.end())
        {
            record.m_forbiddenUsages.push_back("no_unvalidated_runtime_use");
        }
        record.m_evidenceIds = { evidence->m_evidenceId };
        record.m_tags = request.m_tags;

        const QByteArray timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8();
        record.m_createdAt.assign(timestamp.constData(), static_cast<size_t>(timestamp.size()));
        record.m_updatedAt = record.m_createdAt;
        return AZ::Success(AZStd::move(record));
    }

    const PackManifest* CatalogPromotionService::FindPack(
        const AZStd::vector<PackManifest>& packs,
        const AZStd::string& packId)
    {
        for (const PackManifest& pack : packs)
        {
            if (pack.m_packId == packId)
            {
                return &pack;
            }
        }
        return nullptr;
    }
} // namespace TaintedGrailModdingSDK
