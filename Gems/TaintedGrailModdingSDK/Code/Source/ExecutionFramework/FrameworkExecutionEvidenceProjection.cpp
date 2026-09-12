/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkExecutionEvidenceProjection.h"

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    Result ProjectCandidate(const StoredAttempt& attempt, CandidateProjection& output)
    {
        if (!attempt.m_receipt || attempt.m_quarantined || !ValidateStoredPlan(attempt.m_plan))
        {
            return { Error::Quarantined };
        }
        AZStd::vector<CE::PhaseExtensionReferenceV1> extensions;
        for (const auto& phase : attempt.m_receipt->m_phaseReceipts)
        {
            if (phase.m_extension)
            {
                extensions.push_back(*phase.m_extension);
            }
        }
        if (!CE::Validate(*attempt.m_receipt, attempt.m_plan, attempt.m_receipt->m_authorization, extensions).IsSuccess())
        {
            return { Error::Invalid };
        }
        CandidateProjection result;
        result.m_source.m_sourceId = "source." + attempt.m_executionId;
        result.m_source.m_title = "Framework execution observation";
        result.m_source.m_sourceKind = "execution-receipt";
        result.m_source.m_locator = "execution/" + attempt.m_executionId;
        result.m_source.m_fingerprint = attempt.m_receipt->m_fingerprint;
        result.m_source.m_importStatus = "candidate";
        result.m_source.m_mediaType = "application/json";
        result.m_source.m_byteSize = CE::Canonicalize(*attempt.m_receipt).GetValue().m_json.size();
        result.m_source.m_limitations =
            "Candidate host execution evidence. No game runtime, qualification, deployment or release authority.";
        for (const auto& phase : attempt.m_receipt->m_phaseReceipts)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = "evidence." + phase.m_id;
            evidence.m_sourceId = result.m_source.m_sourceId;
            evidence.m_sourceFingerprint = result.m_source.m_fingerprint;
            evidence.m_subjectRef = phase.m_phasePlan.m_id;
            evidence.m_claim = AZStd::string(CE::Token(phase.m_outcome));
            evidence.m_evidenceKind = "execution-observation";
            evidence.m_confidence = "candidate";
            evidence.m_locator = result.m_source.m_locator;
            evidence.m_recordPath = phase.m_id;
            evidence.m_extractedAt = phase.m_finishedAt;
            result.m_evidence.push_back(AZStd::move(evidence));
        }
        output = AZStd::move(result);
        return {};
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
