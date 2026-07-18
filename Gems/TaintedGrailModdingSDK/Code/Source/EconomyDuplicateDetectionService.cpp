/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "EconomyDuplicateDetectionService.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct DuplicateBucket
        {
            AZStd::string m_signal;
            AZStd::string m_matchKey;
            AZStd::string m_recordKind;
            AZStd::vector<const CatalogRecord*> m_records;
        };

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

        void SortUniqueValues(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        bool IsPackOwnedEconomyRecord(const CatalogRecord& record)
        {
            return record.m_domain == "economy"
                && (record.m_recordKind == "item" || record.m_recordKind == "recipe")
                && !record.m_ownerPackId.empty();
        }

        bool IsCurrentValidated(const CatalogRecord& record)
        {
            return record.m_validationState == "validated"
                && record.m_stalenessState == "current"
                && record.m_missingRefs.empty()
                && record.m_conflictRefs.empty()
                && record.m_supersededByRecordId.empty();
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

        DuplicateBucket& FindOrAddBucket(
            AZStd::vector<DuplicateBucket>& buckets,
            const AZStd::string& signal,
            const AZStd::string& matchKey,
            const AZStd::string& recordKind)
        {
            for (DuplicateBucket& bucket : buckets)
            {
                if (bucket.m_signal == signal
                    && bucket.m_matchKey == matchKey
                    && bucket.m_recordKind == recordKind)
                {
                    return bucket;
                }
            }

            DuplicateBucket bucket;
            bucket.m_signal = signal;
            bucket.m_matchKey = matchKey;
            bucket.m_recordKind = recordKind;
            buckets.push_back(AZStd::move(bucket));
            return buckets.back();
        }

        void AddRecordToBucket(DuplicateBucket& bucket, const CatalogRecord& record)
        {
            for (const CatalogRecord* existing : bucket.m_records)
            {
                if (existing->m_recordId == record.m_recordId)
                {
                    return;
                }
            }
            bucket.m_records.push_back(&record);
        }

        AZStd::vector<AZStd::string> ProfileEvidenceIds(
            const CatalogRecord& record,
            const CatalogDatabase& catalog,
            bool& profileMissing)
        {
            profileMissing = false;
            if (record.m_recordKind == "item")
            {
                const EconomyItemProfile* profile = catalog.FindEconomyItem(record.m_recordId);
                if (!profile)
                {
                    profileMissing = true;
                    return {};
                }
                return profile->m_evidenceIds;
            }

            const EconomyRecipeProfile* profile = catalog.FindEconomyRecipe(record.m_recordId);
            if (!profile)
            {
                profileMissing = true;
                return {};
            }
            return profile->m_evidenceIds;
        }

        AZStd::string RecipeDuplicateKey(const CatalogRecord& record, const CatalogDatabase& catalog)
        {
            if (record.m_recordKind != "recipe")
            {
                return {};
            }
            const EconomyRecipeProfile* profile = catalog.FindEconomyRecipe(record.m_recordId);
            return profile ? profile->m_duplicateKey : AZStd::string{};
        }

        void CollectOpenBlockers(
            const CatalogRecord& record,
            const AZStd::vector<BlockerRecord>& blockers,
            EconomyDuplicateCandidate& candidate)
        {
            for (const BlockerRecord& blocker : blockers)
            {
                if (blocker.m_status == "open" && blocker.m_subjectRef == record.m_subjectRef)
                {
                    AddUnique(candidate.m_blockerIds, blocker.m_blockerId);
                    AddUnique(candidate.m_reasons, blocker.m_reason);
                }
            }
        }

        void EvaluateEvidence(
            const CatalogRecord& record,
            const SourceEvidenceRegistry& sourceRegistry,
            EconomyDuplicateCandidate& candidate,
            bool& blocked)
        {
            if (candidate.m_evidenceIds.empty())
            {
                blocked = true;
                AddUnique(
                    candidate.m_reasons,
                    "Duplicate analysis requires evidence for the pack-owned economy record.");
                return;
            }

            AZ::u64 validEvidenceCount = 0;
            for (const AZStd::string& evidenceId : candidate.m_evidenceIds)
            {
                const EvidenceRecord* evidence = sourceRegistry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    blocked = true;
                    AddUnique(candidate.m_reasons, "The duplicate signal references unknown evidence: " + evidenceId);
                    continue;
                }

                const SourceRecord* source = sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source)
                {
                    blocked = true;
                    AddUnique(
                        candidate.m_reasons,
                        "The duplicate evidence source does not exist: " + evidence->m_sourceId);
                    continue;
                }

                if (evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != source->m_profileId
                    || evidence->m_gameVersion != source->m_gameVersion
                    || evidence->m_branch != source->m_branch)
                {
                    blocked = true;
                    AddUnique(
                        candidate.m_reasons,
                        "The duplicate evidence is not bound to its source profile and build: " + evidenceId);
                    continue;
                }

                if (evidence->m_subjectRef != record.m_subjectRef)
                {
                    blocked = true;
                    AddUnique(
                        candidate.m_reasons,
                        "The duplicate evidence belongs to an unrelated subject: " + evidenceId);
                    continue;
                }
                ++validEvidenceCount;
            }

            if (validEvidenceCount == 0)
            {
                blocked = true;
                AddUnique(candidate.m_reasons, "No usable evidence supports this duplicate candidate.");
            }
        }

        EconomyDuplicateCandidate BuildCandidate(
            const CatalogRecord& record,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers)
        {
            EconomyDuplicateCandidate candidate;
            candidate.m_recordId = record.m_recordId;
            candidate.m_recordKind = record.m_recordKind;
            candidate.m_ownerPackId = record.m_ownerPackId;
            candidate.m_subjectRef = record.m_subjectRef;
            candidate.m_duplicateKey = RecipeDuplicateKey(record, catalog);
            candidate.m_validationState = record.m_validationState;
            candidate.m_stalenessState = record.m_stalenessState;
            candidate.m_evidenceIds = record.m_evidenceIds;

            bool profileMissing = false;
            AddAllUnique(candidate.m_evidenceIds, ProfileEvidenceIds(record, catalog, profileMissing));

            bool blocked = IsHardBlocked(record);
            bool partial = !blocked && !IsCurrentValidated(record);
            if (blocked)
            {
                AddUnique(
                    candidate.m_reasons,
                    "The duplicate candidate is stale, failed, blocked, conflicted, missing references, "
                    "or superseded.");
            }
            else if (partial)
            {
                AddUnique(candidate.m_reasons, "The duplicate candidate is not yet validated and current.");
            }

            if (profileMissing)
            {
                partial = true;
                AddUnique(candidate.m_reasons, "The duplicate candidate has no typed economy profile.");
            }

            CollectOpenBlockers(record, blockers, candidate);
            blocked = blocked || !candidate.m_blockerIds.empty();
            EvaluateEvidence(record, sourceRegistry, candidate, blocked);

            candidate.m_status = blocked ? "blocked" : (partial ? "partial" : "review_ready");
            SortUniqueValues(candidate.m_evidenceIds);
            SortUniqueValues(candidate.m_blockerIds);
            SortUniqueValues(candidate.m_reasons);
            return candidate;
        }

        bool HasDistinctPackOwners(const DuplicateBucket& bucket)
        {
            AZStd::vector<AZStd::string> packIds;
            for (const CatalogRecord* record : bucket.m_records)
            {
                AddUnique(packIds, record->m_ownerPackId);
            }
            return packIds.size() >= 2;
        }

        EconomyDuplicateGroup BuildGroup(
            const DuplicateBucket& bucket,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers)
        {
            EconomyDuplicateGroup group;
            group.m_signal = bucket.m_signal;
            group.m_matchKey = bucket.m_matchKey;
            group.m_recordKind = bucket.m_recordKind;

            bool anyBlocked = false;
            bool anyPartial = false;
            for (const CatalogRecord* record : bucket.m_records)
            {
                EconomyDuplicateCandidate candidate = BuildCandidate(
                    *record,
                    sourceRegistry,
                    catalog,
                    blockers);
                anyBlocked = anyBlocked || candidate.m_status == "blocked";
                anyPartial = anyPartial || candidate.m_status == "partial";
                AddUnique(group.m_packIds, candidate.m_ownerPackId);
                AddUnique(group.m_recordIds, candidate.m_recordId);
                AddAllUnique(group.m_evidenceIds, candidate.m_evidenceIds);
                AddAllUnique(group.m_blockerIds, candidate.m_blockerIds);
                AddAllUnique(group.m_reasons, candidate.m_reasons);
                group.m_candidates.push_back(AZStd::move(candidate));
            }

            AddUnique(
                group.m_reasons,
                bucket.m_signal == "subject_ref"
                    ? "The exact economy subject reference is declared by multiple owner packs."
                    : "The exact recipe duplicate key is declared by multiple owner packs.");

            group.m_status = anyBlocked ? "blocked" : (anyPartial ? "partial" : "review_required");
            SortUniqueValues(group.m_packIds);
            SortUniqueValues(group.m_recordIds);
            SortUniqueValues(group.m_evidenceIds);
            SortUniqueValues(group.m_blockerIds);
            SortUniqueValues(group.m_reasons);
            AZStd::sort(
                group.m_candidates.begin(),
                group.m_candidates.end(),
                [](const EconomyDuplicateCandidate& left, const EconomyDuplicateCandidate& right)
                {
                    if (left.m_ownerPackId == right.m_ownerPackId)
                    {
                        return left.m_recordId < right.m_recordId;
                    }
                    return left.m_ownerPackId < right.m_ownerPackId;
                });
            return group;
        }
    } // namespace

    EconomyDuplicateReport EconomyDuplicateDetectionService::BuildCrossPackDuplicateReport(
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog,
        const AZStd::vector<BlockerRecord>& blockers) const
    {
        EconomyDuplicateReport report;
        AZStd::vector<DuplicateBucket> buckets;

        for (const CatalogRecord& record : catalog.GetRecords())
        {
            if (!IsPackOwnedEconomyRecord(record))
            {
                continue;
            }

            ++report.m_scannedPackOwnedRecordCount;
            AddRecordToBucket(
                FindOrAddBucket(buckets, "subject_ref", record.m_subjectRef, record.m_recordKind),
                record);

            if (record.m_recordKind == "recipe")
            {
                const EconomyRecipeProfile* profile = catalog.FindEconomyRecipe(record.m_recordId);
                if (profile && !profile->m_duplicateKey.empty())
                {
                    AddRecordToBucket(
                        FindOrAddBucket(buckets, "recipe_duplicate_key", profile->m_duplicateKey, "recipe"),
                        record);
                }
            }
        }

        for (const DuplicateBucket& bucket : buckets)
        {
            if (!HasDistinctPackOwners(bucket))
            {
                continue;
            }

            EconomyDuplicateGroup group = BuildGroup(bucket, sourceRegistry, catalog, blockers);
            if (group.m_status == "blocked")
            {
                ++report.m_blockedGroupCount;
            }
            else if (group.m_status == "partial")
            {
                ++report.m_partialGroupCount;
            }
            else
            {
                ++report.m_reviewRequiredGroupCount;
            }
            report.m_groups.push_back(AZStd::move(group));
        }

        AZStd::sort(
            report.m_groups.begin(),
            report.m_groups.end(),
            [](const EconomyDuplicateGroup& left, const EconomyDuplicateGroup& right)
            {
                if (left.m_signal != right.m_signal)
                {
                    return left.m_signal < right.m_signal;
                }
                if (left.m_recordKind != right.m_recordKind)
                {
                    return left.m_recordKind < right.m_recordKind;
                }
                return left.m_matchKey < right.m_matchKey;
            });
        report.m_duplicateGroupCount = report.m_groups.size();
        return report;
    }
} // namespace TaintedGrailModdingSDK
