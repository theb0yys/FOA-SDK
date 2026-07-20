/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct ReleaseArtifactValidationFlags
        {
            bool m_reconciliationNotApproved = false;
            bool m_packageNotReady = false;
            bool m_bindingMismatch = false;
            bool m_contentInvalid = false;
            bool m_checksumInvalid = false;
            bool m_provenanceIncomplete = false;
            bool m_legalIncomplete = false;
            bool m_signingIntentInvalid = false;
            bool m_publicationTargetInvalid = false;
        };

        void SortUniqueReleaseValues(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        bool HasStableUniqueReleaseIds(
            const AZStd::vector<AZStd::string>& values,
            bool requireNonEmpty = true)
        {
            if (requireNonEmpty && values.empty())
            {
                return false;
            }
            AZStd::vector<AZStd::string> sorted = values;
            for (const AZStd::string& value : sorted)
            {
                if (!IsAdapterPostDeploymentVerifierStableId(value))
                {
                    return false;
                }
            }
            AZStd::sort(sorted.begin(), sorted.end());
            return AZStd::adjacent_find(sorted.begin(), sorted.end())
                == sorted.end();
        }

        bool IsSafeReleaseLocator(const AZStd::string& value)
        {
            if (value.empty()
                || value.size() > 512
                || value.front() == '/'
                || value.find("//") != AZStd::string::npos)
            {
                return false;
            }
            size_t segmentStart = 0;
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                const bool safeCharacter =
                    (byte >= 'a' && byte <= 'z')
                    || (byte >= 'A' && byte <= 'Z')
                    || (byte >= '0' && byte <= '9')
                    || character == '-'
                    || character == '_'
                    || character == '.'
                    || character == '/';
                if (!safeCharacter)
                {
                    return false;
                }
            }
            while (segmentStart <= value.size())
            {
                const size_t separator = value.find('/', segmentStart);
                const size_t segmentLength = separator == AZStd::string::npos
                    ? value.size() - segmentStart
                    : separator - segmentStart;
                const AZStd::string segment = value.substr(
                    segmentStart,
                    segmentLength);
                if (segment.empty() || segment == "." || segment == "..")
                {
                    return false;
                }
                if (separator == AZStd::string::npos)
                {
                    break;
                }
                segmentStart = separator + 1;
            }
            return true;
        }

        bool IsKnownSigningDecision(
            AdapterReleaseSigningIntentDecision decision)
        {
            switch (decision)
            {
            case AdapterReleaseSigningIntentDecision::Unknown:
            case AdapterReleaseSigningIntentDecision::Unsigned:
            case AdapterReleaseSigningIntentDecision::SignExternally:
                return true;
            default:
                return false;
            }
        }

        bool IsKnownSigningIdentityKind(AdapterReleaseSigningIdentityKind kind)
        {
            switch (kind)
            {
            case AdapterReleaseSigningIdentityKind::None:
            case AdapterReleaseSigningIdentityKind::Pgp:
            case AdapterReleaseSigningIdentityKind::X509:
            case AdapterReleaseSigningIdentityKind::Sigstore:
            case AdapterReleaseSigningIdentityKind::Platform:
            case AdapterReleaseSigningIdentityKind::Other:
                return true;
            default:
                return false;
            }
        }

        bool IsKnownPublicationTargetKind(
            AdapterReleasePublicationTargetKind kind)
        {
            switch (kind)
            {
            case AdapterReleasePublicationTargetKind::GitHubRelease:
            case AdapterReleasePublicationTargetKind::ModPortal:
            case AdapterReleasePublicationTargetKind::InternalArchive:
            case AdapterReleasePublicationTargetKind::Other:
                return true;
            default:
                return false;
            }
        }

        AZStd::string ReleaseSubject(const AZStd::string& artifactId)
        {
            return "release-artifact:" + artifactId;
        }

        bool HasBoundReleaseReviewEvidence(
            const AZStd::vector<AZStd::string>& evidenceIds,
            const AZStd::string& expectedSubjectRef,
            const AZStd::string& declaredSourceId,
            const AZStd::string& declaredSourceFingerprint,
            const AdapterVerifierEvidenceReconciliationEnvelope& reconciliation,
            const SourceEvidenceRegistry& sourceRegistry)
        {
            if (!HasStableUniqueReleaseIds(evidenceIds)
                || expectedSubjectRef.empty())
            {
                return false;
            }
            for (const AZStd::string& evidenceId : evidenceIds)
            {
                if (AZStd::find(
                        reconciliation.m_inputCandidateEvidenceIds.begin(),
                        reconciliation.m_inputCandidateEvidenceIds.end(),
                        evidenceId)
                    == reconciliation.m_inputCandidateEvidenceIds.end())
                {
                    return false;
                }
                const EvidenceRecord* evidence =
                    sourceRegistry.FindEvidence(evidenceId);
                if (!evidence)
                {
                    return false;
                }
                const SourceRecord* source =
                    sourceRegistry.FindSource(evidence->m_sourceId);
                if (!source
                    || !IsUsableImportStatus(source->m_importStatus)
                    || evidence->m_sourceFingerprint != source->m_fingerprint
                    || evidence->m_profileId != reconciliation.m_profileId
                    || evidence->m_gameVersion != reconciliation.m_gameVersion
                    || evidence->m_branch != reconciliation.m_branch
                    || source->m_runtimeTarget != reconciliation.m_runtimeTarget
                    || evidence->m_subjectRef != expectedSubjectRef)
                {
                    return false;
                }
                if (!declaredSourceId.empty()
                    && (evidence->m_sourceId != declaredSourceId
                        || source->m_sourceId != declaredSourceId
                        || source->m_fingerprint != declaredSourceFingerprint))
                {
                    return false;
                }
            }
            return true;
        }

        void AddReleaseArtifactIssue(
            AdapterReleaseArtifactResult& result,
            bool& flag,
            AZStd::string code,
            AZStd::string message,
            AZStd::string subjectRef,
            AZStd::string contentId = {},
            AZStd::string provenanceId = {},
            AZStd::string dispositionId = {},
            AZStd::string targetId = {})
        {
            flag = true;

            AdapterReleaseArtifactIssue issue;
            issue.m_code = code;
            issue.m_message = message;
            issue.m_contentId = contentId;
            issue.m_provenanceId = provenanceId;
            issue.m_dispositionId = dispositionId;
            issue.m_targetId = targetId;
            result.m_issues.push_back(AZStd::move(issue));

            AdapterReleaseArtifactBlocker blocker;
            blocker.m_blockerId = "blocker.release-artifact."
                + result.m_envelope.m_artifactId + "."
                + AZStd::string(code);
            if (!contentId.empty())
            {
                blocker.m_blockerId += "." + contentId;
            }
            else if (!provenanceId.empty())
            {
                blocker.m_blockerId += "." + provenanceId;
            }
            else if (!dispositionId.empty())
            {
                blocker.m_blockerId += "." + dispositionId;
            }
            else if (!targetId.empty())
            {
                blocker.m_blockerId += "." + targetId;
            }
            blocker.m_code = AZStd::move(code);
            blocker.m_subjectRef = AZStd::move(subjectRef);
            blocker.m_message = AZStd::move(message);
            result.m_envelope.m_blockers.push_back(AZStd::move(blocker));
        }

        const AdapterPackageLayoutEntry* FindPackageLayoutEntry(
            const AdapterPackageAssemblyPreview& preview,
            const AZStd::string& entryId)
        {
            for (const AdapterPackageLayoutEntry& entry : preview.m_layout)
            {
                if (entry.m_inventoryEntryId == entryId)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const AdapterReleaseProvenanceRecord* FindReleaseProvenance(
            const AdapterReleaseArtifactRequest& request,
            const AZStd::string& provenanceId)
        {
            for (const AdapterReleaseProvenanceRecord& record : request.m_provenance)
            {
                if (record.m_provenanceId == provenanceId)
                {
                    return &record;
                }
            }
            return nullptr;
        }

        const AdapterReleaseLegalDispositionRecord* FindReleaseDisposition(
            const AdapterReleaseArtifactRequest& request,
            const AZStd::string& dispositionId)
        {
            for (const AdapterReleaseLegalDispositionRecord& record :
                 request.m_legalDispositions)
            {
                if (record.m_dispositionId == dispositionId)
                {
                    return &record;
                }
            }
            return nullptr;
        }

        void ValidateReleaseArtifactUpstream(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const AdapterPackageAssemblyPreview& packagePreview,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            const AdapterVerifierEvidenceReconciliationEnvelope& upstream =
                reconciliation.m_envelope;
            if (!reconciliation.m_accepted
                || upstream.m_status
                    != AdapterVerifierReconciliationEnvelopeStatus::Accepted
                || upstream.m_compatibilityAssessment
                    != AdapterVerifierCompatibilityAssessment::Clear
                || upstream.m_releaseDecision
                    != AdapterVerifierReleaseDecision::Approved
                || upstream.m_humanReviewState
                    != AdapterVerifierHumanReviewState::Complete
                || upstream.m_canonicalJson.empty()
                || upstream.m_verifierExecuted
                || upstream.m_targetAccessed
                || upstream.m_filesMutated
                || upstream.m_evidencePromoted
                || upstream.m_archiveAssembled
                || upstream.m_archiveSigned
                || upstream.m_releasePublished
                || upstream.m_launchPerformed
                || upstream.m_adapterCalled)
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_reconciliationNotApproved,
                    "reconciliation_not_approved",
                    "Release-artifact metadata requires one exact accepted reconciliation "
                    "with clear compatibility, approved human release decision, complete "
                    "review, canonical JSON, and every operational flag false.",
                    ReleaseSubject(request.m_artifactId));
            }

            if (packagePreview.m_status != AdapterPackageAssemblyPreviewStatus::Ready
                || packagePreview.m_previewId.empty()
                || packagePreview.m_canonicalJson.empty()
                || packagePreview.m_layout.empty()
                || packagePreview.m_assemblyAllowed
                || packagePreview.m_archiveAllowed
                || packagePreview.m_deploymentAllowed)
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_packageNotReady,
                    "package_not_ready",
                    "Release-artifact metadata requires one exact ready package preview "
                    "with a non-empty declared layout and assembly, archive, and deployment "
                    "permissions false.",
                    ReleaseSubject(request.m_artifactId));
            }
        }

        void ValidateReleaseArtifactBinding(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const AdapterPackageAssemblyPreview& packagePreview,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            const AdapterVerifierEvidenceReconciliationEnvelope& upstream =
                reconciliation.m_envelope;
            if (!IsAdapterPostDeploymentVerifierStableId(request.m_artifactId)
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    request.m_evaluatedAtUtc)
                || request.m_reconciliationCanonicalJson
                    != upstream.m_canonicalJson
                || request.m_packagePreviewCanonicalJson
                    != packagePreview.m_canonicalJson
                || packagePreview.m_packId != upstream.m_packId
                || packagePreview.m_manifestId.empty()
                || !IsAdapterPostDeploymentVerifierFingerprint(
                    packagePreview.m_manifestFingerprint)
                || request.m_evaluatedAtUtc < upstream.m_evaluatedAtUtc)
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_bindingMismatch,
                    "binding_mismatch",
                    "The release-artifact request must preserve stable identity, UTC "
                    "evaluation time, exact approved reconciliation JSON, exact ready "
                    "package-preview JSON, pack identity, manifest identity/fingerprint, "
                    "and an evaluation time not earlier than reconciliation.",
                    ReleaseSubject(request.m_artifactId));
            }
        }

        void ValidateReleaseArtifactContents(
            const AdapterPackageAssemblyPreview& packagePreview,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> contentIds;
            AZStd::vector<AZStd::string> entryIds;
            for (const AdapterReleaseArtifactContent& content : request.m_contents)
            {
                contentIds.push_back(content.m_contentId);
                entryIds.push_back(content.m_packageEntryId);
                const AdapterPackageLayoutEntry* entry = FindPackageLayoutEntry(
                    packagePreview,
                    content.m_packageEntryId);
                if (!IsAdapterPostDeploymentVerifierStableId(content.m_contentId)
                    || !entry
                    || content.m_packagePath != entry->m_packagePath
                    || content.m_role != entry->m_role
                    || content.m_mediaType != entry->m_mediaType
                    || content.m_byteSize != entry->m_byteSize
                    || content.m_packagePath.empty()
                    || content.m_role.empty()
                    || content.m_mediaType.empty()
                    || !entry->m_projectOwned
                    || !entry->m_redistributable
                    || content.m_legalDispositionId.empty()
                    || !HasStableUniqueReleaseIds(content.m_provenanceIds))
                {
                    AddReleaseArtifactIssue(
                        result,
                        flags.m_contentInvalid,
                        "content_invalid",
                        "Every declared release content must bind exactly once to one "
                        "project-owned redistributable package-layout entry and preserve "
                        "entry identity, path, role, media type, byte size, provenance, and "
                        "legal-disposition identity.",
                        "release-content:" + content.m_contentId,
                        content.m_contentId);
                }

                if (content.m_checksumAlgorithm
                        != AdapterReleaseChecksumAlgorithm::Sha256
                    || !entry
                    || !IsAdapterPostDeploymentVerifierFingerprint(
                        content.m_expectedChecksum)
                    || content.m_expectedChecksum != entry->m_outputDigest)
                {
                    AddReleaseArtifactIssue(
                        result,
                        flags.m_checksumInvalid,
                        "checksum_declaration_invalid",
                        "Each content checksum is a declaration only and must be lowercase "
                        "SHA-256 exactly equal to the reviewed package-layout output digest. "
                        "No file is opened or hashed by this contract.",
                        "release-content:" + content.m_contentId,
                        content.m_contentId);
                }
            }

            AZStd::sort(contentIds.begin(), contentIds.end());
            AZStd::sort(entryIds.begin(), entryIds.end());
            const bool duplicates =
                AZStd::adjacent_find(contentIds.begin(), contentIds.end())
                    != contentIds.end()
                || AZStd::adjacent_find(entryIds.begin(), entryIds.end())
                    != entryIds.end();
            if (request.m_contents.size() != packagePreview.m_layout.size()
                || request.m_contents.empty()
                || duplicates)
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_contentInvalid,
                    "content_coverage_invalid",
                    "Declared release contents must provide one unique exact row for every "
                    "package-layout entry, with no missing, duplicate, or extra entries.",
                    ReleaseSubject(request.m_artifactId));
            }
        }

        void ValidateReleaseArtifactProvenance(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> provenanceIds;
            for (const AdapterReleaseProvenanceRecord& record : request.m_provenance)
            {
                provenanceIds.push_back(record.m_provenanceId);
                if (!IsAdapterPostDeploymentVerifierStableId(record.m_provenanceId)
                    || !IsAdapterPostDeploymentVerifierStableId(record.m_contentId)
                    || record.m_subjectRef
                        != "release-content:" + record.m_contentId
                    || record.m_sourceKind.empty()
                    || !IsAdapterPostDeploymentVerifierStableId(record.m_sourceId)
                    || !IsAdapterPostDeploymentVerifierFingerprint(
                        record.m_sourceFingerprint)
                    || !HasStableUniqueReleaseIds(record.m_evidenceIds)
                    || !HasBoundReleaseReviewEvidence(
                        record.m_evidenceIds,
                        record.m_subjectRef,
                        record.m_sourceId,
                        record.m_sourceFingerprint,
                        reconciliation.m_envelope,
                        sourceRegistry)
                    || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                        record.m_capturedAtUtc)
                    || record.m_capturedAtUtc > request.m_evaluatedAtUtc)
                {
                    AddReleaseArtifactIssue(
                        result,
                        flags.m_provenanceIncomplete,
                        "provenance_invalid",
                        "Every provenance record must use stable identities, exact content "
                        "subject binding, source kind and fingerprint, unique evidence, and "
                        "a UTC capture time not later than evaluation.",
                        "release-content:" + record.m_contentId,
                        record.m_contentId,
                        record.m_provenanceId);
                }
            }
            AZStd::sort(provenanceIds.begin(), provenanceIds.end());
            if (request.m_provenance.empty()
                || AZStd::adjacent_find(provenanceIds.begin(), provenanceIds.end())
                    != provenanceIds.end())
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_provenanceIncomplete,
                    "provenance_identity_invalid",
                    "Release provenance identities must be non-empty and unique.",
                    ReleaseSubject(request.m_artifactId));
            }

            for (const AdapterReleaseArtifactContent& content : request.m_contents)
            {
                for (const AZStd::string& provenanceId : content.m_provenanceIds)
                {
                    const AdapterReleaseProvenanceRecord* record =
                        FindReleaseProvenance(request, provenanceId);
                    if (!record || record->m_contentId != content.m_contentId)
                    {
                        AddReleaseArtifactIssue(
                            result,
                            flags.m_provenanceIncomplete,
                            "provenance_binding_missing",
                            "Every content provenance identity must resolve to one exact "
                            "same-content provenance record.",
                            "release-content:" + content.m_contentId,
                            content.m_contentId,
                            provenanceId);
                    }
                }
            }
        }

        void ValidateReleaseArtifactLegalDispositions(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> dispositionIds;
            AZStd::vector<AZStd::string> dispositionContentIds;
            for (const AdapterReleaseLegalDispositionRecord& disposition :
                 request.m_legalDispositions)
            {
                dispositionIds.push_back(disposition.m_dispositionId);
                dispositionContentIds.push_back(disposition.m_contentId);
                if (!IsAdapterPostDeploymentVerifierStableId(
                        disposition.m_dispositionId)
                    || !IsAdapterPostDeploymentVerifierStableId(
                        disposition.m_contentId)
                    || disposition.m_disposition
                        != AdapterReleaseLegalDisposition::Approved
                    || disposition.m_reviewer.empty()
                    || !HasStableUniqueReleaseIds(disposition.m_evidenceIds)
                    || !HasBoundReleaseReviewEvidence(
                        disposition.m_evidenceIds,
                        "release-content:" + disposition.m_contentId,
                        {},
                        {},
                        reconciliation.m_envelope,
                        sourceRegistry)
                    || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                        disposition.m_reviewedAtUtc)
                    || disposition.m_reviewedAtUtc > request.m_evaluatedAtUtc
                    || disposition.m_rationale.empty())
                {
                    AddReleaseArtifactIssue(
                        result,
                        flags.m_legalIncomplete,
                        "legal_disposition_incomplete",
                        "Every release content requires one approved, named, evidence-backed "
                        "legal/redistribution disposition with UTC review time and rationale.",
                        "release-content:" + disposition.m_contentId,
                        disposition.m_contentId,
                        {},
                        disposition.m_dispositionId);
                }
            }
            AZStd::sort(dispositionIds.begin(), dispositionIds.end());
            AZStd::sort(dispositionContentIds.begin(), dispositionContentIds.end());
            if (request.m_legalDispositions.size() != request.m_contents.size()
                || AZStd::adjacent_find(dispositionIds.begin(), dispositionIds.end())
                    != dispositionIds.end()
                || AZStd::adjacent_find(
                       dispositionContentIds.begin(),
                       dispositionContentIds.end())
                    != dispositionContentIds.end())
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_legalIncomplete,
                    "legal_disposition_coverage_invalid",
                    "Legal dispositions must provide exactly one unique reviewed decision "
                    "for every declared release content.",
                    ReleaseSubject(request.m_artifactId));
            }

            for (const AdapterReleaseArtifactContent& content : request.m_contents)
            {
                const AdapterReleaseLegalDispositionRecord* disposition =
                    FindReleaseDisposition(request, content.m_legalDispositionId);
                if (!disposition || disposition->m_contentId != content.m_contentId)
                {
                    AddReleaseArtifactIssue(
                        result,
                        flags.m_legalIncomplete,
                        "legal_disposition_binding_missing",
                        "Every content legal-disposition identity must resolve to one exact "
                        "same-content reviewed disposition.",
                        "release-content:" + content.m_contentId,
                        content.m_contentId,
                        {},
                        content.m_legalDispositionId);
                }
            }
        }

        void ValidateReleaseArtifactSigningIntent(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            const AdapterReleaseSigningIntent& intent = request.m_signingIntent;
            bool invalid = !IsAdapterPostDeploymentVerifierStableId(intent.m_intentId)
                || !IsKnownSigningDecision(intent.m_decision)
                || !IsKnownSigningIdentityKind(intent.m_identityKind)
                || intent.m_decision == AdapterReleaseSigningIntentDecision::Unknown
                || intent.m_reviewer.empty()
                || !HasStableUniqueReleaseIds(intent.m_evidenceIds)
                || !HasBoundReleaseReviewEvidence(
                    intent.m_evidenceIds,
                    ReleaseSubject(request.m_artifactId),
                    {},
                    {},
                    reconciliation.m_envelope,
                    sourceRegistry)
                || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                    intent.m_reviewedAtUtc)
                || intent.m_reviewedAtUtc > request.m_evaluatedAtUtc
                || intent.m_rationale.empty();

            if (intent.m_decision == AdapterReleaseSigningIntentDecision::Unsigned)
            {
                invalid = invalid
                    || intent.m_identityKind
                        != AdapterReleaseSigningIdentityKind::None
                    || !intent.m_signerId.empty()
                    || !intent.m_identityLocator.empty()
                    || !intent.m_identityFingerprint.empty();
            }
            else if (intent.m_decision
                == AdapterReleaseSigningIntentDecision::SignExternally)
            {
                invalid = invalid
                    || intent.m_identityKind
                        == AdapterReleaseSigningIdentityKind::None
                    || !IsAdapterPostDeploymentVerifierStableId(intent.m_signerId)
                    || !IsSafeReleaseLocator(intent.m_identityLocator)
                    || !IsAdapterPostDeploymentVerifierFingerprint(
                        intent.m_identityFingerprint);
            }

            if (invalid)
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_signingIntentInvalid,
                    "signing_intent_invalid",
                    "Signing intent must be explicitly reviewed. Unsigned intent carries no "
                    "identity; external-signing intent requires stable signer identity, "
                    "typed identity kind, safe locator, fingerprint, evidence, UTC review, "
                    "and rationale. No signature is produced.",
                    ReleaseSubject(request.m_artifactId));
            }
        }

        void ValidateReleaseArtifactPublicationTargets(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const SourceEvidenceRegistry& sourceRegistry,
            const AdapterReleaseArtifactRequest& request,
            AdapterReleaseArtifactResult& result,
            ReleaseArtifactValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> targetIds;
            for (const AdapterReleasePublicationTarget& target :
                 request.m_publicationTargets)
            {
                targetIds.push_back(target.m_targetId);
                if (!IsAdapterPostDeploymentVerifierStableId(target.m_targetId)
                    || !IsKnownPublicationTargetKind(target.m_kind)
                    || !IsSafeReleaseLocator(target.m_locator)
                    || target.m_channel.empty()
                    || target.m_reviewer.empty()
                    || !HasStableUniqueReleaseIds(target.m_evidenceIds)
                    || !HasBoundReleaseReviewEvidence(
                        target.m_evidenceIds,
                        ReleaseSubject(request.m_artifactId),
                        {},
                        {},
                        reconciliation.m_envelope,
                        sourceRegistry)
                    || !IsAdapterPostDeploymentVerifierUtcTimestamp(
                        target.m_reviewedAtUtc)
                    || target.m_reviewedAtUtc > request.m_evaluatedAtUtc
                    || target.m_rationale.empty())
                {
                    AddReleaseArtifactIssue(
                        result,
                        flags.m_publicationTargetInvalid,
                        "publication_target_invalid",
                        "Every declared publication target requires stable identity, typed "
                        "kind, safe locator, channel, named evidence-backed reviewer, UTC "
                        "review time, and rationale. No upload or publication occurs.",
                        ReleaseSubject(request.m_artifactId),
                        {},
                        {},
                        {},
                        target.m_targetId);
                }
            }
            AZStd::sort(targetIds.begin(), targetIds.end());
            if (request.m_publicationTargets.empty()
                || AZStd::adjacent_find(targetIds.begin(), targetIds.end())
                    != targetIds.end())
            {
                AddReleaseArtifactIssue(
                    result,
                    flags.m_publicationTargetInvalid,
                    "publication_target_coverage_invalid",
                    "At least one unique reviewed publication target declaration is "
                    "required.",
                    ReleaseSubject(request.m_artifactId));
            }
        }

        AdapterReleaseArtifactEnvelopeStatus ResolveReleaseArtifactStatus(
            const ReleaseArtifactValidationFlags& flags)
        {
            if (flags.m_reconciliationNotApproved)
            {
                return AdapterReleaseArtifactEnvelopeStatus::
                    ReconciliationNotApproved;
            }
            if (flags.m_packageNotReady)
            {
                return AdapterReleaseArtifactEnvelopeStatus::PackageNotReady;
            }
            if (flags.m_bindingMismatch)
            {
                return AdapterReleaseArtifactEnvelopeStatus::BindingMismatch;
            }
            if (flags.m_contentInvalid)
            {
                return AdapterReleaseArtifactEnvelopeStatus::ContentInvalid;
            }
            if (flags.m_checksumInvalid)
            {
                return AdapterReleaseArtifactEnvelopeStatus::
                    ChecksumDeclarationInvalid;
            }
            if (flags.m_provenanceIncomplete)
            {
                return AdapterReleaseArtifactEnvelopeStatus::ProvenanceIncomplete;
            }
            if (flags.m_legalIncomplete)
            {
                return AdapterReleaseArtifactEnvelopeStatus::
                    LegalDispositionIncomplete;
            }
            if (flags.m_signingIntentInvalid)
            {
                return AdapterReleaseArtifactEnvelopeStatus::SigningIntentInvalid;
            }
            if (flags.m_publicationTargetInvalid)
            {
                return AdapterReleaseArtifactEnvelopeStatus::
                    PublicationTargetInvalid;
            }
            return AdapterReleaseArtifactEnvelopeStatus::Ready;
        }

        void SortReleaseArtifactIssues(AdapterReleaseArtifactResult& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                [](const AdapterReleaseArtifactIssue& left,
                    const AdapterReleaseArtifactIssue& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_contentId != right.m_contentId)
                    {
                        return left.m_contentId < right.m_contentId;
                    }
                    if (left.m_provenanceId != right.m_provenanceId)
                    {
                        return left.m_provenanceId < right.m_provenanceId;
                    }
                    if (left.m_dispositionId != right.m_dispositionId)
                    {
                        return left.m_dispositionId < right.m_dispositionId;
                    }
                    return left.m_targetId < right.m_targetId;
                });
            AZStd::sort(
                result.m_envelope.m_blockers.begin(),
                result.m_envelope.m_blockers.end(),
                [](const AdapterReleaseArtifactBlocker& left,
                    const AdapterReleaseArtifactBlocker& right)
                {
                    return left.m_blockerId < right.m_blockerId;
                });
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
