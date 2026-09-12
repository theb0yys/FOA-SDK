/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "CatalogDatabase.h"

namespace TaintedGrailModdingSDK
{
    struct EncounterPreviewRow
    {
        AZStd::string m_entryId;
        AZStd::string m_targetRecordId;
        AZStd::string m_name;
        AZStd::string m_kind;
        AZ::u64 m_minimumActors = 0;
        AZ::u64 m_maximumActors = 0;
    };
    struct EncounterPreview
    {
        AZStd::vector<EncounterPreviewRow> m_rows;
        AZStd::vector<AZStd::string> m_errors;
        AZStd::vector<AZStd::string> m_warnings;
        AZ::u64 m_minimumActors = 0;
        AZ::u64 m_maximumActors = 0;
        AZ::u64 m_maximumConcurrentActors = 0;
        bool IsValid() const { return m_errors.empty(); }
    };
    class EncounterPlanningService
    {
    public:
        //! Pure authoring analysis. Does not resolve native scene data or execute conditions.
        EncounterPreview Preview(const EncounterDefinition& definition, const CatalogDatabase& catalog) const;
    };
}
