/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyAuthoringService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <QByteArray>
#include <QDateTime>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool IsSupportedRelationshipKind(const AZStd::string& value)
        {
            return value == "sold_by" || value == "dropped_by" || value == "found_at"
                || value == "rewarded_by" || value == "learned_from" || value == "granted_by"
                || value == "crafted_at";
        }

        AZStd::string Timestamp()
        {
            const QByteArray value = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8();
            return AZStd::string(value.constData(), static_cast<size_t>(value.size()));
        }

        void AddLane(
            AZStd::vector<EconomyActionLaneStatus>& lanes,
            const CatalogRecord& record,
            const char* lane)
        {
            EconomyActionLaneStatus status;
            status.m_lane = lane;
            if (Contains(record.m_forbiddenUsages, status.m_lane))
            {
                status.m_status = "forbidden";
            }
            else if (Contains(record.m_allowedUsages, status.m_lane))
            {
                status.m_status = "allowed";
            }
            else
            {
                status.m_status = "unset";
            }
            lanes.push_back(AZStd::move(status));
        }
    } // namespace

    AZ::Outcome<CatalogRelationship, AZStd::string> EconomyAuthoringService::BuildAcquisitionRelationship(
        const EconomyAcquisitionRequest& request,
        const CatalogDatabase& catalog) const
    {
        if (request.m_relationshipId.empty() || request.m_sourceRecordId.empty()
            || request.m_relationshipKind.empty())
        {
            return AZ::Failure(AZStd::string(
                "Economy acquisition relationships require relationship ID, source record, and relationship kind."));
        }
        if (!IsSupportedRelationshipKind(request.m_relationshipKind))
        {
            return AZ::Failure(AZStd::string(
                "Economy acquisition relationship kind must be sold_by, dropped_by, found_at, rewarded_by, learned_from, granted_by, or crafted_at."));
        }
        const CatalogRecord* source = catalog.FindByRecordId(request.m_sourceRecordId);
        if (!source || source->m_domain != "economy"
            || (source->m_recordKind != "item" && source->m_recordKind != "recipe"))
        {
            return AZ::Failure(AZStd::string(
                "Economy acquisition relationships must originate from a canonical economy item or recipe."));
        }
        if (request.m_targetRecordId.empty() == request.m_targetSubjectRef.empty())
        {
            return AZ::Failure(AZStd::string(
                "Economy acquisition relationships require exactly one target record ID or unresolved target subject ref."));
        }
        if (!request.m_targetRecordId.empty() && !catalog.FindByRecordId(request.m_targetRecordId))
        {
            return AZ::Failure(AZStd::string("Economy acquisition target record does not exist."));
        }
        if (request.m_evidenceIds.empty())
        {
            return AZ::Failure(AZStd::string("Economy acquisition relationships require evidence IDs."));
        }

        CatalogRelationship relationship;
        relationship.m_relationshipId = request.m_relationshipId;
        relationship.m_fromRecordId = request.m_sourceRecordId;
        relationship.m_toRecordId = request.m_targetRecordId;
        relationship.m_targetSubjectRef = request.m_targetSubjectRef;
        relationship.m_relationshipKind = request.m_relationshipKind;
        relationship.m_evidenceIds = request.m_evidenceIds;
        relationship.m_researchStage = "S2";
        relationship.m_confidence = "inferred";
        relationship.m_operationalRisk = "unknown";
        relationship.m_validationState = "unvalidated";
        relationship.m_stalenessState = "unknown";
        relationship.m_forbiddenUsages = { "no_unvalidated_runtime_use" };
        relationship.m_attributes = request.m_attributes;
        relationship.m_createdAt = Timestamp();
        relationship.m_updatedAt = relationship.m_createdAt;
        return AZ::Success(AZStd::move(relationship));
    }

    AZStd::vector<EconomyActionLaneStatus> EconomyAuthoringService::BuildActionLaneMatrix(
        const CatalogRecord& record) const
    {
        AZStd::vector<EconomyActionLaneStatus> lanes;
        if (record.m_recordKind == "item")
        {
            AddLane(lanes, record, "existing_item_grant");
            AddLane(lanes, record, "existing_item_consume");
            AddLane(lanes, record, "custom_item_registration");
            AddLane(lanes, record, "asset_localisation_injection");
            AddLane(lanes, record, "vendor_or_loot_injection");
            AddLane(lanes, record, "quest_or_contract_reward_injection");
        }
        else if (record.m_recordKind == "recipe")
        {
            AddLane(lanes, record, "existing_recipe_learn");
            AddLane(lanes, record, "runtime_recipe_append");
            AddLane(lanes, record, "custom_recipe_registration");
            AddLane(lanes, record, "asset_localisation_injection");
            AddLane(lanes, record, "quest_or_contract_reward_injection");
        }
        return lanes;
    }
} // namespace TaintedGrailModdingSDK
