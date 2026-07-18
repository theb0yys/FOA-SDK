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
    struct EconomyDuplicateCandidate
    {
        AZStd::string m_recordId;
        AZStd::string m_recordKind;
        AZStd::string m_ownerPackId;
        AZStd::string m_subjectRef;
        AZStd::string m_duplicateKey;
        AZStd::string m_validationState;
        AZStd::string m_stalenessState;
        AZStd::string m_status;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_blockerIds;
        AZStd::vector<AZStd::string> m_reasons;
    };

    struct EconomyDuplicateGroup
    {
        AZStd::string m_signal;
        AZStd::string m_matchKey;
        AZStd::string m_recordKind;
        AZStd::string m_status;
        AZStd::vector<AZStd::string> m_packIds;
        AZStd::vector<AZStd::string> m_recordIds;
        AZStd::vector<AZStd::string> m_evidenceIds;
        AZStd::vector<AZStd::string> m_blockerIds;
        AZStd::vector<AZStd::string> m_reasons;
        AZStd::vector<EconomyDuplicateCandidate> m_candidates;
    };

    struct EconomyDuplicateReport
    {
        AZ::u64 m_scannedPackOwnedRecordCount = 0;
        AZ::u64 m_duplicateGroupCount = 0;
        AZ::u64 m_reviewRequiredGroupCount = 0;
        AZ::u64 m_partialGroupCount = 0;
        AZ::u64 m_blockedGroupCount = 0;
        AZStd::vector<EconomyDuplicateGroup> m_groups;
    };

    class EconomyDuplicateDetectionService
    {
    public:
        EconomyDuplicateReport BuildCrossPackDuplicateReport(
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::vector<BlockerRecord>& blockers) const;
    };
} // namespace TaintedGrailModdingSDK
