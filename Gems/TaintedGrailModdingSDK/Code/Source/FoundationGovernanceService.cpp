/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    bool FoundationService::ApplyCatalogGovernanceDecision(
        const CatalogGovernanceRequest& request,
        AZStd::string* error)
    {
        CatalogDatabase candidate = m_catalog;
        AZ::Outcome<CatalogGovernanceEvent, AZStd::string> result = m_catalogGovernance.ApplyDecision(
            request,
            m_workspace,
            m_sourceRegistry,
            candidate);
        if (!result.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(result.GetError());
            }
            return false;
        }
        return PersistCatalogCandidate(candidate, error);
    }

    bool FoundationService::ApplyCatalogValidationDecision(
        const CatalogValidationRequest& request,
        AZStd::string* error)
    {
        CatalogDatabase candidate = m_catalog;
        AZ::Outcome<CatalogValidationEvent, AZStd::string> result = m_catalogGovernance.ApplyValidation(
            request,
            m_workspace,
            m_sourceRegistry,
            candidate);
        if (!result.IsSuccess())
        {
            if (error)
            {
                *error = AZStd::string(result.GetError());
            }
            return false;
        }
        return PersistCatalogCandidate(candidate, error);
    }
} // namespace TaintedGrailModdingSDK
