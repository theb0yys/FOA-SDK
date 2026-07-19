/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPostDeploymentVerifierEvidenceService.h"
#include "AdapterVerifierEvidenceReconciliationContracts.h"
#include "FoundationModels.h"

namespace TaintedGrailModdingSDK
{
    struct AdapterVerifierEvidenceReconciliationIssue
    {
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_findingId;
        AZStd::string m_reportBlockerId;
        AZStd::string m_checkId;
        AZStd::string m_dispositionFindingId;
    };

    struct AdapterVerifierEvidenceReconciliationResult
    {
        AdapterVerifierEvidenceReconciliationEnvelope m_envelope;
        AZ::u64 m_sourceDocumentCount = 0;
        AZ::u64 m_evidenceRecordCount = 0;
        AZStd::vector<SourceDocument> m_sourceDocuments;
        AZStd::vector<EvidenceDocument> m_evidenceDocuments;
        AZStd::vector<AdapterVerifierEvidenceReconciliationIssue> m_issues;
        bool m_accepted = false;
    };

    // Reconciles already supplied report and verifier metadata only.
    // No verifier, filesystem, deployment, promotion, signing, or publication occurs.
    class AdapterVerifierEvidenceReconciliationService
    {
    public:
        AZStd::string SerializeCanonicalEnvelope(
            const AdapterVerifierEvidenceReconciliationEnvelope& envelope) const;

        AdapterVerifierEvidenceReconciliationResult BuildReconciliation(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            const AdapterVerifierEvidenceReconciliationRequest& request) const;
    };
} // namespace TaintedGrailModdingSDK
