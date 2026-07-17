/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/algorithm.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    class CatalogGovernanceService
    {
    public:
        AZ::Outcome<CatalogGovernanceEvent, AZStd::string> ApplyDecision(
            const CatalogGovernanceRequest& request,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            CatalogDatabase& catalog) const;

        AZ::Outcome<CatalogValidationEvent, AZStd::string> ApplyValidation(
            const CatalogValidationRequest& request,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            CatalogDatabase& catalog) const;

    private:
        static bool ValidateEvidence(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::string& error);

        static bool ValidatePermissionBasis(
            const CatalogGovernanceRequest& request,
            const CatalogDatabase& catalog,
            AZStd::string& error);

        static AZStd::string BuildEventId(
            const char* prefix,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            size_t sequence);
    };
} // namespace TaintedGrailModdingSDK
