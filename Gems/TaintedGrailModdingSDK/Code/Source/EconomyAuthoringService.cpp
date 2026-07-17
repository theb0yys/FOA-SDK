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

        void AddUnique(AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            if (!value.empty() && !Contains(values, value))
            {
                values.push_back(value);
            }
        }

        void AddAllUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::vector<AZStd::string>& additions)
        {
            for (const AZStd::string& value : additions)
            {
                AddUnique(values, value);
            }
        }

        bool IsSupportedRelationshipKind(const AZStd::string& value)
        {
            return value == "sold_by" || value == "dropped_by" || value == "found_at"
                || value == "rewarded_by" || value == "learned_from" || value == "granted_by"
                || value == "crafted_at";
        }

        bool IsStationKind(const CatalogRecord& record)
        {
            return record.m_domain == "economy"
                && (record.m_recordKind == "station"
                    || record.m_recordKind == "crafting_station"
                    || record.m_recordKind == "interaction_target");
        }

        bool IsCurrentValidated(const CatalogRecord& record)
        {
            return record.m_validationState == "validated"
                && record.m_stalenessState == "current"
                && record.m_missingRefs.empty()
                && record.m_conflictRefs.empty()
                && record.m_supersededByRecordId.empty();
        }

        bool IsCurrentValidated(const CatalogRelationship& relationship)
        {
            return relationship.m_validationState == "validated"
                && relationship.m_stalenessState == "current"
                && relationship.m_missingRefs.empty()
                && relationship.m_conflictRefs.empty()
                && relationship.m_supersededByRelationshipId.empty();
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

        EconomyRecipeStationEvidenceRow* FindRow(
            AZStd::vector<EconomyRecipeStationEvidenceRow>& rows,
            const AZStd::string& stationRecordId,
            const AZStd::string& stationSubjectRef)
        {
            for (EconomyRecipeStationEvidenceRow& row : rows)
            {
                if (row.m_stationRecordId == stationRecordId
                    && row.m_stationSubjectRef == stationSubjectRef)
                {
                    return &row;
                }
            }
            return nullptr;
        }

        EconomyRecipeStationEvidenceRow& FindOrAddRow(
            AZStd::vector<EconomyRecipeStationEvidenceRow>& rows,
            const AZStd::string& recipeRecordId,
            const AZStd::string& stationRecordId,
            const AZStd::string& stationSubjectRef)
        {
            if (EconomyRecipeStationEvidenceRow* row = FindRow(rows, stationRecordId, stationSubjectRef))
            {
                return *row;
            }

            EconomyRecipeStationEvidenceRow row;
            row.m_recipeRecordId = recipeRecordId;
            row.m_stationRecordId = stationRecordId;
            row.m_stationSubjectRef = stationSubjectRef;
            rows.push_back(AZStd::move(row));
            return rows.back();
        }

        void MarkBlocked(EconomyRecipeStationEvidenceRow& row, const AZStd::string& reason)
        {
            row.m_status = "blocked";
            AddUnique(row.m_reasons, reason);
        }

        AZStd::string StationIdentity(const CatalogRecord& station)
        {
            if (!station.m_nativeRefExact.empty())
            {
                return station.m_nativeRefExact;
            }
            if (!station.m_ownerPackId.empty())
            {
                return "pack:" + station.m_ownerPackId;
            }
            return station.m_recordId;
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

    AZStd::vector<EconomyRecipeStationEvidenceRow> EconomyAuthoringService::BuildRecipeStationEvidence(
        const AZStd::string& recipeRecordId,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        AZStd::vector<EconomyRecipeStationEvidenceRow> rows;
        const CatalogRecord* recipe = catalog.FindByRecordId(recipeRecordId);
        const EconomyRecipeProfile* profile = catalog.FindEconomyRecipe(recipeRecordId);
        if (!recipe || recipe->m_domain != "economy" || recipe->m_recordKind != "recipe" || !profile)
        {
            return rows;
        }

        for (const AZStd::string& stationRecordId : profile->m_stationRecordIds)
        {
            EconomyRecipeStationEvidenceRow& row = FindOrAddRow(rows, recipeRecordId, stationRecordId, {});
            AddUnique(row.m_associationSources, "recipe profile");
            AddAllUnique(row.m_evidenceIds, profile->m_evidenceIds);
        }

        AZStd::vector<AZStd::string> learnedFromRelationshipIds;
        AZStd::vector<AZStd::string> learnedFromEvidenceIds;
        AZStd::vector<AZStd::string> learnedFromBlockReasons;
        AZStd::vector<AZStd::string> learnabilitySubjects = profile->m_unlockSubjectRefs;
        for (const CatalogRelationship& relationship : catalog.FindRelationshipsForRecord(recipeRecordId))
        {
            if (relationship.m_fromRecordId != recipeRecordId)
            {
                continue;
            }

            if (relationship.m_relationshipKind == "crafted_at")
            {
                EconomyRecipeStationEvidenceRow& row = FindOrAddRow(
                    rows,
                    recipeRecordId,
                    relationship.m_toRecordId,
                    relationship.m_targetSubjectRef);
                AddUnique(row.m_associationSources, "crafted_at:" + relationship.m_relationshipId);
                AddAllUnique(row.m_evidenceIds, relationship.m_evidenceIds);
                if (!IsCurrentValidated(relationship))
                {
                    MarkBlocked(
                        row,
                        "The crafted_at relationship is not validated, current, reference-complete, conflict-free, and non-superseded: "
                            + relationship.m_relationshipId);
                }
            }
            else if (relationship.m_relationshipKind == "learned_from")
            {
                AddUnique(learnedFromRelationshipIds, relationship.m_relationshipId);
                AddAllUnique(learnedFromEvidenceIds, relationship.m_evidenceIds);
                if (!IsCurrentValidated(relationship))
                {
                    AddUnique(
                        learnedFromBlockReasons,
                        "The learned_from relationship is not validated, current, reference-complete, conflict-free, and non-superseded: "
                            + relationship.m_relationshipId);
                }
                if (!relationship.m_toRecordId.empty())
                {
                    if (const CatalogRecord* target = catalog.FindByRecordId(relationship.m_toRecordId))
                    {
                        AddUnique(learnabilitySubjects, target->m_subjectRef);
                        if (!IsCurrentValidated(*target))
                        {
                            AddUnique(
                                learnedFromBlockReasons,
                                "The learned_from target is not validated, current, reference-complete, conflict-free, and non-superseded: "
                                    + relationship.m_toRecordId);
                        }
                    }
                }
                else
                {
                    AddUnique(learnabilitySubjects, relationship.m_targetSubjectRef);
                    AddUnique(
                        learnedFromBlockReasons,
                        "The learned_from target remains unresolved: " + relationship.m_targetSubjectRef);
                }
            }
        }

        if (rows.empty())
        {
            EconomyRecipeStationEvidenceRow row;
            row.m_recipeRecordId = recipeRecordId;
            row.m_status = "missing evidence";
            row.m_reasons.push_back("The recipe has no canonical or unresolved station association.");
            rows.push_back(AZStd::move(row));
        }

        for (EconomyRecipeStationEvidenceRow& row : rows)
        {
            row.m_unlockMode = profile->m_unlockMode;
            row.m_unlockSubjectRefs = profile->m_unlockSubjectRefs;
            row.m_learnedFromRelationshipIds = learnedFromRelationshipIds;
            row.m_validationState = recipe->m_validationState;
            row.m_stalenessState = recipe->m_stalenessState;
            row.m_actionLanes = BuildActionLaneMatrix(*recipe);
            AddAllUnique(row.m_evidenceIds, learnedFromEvidenceIds);
            for (const AZStd::string& reason : learnedFromBlockReasons)
            {
                MarkBlocked(row, reason);
            }

            if (!IsCurrentValidated(*recipe))
            {
                MarkBlocked(
                    row,
                    "The recipe is not validated, current, reference-complete, conflict-free, and non-superseded.");
            }

            bool unresolved = false;
            if (!row.m_stationRecordId.empty())
            {
                const CatalogRecord* station = catalog.FindByRecordId(row.m_stationRecordId);
                if (!station)
                {
                    unresolved = true;
                    AddUnique(row.m_reasons, "The station record does not exist: " + row.m_stationRecordId);
                }
                else
                {
                    row.m_stationSubjectRef = station->m_subjectRef;
                    row.m_stationIdentity = StationIdentity(*station);
                    if (!IsStationKind(*station))
                    {
                        MarkBlocked(row, "The associated target is not a canonical economy station.");
                    }
                    if (!IsCurrentValidated(*station))
                    {
                        MarkBlocked(
                            row,
                            "The station is not validated, current, reference-complete, conflict-free, and non-superseded.");
                    }
                }
            }
            else if (!row.m_stationSubjectRef.empty())
            {
                unresolved = true;
                AddUnique(row.m_reasons, "The station association remains unresolved: " + row.m_stationSubjectRef);
            }
            else
            {
                unresolved = true;
            }

            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_status != "open")
                {
                    continue;
                }
                if (blocker.m_subjectRef == recipe->m_subjectRef
                    || (!row.m_stationSubjectRef.empty() && blocker.m_subjectRef == row.m_stationSubjectRef))
                {
                    AddUnique(row.m_blockerIds, blocker.m_blockerId);
                    MarkBlocked(row, blocker.m_reason);
                }
            }

            AZStd::vector<AZStd::string> allowedEvidenceSubjects;
            AddUnique(allowedEvidenceSubjects, recipe->m_subjectRef);
            AddUnique(allowedEvidenceSubjects, row.m_stationSubjectRef);
            AddAllUnique(allowedEvidenceSubjects, learnabilitySubjects);

            size_t validEvidenceCount = 0;
            for (const AZStd::string& evidenceId : row.m_evidenceIds)
            {
                const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    MarkBlocked(row, "The evidence ID is unknown: " + evidenceId);
                    continue;
                }

                const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source)
                {
                    MarkBlocked(row, "The evidence source is unknown: " + evidence->m_sourceId);
                    continue;
                }
                if (evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != source->m_profileId
                    || evidence->m_gameVersion != source->m_gameVersion
                    || evidence->m_branch != source->m_branch)
                {
                    MarkBlocked(row, "The evidence is not bound to its source profile and build: " + evidenceId);
                    continue;
                }
                if (!Contains(allowedEvidenceSubjects, evidence->m_subjectRef))
                {
                    MarkBlocked(row, "The evidence belongs to an unrelated subject: " + evidenceId);
                    continue;
                }
                ++validEvidenceCount;
            }

            if (row.m_status != "blocked")
            {
                if (unresolved)
                {
                    row.m_status = "unresolved";
                }
                else if (row.m_evidenceIds.empty() || validEvidenceCount == 0)
                {
                    row.m_status = "missing evidence";
                    AddUnique(row.m_reasons, "No usable evidence supports this station association.");
                }
                else
                {
                    const bool profileSource = Contains(row.m_associationSources, "recipe profile");
                    bool relationshipSource = false;
                    for (const AZStd::string& source : row.m_associationSources)
                    {
                        relationshipSource = relationshipSource || source.find("crafted_at:") == 0;
                    }
                    row.m_status = profileSource && relationshipSource
                        ? "supported evidence"
                        : "partial evidence";
                }
            }
        }

        AZStd::sort(
            rows.begin(),
            rows.end(),
            [](const EconomyRecipeStationEvidenceRow& left, const EconomyRecipeStationEvidenceRow& right)
            {
                const AZStd::string leftKey = left.m_stationRecordId.empty()
                    ? left.m_stationSubjectRef
                    : left.m_stationRecordId;
                const AZStd::string rightKey = right.m_stationRecordId.empty()
                    ? right.m_stationSubjectRef
                    : right.m_stationRecordId;
                return leftKey < rightKey;
            });
        return rows;
    }
} // namespace TaintedGrailModdingSDK
