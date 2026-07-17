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
            for (const AZStd::string& candidate : values)
            {
                if (candidate == value)
                {
                    return true;
                }
            }
            return false;
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
        CatalogRecord* replacing = nullptr;
        for (CatalogRecord& existing : m_records)
        {
            if (existing.m_recordId == record.m_recordId)
            {
                replacing = &existing;
                break;
            }
        }

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

        for (CatalogRelationship& existing : m_relationships)
        {
            if (existing.m_relationshipId == relationship.m_relationshipId)
            {
                existing = relationship;
                return true;
            }
        }
        m_relationships.push_back(relationship);
        return true;
    }

    bool CatalogDatabase::AddValidationEvent(const CatalogValidationEvent& validation, AZStd::string* error)
    {
        if (validation.m_validationId.empty() || validation.m_recordId.empty() || validation.m_state.empty()
            || validation.m_method.empty() || validation.m_checkedAt.empty())
        {
            if (error)
            {
                *error = "Validation history requires validation ID, record ID, state, method, and check time.";
            }
            return false;
        }
        if (!FindByRecordId(validation.m_recordId))
        {
            if (error)
            {
                *error = "Validation history references an unknown catalog record.";
            }
            return false;
        }
        for (const CatalogValidationEvent& existing : m_validationHistory)
        {
            if (existing.m_validationId == validation.m_validationId)
            {
                if (error)
                {
                    *error = "Validation history ID already exists.";
                }
                return false;
            }
        }
        m_validationHistory.push_back(validation);
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

        *this = AZStd::move(candidate);
        return true;
    }

    void CatalogDatabase::Clear()
    {
        m_records.clear();
        m_relationships.clear();
        m_validationHistory.clear();
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
            if (!query.m_confidence.empty() && record.m_confidence != query.m_confidence)
            {
                continue;
            }
            if (!query.m_validationState.empty() && record.m_validationState != query.m_validationState)
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
        AZStd::vector<CatalogValidationEvent> matches;
        for (const CatalogValidationEvent& validation : m_validationHistory)
        {
            if (validation.m_recordId == recordId)
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
        if (record.m_identityKind == "native" && record.m_nativeRefExact.empty())
        {
            if (error)
            {
                *error = "Native catalog records require an exact native reference.";
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
        if (record.m_evidenceIds.empty())
        {
            if (error)
            {
                *error = "Canonical catalog records require at least one evidence ID.";
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
        return true;
    }
} // namespace TaintedGrailModdingSDK
