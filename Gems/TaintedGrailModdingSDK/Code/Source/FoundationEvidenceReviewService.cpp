/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FoundationService.h"

namespace TaintedGrailModdingSDK
{
    bool FoundationService::PromoteCandidateEvidence(
        const AZStd::string& evidenceId,
        AZStd::string* error)
    {
        const bool promoted =
            m_sourceRegistry.PromoteCandidateEvidence(evidenceId, error);
        if (promoted)
        {
            RefreshSnapshot();
        }
        return promoted;
    }

    bool FoundationService::RejectCandidateEvidence(
        const AZStd::string& evidenceId,
        AZStd::string* error)
    {
        const bool rejected =
            m_sourceRegistry.RejectCandidateEvidence(evidenceId, error);
        if (rejected)
        {
            RefreshSnapshot();
        }
        return rejected;
    }
} // namespace TaintedGrailModdingSDK
