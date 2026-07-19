/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyAuthoringService.h"

#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value)
                != values.end();
        }

        void AddUnique(
            AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
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

        bool IsItemOrRecipe(const CatalogRecord& record)
        {
            return record.m_domain == "economy"
                && (record.m_recordKind == "item"
                    || record.m_recordKind == "recipe");
        }

        bool IsStationKind(const CatalogRecord& record)
        {
            return record.m_domain == "economy"
                && (record.m_recordKind == "station"
                    || record.m_recordKind == "crafting_station");
        }

        bool IsSupportedAcquisitionKind(const AZStd::string& value)
        {
            return value == "sold_by"
                || value == "dropped_by"
                || value == "found_at"
                || value == "rewarded_by"
                || value == "granted_by"
                || value == "learned_from";
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

        bool EvidenceProvesAnySubject(
            const EvidenceRecord& evidence,
            const SourceEvidenceRegistry& sourceRegistry,
            const AZStd::vector<AZStd::string>& expectedSubjectRefs)
        {
            if (!Contains(expectedSubjectRefs, evidence.m_subjectRef)
                || evidence.m_claim.empty()
                || evidence.m_evidenceKind.empty()
                || evidence.m_confidence.empty()
                || evidence.m_locator.empty()
                || evidence.m_recordPath.empty()
                || !IsStrictUtcTimestamp(evidence.m_extractedAt)
                || !IsSha256Fingerprint(evidence.m_sourceFingerprint))
            {
                return false;
            }
            const SourceRecord* source =
                sourceRegistry.FindSource(evidence.m_sourceId);
            return source
                && IsUsableImportStatus(source->m_importStatus)
                && source->m_fingerprint == evidence.m_sourceFingerprint
                && source->m_profileId == evidence.m_profileId
                && source->m_gameVersion == evidence.m_gameVersion
                && source->m_branch == evidence.m_branch
                && source->m_capturedAt <= evidence.m_extractedAt
                && evidence.m_extractedAt <= source->m_importedAt;
        }

        bool EvidenceIdsProveSubject(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::string& expectedSubjectRef,
            const SourceEvidenceRegistry& sourceRegistry,
            AZStd::string& error)
        {
            if (evidenceIds.empty())
            {
                error = "At least one exact-association evidence ID is required.";
                return false;
            }
            AZStd::vector<AZStd::string> sorted = evidenceIds;
            AZStd::sort(sorted.begin(), sorted.end());
            if (AZStd::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
            {
                error = "Association evidence IDs must be unique.";
                return false;
            }
            const AZStd::vector<AZStd::string> expected{ expectedSubjectRef };
            for (const AZStd::string& evidenceId : sorted)
            {
                if (!IsStableContractId(evidenceId))
                {
                    error = "Association evidence IDs must be stable bounded identities.";
                    return false;
                }
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                if (!evidence
                    || !EvidenceProvesAnySubject(
                        *evidence,
                        sourceRegistry,
                        expected))
                {
                    error = "Evidence does not prove the exact association subject "
                        + expectedSubjectRef + ": " + evidenceId;
                    return false;
                }
            }
            return true;
        }

        AZStd::string StationAssociationSubject(
            const AZStd::string& recipeRecordId,
            const AZStd::string& stationRecordId)
        {
            return "economy-recipe-station:" + recipeRecordId
                + ":" + stationRecordId;
        }

        AZStd::string RelationshipSubject(
            const AZStd::string& relationshipId)
        {
            return "relationship:" + relationshipId;
        }

        AZStd::string StationIdentity(const CatalogRecord& station)
        {
            if (!station.m_nativeRefExact.empty())
            {
                return station.m_nativeRefExact;
            }
            if (!station.m_subjectRef.empty())
            {
                return station.m_subjectRef;
            }
            return station.m_recordId;
        }

        void MarkBlocked(
            EconomyRecipeStationEvidenceRow& row,
            const AZStd::string& reason)
        {
            row.m_status = "blocked";
            AddUnique(row.m_reasons, reason);
        }

        EconomyRecipeStationEvidenceRow& FindOrAddRow(
            AZStd::vector<EconomyRecipeStationEvidenceRow>& rows,
            const CatalogRecord& recipe,
            const AZStd::string& stationRecordId,
            const AZStd::string& stationSubjectRef)
        {
            for (EconomyRecipeStationEvidenceRow& row : rows)
            {
                if ((!stationRecordId.empty()
                        && row.m_stationRecordId == stationRecordId)
                    || (stationRecordId.empty()
                        && row.m_stationRecordId.empty()
                        && row.m_stationSubjectRef == stationSubjectRef))
                {
                    return row;
                }
            }

            EconomyRecipeStationEvidenceRow row;
            row.m_recipeRecordId = recipe.m_recordId;
            row.m_stationRecordId = stationRecordId;
            row.m_stationSubjectRef = stationSubjectRef;
            row.m_validationState = recipe.m_validationState;
            row.m_stalenessState = recipe.m_stalenessState;
            row.m_status = "missing evidence";
            rows.push_back(AZStd::move(row));
            return rows.back();
        }
    } // namespace

    AZ::Outcome<CatalogRelationship, AZStd::string>
    EconomyAuthoringService::BuildAcquisitionRelationship(
        const EconomyAcquisitionRequest& request,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        if (!IsStableContractId(request.m_relationshipId)
            || !IsSupportedAcquisitionKind(request.m_relationshipKind))
        {
            return AZ::Failure(AZStd::string(
                "A stable relationship ID and supported acquisition relationship kind are required."));
        }
        const bool hasTargetRecord = !request.m_targetRecordId.empty();
        const bool hasTargetSubject = !request.m_targetSubjectRef.empty();
        if (hasTargetRecord == hasTargetSubject)
        {
            return AZ::Failure(AZStd::string(
                "Acquisition relationships require exactly one target record ID or unresolved target subject reference."));
        }

        const CatalogRecord* source =
            catalog.FindByRecordId(request.m_sourceRecordId);
        if (!source || !IsItemOrRecipe(*source))
        {
            return AZ::Failure(AZStd::string(
                "The acquisition source must be a canonical economy item or recipe record."));
        }
        if (hasTargetRecord)
        {
            const CatalogRecord* target =
                catalog.FindByRecordId(request.m_targetRecordId);
            if (!target)
            {
                return AZ::Failure(AZStd::string(
                    "The acquisition target record does not exist."));
            }
            if (target->m_recordId == source->m_recordId)
            {
                return AZ::Failure(AZStd::string(
                    "An acquisition relationship cannot target its own source record."));
            }
        }

        AZStd::string evidenceError;
        if (!EvidenceIdsProveSubject(
                request.m_evidenceIds,
                RelationshipSubject(request.m_relationshipId),
                sourceRegistry,
                evidenceError))
        {
            return AZ::Failure(AZStd::move(evidenceError));
        }

        CatalogRelationship relationship;
        relationship.m_relationshipId = request.m_relationshipId;
        relationship.m_fromRecordId = request.m_sourceRecordId;
        relationship.m_toRecordId = request.m_targetRecordId;
        relationship.m_targetSubjectRef = request.m_targetSubjectRef;
        relationship.m_relationshipKind = request.m_relationshipKind;
        relationship.m_evidenceIds = request.m_evidenceIds;
        relationship.m_attributes = request.m_attributes;
        relationship.m_researchStage = "observed";
        relationship.m_confidence = "medium";
        relationship.m_operationalRisk = "unknown";
        relationship.m_validationState = "unvalidated";
        relationship.m_stalenessState = "unknown";
        relationship.m_forbiddenUsages.push_back("no_unvalidated_runtime_use");
        return AZ::Success(AZStd::move(relationship));
    }

    AZ::Outcome<CatalogRelationship, AZStd::string>
    EconomyAuthoringService::BuildAcquisitionRelationship(
        const EconomyAcquisitionRequest&,
        const CatalogDatabase&) const
    {
        return AZ::Failure(AZStd::string(
            "The active source/evidence registry is required to prove the exact acquisition association."));
    }

    AZStd::vector<EconomyActionLaneStatus>
    EconomyAuthoringService::BuildActionLaneMatrix(
        const CatalogRecord& record) const
    {
        const AZStd::vector<AZStd::string> lanes = {
            "existing_item_grant",
            "existing_recipe_learn",
            "runtime_recipe_append",
            "custom_item_registration",
            "custom_recipe_registration",
            "vendor_or_loot_injection",
            "quest_or_contract_reward_injection",
            "asset_use",
            "localisation_use",
        };

        AZStd::vector<EconomyActionLaneStatus> results;
        for (const AZStd::string& lane : lanes)
        {
            EconomyActionLaneStatus result;
            result.m_lane = lane;
            if (Contains(record.m_forbiddenUsages, lane))
            {
                result.m_status = "forbidden";
            }
            else if (Contains(record.m_allowedUsages, lane))
            {
                result.m_status = "allowed";
            }
            else
            {
                result.m_status = "unreviewed";
            }
            results.push_back(AZStd::move(result));
        }
        return results;
    }

    AZStd::vector<EconomyRecipeStationEvidenceRow>
    EconomyAuthoringService::BuildRecipeStationEvidence(
        const AZStd::string& recipeRecordId,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        AZStd::vector<EconomyRecipeStationEvidenceRow> rows;
        const CatalogRecord* recipe = catalog.FindByRecordId(recipeRecordId);
        if (!recipe
            || recipe->m_domain != "economy"
            || recipe->m_recordKind != "recipe")
        {
            return rows;
        }

        const EconomyRecipeProfile* recipeProfile =
            catalog.FindEconomyRecipe(recipeRecordId);
        if (recipeProfile)
        {
            for (const AZStd::string& stationRecordId :
                 recipeProfile->m_stationRecordIds)
            {
                EconomyRecipeStationEvidenceRow& row = FindOrAddRow(
                    rows,
                    *recipe,
                    stationRecordId,
                    {});
                AddUnique(row.m_associationSources, "recipe profile");
                AddUnique(
                    row.m_stationEvidenceSubjectRefs,
                    StationAssociationSubject(recipeRecordId, stationRecordId));
                AddAllUnique(row.m_evidenceIds, recipeProfile->m_evidenceIds);
                row.m_unlockMode = recipeProfile->m_unlockMode;
                row.m_unlockSubjectRefs = recipeProfile->m_unlockSubjectRefs;
            }
        }

        AZStd::vector<AZStd::string> learnedFromEvidenceIds;
        AZStd::vector<AZStd::string> learnedFromRelationshipIds;
        AZStd::vector<AZStd::string> learnedFromEvidenceSubjects;
        AZStd::vector<AZStd::string> learnedFromBlockReasons;
        for (const CatalogRelationship& relationship :
             catalog.FindRelationshipsForRecord(recipeRecordId))
        {
            if (relationship.m_fromRecordId != recipeRecordId)
            {
                continue;
            }
            const AZStd::string relationshipSubject =
                RelationshipSubject(relationship.m_relationshipId);
            if (relationship.m_relationshipKind == "crafted_at")
            {
                EconomyRecipeStationEvidenceRow& row = FindOrAddRow(
                    rows,
                    *recipe,
                    relationship.m_toRecordId,
                    relationship.m_targetSubjectRef);
                AddUnique(
                    row.m_associationSources,
                    "crafted_at:" + relationship.m_relationshipId);
                AddUnique(
                    row.m_stationEvidenceSubjectRefs,
                    relationshipSubject);
                AddAllUnique(row.m_evidenceIds, relationship.m_evidenceIds);
                if (!IsCurrentValidated(relationship))
                {
                    MarkBlocked(
                        row,
                        "The crafted_at relationship is not validated, current, reference-complete, conflict-free, and non-superseded.");
                }
            }
            else if (relationship.m_relationshipKind == "learned_from")
            {
                AddUnique(
                    learnedFromRelationshipIds,
                    relationship.m_relationshipId);
                AddUnique(
                    learnedFromEvidenceSubjects,
                    relationshipSubject);
                AddAllUnique(
                    learnedFromEvidenceIds,
                    relationship.m_evidenceIds);
                if (!IsCurrentValidated(relationship))
                {
                    AddUnique(
                        learnedFromBlockReasons,
                        "The learned_from relationship is not validated, current, reference-complete, conflict-free, and non-superseded: "
                            + relationship.m_relationshipId);
                }
            }
        }

        if (rows.empty())
        {
            rows.push_back(FindOrAddRow(rows, *recipe, {}, {}));
        }

        for (EconomyRecipeStationEvidenceRow& row : rows)
        {
            row.m_learnedFromRelationshipIds = learnedFromRelationshipIds;
            AddAllUnique(
                row.m_learnabilityEvidenceSubjectRefs,
                learnedFromEvidenceSubjects);
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
                const CatalogRecord* station =
                    catalog.FindByRecordId(row.m_stationRecordId);
                if (!station)
                {
                    unresolved = true;
                    AddUnique(
                        row.m_reasons,
                        "The station record does not exist: "
                            + row.m_stationRecordId);
                }
                else
                {
                    row.m_stationSubjectRef = station->m_subjectRef;
                    row.m_stationIdentity = StationIdentity(*station);
                    if (!IsStationKind(*station))
                    {
                        MarkBlocked(
                            row,
                            "The associated target is not a canonical economy station.");
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
                AddUnique(
                    row.m_reasons,
                    "The station association remains unresolved: "
                        + row.m_stationSubjectRef);
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
                    || blocker.m_subjectRef == row.m_stationSubjectRef
                    || Contains(
                        row.m_stationEvidenceSubjectRefs,
                        blocker.m_subjectRef)
                    || Contains(
                        row.m_learnabilityEvidenceSubjectRefs,
                        blocker.m_subjectRef))
                {
                    AddUnique(row.m_blockerIds, blocker.m_blockerId);
                    if (blocker.m_severity == "error")
                    {
                        MarkBlocked(row, blocker.m_reason);
                    }
                    else
                    {
                        AddUnique(row.m_reasons, blocker.m_reason);
                    }
                }
            }

            size_t validStationEvidenceCount = 0;
            size_t validLearnabilityEvidenceCount = 0;
            for (const AZStd::string& evidenceId : row.m_evidenceIds)
            {
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    MarkBlocked(
                        row,
                        "The evidence ID is unknown: " + evidenceId);
                    continue;
                }
                AZStd::vector<AZStd::string> allExpectedSubjects =
                    row.m_stationEvidenceSubjectRefs;
                AddAllUnique(
                    allExpectedSubjects,
                    row.m_learnabilityEvidenceSubjectRefs);
                if (!EvidenceProvesAnySubject(
                        *evidence,
                        sourceRegistry,
                        allExpectedSubjects))
                {
                    MarkBlocked(
                        row,
                        "The evidence does not prove an exact station or learnability association: "
                            + evidenceId);
                    continue;
                }
                if (Contains(
                        row.m_stationEvidenceSubjectRefs,
                        evidence->m_subjectRef))
                {
                    ++validStationEvidenceCount;
                }
                if (Contains(
                        row.m_learnabilityEvidenceSubjectRefs,
                        evidence->m_subjectRef))
                {
                    ++validLearnabilityEvidenceCount;
                }
            }

            if (row.m_status != "blocked")
            {
                if (unresolved)
                {
                    row.m_status = "unresolved";
                }
                else if (validStationEvidenceCount == 0)
                {
                    row.m_status = "missing evidence";
                    AddUnique(
                        row.m_reasons,
                        "No usable evidence proves the exact recipe-to-station association.");
                }
                else if (!row.m_learnedFromRelationshipIds.empty()
                    && validLearnabilityEvidenceCount == 0)
                {
                    row.m_status = "partial evidence";
                    AddUnique(
                        row.m_reasons,
                        "Station evidence is present, but no usable evidence proves the exact learned_from association.");
                }
                else
                {
                    const bool profileSource = Contains(
                        row.m_associationSources,
                        "recipe profile");
                    bool relationshipSource = false;
                    for (const AZStd::string& source : row.m_associationSources)
                    {
                        relationshipSource = relationshipSource
                            || source.find("crafted_at:") == 0;
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
            [](const EconomyRecipeStationEvidenceRow& left,
               const EconomyRecipeStationEvidenceRow& right)
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
