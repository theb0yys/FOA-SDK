namespace TaintedGrailModdingSDK
{
    namespace
    {
        SourceDocument BuildVerifierSourceDocument(
            const AZStd::string& sourceId,
            const AZStd::string& title,
            const AZStd::string& sourceKind,
            const AZStd::string& locator,
            const AZStd::string& fingerprint,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AZStd::string& mediaType,
            const AZStd::string& limitations)
        {
            SourceDocument document;
            document.m_source.m_sourceId = sourceId;
            document.m_source.m_title = title;
            document.m_source.m_sourceKind = sourceKind;
            document.m_source.m_locator = locator;
            document.m_source.m_fingerprint = fingerprint;
            document.m_source.m_profileId = envelope.m_profileId;
            document.m_source.m_gameVersion = envelope.m_gameVersion;
            document.m_source.m_branch = envelope.m_branch;
            document.m_source.m_runtimeTarget = envelope.m_runtimeTarget;
            document.m_source.m_toolName = envelope.m_verifierReview.m_verifierId;
            document.m_source.m_toolVersion =
                envelope.m_verifierReview.m_verifierVersion;
            document.m_source.m_importerId =
                "tg.post-deployment-verifier-result";
            document.m_source.m_importerVersion = "1";
            document.m_source.m_capturedAt = envelope.m_capturedAtUtc;
            document.m_source.m_importedAt = envelope.m_capturedAtUtc;
            document.m_source.m_limitations = limitations;
            document.m_source.m_mediaType = mediaType;
            document.m_source.m_importStatus = "contract_validated";
            return document;
        }

        EvidenceRecord BuildVerifierEvidence(
            const AZStd::string& evidenceId,
            const SourceRecord& source,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AZStd::string& subjectRef,
            const AZStd::string& claim,
            const AZStd::string& evidenceKind,
            const AZStd::string& locator,
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
            evidence.m_locator = locator;
            evidence.m_recordPath = recordPath;
            evidence.m_extractedAt = envelope.m_capturedAtUtc;
            return evidence;
        }

        AZStd::string VerifierReportSubject(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope)
        {
            return "post-deployment-report:" + envelope.m_reportId;
        }

        AZStd::string VerifierResultSubject(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope)
        {
            return "post-deployment-verifier-result:"
                + envelope.m_verifierResultId;
        }

        AZStd::string VerifierStepSubject(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            return "deployment-work-order:" + envelope.m_workOrderId
                + ":step:" + stepId;
        }

        void BuildVerifierPrimaryEvidence(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result)
        {
            const AZStd::string sourceId =
                "source.post-deployment-verifier." + envelope.m_verifierResultId;
            const AZStd::string locator =
                "post-deployment-verifier-results/"
                + envelope.m_verifierResultId + ".json";
            SourceDocument sourceDocument = BuildVerifierSourceDocument(
                sourceId,
                "Post-deployment verifier result " + envelope.m_verifierResultId,
                "post_deployment_verifier_result",
                locator,
                envelope.m_resultFingerprint,
                envelope,
                "application/vnd.taintedgrail.post-deployment-verifier-result+json",
                "Contract-validated separately supplied verifier metadata only. No "
                "verifier was executed and no evidence was imported, promoted, validated, "
                "permitted, signed, published, or used to launch FoA automatically.");

            EvidenceDocument evidenceDocument;
            evidenceDocument.m_sourceId = sourceId;
            evidenceDocument.m_sourceFingerprint = envelope.m_resultFingerprint;
            evidenceDocument.m_profileId = envelope.m_profileId;
            evidenceDocument.m_gameVersion = envelope.m_gameVersion;
            evidenceDocument.m_branch = envelope.m_branch;

            evidenceDocument.m_evidence.push_back(BuildVerifierEvidence(
                "evidence.post-deployment-verifier."
                    + envelope.m_verifierResultId + ".report-binding",
                sourceDocument.m_source,
                envelope,
                VerifierReportSubject(envelope),
                "Independent verifier result binds exactly to review-ready report "
                    + envelope.m_reportId + ", execution result "
                    + envelope.m_resultId + ", and work order "
                    + envelope.m_workOrderId + ".",
                "post_deployment_verifier_report_binding",
                locator,
                "ReportBinding"));

            evidenceDocument.m_evidence.push_back(BuildVerifierEvidence(
                "evidence.post-deployment-verifier."
                    + envelope.m_verifierResultId + ".verifier-review",
                sourceDocument.m_source,
                envelope,
                VerifierResultSubject(envelope),
                "Verifier " + envelope.m_verifierReview.m_verifierId
                    + " version " + envelope.m_verifierReview.m_verifierVersion
                    + " was supplied with accepted review "
                    + envelope.m_verifierReview.m_reviewId + ".",
                "post_deployment_verifier_review",
                locator,
                "VerifierReview"));

            AZStd::vector<const AdapterPostDeploymentVerifierCheckResult*> checks;
            for (const AdapterPostDeploymentVerifierCheckResult& check :
                envelope.m_checkResults)
            {
                checks.push_back(&check);
            }
            AZStd::sort(
                checks.begin(),
                checks.end(),
                [](const AdapterPostDeploymentVerifierCheckResult* left,
                    const AdapterPostDeploymentVerifierCheckResult* right)
                {
                    if (left->m_sequence != right->m_sequence)
                    {
                        return left->m_sequence < right->m_sequence;
                    }
                    return left->m_checkId < right->m_checkId;
                });

            for (const AdapterPostDeploymentVerifierCheckResult* check : checks)
            {
                const AZStd::string sequence =
                    UnsignedVerifierString(check->m_sequence);
                evidenceDocument.m_evidence.push_back(BuildVerifierEvidence(
                    "evidence.post-deployment-verifier."
                        + envelope.m_verifierResultId + ".check." + sequence,
                    sourceDocument.m_source,
                    envelope,
                    VerifierStepSubject(envelope, check->m_stepId),
                    "Separately supplied verifier metadata reported "
                        + ToString(check->m_outcome) + " for target "
                        + check->m_targetPath + " with expected fingerprint "
                        + check->m_expectedFingerprint + " and observed fingerprint "
                        + check->m_observedFingerprint + ".",
                    "post_deployment_verifier_check_result",
                    locator,
                    "Checks/" + sequence));
            }

            AZStd::vector<const AdapterPostDeploymentVerifierFailure*> failures;
            for (const AdapterPostDeploymentVerifierFailure& failure :
                envelope.m_failures)
            {
                failures.push_back(&failure);
            }
            AZStd::sort(
                failures.begin(),
                failures.end(),
                [](const AdapterPostDeploymentVerifierFailure* left,
                    const AdapterPostDeploymentVerifierFailure* right)
                {
                    return left->m_failureId < right->m_failureId;
                });
            for (const AdapterPostDeploymentVerifierFailure* failure : failures)
            {
                const AdapterPostDeploymentVerifierCheckResult* check =
                    FindVerifierCheck(envelope, failure->m_checkId);
                const AZStd::string subject = check
                    ? VerifierStepSubject(envelope, check->m_stepId)
                    : VerifierResultSubject(envelope);
                evidenceDocument.m_evidence.push_back(BuildVerifierEvidence(
                    "evidence.post-deployment-verifier."
                        + envelope.m_verifierResultId + ".failure."
                        + failure->m_failureId,
                    sourceDocument.m_source,
                    envelope,
                    subject,
                    "Independent verifier failure " + failure->m_failureId
                        + " reported kind " + ToString(failure->m_kind)
                        + ", code " + failure->m_code + ": "
                        + failure->m_message,
                    "post_deployment_verifier_failure",
                    locator,
                    "Failures/" + failure->m_failureId));
            }

            result.m_sourceDocuments.push_back(AZStd::move(sourceDocument));
            result.m_evidenceDocuments.push_back(AZStd::move(evidenceDocument));
        }

        void BuildVerifierDiagnosticEvidence(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result)
        {
            AZStd::vector<const AdapterPostDeploymentVerifierDiagnosticReference*>
                diagnostics;
            for (const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic :
                envelope.m_diagnosticReferences)
            {
                diagnostics.push_back(&diagnostic);
            }
            AZStd::sort(
                diagnostics.begin(),
                diagnostics.end(),
                [](const AdapterPostDeploymentVerifierDiagnosticReference* left,
                    const AdapterPostDeploymentVerifierDiagnosticReference* right)
                {
                    return left->m_diagnosticId < right->m_diagnosticId;
                });

            for (const AdapterPostDeploymentVerifierDiagnosticReference* diagnostic :
                diagnostics)
            {
                const AZStd::string sourceId =
                    "source.post-deployment-verifier-diagnostic."
                    + envelope.m_verifierResultId + "."
                    + diagnostic->m_diagnosticId;
                SourceDocument sourceDocument = BuildVerifierSourceDocument(
                    sourceId,
                    "Post-deployment verifier diagnostic "
                        + diagnostic->m_diagnosticId,
                    "post_deployment_verifier_diagnostic",
                    diagnostic->m_reference,
                    diagnostic->m_fingerprint,
                    envelope,
                    "text/plain",
                    "Referenced verifier diagnostic content is not opened, persisted, "
                    "or inspected by the TG SDK.");

                EvidenceDocument evidenceDocument;
                evidenceDocument.m_sourceId = sourceId;
                evidenceDocument.m_sourceFingerprint = diagnostic->m_fingerprint;
                evidenceDocument.m_profileId = envelope.m_profileId;
                evidenceDocument.m_gameVersion = envelope.m_gameVersion;
                evidenceDocument.m_branch = envelope.m_branch;

                AZStd::vector<AZStd::string> checkIds = diagnostic->m_checkIds;
                SortUniqueVerifierValues(checkIds);
                for (size_t index = 0; index < checkIds.size(); ++index)
                {
                    const AdapterPostDeploymentVerifierCheckResult* check =
                        FindVerifierCheck(envelope, checkIds[index]);
                    const AZStd::string subject = check
                        ? VerifierStepSubject(envelope, check->m_stepId)
                        : VerifierResultSubject(envelope);
                    evidenceDocument.m_evidence.push_back(BuildVerifierEvidence(
                        "evidence.post-deployment-verifier-diagnostic."
                            + envelope.m_verifierResultId + "."
                            + diagnostic->m_diagnosticId + "."
                            + UnsignedVerifierString(index + 1),
                        sourceDocument.m_source,
                        envelope,
                        subject,
                        "Independent verifier " + ToString(diagnostic->m_kind)
                            + " diagnostic " + diagnostic->m_diagnosticId
                            + " is referenced with fingerprint "
                            + diagnostic->m_fingerprint + ".",
                        "post_deployment_verifier_diagnostic_reference",
                        diagnostic->m_reference,
                        "Diagnostics/" + diagnostic->m_diagnosticId + "/"
                            + UnsignedVerifierString(index + 1)));
                }

                result.m_sourceDocuments.push_back(AZStd::move(sourceDocument));
                result.m_evidenceDocuments.push_back(AZStd::move(evidenceDocument));
            }
        }

        void FinalizeVerifierCounts(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result)
        {
            result.m_checkCount =
                static_cast<AZ::u64>(envelope.m_checkResults.size());
            for (const AdapterPostDeploymentVerifierCheckResult& check :
                envelope.m_checkResults)
            {
                switch (check.m_outcome)
                {
                case AdapterPostDeploymentVerifierCheckOutcome::Matched:
                    ++result.m_matchedCheckCount;
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Mismatched:
                    ++result.m_mismatchedCheckCount;
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Failed:
                    ++result.m_failedCheckCount;
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Inconclusive:
                    ++result.m_inconclusiveCheckCount;
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::NotRun:
                    ++result.m_notRunCheckCount;
                    break;
                }
            }
            result.m_failureCount =
                static_cast<AZ::u64>(envelope.m_failures.size());
            result.m_diagnosticReferenceCount = static_cast<AZ::u64>(
                envelope.m_diagnosticReferences.size());
            result.m_sourceDocumentCount =
                static_cast<AZ::u64>(result.m_sourceDocuments.size());
            result.m_evidenceRecordCount = 0;
            for (const EvidenceDocument& document : result.m_evidenceDocuments)
            {
                result.m_evidenceRecordCount +=
                    static_cast<AZ::u64>(document.m_evidence.size());
            }
        }
    } // namespace

    AZStd::string
    AdapterPostDeploymentVerifierEvidenceService::SerializeCanonicalReport(
        const AdapterPostDeploymentVerificationReport& report) const
    {
        return SerializeVerifierInputReport(report);
    }

    AdapterPostDeploymentVerifierEvidenceReturn
    AdapterPostDeploymentVerifierEvidenceService::BuildEvidenceReturn(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
        const AdapterPostDeploymentVerificationReport& report,
        const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope) const
    {
        AdapterPostDeploymentVerifierEvidenceReturn result;
        result.m_verifierResultId = verifierEnvelope.m_verifierResultId;
        result.m_reportId = verifierEnvelope.m_reportId;
        result.m_resultId = verifierEnvelope.m_resultId;
        result.m_workOrderId = verifierEnvelope.m_workOrderId;
        result.m_resultFingerprint = verifierEnvelope.m_resultFingerprint;

        VerifierValidationFlags flags;
        ValidateVerifierReportReadiness(
            workOrder,
            executionEnvelope,
            report,
            result,
            flags);
        ValidateVerifierReview(workOrder, verifierEnvelope, result, flags);
        ValidateVerifierReportBinding(report, verifierEnvelope, result, flags);
        ValidateVerifierEnvelopeShape(verifierEnvelope, result, flags);
        ValidateVerifierCheckCoverage(workOrder, verifierEnvelope, result, flags);
        ValidateVerifierFailureDiagnosticBindings(
            verifierEnvelope,
            result,
            flags);

        result.m_status = ResolveVerifierStatus(flags);
        result.m_contractValid = VerifierContractIsValid(flags);
        result.m_accepted = result.m_status
            == AdapterPostDeploymentVerifierEnvelopeStatus::Accepted;
        SortVerifierIssues(result);

        if (result.m_contractValid)
        {
            BuildVerifierPrimaryEvidence(verifierEnvelope, result);
            BuildVerifierDiagnosticEvidence(verifierEnvelope, result);
        }
        FinalizeVerifierCounts(verifierEnvelope, result);
        return result;
    }
} // namespace TaintedGrailModdingSDK
