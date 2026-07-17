/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"
#include "CatalogGovernanceTypes.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string_view.h>

#include <cstddef>

namespace TaintedGrailModdingSDK
{
    struct CatalogGovernanceApplyResult
    {
        CatalogDatabase m_catalog;
        CatalogGovernanceEvent m_event;
    };

    struct CatalogValidationApplyResult
    {
        CatalogDatabase m_catalog;
        CatalogValidationEvent m_event;
    };

    class CatalogGovernanceService
    {
    public:
        AZ::Outcome<CatalogGovernanceApplyResult, AZStd::string> ApplyDecision(
            const CatalogGovernanceRequest& request,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;

        AZ::Outcome<CatalogGovernanceApplyResult, AZStd::string> ApplyDecision(
            const CatalogGovernanceRequest& request,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::string& eventIdOverride) const;

        AZ::Outcome<CatalogValidationApplyResult, AZStd::string> ApplyValidation(
            const CatalogValidationRequest& request,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog) const;

        AZ::Outcome<CatalogValidationApplyResult, AZStd::string> ApplyValidation(
            const CatalogValidationRequest& request,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            const AZStd::string& validationIdOverride) const;

    private:
        static AZ::Outcome<GovernedSubjectState, AZStd::string> ReadSubjectState(
            CatalogSubjectKind subjectKind,
            const AZStd::string& subjectId,
            const CatalogDatabase& catalog);

        static bool WriteSubjectState(
            const GovernedSubjectState& state,
            CatalogDatabase& catalog,
            AZStd::string& error);

        static AZ::Outcome<CatalogGovernanceEvent, AZStd::string> ApplyTypedTransition(
            const CatalogGovernanceRequest& request,
            GovernanceAxis axis,
            GovernedSubjectState& state,
            const CatalogDatabase& catalog,
            const AZStd::string& decidedAt);

        static void ApplyValidationState(
            ValidationState validationState,
            GovernedSubjectState& state);

        static bool ValidateEvidence(
            const AZStd::vector<AZStd::string>& evidenceIds,
            CatalogSubjectKind subjectKind,
            const AZStd::string& subjectId,
            const WorkspaceModel& workspace,
            const SourceEvidenceRegistry& sourceRegistry,
            const CatalogDatabase& catalog,
            AZStd::string& error);

        static bool ValidatePermissionBasis(
            const CatalogGovernanceRequest& request,
            CatalogSubjectKind subjectKind,
            const CatalogDatabase& catalog,
            AZStd::string& error);

        static AZStd::string BuildUniqueEventId(
            const char* prefix,
            CatalogSubjectKind subjectKind,
            const AZStd::string& subjectId,
            const CatalogDatabase& catalog);

        static AZStd::string BuildEventId(
            const char* prefix,
            CatalogSubjectKind subjectKind,
            const AZStd::string& subjectId,
            size_t sequence);
    };

    inline AZ::Outcome<CatalogGovernanceApplyResult, AZStd::string> CatalogGovernanceService::ApplyDecision(
        const CatalogGovernanceRequest& request,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        return ApplyDecision(request, workspace, sourceRegistry, catalog, {});
    }

    inline AZ::Outcome<CatalogValidationApplyResult, AZStd::string> CatalogGovernanceService::ApplyValidation(
        const CatalogValidationRequest& request,
        const WorkspaceModel& workspace,
        const SourceEvidenceRegistry& sourceRegistry,
        const CatalogDatabase& catalog) const
    {
        return ApplyValidation(request, workspace, sourceRegistry, catalog, {});
    }
} // namespace TaintedGrailModdingSDK
