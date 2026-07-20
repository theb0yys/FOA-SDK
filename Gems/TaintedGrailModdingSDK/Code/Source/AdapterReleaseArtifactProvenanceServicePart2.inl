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
        AZStd::string ReleaseUnsignedString(AZ::u64 value)
        {
            char buffer[32];
            size_t position = sizeof(buffer);
            do
            {
                buffer[--position] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            return AZStd::string(buffer + position, sizeof(buffer) - position);
        }

        void AppendReleaseJsonEscaped(
            AZStd::string& output,
            const AZStd::string& value)
        {
            constexpr char HexDigits[] = "0123456789abcdef";
            output.push_back('"');
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                switch (character)
                {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (byte < 0x20)
                    {
                        output += "\\u00";
                        output.push_back(HexDigits[(byte >> 4) & 0x0f]);
                        output.push_back(HexDigits[byte & 0x0f]);
                    }
                    else
                    {
                        output.push_back(character);
                    }
                    break;
                }
            }
            output.push_back('"');
        }

        void AppendReleaseJsonName(AZStd::string& output, const char* name)
        {
            AppendReleaseJsonEscaped(output, name);
            output.push_back(':');
        }

        void AppendReleaseJsonString(
            AZStd::string& output,
            const char* name,
            const AZStd::string& value,
            bool comma = true)
        {
            AppendReleaseJsonName(output, name);
            AppendReleaseJsonEscaped(output, value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendReleaseJsonUnsigned(
            AZStd::string& output,
            const char* name,
            AZ::u64 value,
            bool comma = true)
        {
            AppendReleaseJsonName(output, name);
            output += ReleaseUnsignedString(value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendReleaseJsonBool(
            AZStd::string& output,
            const char* name,
            bool value,
            bool comma = true)
        {
            AppendReleaseJsonName(output, name);
            output += value ? "true" : "false";
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendReleaseJsonStringArray(
            AZStd::string& output,
            const char* name,
            AZStd::vector<AZStd::string> values,
            bool comma = true)
        {
            SortUniqueReleaseValues(values);
            AppendReleaseJsonName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                AppendReleaseJsonEscaped(output, values[index]);
            }
            output.push_back(']');
            if (comma)
            {
                output.push_back(',');
            }
        }

        AZStd::string SerializeReleaseArtifactEnvelope(
            const AdapterReleaseArtifactEnvelope& envelope)
        {
            AZStd::string output;
            output.push_back('{');
            AppendReleaseJsonUnsigned(output, "ContractVersion", envelope.m_contractVersion);
            AppendReleaseJsonString(output, "ArtifactId", envelope.m_artifactId);
            AppendReleaseJsonString(output, "ReconciliationId", envelope.m_reconciliationId);
            AppendReleaseJsonString(
                output,
                "ReconciliationCanonicalJson",
                envelope.m_reconciliationCanonicalJson);
            AppendReleaseJsonString(output, "ReportId", envelope.m_reportId);
            AppendReleaseJsonString(
                output,
                "VerifierResultId",
                envelope.m_verifierResultId);
            AppendReleaseJsonString(
                output,
                "WorkOrderFingerprint",
                envelope.m_workOrderFingerprint);
            AppendReleaseJsonString(
                output,
                "ExecutionResultFingerprint",
                envelope.m_executionResultFingerprint);
            AppendReleaseJsonString(
                output,
                "VerifierResultFingerprint",
                envelope.m_verifierResultFingerprint);
            AppendReleaseJsonString(
                output,
                "PackagePreviewId",
                envelope.m_packagePreviewId);
            AppendReleaseJsonString(
                output,
                "PackagePreviewCanonicalJson",
                envelope.m_packagePreviewCanonicalJson);
            AppendReleaseJsonString(output, "ManifestId", envelope.m_manifestId);
            AppendReleaseJsonString(
                output,
                "ManifestFingerprint",
                envelope.m_manifestFingerprint);
            AppendReleaseJsonString(output, "PackId", envelope.m_packId);
            AppendReleaseJsonString(output, "PackVersion", envelope.m_packVersion);
            AppendReleaseJsonString(output, "AdapterId", envelope.m_adapterId);
            AppendReleaseJsonString(
                output,
                "AdapterVersion",
                envelope.m_adapterVersion);
            AppendReleaseJsonString(output, "InventoryId", envelope.m_inventoryId);
            AppendReleaseJsonString(output, "ProfileId", envelope.m_profileId);
            AppendReleaseJsonString(output, "GameVersion", envelope.m_gameVersion);
            AppendReleaseJsonString(output, "Branch", envelope.m_branch);
            AppendReleaseJsonString(output, "RuntimeTarget", envelope.m_runtimeTarget);
            AppendReleaseJsonString(
                output,
                "EvaluatedAtUtc",
                envelope.m_evaluatedAtUtc);
            AppendReleaseJsonString(output, "Status", ToString(envelope.m_status));
            AppendReleaseJsonUnsigned(output, "ContentCount", envelope.m_contentCount);
            AppendReleaseJsonUnsigned(
                output,
                "ProvenanceCount",
                envelope.m_provenanceCount);
            AppendReleaseJsonUnsigned(
                output,
                "LegalDispositionCount",
                envelope.m_legalDispositionCount);
            AppendReleaseJsonUnsigned(
                output,
                "PublicationTargetCount",
                envelope.m_publicationTargetCount);

            AppendReleaseJsonName(output, "Contents");
            output.push_back('[');
            for (size_t index = 0; index < envelope.m_contents.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterReleaseArtifactContent& content =
                    envelope.m_contents[index];
                output.push_back('{');
                AppendReleaseJsonString(output, "ContentId", content.m_contentId);
                AppendReleaseJsonString(
                    output,
                    "PackageEntryId",
                    content.m_packageEntryId);
                AppendReleaseJsonString(
                    output,
                    "PackagePath",
                    content.m_packagePath);
                AppendReleaseJsonString(output, "Role", content.m_role);
                AppendReleaseJsonString(output, "MediaType", content.m_mediaType);
                AppendReleaseJsonUnsigned(output, "ByteSize", content.m_byteSize);
                AppendReleaseJsonString(
                    output,
                    "ChecksumAlgorithm",
                    ToString(content.m_checksumAlgorithm));
                AppendReleaseJsonString(
                    output,
                    "ExpectedChecksum",
                    content.m_expectedChecksum);
                AppendReleaseJsonStringArray(
                    output,
                    "ProvenanceIds",
                    content.m_provenanceIds);
                AppendReleaseJsonString(
                    output,
                    "LegalDispositionId",
                    content.m_legalDispositionId,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');

            AppendReleaseJsonName(output, "Provenance");
            output.push_back('[');
            for (size_t index = 0; index < envelope.m_provenance.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterReleaseProvenanceRecord& record =
                    envelope.m_provenance[index];
                output.push_back('{');
                AppendReleaseJsonString(
                    output,
                    "ProvenanceId",
                    record.m_provenanceId);
                AppendReleaseJsonString(output, "ContentId", record.m_contentId);
                AppendReleaseJsonString(output, "SubjectRef", record.m_subjectRef);
                AppendReleaseJsonString(output, "SourceKind", record.m_sourceKind);
                AppendReleaseJsonString(output, "SourceId", record.m_sourceId);
                AppendReleaseJsonString(
                    output,
                    "SourceFingerprint",
                    record.m_sourceFingerprint);
                AppendReleaseJsonStringArray(
                    output,
                    "EvidenceIds",
                    record.m_evidenceIds);
                AppendReleaseJsonString(
                    output,
                    "CapturedAtUtc",
                    record.m_capturedAtUtc);
                AppendReleaseJsonString(
                    output,
                    "Limitations",
                    record.m_limitations,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');

            AppendReleaseJsonName(output, "LegalDispositions");
            output.push_back('[');
            for (size_t index = 0;
                 index < envelope.m_legalDispositions.size();
                 ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterReleaseLegalDispositionRecord& record =
                    envelope.m_legalDispositions[index];
                output.push_back('{');
                AppendReleaseJsonString(
                    output,
                    "DispositionId",
                    record.m_dispositionId);
                AppendReleaseJsonString(output, "ContentId", record.m_contentId);
                AppendReleaseJsonString(
                    output,
                    "Disposition",
                    ToString(record.m_disposition));
                AppendReleaseJsonString(output, "Reviewer", record.m_reviewer);
                AppendReleaseJsonStringArray(
                    output,
                    "EvidenceIds",
                    record.m_evidenceIds);
                AppendReleaseJsonString(
                    output,
                    "ReviewedAtUtc",
                    record.m_reviewedAtUtc);
                AppendReleaseJsonString(
                    output,
                    "Rationale",
                    record.m_rationale,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');

            AppendReleaseJsonName(output, "SigningIntent");
            output.push_back('{');
            AppendReleaseJsonString(
                output,
                "IntentId",
                envelope.m_signingIntent.m_intentId);
            AppendReleaseJsonString(
                output,
                "Decision",
                ToString(envelope.m_signingIntent.m_decision));
            AppendReleaseJsonString(
                output,
                "IdentityKind",
                ToString(envelope.m_signingIntent.m_identityKind));
            AppendReleaseJsonString(
                output,
                "SignerId",
                envelope.m_signingIntent.m_signerId);
            AppendReleaseJsonString(
                output,
                "IdentityLocator",
                envelope.m_signingIntent.m_identityLocator);
            AppendReleaseJsonString(
                output,
                "IdentityFingerprint",
                envelope.m_signingIntent.m_identityFingerprint);
            AppendReleaseJsonString(
                output,
                "Reviewer",
                envelope.m_signingIntent.m_reviewer);
            AppendReleaseJsonStringArray(
                output,
                "EvidenceIds",
                envelope.m_signingIntent.m_evidenceIds);
            AppendReleaseJsonString(
                output,
                "ReviewedAtUtc",
                envelope.m_signingIntent.m_reviewedAtUtc);
            AppendReleaseJsonString(
                output,
                "Rationale",
                envelope.m_signingIntent.m_rationale,
                false);
            output.push_back('}');
            output.push_back(',');

            AppendReleaseJsonName(output, "PublicationTargets");
            output.push_back('[');
            for (size_t index = 0;
                 index < envelope.m_publicationTargets.size();
                 ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterReleasePublicationTarget& target =
                    envelope.m_publicationTargets[index];
                output.push_back('{');
                AppendReleaseJsonString(output, "TargetId", target.m_targetId);
                AppendReleaseJsonString(output, "Kind", ToString(target.m_kind));
                AppendReleaseJsonString(output, "Locator", target.m_locator);
                AppendReleaseJsonString(output, "Channel", target.m_channel);
                AppendReleaseJsonString(output, "Reviewer", target.m_reviewer);
                AppendReleaseJsonStringArray(
                    output,
                    "EvidenceIds",
                    target.m_evidenceIds);
                AppendReleaseJsonString(
                    output,
                    "ReviewedAtUtc",
                    target.m_reviewedAtUtc);
                AppendReleaseJsonString(
                    output,
                    "Rationale",
                    target.m_rationale,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');

            AppendReleaseJsonName(output, "Blockers");
            output.push_back('[');
            for (size_t index = 0; index < envelope.m_blockers.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterReleaseArtifactBlocker& blocker =
                    envelope.m_blockers[index];
                output.push_back('{');
                AppendReleaseJsonString(
                    output,
                    "BlockerId",
                    blocker.m_blockerId);
                AppendReleaseJsonString(output, "Code", blocker.m_code);
                AppendReleaseJsonString(
                    output,
                    "SubjectRef",
                    blocker.m_subjectRef);
                AppendReleaseJsonString(
                    output,
                    "Message",
                    blocker.m_message,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');

            AppendReleaseJsonBool(output, "MetadataReady", envelope.m_metadataReady);
            AppendReleaseJsonBool(
                output,
                "HumanReviewRequired",
                envelope.m_humanReviewRequired);
            AppendReleaseJsonBool(output, "FilesRead", envelope.m_filesRead);
            AppendReleaseJsonBool(output, "FilesHashed", envelope.m_filesHashed);
            AppendReleaseJsonBool(
                output,
                "ChecksumGenerated",
                envelope.m_checksumGenerated);
            AppendReleaseJsonBool(output, "FilesCopied", envelope.m_filesCopied);
            AppendReleaseJsonBool(
                output,
                "ArchiveAssembled",
                envelope.m_archiveAssembled);
            AppendReleaseJsonBool(
                output,
                "SigningPerformed",
                envelope.m_signingPerformed);
            AppendReleaseJsonBool(
                output,
                "UploadPerformed",
                envelope.m_uploadPerformed);
            AppendReleaseJsonBool(
                output,
                "ReleasePublished",
                envelope.m_releasePublished);
            AppendReleaseJsonBool(
                output,
                "LaunchPerformed",
                envelope.m_launchPerformed);
            AppendReleaseJsonBool(
                output,
                "AdapterCalled",
                envelope.m_adapterCalled);
            AppendReleaseJsonBool(
                output,
                "DeploymentMutated",
                envelope.m_deploymentMutated,
                false);
            output.push_back('}');
            return output;
        }

        EvidenceRecord BuildReleaseArtifactEvidence(
            const AZStd::string& evidenceId,
            const SourceRecord& source,
            const AdapterReleaseArtifactEnvelope& envelope,
            const AZStd::string& subjectRef,
            const AZStd::string& claim,
            const AZStd::string& evidenceKind,
            const AZStd::string& recordPath)
        {
            EvidenceRecord evidence;
            evidence.m_evidenceId = evidenceId;
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = envelope.m_profileId;
            evidence.m_gameVersion = envelope.m_gameVersion;
            evidence.m_branch = envelope.m_branch;
            evidence.m_subjectRef = subjectRef;
            evidence.m_claim = claim;
            evidence.m_evidenceKind = evidenceKind;
            evidence.m_confidence = "unrated";
            evidence.m_locator = source.m_locator;
            evidence.m_recordPath = recordPath;
            evidence.m_extractedAt = envelope.m_evaluatedAtUtc;
            return evidence;
        }

        void BuildReleaseArtifactCandidateEvidence(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            AdapterReleaseArtifactResult& result)
        {
            result.m_sourceDocuments = reconciliation.m_sourceDocuments;
            result.m_evidenceDocuments = reconciliation.m_evidenceDocuments;
            if (result.m_sourceDocuments.empty())
            {
                return;
            }

            const SourceRecord& source = result.m_sourceDocuments.front().m_source;
            EvidenceDocument document;
            document.m_sourceId = source.m_sourceId;
            document.m_sourceFingerprint = source.m_fingerprint;
            document.m_profileId = result.m_envelope.m_profileId;
            document.m_gameVersion = result.m_envelope.m_gameVersion;
            document.m_branch = result.m_envelope.m_branch;

            const AZStd::string prefix =
                "evidence.release-artifact." + result.m_envelope.m_artifactId;
            document.m_evidence.push_back(BuildReleaseArtifactEvidence(
                prefix + ".binding",
                source,
                result.m_envelope,
                ReleaseSubject(result.m_envelope.m_artifactId),
                "Release-artifact metadata binds the exact approved reconciliation, ready "
                "package layout, declared checksums, provenance, legal dispositions, "
                "signing intent, and publication targets without performing operations.",
                "release_artifact_binding",
                "ReleaseArtifact/Binding"));

            for (size_t index = 0; index < result.m_envelope.m_contents.size(); ++index)
            {
                const AdapterReleaseArtifactContent& content =
                    result.m_envelope.m_contents[index];
                document.m_evidence.push_back(BuildReleaseArtifactEvidence(
                    prefix + ".content." + ReleaseUnsignedString(index + 1),
                    source,
                    result.m_envelope,
                    "release-content:" + content.m_contentId,
                    "Declared package content " + content.m_packagePath
                        + " expects " + ToString(content.m_checksumAlgorithm)
                        + " " + content.m_expectedChecksum
                        + "; no file was read or hashed.",
                    "release_artifact_content_declaration",
                    "ReleaseArtifact/Contents/"
                        + ReleaseUnsignedString(index + 1)));
            }

            document.m_evidence.push_back(BuildReleaseArtifactEvidence(
                prefix + ".signing-intent",
                source,
                result.m_envelope,
                ReleaseSubject(result.m_envelope.m_artifactId),
                "Reviewed signing intent is "
                    + ToString(result.m_envelope.m_signingIntent.m_decision)
                    + " using identity kind "
                    + ToString(result.m_envelope.m_signingIntent.m_identityKind)
                    + "; no signature was produced.",
                "release_artifact_signing_intent",
                "ReleaseArtifact/SigningIntent"));

            for (size_t index = 0;
                 index < result.m_envelope.m_publicationTargets.size();
                 ++index)
            {
                const AdapterReleasePublicationTarget& target =
                    result.m_envelope.m_publicationTargets[index];
                document.m_evidence.push_back(BuildReleaseArtifactEvidence(
                    prefix + ".publication-target."
                        + ReleaseUnsignedString(index + 1),
                    source,
                    result.m_envelope,
                    ReleaseSubject(result.m_envelope.m_artifactId),
                    "Reviewed publication target declaration " + target.m_targetId
                        + " names " + target.m_locator
                        + "; no upload or publication occurred.",
                    "release_artifact_publication_target",
                    "ReleaseArtifact/PublicationTargets/"
                        + ReleaseUnsignedString(index + 1)));
            }

            result.m_evidenceDocuments.push_back(AZStd::move(document));
            result.m_sourceDocumentCount = static_cast<AZ::u64>(
                result.m_sourceDocuments.size());
            result.m_evidenceRecordCount = 0;
            for (const EvidenceDocument& evidenceDocument :
                 result.m_evidenceDocuments)
            {
                result.m_evidenceRecordCount += static_cast<AZ::u64>(
                    evidenceDocument.m_evidence.size());
            }
        }

        void SortReleaseArtifactEnvelope(AdapterReleaseArtifactEnvelope& envelope)
        {
            AZStd::sort(
                envelope.m_contents.begin(),
                envelope.m_contents.end(),
                [](const AdapterReleaseArtifactContent& left,
                    const AdapterReleaseArtifactContent& right)
                {
                    return left.m_contentId < right.m_contentId;
                });
            AZStd::sort(
                envelope.m_provenance.begin(),
                envelope.m_provenance.end(),
                [](const AdapterReleaseProvenanceRecord& left,
                    const AdapterReleaseProvenanceRecord& right)
                {
                    return left.m_provenanceId < right.m_provenanceId;
                });
            AZStd::sort(
                envelope.m_legalDispositions.begin(),
                envelope.m_legalDispositions.end(),
                [](const AdapterReleaseLegalDispositionRecord& left,
                    const AdapterReleaseLegalDispositionRecord& right)
                {
                    return left.m_dispositionId < right.m_dispositionId;
                });
            AZStd::sort(
                envelope.m_publicationTargets.begin(),
                envelope.m_publicationTargets.end(),
                [](const AdapterReleasePublicationTarget& left,
                    const AdapterReleasePublicationTarget& right)
                {
                    return left.m_targetId < right.m_targetId;
                });
            for (AdapterReleaseArtifactContent& content : envelope.m_contents)
            {
                SortUniqueReleaseValues(content.m_provenanceIds);
            }
            for (AdapterReleaseProvenanceRecord& record : envelope.m_provenance)
            {
                SortUniqueReleaseValues(record.m_evidenceIds);
            }
            for (AdapterReleaseLegalDispositionRecord& record :
                 envelope.m_legalDispositions)
            {
                SortUniqueReleaseValues(record.m_evidenceIds);
            }
            SortUniqueReleaseValues(envelope.m_signingIntent.m_evidenceIds);
            for (AdapterReleasePublicationTarget& target :
                 envelope.m_publicationTargets)
            {
                SortUniqueReleaseValues(target.m_evidenceIds);
            }
        }
    } // namespace

    AdapterReleaseArtifactResult
    AdapterReleaseArtifactProvenanceService::BuildEnvelope(
        const AdapterVerifierEvidenceReconciliationResult& reconciliation,
        const AdapterPackageAssemblyPreview& packagePreview,
        const SourceEvidenceRegistry& sourceRegistry,
        const AdapterReleaseArtifactRequest& request) const
    {
        AdapterReleaseArtifactResult result;
        AdapterReleaseArtifactEnvelope& envelope = result.m_envelope;
        const AdapterVerifierEvidenceReconciliationEnvelope& upstream =
            reconciliation.m_envelope;

        envelope.m_artifactId = request.m_artifactId;
        envelope.m_reconciliationId = upstream.m_reconciliationId;
        envelope.m_reconciliationCanonicalJson =
            request.m_reconciliationCanonicalJson;
        envelope.m_reportId = upstream.m_reportId;
        envelope.m_verifierResultId = upstream.m_verifierResultId;
        envelope.m_workOrderFingerprint = upstream.m_workOrderFingerprint;
        envelope.m_executionResultFingerprint =
            upstream.m_executionResultFingerprint;
        envelope.m_verifierResultFingerprint =
            upstream.m_verifierResultFingerprint;
        envelope.m_packagePreviewId = packagePreview.m_previewId;
        envelope.m_packagePreviewCanonicalJson =
            request.m_packagePreviewCanonicalJson;
        envelope.m_manifestId = packagePreview.m_manifestId;
        envelope.m_manifestFingerprint = packagePreview.m_manifestFingerprint;
        envelope.m_packId = packagePreview.m_packId;
        envelope.m_packVersion = packagePreview.m_packVersion;
        envelope.m_adapterId = packagePreview.m_adapterId;
        envelope.m_adapterVersion = packagePreview.m_adapterVersion;
        envelope.m_inventoryId = packagePreview.m_inventoryId;
        envelope.m_profileId = upstream.m_profileId;
        envelope.m_gameVersion = upstream.m_gameVersion;
        envelope.m_branch = upstream.m_branch;
        envelope.m_runtimeTarget = upstream.m_runtimeTarget;
        envelope.m_evaluatedAtUtc = request.m_evaluatedAtUtc;
        envelope.m_contents = request.m_contents;
        envelope.m_provenance = request.m_provenance;
        envelope.m_legalDispositions = request.m_legalDispositions;
        envelope.m_signingIntent = request.m_signingIntent;
        envelope.m_publicationTargets = request.m_publicationTargets;
        SortReleaseArtifactEnvelope(envelope);

        envelope.m_contentCount = static_cast<AZ::u64>(envelope.m_contents.size());
        envelope.m_provenanceCount = static_cast<AZ::u64>(
            envelope.m_provenance.size());
        envelope.m_legalDispositionCount = static_cast<AZ::u64>(
            envelope.m_legalDispositions.size());
        envelope.m_publicationTargetCount = static_cast<AZ::u64>(
            envelope.m_publicationTargets.size());

        ReleaseArtifactValidationFlags flags;
        ValidateReleaseArtifactUpstream(
            reconciliation,
            packagePreview,
            request,
            result,
            flags);
        ValidateReleaseArtifactBinding(
            reconciliation,
            packagePreview,
            request,
            result,
            flags);
        ValidateReleaseArtifactContents(
            packagePreview,
            request,
            result,
            flags);
        ValidateReleaseArtifactProvenance(
            reconciliation, sourceRegistry, request, result, flags);
        ValidateReleaseArtifactLegalDispositions(
            reconciliation, sourceRegistry, request, result, flags);
        ValidateReleaseArtifactSigningIntent(
            reconciliation, sourceRegistry, request, result, flags);
        ValidateReleaseArtifactPublicationTargets(
            reconciliation, sourceRegistry, request, result, flags);

        envelope.m_status = ResolveReleaseArtifactStatus(flags);
        envelope.m_metadataReady = envelope.m_status
            == AdapterReleaseArtifactEnvelopeStatus::Ready;
        result.m_ready = envelope.m_metadataReady;
        SortReleaseArtifactIssues(result);
        envelope.m_canonicalJson = SerializeReleaseArtifactEnvelope(envelope);

        if (result.m_ready)
        {
            BuildReleaseArtifactCandidateEvidence(reconciliation, result);
        }
        return result;
    }
} // namespace TaintedGrailModdingSDK
