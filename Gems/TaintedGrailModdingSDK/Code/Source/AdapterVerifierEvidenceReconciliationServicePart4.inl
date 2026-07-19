namespace TaintedGrailModdingSDK
{
    namespace
    {
        EvidenceRecord BuildReconciliationEvidence(
            const AZStd::string& evidenceId,
            const SourceRecord& source,
            const AdapterVerifierEvidenceReconciliationEnvelope& envelope,
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

        void BuildReconciliationCandidateEvidence(
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AdapterVerifierEvidenceReconciliationResult& result)
        {
            result.m_sourceDocuments = verifierEvidence.m_sourceDocuments;
            result.m_evidenceDocuments = verifierEvidence.m_evidenceDocuments;
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
                "evidence.verifier-reconciliation."
                + result.m_envelope.m_reconciliationId;
            document.m_evidence.push_back(BuildReconciliationEvidence(
                prefix + ".binding",
                source,
                result.m_envelope,
                "post-deployment-report:" + result.m_envelope.m_reportId,
                "Reconciliation binds the exact post-deployment report, verifier "
                "evidence return, execution result, work order, runtime context, "
                "candidate identities, and named human release review.",
                "verifier_reconciliation_binding",
                "Reconciliation/Binding"));

            for (size_t index = 0;
                 index < result.m_envelope.m_findings.size();
                 ++index)
            {
                const AdapterVerifierReconciliationFinding& finding =
                    result.m_envelope.m_findings[index];
                document.m_evidence.push_back(BuildReconciliationEvidence(
                    prefix + ".finding."
                        + ReconciliationUnsignedString(index + 1),
                    source,
                    result.m_envelope,
                    finding.m_subjectRef,
                    "Reconciliation preserved finding " + finding.m_findingId
                        + " as " + ToString(finding.m_kind)
                        + " with relationship "
                        + ToString(finding.m_relationship) + ".",
                    "verifier_reconciliation_finding",
                    "Reconciliation/Findings/"
                        + ReconciliationUnsignedString(index + 1)));
            }

            document.m_evidence.push_back(BuildReconciliationEvidence(
                prefix + ".human-review",
                source,
                result.m_envelope,
                "post-deployment-report:" + result.m_envelope.m_reportId,
                "Named human review "
                    + result.m_envelope.m_releaseReview.m_reviewId
                    + " recorded "
                    + ToString(result.m_envelope.m_releaseReview.m_decision)
                    + " while preserving compatibility assessment "
                    + ToString(result.m_envelope.m_compatibilityAssessment)
                    + " and release decision "
                    + ToString(result.m_envelope.m_releaseDecision) + ".",
                "verifier_reconciliation_human_review",
                "Reconciliation/HumanReview"));

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
    } // namespace

    AZStd::string
    AdapterVerifierEvidenceReconciliationService::SerializeCanonicalEnvelope(
        const AdapterVerifierEvidenceReconciliationEnvelope& envelope) const
    {
        return SerializeReconciliationEnvelope(envelope);
    }

    AdapterVerifierEvidenceReconciliationResult
    AdapterVerifierEvidenceReconciliationService::BuildReconciliation(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
        const AdapterPostDeploymentVerificationReport& report,
        const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
        const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
        const AdapterVerifierEvidenceReconciliationRequest& request) const
    {
        AdapterVerifierEvidenceReconciliationResult result;
        AdapterVerifierEvidenceReconciliationEnvelope& envelope =
            result.m_envelope;
        envelope.m_reconciliationId = request.m_reconciliationId;
        envelope.m_reportId = report.m_reportId;
        envelope.m_reportStatus = report.m_status;
        envelope.m_reportCanonicalJson = verifierEnvelope.m_reportCanonicalJson;
        envelope.m_verifierResultId = verifierEnvelope.m_verifierResultId;
        envelope.m_verifierEvidenceStatus = verifierEvidence.m_status;
        envelope.m_executionResultId = executionEnvelope.m_resultId;
        envelope.m_workOrderId = workOrder.m_workOrderId;
        envelope.m_workOrderFingerprint =
            executionEnvelope.m_workOrderFingerprint;
        envelope.m_executionResultFingerprint =
            executionEnvelope.m_resultFingerprint;
        envelope.m_verifierResultFingerprint =
            verifierEnvelope.m_resultFingerprint;
        envelope.m_profileId = executionEnvelope.m_profileId;
        envelope.m_gameVersion = executionEnvelope.m_gameVersion;
        envelope.m_branch = executionEnvelope.m_branch;
        envelope.m_runtimeTarget = executionEnvelope.m_runtimeTarget;
        envelope.m_packId = executionEnvelope.m_packId;
        envelope.m_previewId = executionEnvelope.m_previewId;
        envelope.m_previewFingerprint = executionEnvelope.m_previewFingerprint;
        envelope.m_targetInventoryId = executionEnvelope.m_targetInventoryId;
        envelope.m_evaluatedAtUtc = request.m_evaluatedAtUtc;
        envelope.m_releaseReview = request.m_releaseReview;

        GatherReconciliationCandidateIds(report, verifierEvidence, envelope);
        BuildReconciliationFindings(report, verifierEnvelope, envelope);
        ResolveReconciliationCompatibility(verifierEvidence, envelope);

        ReconciliationFlags flags;
        ValidateReconciliationReportReadiness(
            workOrder,
            executionEnvelope,
            report,
            result,
            flags);
        ValidateReconciliationVerifierEvidence(
            report,
            verifierEnvelope,
            verifierEvidence,
            result,
            flags);
        ValidateReconciliationBinding(
            workOrder,
            executionEnvelope,
            report,
            verifierEnvelope,
            request,
            result,
            flags);
        ValidateReconciliationReleaseReview(request, result, flags);
        ResolveReconciliationReleaseDecision(
            report,
            verifierEvidence,
            result,
            flags);

        envelope.m_status = ResolveReconciliationStatus(flags);
        result.m_accepted = envelope.m_status
            == AdapterVerifierReconciliationEnvelopeStatus::Accepted;
        SortReconciliationIssues(result);
        envelope.m_canonicalJson = SerializeReconciliationEnvelope(envelope);

        if (result.m_accepted)
        {
            BuildReconciliationCandidateEvidence(verifierEvidence, result);
        }
        return result;
    }
} // namespace TaintedGrailModdingSDK
