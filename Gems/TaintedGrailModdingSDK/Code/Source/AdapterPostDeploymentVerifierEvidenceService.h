/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterPostDeploymentVerifierContracts.h"
#include "FoundationModels.h"
#include "SourceEvidenceRegistry.h"

namespace TaintedGrailModdingSDK
{
    struct AdapterPostDeploymentVerifierIssue
    {
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_stepId;
        AZStd::string m_checkId;
        AZStd::string m_failureId;
        AZStd::string m_diagnosticId;
    };

    struct AdapterPostDeploymentVerifierEvidenceReturn
    {
        AZStd::string m_verifierResultId;
        AZStd::string m_reportId;
        AZStd::string m_resultId;
        AZStd::string m_workOrderId;
        AZStd::string m_resultFingerprint;
        AdapterPostDeploymentVerifierEnvelopeStatus m_status =
            AdapterPostDeploymentVerifierEnvelopeStatus::ReportNotReady;
        AZ::u64 m_checkCount = 0;
        AZ::u64 m_matchedCheckCount = 0;
        AZ::u64 m_mismatchedCheckCount = 0;
        AZ::u64 m_failedCheckCount = 0;
        AZ::u64 m_inconclusiveCheckCount = 0;
        AZ::u64 m_notRunCheckCount = 0;
        AZ::u64 m_failureCount = 0;
        AZ::u64 m_diagnosticReferenceCount = 0;
        AZ::u64 m_sourceDocumentCount = 0;
        AZ::u64 m_evidenceRecordCount = 0;
        AZStd::vector<SourceDocument> m_sourceDocuments;
        AZStd::vector<EvidenceDocument> m_evidenceDocuments;
        AZStd::vector<AdapterPostDeploymentVerifierIssue> m_issues;
        bool m_contractValid = false;
        bool m_accepted = false;
    };

    // Validates separately supplied verifier metadata and returns candidate evidence.
    // No verifier, filesystem mutation, launch, promotion, signing, or publication occurs.
    class AdapterPostDeploymentVerifierEvidenceService
    {
    public:
        AZStd::string SerializeCanonicalReport(
            const AdapterPostDeploymentVerificationReport& report) const;

        AdapterPostDeploymentVerifierEvidenceReturn BuildEvidenceReturn(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope) const;

        // Compatibility entry point. It cannot establish research-evidence binding and
        // therefore deliberately resolves fail-closed as verifier_unreviewed.
        AdapterPostDeploymentVerifierEvidenceReturn BuildEvidenceReturn(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope) const;
    };
} // namespace TaintedGrailModdingSDK
