/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogDatabase.h"

namespace TaintedGrailModdingSDK
{
    bool CatalogDatabase::Upsert(const CatalogRecord& record, AZStd::string* error)
    {
        if (record.m_recordId.empty() || record.m_subjectRef.empty() || record.m_domain.empty() || record.m_recordKind.empty())
        {
            if (error)
            {
                *error = "Catalog records require record ID, subject ref, domain, and record kind.";
            }
            return false;
        }

        for (CatalogRecord& existing : m_records)
        {
            if (existing.m_recordId == record.m_recordId)
            {
                existing = record;
                return true;
            }
        }

        m_records.push_back(record);
        return true;
    }

    void CatalogDatabase::Clear()
    {
        m_records.clear();
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

    AZStd::vector<CatalogRecord> CatalogDatabase::Query(const CatalogQuery& query) const
    {
        AZStd::vector<CatalogRecord> matches;
        for (const CatalogRecord& record : m_records)
        {
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
            if (query.m_blockedOnly && record.m_missingRefs.empty() && record.m_forbiddenUsages.empty())
            {
                continue;
            }
            matches.push_back(record);
        }
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
            if (!record.m_missingRefs.empty() || !record.m_forbiddenUsages.empty())
            {
                ++domainCoverage->m_blockedRecordCount;
            }
        }
        return coverage;
    }

    const AZStd::vector<CatalogRecord>& CatalogDatabase::GetRecords() const
    {
        return m_records;
    }
} // namespace TaintedGrailModdingSDK
