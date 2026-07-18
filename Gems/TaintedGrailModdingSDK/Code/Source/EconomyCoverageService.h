/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct EconomyAcquisitionCoverageLane
    {
        AZStd::string m_lane;
        AZStd::string m_status;
        AZ::u64 m_relationshipCount = 0;
        AZ::u64 m_coveredRelationshipCount = 0;
        AZStd::vector<AZStd::string> m_relationshipIds;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_blockerIds;
        AZStd::vector<AZStd::string> m_reasons;
    };

    struct EconomyAcquisitionCoverageRow
    {
        AZStd::string m_recordId;
        AZStd::string m_recordKind;
        AZStd::string m_subjectRef;
        AZStd::string m_displayName;
        AZStd::string m_ownerPackId;
        AZStd::string m_validationState;
        AZStd::string m_stalenessState;
        AZStd::string m_overallStatus;
        AZStd::vector<EconomyAcquisitionCoverageLane> m_lanes;
    };

    struct EconomyAcquisitionCoverageSummary
    {
        AZ::u64 m_subjectCount = 0;
        AZ::u64 m_coveredSubjectCount = 0;
        AZ::u64 m_partialSubjectCount = 0;
        AZ::u64 m_blockedSubjectCount = 0;
        AZ::u64 m_missingSubjectCount = 0;
        AZStd::vector<EconomyAcquisitionCoverageRow> m_rows;
    };

    class EconomyCoverageService
    {
    public:
        EconomyAcquisitionCoverageSummary BuildAcquisitionCoverage(
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers) const;
    };
} // namespace TaintedGrailModdingSDK
