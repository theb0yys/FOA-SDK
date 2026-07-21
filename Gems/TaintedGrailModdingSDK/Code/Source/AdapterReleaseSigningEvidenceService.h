/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterReleaseArtifactContracts.h"
#include "AdapterReleaseAssemblyEvidenceService.h"
#include "AdapterReleaseAssemblyResultContracts.h"
#include "AdapterReleaseSigningResultContracts.h"
#include "FoundationModels.h"

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct AdapterReleaseSigningIssue
    {
        AZStd::string m_code;
        AZStd::string m_message;
        AZStd::string m_signatureArtifactId;
        AZStd::string m_failureId;
        AZStd::string m_diagnosticId;
    };

    struct AdapterReleaseSigningEvidenceReturn
    {
        AZStd::string m_resultId;
        AZStd::string m_reportedResultFingerprint;
        AZStd::string m_authoritativeResultFingerprint;
        AZStd::string m_artifactId;
        AZStd::string m_artifactFingerprint;
        AZStd::string m_assemblyResultId;
        AZStd::string m_assemblyResultFingerprint;
        AZStd::string m_archiveId;
        AZStd::string m_archiveFingerprint;
        AdapterReleaseSigningEnvelopeStatus m_status =
            AdapterReleaseSigningEnvelopeStatus::AssemblyResultNotAccepted;
        AZ::u64 m_signatureArtifactCount = 0;
        AZ::u64 m_failureCount = 0;
        AZ::u64 m_diagnosticReferenceCount = 0;
        AZ::u64 m_sourceDocumentCount = 0;
        AZ::u64 m_evidenceRecordCount = 0;
        AZStd::vector<SourceDocument> m_sourceDocuments;
        AZStd::vector<EvidenceDocument> m_evidenceDocuments;
        AZStd::vector<AdapterReleaseSigningIssue> m_issues;
        bool m_contractValid = false;
        bool m_accepted = false;
        bool m_filesRead = false;
        bool m_filesHashed = false;
        bool m_archiveOpened = false;
        bool m_archiveModified = false;
        bool m_keysLoaded = false;
        bool m_identityResolved = false;
        bool m_signingPerformed = false;
        bool m_signatureVerified = false;
        bool m_signatureArtifactsWritten = false;
        bool m_uploadPerformed = false;
        bool m_releasePublished = false;
        bool m_launchPerformed = false;
        bool m_adapterCalled = false;
        bool m_deploymentMutated = false;
        bool m_saveMutated = false;
    };

    // Validates separately supplied signer metadata and returns candidate evidence
    // by value. The supplied result fingerprint is preserved as a reported claim;
    // source/evidence identity uses a deterministic SDK-derived fingerprint over
    // the bounded signing-result metadata. No archive, key, cryptographic,
    // filesystem, upload, publication, launch, adapter, deployment, or save
    // operation occurs.
    class AdapterReleaseSigningEvidenceService
    {
    public:
        AdapterReleaseSigningEvidenceReturn BuildEvidenceReturn(
            const AdapterReleaseArtifactEnvelope& artifactEnvelope,
            const AdapterReleaseAssemblyResultEnvelope& assemblyEnvelope,
            const AdapterReleaseAssemblyEvidenceReturn& assemblyEvidence,
            const AdapterReleaseSigningResultEnvelope& signingEnvelope) const;
    };
} // namespace TaintedGrailModdingSDK
