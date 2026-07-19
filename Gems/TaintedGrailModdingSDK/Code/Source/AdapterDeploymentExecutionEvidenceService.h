/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterDeploymentExecutionResultContracts.h"
#include "FoundationModels.h"

namespace TaintedGrailModdingSDK
{
    struct AdapterDeploymentExecutionResultIssue
    {
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_stepId;
        AZStd::string m_rollbackResultId;
    };

    struct AdapterDeploymentExecutionEvidenceReturn
    {
        AZStd::string m_resultId;
        AZStd::string m_workOrderId;
        AZStd::string m_workOrderFingerprint;
        AZStd::string m_resultFingerprint;
        AdapterDeploymentExecutionEnvelopeStatus m_status =
            AdapterDeploymentExecutionEnvelopeStatus::WorkOrderNotReady;
        AZ::u64 m_stepResultCount = 0;
        AZ::u64 m_backupResultCount = 0;
        AZ::u64 m_verificationCount = 0;
        AZ::u64 m_rollbackResultCount = 0;
        AZ::u64 m_failureCount = 0;
        AZ::u64 m_logReferenceCount = 0;
        AZ::u64 m_sourceDocumentCount = 0;
        AZ::u64 m_evidenceRecordCount = 0;
        AZStd::vector<SourceDocument> m_sourceDocuments;
        AZStd::vector<EvidenceDocument> m_evidenceDocuments;
        AZStd::vector<AdapterDeploymentExecutionResultIssue> m_issues;
        bool m_accepted = false;
    };

    // Nothing is executed, imported, promoted, validated, or permitted here.
    class AdapterDeploymentExecutionEvidenceService
    {
    public:
        AdapterDeploymentExecutionEvidenceReturn BuildEvidenceReturn(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope) const;
    };
} // namespace TaintedGrailModdingSDK
