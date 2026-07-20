/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterReleaseArtifactContracts.h"
#include "FoundationModels.h"
#include "SourceEvidenceRegistry.h"

namespace TaintedGrailModdingSDK
{
    struct AdapterReleaseArtifactIssue
    {
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_contentId;
        AZStd::string m_provenanceId;
        AZStd::string m_dispositionId;
        AZStd::string m_targetId;
    };

    struct AdapterReleaseArtifactResult
    {
        AdapterReleaseArtifactEnvelope m_envelope;
        AZ::u64 m_sourceDocumentCount = 0;
        AZ::u64 m_evidenceRecordCount = 0;
        AZStd::vector<SourceDocument> m_sourceDocuments;
        AZStd::vector<EvidenceDocument> m_evidenceDocuments;
        AZStd::vector<AdapterReleaseArtifactIssue> m_issues;
        bool m_ready = false;
    };

    // Validates declared release metadata only. It does not read or hash files,
    // assemble an archive, sign, upload, publish, launch, call an adapter, or deploy.
    class AdapterReleaseArtifactProvenanceService
    {
    public:
        AdapterReleaseArtifactResult BuildEnvelope(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const AdapterPackageAssemblyPreview& packagePreview,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterReleaseArtifactRequest& request) const;
    };
} // namespace TaintedGrailModdingSDK
