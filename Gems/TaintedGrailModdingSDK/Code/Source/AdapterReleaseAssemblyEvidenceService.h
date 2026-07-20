/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterReleaseArtifactContracts.h"
#include "AdapterReleaseAssemblyResultContracts.h"
#include "FoundationModels.h"

namespace TaintedGrailModdingSDK
{
    struct AdapterReleaseAssemblyIssue
    {
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_contentId;
        AZStd::string m_observationId;
        AZStd::string m_failureId;
        AZStd::string m_diagnosticId;
    };

    struct AdapterReleaseAssemblyEvidenceReturn
    {
        AZStd::string m_resultId;
        AZStd::string m_artifactId;
        AZStd::string m_artifactFingerprint;
        AZStd::string m_resultFingerprint;
        AdapterReleaseAssemblyEnvelopeStatus m_status =
            AdapterReleaseAssemblyEnvelopeStatus::ArtifactNotReady;
        AZ::u64 m_checksumObservationCount = 0;
        AZ::u64 m_matchedChecksumCount = 0;
        AZ::u64 m_mismatchedChecksumCount = 0;
        AZ::u64 m_failedChecksumCount = 0;
        AZ::u64 m_inconclusiveChecksumCount = 0;
        AZ::u64 m_notObservedChecksumCount = 0;
        AZ::u64 m_failureCount = 0;
        AZ::u64 m_diagnosticReferenceCount = 0;
        AZ::u64 m_sourceDocumentCount = 0;
        AZ::u64 m_evidenceRecordCount = 0;
        AZStd::vector<SourceDocument> m_sourceDocuments;
        AZStd::vector<EvidenceDocument> m_evidenceDocuments;
        AZStd::vector<AdapterReleaseAssemblyIssue> m_issues;
        bool m_contractValid = false;
        bool m_accepted = false;
        bool m_filesRead = false;
        bool m_filesHashed = false;
        bool m_archiveAssembled = false;
        bool m_signingPerformed = false;
        bool m_uploadPerformed = false;
        bool m_releasePublished = false;
        bool m_launchPerformed = false;
        bool m_adapterCalled = false;
        bool m_deploymentMutated = false;
    };

    // Validates a separately supplied assembler/checksummer result and returns
    // candidate evidence. No file, archive, signing, upload, publication, launch,
    // adapter, or deployment operation occurs.
    class AdapterReleaseAssemblyEvidenceService
    {
    public:
        AdapterReleaseAssemblyEvidenceReturn BuildEvidenceReturn(
            const AdapterReleaseArtifactEnvelope& artifactEnvelope,
            const AdapterReleaseAssemblyResultEnvelope& resultEnvelope) const;
    };
} // namespace TaintedGrailModdingSDK
