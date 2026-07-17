/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

namespace TaintedGrailModdingSDK
{
    class CatalogDatabase
    {
    public:
        bool Upsert(const CatalogRecord& record, AZStd::string* error = nullptr);
        void Clear();

        const CatalogRecord* FindByRecordId(const AZStd::string& recordId) const;
        const CatalogRecord* FindByExactNativeRef(const AZStd::string& nativeRefExact) const;
        AZStd::vector<CatalogRecord> Query(const CatalogQuery& query) const;
        AZStd::vector<DomainCoverage> BuildCoverage() const;

        const AZStd::vector<CatalogRecord>& GetRecords() const;

    private:
        AZStd::vector<CatalogRecord> m_records;
    };
} // namespace TaintedGrailModdingSDK
