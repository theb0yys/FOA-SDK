/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

#include <AzCore/std/algorithm.h>

#include <cctype>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool Contains(const AZStd::vector<AZStd::string>& values, const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        AZStd::string LowerAscii(AZStd::string value)
        {
            AZStd::transform(value.begin(), value.end(), value.begin(), [](char character)
            {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            });
            return value;
        }

        bool ContainsText(const AZStd::string& value, const AZStd::string& loweredNeedle)
        {
            return loweredNeedle.empty() || LowerAscii(value).find(loweredNeedle) != AZStd::string::npos;
        }

        bool AnyContainsText(const AZStd::vector<AZStd::string>& values, const AZStd::string& loweredNeedle)
        {
            for (const AZStd::string& value : values)
            {
                if (ContainsText(value, loweredNeedle))
                {
                    return true;
                }
            }
            return false;
        }

        bool IsKnownResearchStage(const AZStd::string& value)
        {
            return value.empty() || value == "unknown"
                || value == "S0" || value == "S1" || value == "S2" || value == "S3" || value == "S4"
                || value == "S5" || value == "S6" || value == "S7" || value == "S8"
                || value == "reviewed" || value == "reconciled" || value == "validated"
                || value == "authoring_ready" || value == "runtime_approved";
        }

        bool IsKnownConfidence(const AZStd::string& value)
        {
            return value.empty() || value == "unknown" || value == "unrated" || value == "hypothesis"
                || value == "inferred" || value == "documented" || value == "runtime_observed"
                || value == "validated";
        }

        bool IsKnownOperationalRisk(const AZStd::string& value)
        {
            return value.empty() || value == "unknown" || value == "low" || value == "medium"
                || value == "high" || value == "critical";
        }

        bool IsKnownValidationState(const AZStd::string& value)
        {
            return value.empty() || value == "unvalidated" || value == "pending" || value == "validated"
                || value == "failed" || value == "stale" || value == "blocked";
        }

        bool IsKnownStalenessState(const AZStd::string& value)
        {
            return value.empty() || value == "unknown" || value == "current"
                || value == "potentially_stale" || value == "stale";
        }

        bool HasPermissionConflict(
            const AZStd::vector<AZStd::string>& allowed,
            const AZStd::vector<AZStd::string>& forbidden)
        {
            for (const AZStd::string& usage : allowed)
            {
                if (Contains(forbidden, usage))
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    bool CatalogDatabase::InsertNew(const CatalogRecord& record, AZStd::string* error)
    {
        if (FindByRecordId(record.m_recordId))
        {
            if (error)
            {
                *error = "Catalog record ID already exists; promotion never merges by display name.";
            }
            return false;
        }
        if (!ValidateRecord(record, nullptr, error))
        {
            return false;
        }

        m_records.push_back(record);
        return true;
    }

    bool CatalogDatabase::Upsert(const CatalogRecord& record, AZStd::string* error)
    {
        CatalogRecord* replacing = FindMutableRecordById(record.m_recordId);
        if (!ValidateRecord(record, replacing, error))
        {
            return false;
        }

        if (replacing)
        {
            *replacing = record;
        }
        else
        {
            m_records.push_back(record);
        }
        return true;
    }

    bool CatalogDatabase::UpsertRelationship(const CatalogRelationship& relationship, AZStd::string* error)
    {
        if (!ValidateRelationship(relationship, error))
        {
            return false;
        }

        if (CatalogRelationship* existing = FindMutableRelationshipById(relationship.m_relationshipId))
        {
            *existing = relationship;
        }
        else
        {
            m_relationships.push_back(relationship);
        }
        return true;
    }

    bool CatalogDatabase::AddValidationEvent(const CatalogValidationEvent& validation, AZStd::string* error)
    {
        const AZStd::string subjectKind = validation.GetSubjectKind();
        const AZStd::string subjectId = validation.GetSubjectId();
        if (validation.m_validationId.empty() || subjectId.empty() || validation.m_state.empty()
            || validation.m_method.empty() || validation.m_checkedAt.empty() || validation.m_validator.empty())
        {
            if (error)
            {
                *error = "Validation history requires validation ID, subject, state, method, validator, and check time.";
            }
            return false;
        }
        if (subjectKind != "record" && subjectKind != "relationship")
        {
            if (error)
            {
                *error = "Validation subject kind must be record or relationship.";
            }
            return false;
        }
        if ((subjectKind == "record" && !FindByRecordId(subjectId))
            || (subjectKind == "relationship" && !FindRelationshipById(subjectId)))
        {
            if (error)
            {
                *error = "Validation history references an unknown catalog subject.";
            }
            return false;
        }
        if (!IsKnownValidationState(validation.m_state))
        {
            if (error)
            {
                *error = "Validation history contains an unsupported validation state.";
            }
            return false;
        }
        if (validation.m_evidenceIds.empty())
        {
            if (error)
            {
                *error = "Validation history requires evidence IDs.";
            }
            return false;
        }
        if (FindValidationById(validation.m_validationId))
        {
            if (error)
            {
                *error = "Validation history ID already exists.";
            }
            return false;
        }
        m_validationHistory.push_back(validation);
        return true;
    }

    bool CatalogDatabase::AddGovernanceEvent(const CatalogGovernanceEvent& event, AZStd::string* error)
    {
        if (!ValidateGovernanceEvent(event, error))
        {
            return false;
        }
        for (const CatalogGovernanceEvent& existing : m_governanceHistory)
        {
            if (existing.m_eventId == event.m_eventId)
            {
                if (error)
                {
                    *error = "Catalog governance event ID already exists.";
                }
                return false;
            }
        }
        m_governanceHistory.push_back(event);
        return true;
    }

    bool CatalogDatabase::ReplaceFromDocument(const CatalogDocument& document, AZStd::string* error)
    {
        if (!document.UsesSupportedSchema())
        {
            if (error)
            {
                *error = "Unsupported canonical catalog schema version.";
            }
            return false;
        }

        CatalogDatabase candidate;
        for (const CatalogRecord& record : document.m_records)
        {
            if (!candidate.InsertNew(record, error))
            {
                return false;
            }
        }
        for (const CatalogRelationship& relationship : document.m_relationships)
        {
            if (!candidate.UpsertRelationship(relationship, error))
            {
                return false;
            }
        }
        for (const CatalogValidationEvent& validation : document.m_validationHistory)
        {
            if (!candidate.AddValidationEvent(validation, error))
            {
                return false;
            }
        }
        for (const CatalogGovernanceEvent& event : document.m_governanceHistory)
        {
            if (!candidate.AddGovernanceEvent(event, error))
            {
                return false;
            }
        }

        *this = AZStd::move(candidate);
        return true;
    }

    void CatalogDatabase::Clear()
    {
        m_records.clear();
        m_relationships.clear();
        m_validationHistory.clear();
        m_governanceHistory.clear();
    }

    CatalogRecord* CatalogDatabase::FindMutableRecordById(const AZStd::string& recordId)
    {
        for (CatalogRecord& record : m_records)
        {
            if (record.m_recordId == recordId)
            {
                return &record;
            }
        }
        return nullptr;
    }

    CatalogRelationship* CatalogDatabase::FindMutableRelationshipById(const AZStd::string& relationshipId)
    {
        for (CatalogRelationship& relationship : m_relationships)
        {
            if (relationship.m_relationshipId == relationshipId)
            {
                return &relationship;
            }
        }
        return nullptr;
    }

    const CatalogRecord* CatalogDatabase::FindByRecordId(const AZStd::string& recordId) const
    {
        for (const CatalogRecord& record : m_records)
        {
            if (record.m_recordId == recordId)
            {
                return &record;
            }
        }
        return nullptr;
    }

    const CatalogRecord* CatalogDatabase::FindByExactNativeRef(const AZStd::string& nativeRefExact) const
    {
        for (const CatalogRecord& record : m_records)
        {
            if (!nativeRefExact.empty() && record.m_nativeRefExact == nativeRefExact)
            {
                return &record;
            }
        }
        return nullptr;
    }

    const CatalogRelationship* CatalogDatabase::FindRelationshipById(const AZStd::string& relationshipId) const
    {
        for (const CatalogRelationship& relationship : m_relationships)
        {
            if (relationship.m_relationshipId == relationshipId)
            {
                return &relationship;
            }
        }
        return nullptr;
    }

    const CatalogValidationEvent* CatalogDatabase::FindValidationById(const AZStd::string& validationId) const
    {
        for (const CatalogValidationEvent& validation : m_validationHistory)
        {
            if (validation.m_validationId == validationId)
            {
                return &validation;
            }
        }
        return nullptr;
    }

    AZStd::vector<CatalogRecord> CatalogDatabase::Query(const CatalogQuery& query) const
    {
        AZStd::vector<CatalogRecord> matches;
        const AZStd::string loweredSearch = LowerAscii(query.m_searchText);

        for (const CatalogRecord& record : m_records)
        {
            if (!query.m_includeSuperseded && !record.m_supersededByRecordId.empty())
            {
                continue;
            }
            if (!query.m_recordId.empty() && record.m_recordId != query.m_recordId)
            {
                continue;
            }
            if (!query.m_ownerPackId.empty() && record.m_ownerPackId != query.m_ownerPackId)
            {
                continue;
            }
            if (!query.m_domain.empty() && record.m_domain != query.m_domain)
            {
                continue;
            }
            if (!query.m_recordKind.empty() && record.m_recordKind != query.m_recordKind)
            {
                continue;
            }
            if (!query.m_subjectRef.empty() && record.m_subjectRef != query.m_subjectRef)
            {
                continue;
            }
            if (!query.m_nativeRefExact.empty() && record.m_nativeRefExact != query.m_nativeRefExact)
            {
                continue;
            }
            if (!query.m_identityKind.empty() && record.m_identityKind != query.m_identityKind)
            {
                continue;
            }
            if (!query.m_researchStage.empty() && record.m_researchStage != query.m_researchStage)
            {
                continue;
            }
            if (!query.m_confidence.empty() && record.m_confidence != query.m_confidence)
            {
                continue;
            }
            if (!query.m_operationalRisk.empty() && record.m_operationalRisk != query.m_operationalRisk)
            {
                continue;
            }
            if (!query.m_validationState.empty() && record.m_validationState != query.m_validationState)
            {
                continue;
            }
            if (!query.m_stalenessState.empty() && record.m_stalenessState != query.m_stalenessState)
            {
                continue;
            }
            if (!query.m_permission.empty()
                && !Contains(record.m_allowedUsages, query.m_permission)
                && !Contains(record.m_forbiddenUsages, query.m_permission))
            {
                continue;
            }
            if (!query.m_evidenceId.empty() && !Contains(record.m_evidenceIds, query.m_evidenceId))
            {
                continue;
            }
            if (query.m_blockedOnly && !record.IsBlocked())
            {
                continue;
            }

            if (!loweredSearch.empty())
            {
                const bool textMatch = ContainsText(record.m_recordId, loweredSearch)
                    || ContainsText(record.m_ownerPackId, loweredSearch)
                    || ContainsText(record.m_domain, loweredSearch)
                    || ContainsText(record.m_recordKind, loweredSearch)
                    || ContainsText(record.m_subjectRef, loweredSearch)
                    || ContainsText(record.m_nativeRefExact, loweredSearch)
                    || ContainsText(record.m_displayName, loweredSearch)
                    || AnyContainsText(record.m_aliases, loweredSearch)
                    || AnyContainsText(record.m_sourceScopedRefs, loweredSearch)
                    || AnyContainsText(record.m_tags, loweredSearch);
                if (!textMatch)
                {
                    continue;
                }
            }

            matches.push_back(record);
        }

        AZStd::sort(matches.begin(), matches.end(), [](const CatalogRecord& left, const CatalogRecord& right)
        {
            return left.m_recordId < right.m_recordId;
        });
        return matches;
    }

    AZStd::vector<CatalogRelationship> CatalogDatabase::FindRelationshipsForRecord(const AZStd::string& recordId) const
    {
        AZStd::vector<CatalogRelationship> matches;
        for (const CatalogRelationship& relationship : m_relationships)
        {
            if (relationship.m_fromRecordId == recordId || relationship.m_toRecordId == recordId)
            {
                matches.push_back(relationship);
            }
        }
        AZStd::sort(matches.begin(), matches.end(), [](const CatalogRelationship& left, const CatalogRelationship& right)
        {
            return left.m_relationshipId < right.m_relationshipId;
        });
        return matches;
    }

    AZStd::vector<CatalogValidationEvent> CatalogDatabase::FindValidationForRecord(const AZStd::string& recordId) const
    {
        return FindValidationForSubject("record", recordId);
    }

    AZStd::vector<CatalogValidationEvent> CatalogDatabase::FindValidationForSubject(
        const AZStd::string& subjectKind,
        const AZStd::string& subjectId) const
    {
        AZStd::vector<CatalogValidationEvent> matches;
        for (const CatalogValidationEvent& validation : m_validationHistory)
        {
            if (validation.GetSubjectKind() == subjectKind && validation.GetSubjectId() == subjectId)
            {
                matches.push_back(validation);
            }
        }
        AZStd::sort(matches.begin(), matches.end(), [](const CatalogValidationEvent& left, const CatalogValidationEvent& right)
        {
            if (left.m_checkedAt == right.m_checkedAt)
            {
                return left.m_validationId < right.m_validationId;
            }
            return left.m_checkedAt < right.m_checkedAt;
        });
        return matches;
    }

    AZStd::vector<CatalogGovernanceEvent> CatalogDatabase::FindGovernanceForSubject(
        const AZStd::string& subjectKind,
        const AZStd::string& subjectId) const
    {
        AZStd::vector<CatalogGovernanceEvent> matches;
        for (const CatalogGovernanceEvent& event : m_governanceHistory)
        {
            if (event.m_subjectKind == subjectKind && event.m_subjectId == subjectId)
            {
                matches.push_back(event);
            }
        }
        AZStd::sort(matches.begin(), matches.end(), [](const CatalogGovernanceEvent& left, const CatalogGovernanceEvent& right)
        {
            if (left.m_decidedAt == right.m_decidedAt)
            {
                return left.m_eventId < right.m_eventId;
            }
            return left.m_decidedAt < right.m_decidedAt;
        });
        return matches;
    }

    AZStd::vector<DomainCoverage> CatalogDatabase::BuildCoverage() const
    {
        AZStd::vector<DomainCoverage> coverage;
        for (const CatalogRecord& record : m_records)
        {
            DomainCoverage* domainCoverage = nullptr;
            for (DomainCoverage& candidate : coverage)
            {
                if (candidate.m_domain == record.m_domain)
                {
                    domainCoverage = &candidate;
                    break;
                }
            }
            if (!domainCoverage)
            {
                DomainCoverage newCoverage;
                newCoverage.m_domain = record.m_domain;
                coverage.push_back(newCoverage);
                domainCoverage = &coverage.back();
            }

            ++domainCoverage->m_recordCount;
            if (record.IsBlocked())
            {
                ++domainCoverage->m_blockedRecordCount;
            }
        }
        AZStd::sort(coverage.begin(), coverage.end(), [](const DomainCoverage& left, const DomainCoverage& right)
        {
            return left.m_domain < right.m_domain;
        });
        return coverage;
    }

    CatalogDocument CatalogDatabase::BuildDocument(
        const WorkspaceModel& workspace,
        const GameProfile& profile) const
    {
        CatalogDocument document;
        document.m_workspaceId = workspace.m_workspaceId;
        document.m_profileId = profile.m_profileId;
        document.m_gameVersion = profile.m_gameVersion;
        document.m_branch = profile.m_branch;
        document.m_records = m_records;
        document.m_relationships = m_relationships;
        document.m_validationHistory = m_validationHistory;
        document.m_governanceHistory = m_governanceHistory;
        return document;
    }

    const AZStd::vector<CatalogRecord>& CatalogDatabase::GetRecords() const
    {
        return m_records;
    }

    const AZStd::vector<CatalogRelationship>& CatalogDatabase::GetRelationships() const
    {
        return m_relationships;
    }

    const AZStd::vector<CatalogValidationEvent>& CatalogDatabase::GetValidationHistory() const
    {
        return m_validationHistory;
    }

    const AZStd::vector<CatalogGovernanceEvent>& CatalogDatabase::GetGovernanceHistory() const
    {
        return m_governanceHistory;
    }

    bool CatalogDatabase::ValidateRecord(
        const CatalogRecord& record,
        const CatalogRecord* replacing,
        AZStd::string* error) const
    {
        if (record.m_recordId.empty() || record.m_subjectRef.empty() || record.m_domain.empty()
            || record.m_recordKind.empty() || record.m_identityKind.empty())
        {
            if (error)
            {
                *error = "Catalog records require record ID, subject ref, domain, record kind, and identity kind.";
            }
            return false;
        }
        if (record.m_identityKind != "native" && record.m_identityKind != "synthetic"
            && record.m_identityKind != "composite" && record.m_identityKind != "source_scoped")
        {
            if (error)
            {
                *error = "Catalog identity kind must be native, synthetic, composite, or source_scoped.";
            }
            return false;
        }
        if (record.m_identityKind == "native" && record.m_nativeRefExact.empty())
        {
            if (error)
            {
                *error = "Native catalog records require an exact native reference.";
            }
            return false;
        }
        if (record.m_identityKind == "native" && !record.m_ownerPackId.empty())
        {
            if (error)
            {
                *error = "Native catalog records must not claim custom pack ownership.";
            }
            return false;
        }
        if (record.IsSynthetic() && record.m_ownerPackId.empty())
        {
            if (error)
            {
                *error = "Synthetic catalog records require an owning pack ID.";
            }
            return false;
        }
        if (record.IsSynthetic() && !record.m_nativeRefExact.empty())
        {
            if (error)
            {
                *error = "Synthetic catalog records must not borrow an exact native reference.";
            }
            return false;
        }
        if (record.m_evidenceIds.empty())
        {
            if (error)
            {
                *error = "Canonical catalog records require at least one evidence ID.";
            }
            return false;
        }
        if (!IsKnownResearchStage(record.m_researchStage) || !IsKnownConfidence(record.m_confidence)
            || !IsKnownOperationalRisk(record.m_operationalRisk)
            || !IsKnownValidationState(record.m_validationState)
            || !IsKnownStalenessState(record.m_stalenessState))
        {
            if (error)
            {
                *error = "Catalog record contains an unsupported maturity, confidence, risk, validation, or staleness value.";
            }
            return false;
        }
        if (HasPermissionConflict(record.m_allowedUsages, record.m_forbiddenUsages))
        {
            if (error)
            {
                *error = "A catalog usage cannot be both allowed and forbidden.";
            }
            return false;
        }
        if (!record.m_allowedUsages.empty()
            && (record.m_validationState != "validated" || record.m_stalenessState != "current"
                || !record.m_missingRefs.empty() || !record.m_conflictRefs.empty()
                || !record.m_supersededByRecordId.empty()))
        {
            if (error)
            {
                *error = "Allowed usages require a validated, current, unresolved-free, non-superseded catalog record.";
            }
            return false;
        }
        if (!record.m_nativeRefExact.empty())
        {
            const CatalogRecord* existingNative = FindByExactNativeRef(record.m_nativeRefExact);
            if (existingNative && existingNative != replacing && existingNative->m_recordId != record.m_recordId)
            {
                if (error)
                {
                    *error = "Exact native reference is already owned by another canonical catalog record.";
                }
                return false;
            }
        }
        return true;
    }

    bool CatalogDatabase::ValidateRelationship(
        const CatalogRelationship& relationship,
        AZStd::string* error) const
    {
        if (relationship.m_relationshipId.empty() || relationship.m_fromRecordId.empty()
            || relationship.m_relationshipKind.empty())
        {
            if (error)
            {
                *error = "Catalog relationships require relationship ID, source record, and relationship kind.";
            }
            return false;
        }
        if (!FindByRecordId(relationship.m_fromRecordId))
        {
            if (error)
            {
                *error = "Catalog relationship source record does not exist.";
            }
            return false;
        }
        if (relationship.m_toRecordId.empty() && relationship.m_targetSubjectRef.empty())
        {
            if (error)
            {
                *error = "Catalog relationships require a target record ID or target subject reference.";
            }
            return false;
        }
        if (!relationship.m_toRecordId.empty() && !FindByRecordId(relationship.m_toRecordId))
        {
            if (error)
            {
                *error = "Catalog relationship target record does not exist.";
            }
            return false;
        }
        if (relationship.m_evidenceIds.empty())
        {
            if (error)
            {
                *error = "Canonical catalog relationships require at least one evidence ID.";
            }
            return false;
        }
        if (!IsKnownResearchStage(relationship.m_researchStage)
            || !IsKnownConfidence(relationship.m_confidence)
            || !IsKnownOperationalRisk(relationship.m_operationalRisk)
            || !IsKnownValidationState(relationship.m_validationState)
            || !IsKnownStalenessState(relationship.m_stalenessState))
        {
            if (error)
            {
                *error = "Catalog relationship contains an unsupported maturity, confidence, risk, validation, or staleness value.";
            }
            return false;
        }
        if (HasPermissionConflict(relationship.m_allowedUsages, relationship.m_forbiddenUsages))
        {
            if (error)
            {
                *error = "A relationship usage cannot be both allowed and forbidden.";
            }
            return false;
        }
        if (!relationship.m_allowedUsages.empty()
            && (relationship.m_validationState != "validated" || relationship.m_stalenessState != "current"
                || !relationship.m_missingRefs.empty() || !relationship.m_conflictRefs.empty()
                || !relationship.m_supersededByRelationshipId.empty()))
        {
            if (error)
            {
                *error = "Allowed usages require a validated, current, unresolved-free, non-superseded relationship.";
            }
            return false;
        }
        return true;
    }

    bool CatalogDatabase::ValidateGovernanceEvent(
        const CatalogGovernanceEvent& event,
        AZStd::string* error) const
    {
        if (event.m_eventId.empty() || event.m_subjectKind.empty() || event.m_subjectId.empty()
            || event.m_axis.empty() || event.m_reviewer.empty() || event.m_decidedAt.empty())
        {
            if (error)
            {
                *error = "Governance history requires event ID, subject, axis, reviewer, and decision time.";
            }
            return false;
        }
        if (event.m_subjectKind != "record" && event.m_subjectKind != "relationship")
        {
            if (error)
            {
                *error = "Governance subject kind must be record or relationship.";
            }
            return false;
        }
        if ((event.m_subjectKind == "record" && !FindByRecordId(event.m_subjectId))
            || (event.m_subjectKind == "relationship" && !FindRelationshipById(event.m_subjectId)))
        {
            if (error)
            {
                *error = "Governance history references an unknown catalog subject.";
            }
            return false;
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
