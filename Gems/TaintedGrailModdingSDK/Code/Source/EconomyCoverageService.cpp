/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyCoverageService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

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

        void SortUniqueValues(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
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

        bool IsHardBlocked(const CatalogRecord& record)
        {
            return record.m_validationState == "failed"
                || record.m_validationState == "blocked"
                || record.m_stalenessState == "stale"
                || !record.m_missingRefs.empty()
                || !record.m_conflictRefs.empty()
                || !record.m_supersededByRecordId.empty();
        }

        bool IsHardBlocked(const CatalogRelationship& relationship)
        {
            return relationship.m_validationState == "failed"
                || relationship.m_validationState == "blocked"
                || relationship.m_stalenessState == "stale"
                || !relationship.m_missingRefs.empty()
                || !relationship.m_conflictRefs.empty()
                || !relationship.m_supersededByRelationshipId.empty();
        }

        bool IsEconomySubject(const CatalogRecord& record)
        {
            return record.m_domain == "economy"
                && (record.m_recordKind == "item" || record.m_recordKind == "recipe");
        }

        AZStd::vector<AZStd::string> BuildApplicableLanes(const CatalogRecord& record)
        {
            if (record.m_recordKind == "item")
            {
                return { "vendor", "loot", "reward" };
            }
            return { "reward", "learnability", "crafting" };
        }

        bool RelationshipMatchesLane(const AZStd::string& lane, const AZStd::string& kind)
        {
            if (lane == "vendor")
            {
                return kind == "sold_by";
            }
            if (lane == "loot")
            {
                return kind == "dropped_by" || kind == "found_at";
            }
            if (lane == "reward")
            {
                return kind == "rewarded_by" || kind == "granted_by";
            }
            if (lane == "learnability")
            {
                return kind == "learned_from";
            }
            if (lane == "crafting")
            {
                return kind == "crafted_at";
            }
            return false;
        }

        void CollectOpenBlockers(
            const AZStd::vector<BlockerRecord>& blockers,
            const AZStd::string& sourceSubjectRef,
            const AZStd::string& targetSubjectRef,
            EconomyAcquisitionCoverageLane& lane)
        {
            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_status != "open")
                {
                    continue;
                }
                if (blocker.m_subjectRef == sourceSubjectRef
                    || (!targetSubjectRef.empty() && blocker.m_subjectRef == targetSubjectRef))
                {
                    AddUnique(lane.m_blockerIds, blocker.m_blockerId);
                    AddUnique(lane.m_reasons, blocker.m_reason);
                }
            }
        }

        bool EvaluateEvidence(
            const CatalogRecord& sourceRecord,
            const CatalogRecord* targetRecord,
            const CatalogRelationship& relationship,
            const SourceEvidenceRegistry& sourceRegistry,
            EconomyAcquisitionCoverageLane& lane,
            bool& blocked,
            bool& partial)
        {
            if (relationship.m_evidenceIds.empty())
            {
                partial = true;
                AddUnique(lane.m_reasons, "No evidence is attached to the acquisition relationship: " + relationship.m_relationshipId);
                return false;
            }

            AZStd::vector<AZStd::string> allowedSubjects;
            AddUnique(allowedSubjects, sourceRecord.m_subjectRef);
            if (targetRecord)
            {
                AddUnique(allowedSubjects, targetRecord->m_subjectRef);
            }
            else
            {
                AddUnique(allowedSubjects, relationship.m_targetSubjectRef);
            }

            AZ::u64 validEvidenceCount = 0;
            for (const AZStd::string& evidenceId : relationship.m_evidenceIds)
            {
                AddUnique(lane.m_evidenceIds, evidenceId);
                const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    blocked = true;
                    AddUnique(lane.m_reasons, "The acquisition relationship references unknown evidence: " + evidenceId);
                    continue;
                }
                const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source)
                {
                    blocked = true;
                    AddUnique(lane.m_reasons, "The acquisition evidence source does not exist: " + evidence->m_sourceId);
                    continue;
                }
                if (evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != source->m_profileId
                    || evidence->m_gameVersion != source->m_gameVersion
                    || evidence->m_branch != source->m_branch)
                {
                    blocked = true;
                    AddUnique(lane.m_reasons, "The acquisition evidence is not bound to its source profile and build: " + evidenceId);
                    continue;
                }
                if (!Contains(allowedSubjects, evidence->m_subjectRef))
                {
                    blocked = true;
                    AddUnique(lane.m_reasons, "The acquisition evidence belongs to an unrelated subject: " + evidenceId);
                    continue;
                }
                ++validEvidenceCount;
            }

            if (validEvidenceCount == 0 && !blocked)
            {
                partial = true;
                AddUnique(lane.m_reasons, "No usable evidence supports the acquisition relationship: " + relationship.m_relationshipId);
            }
            return validEvidenceCount > 0 && !blocked;
        }

        EconomyAcquisitionCoverageLane BuildLane(
            const AZStd::string& laneName,
            const CatalogRecord& record,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers)
        {
            EconomyAcquisitionCoverageLane lane;
            lane.m_lane = laneName;

            CollectOpenBlockers(blockers, record.m_subjectRef, {}, lane);
            const bool sourceBlocked = IsHardBlocked(record) || !lane.m_blockerIds.empty();
            const bool sourcePartial = !sourceBlocked && !IsCurrentValidated(record);

            bool anyBlocked = sourceBlocked;
            bool anyPartial = sourcePartial;
            for (const CatalogRelationship& relationship : catalog.FindRelationshipsForRecord(record.m_recordId))
            {
                if (relationship.m_fromRecordId != record.m_recordId
                    || !RelationshipMatchesLane(laneName, relationship.m_relationshipKind))
                {
                    continue;
                }

                ++lane.m_relationshipCount;
                AddUnique(lane.m_relationshipIds, relationship.m_relationshipId);

                bool relationshipBlocked = sourceBlocked;
                bool relationshipPartial = sourcePartial;
                if (IsHardBlocked(relationship))
                {
                    relationshipBlocked = true;
                    AddUnique(
                        lane.m_reasons,
                        "The acquisition relationship is stale, failed, blocked, conflicted, missing references, or superseded: "
                            + relationship.m_relationshipId);
                }
                else if (!IsCurrentValidated(relationship))
                {
                    relationshipPartial = true;
                    AddUnique(lane.m_reasons, "The acquisition relationship is not yet validated and current: " + relationship.m_relationshipId);
                }

                const CatalogRecord* target = nullptr;
                AZStd::string targetSubjectRef = relationship.m_targetSubjectRef;
                if (!relationship.m_toRecordId.empty())
                {
                    target = catalog.FindByRecordId(relationship.m_toRecordId);
                    if (!target)
                    {
                        relationshipBlocked = true;
                        AddUnique(lane.m_reasons, "The acquisition target record does not exist: " + relationship.m_toRecordId);
                    }
                    else
                    {
                        targetSubjectRef = target->m_subjectRef;
                        if (IsHardBlocked(*target))
                        {
                            relationshipBlocked = true;
                            AddUnique(
                                lane.m_reasons,
                                "The acquisition target is stale, failed, blocked, conflicted, missing references, or superseded: "
                                    + target->m_recordId);
                        }
                        else if (!IsCurrentValidated(*target))
                        {
                            relationshipPartial = true;
                            AddUnique(lane.m_reasons, "The acquisition target is not yet validated and current: " + target->m_recordId);
                        }
                    }
                }
                else if (!relationship.m_targetSubjectRef.empty())
                {
                    relationshipPartial = true;
                    AddUnique(lane.m_reasons, "The acquisition target remains unresolved: " + relationship.m_targetSubjectRef);
                }
                else
                {
                    relationshipBlocked = true;
                    AddUnique(lane.m_reasons, "The acquisition relationship has no target: " + relationship.m_relationshipId);
                }

                const size_t blockerCountBefore = lane.m_blockerIds.size();
                CollectOpenBlockers(blockers, record.m_subjectRef, targetSubjectRef, lane);
                if (lane.m_blockerIds.size() > blockerCountBefore)
                {
                    relationshipBlocked = true;
                }

                const bool evidenceUsable = EvaluateEvidence(
                    record,
                    target,
                    relationship,
                    sourceRegistry,
                    lane,
                    relationshipBlocked,
                    relationshipPartial);

                if (!relationshipBlocked && !relationshipPartial && evidenceUsable)
                {
                    ++lane.m_coveredRelationshipCount;
                }
                anyBlocked = anyBlocked || relationshipBlocked;
                anyPartial = anyPartial || relationshipPartial;
            }

            if (lane.m_relationshipCount == 0)
            {
                lane.m_status = sourceBlocked ? "blocked" : "missing";
                if (sourceBlocked)
                {
                    AddUnique(lane.m_reasons, "The source catalog subject is blocked before acquisition coverage can be evaluated.");
                }
            }
            else if (anyBlocked)
            {
                lane.m_status = "blocked";
            }
            else if (!anyPartial && lane.m_coveredRelationshipCount == lane.m_relationshipCount)
            {
                lane.m_status = "covered";
            }
            else
            {
                lane.m_status = "partial";
            }

            SortUniqueValues(lane.m_relationshipIds);
            SortUniqueValues(lane.m_evidenceIds);
            SortUniqueValues(lane.m_blockerIds);
            SortUniqueValues(lane.m_reasons);
            return lane;
        }

        void CountRow(EconomyAcquisitionCoverageSummary& summary, const EconomyAcquisitionCoverageRow& row)
        {
            ++summary.m_subjectCount;
            if (row.m_overallStatus == "covered")
            {
                ++summary.m_coveredSubjectCount;
            }
            else if (row.m_overallStatus == "partial")
            {
                ++summary.m_partialSubjectCount;
            }
            else if (row.m_overallStatus == "blocked")
            {
                ++summary.m_blockedSubjectCount;
            }
            else
            {
                ++summary.m_missingSubjectCount;
            }
        }
    } // namespace

    EconomyAcquisitionCoverageSummary EconomyCoverageService::BuildAcquisitionCoverage(
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        EconomyAcquisitionCoverageSummary summary;
        for (const CatalogRecord& record : catalog.GetRecords())
        {
            if (!IsEconomySubject(record))
            {
                continue;
            }

            EconomyAcquisitionCoverageRow row;
            row.m_recordId = record.m_recordId;
            row.m_recordKind = record.m_recordKind;
            row.m_subjectRef = record.m_subjectRef;
            row.m_displayName = record.m_displayName;
            row.m_ownerPackId = record.m_ownerPackId;
            row.m_validationState = record.m_validationState;
            row.m_stalenessState = record.m_stalenessState;

            bool allCovered = true;
            bool anyBlocked = false;
            bool anyPresent = false;
            for (const AZStd::string& laneName : BuildApplicableLanes(record))
            {
                EconomyAcquisitionCoverageLane lane = BuildLane(
                    laneName,
                    record,
                    sourceRegistry,
                    catalog,
                    blockers);
                allCovered = allCovered && lane.m_status == "covered";
                anyBlocked = anyBlocked || lane.m_status == "blocked";
                anyPresent = anyPresent || lane.m_status == "covered" || lane.m_status == "partial";
                row.m_lanes.push_back(AZStd::move(lane));
            }

            if (allCovered)
            {
                row.m_overallStatus = "covered";
            }
            else if (anyBlocked)
            {
                row.m_overallStatus = "blocked";
            }
            else if (anyPresent)
            {
                row.m_overallStatus = "partial";
            }
            else
            {
                row.m_overallStatus = "missing";
            }
            summary.m_rows.push_back(AZStd::move(row));
        }

        AZStd::sort(
            summary.m_rows.begin(),
            summary.m_rows.end(),
            [](const EconomyAcquisitionCoverageRow& left, const EconomyAcquisitionCoverageRow& right)
            {
                return left.m_recordId < right.m_recordId;
            });
        for (const EconomyAcquisitionCoverageRow& row : summary.m_rows)
        {
            CountRow(summary, row);
        }
        return summary;
    }
} // namespace TaintedGrailModdingSDK
