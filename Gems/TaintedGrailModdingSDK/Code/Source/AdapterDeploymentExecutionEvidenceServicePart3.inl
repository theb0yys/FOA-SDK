namespace TaintedGrailModdingSDK
{
    namespace
    {
        SourceDocument BuildSourceDocument(
            const AZStd::string& sourceId,
            const AZStd::string& title,
            const AZStd::string& sourceKind,
            const AZStd::string& locator,
            const AZStd::string& fingerprint,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
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
            document.m_source.m_toolName =
                envelope.m_executorReview.m_executorId;
            document.m_source.m_toolVersion =
                envelope.m_executorReview.m_executorVersion;
            document.m_source.m_importerId =
                "tg.deployment-execution-result";
            document.m_source.m_importerVersion = "1";
            document.m_source.m_capturedAt = envelope.m_capturedAtUtc;
            document.m_source.m_importedAt = envelope.m_capturedAtUtc;
            document.m_source.m_limitations = limitations;
            document.m_source.m_mediaType = mediaType;
            document.m_source.m_importStatus = "contract_validated";
            return document;
        }

        EvidenceRecord BuildEvidence(
            const AZStd::string& evidenceId,
            const SourceRecord& source,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
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

        AZStd::string ResultSubject(
            const AdapterDeploymentExecutionResultEnvelope& envelope)
        {
            return "deployment-result:" + envelope.m_resultId;
        }

        AZStd::string WorkOrderSubject(
            const AdapterDeploymentExecutionResultEnvelope& envelope)
        {
            return "deployment-work-order:" + envelope.m_workOrderId;
        }

        AZStd::string StepSubject(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            return WorkOrderSubject(envelope) + ":step:" + stepId;
        }

        AZStd::string RollbackSubject(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& rollbackId)
        {
            return WorkOrderSubject(envelope) + ":rollback:" + rollbackId;
        }

        void BuildPrimaryEvidence(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result)
        {
            const AZStd::string sourceId =
                "source.deployment-execution." + envelope.m_resultId;
            const AZStd::string locator =
                "deployment-results/" + envelope.m_resultId + ".json";
            SourceDocument sourceDocument = BuildSourceDocument(
                sourceId,
                "Deployment execution result " + envelope.m_resultId,
                "deployment_execution_result",
                locator,
                envelope.m_resultFingerprint,
                envelope,
                "application/vnd.taintedgrail.deployment-execution-result+json",
                "Contract-validated executor-supplied metadata only. No executor "
                "was invoked and no evidence was imported, promoted, validated, "
                "permitted, or published automatically.");

            EvidenceDocument evidenceDocument;
            evidenceDocument.m_sourceId = sourceId;
            evidenceDocument.m_sourceFingerprint =
                envelope.m_resultFingerprint;
            evidenceDocument.m_profileId = envelope.m_profileId;
            evidenceDocument.m_gameVersion = envelope.m_gameVersion;
            evidenceDocument.m_branch = envelope.m_branch;

            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.deployment-execution." + envelope.m_resultId
                    + ".work-order-binding",
                sourceDocument.m_source,
                envelope,
                WorkOrderSubject(envelope),
                "Deployment result binds exactly to review-ready work order "
                    + workOrder.m_workOrderId + " with declared fingerprint "
                    + envelope.m_workOrderFingerprint + ".",
                "deployment_work_order_binding",
                locator,
                "WorkOrderBinding"));

            evidenceDocument.m_evidence.push_back(BuildEvidence(
                "evidence.deployment-execution." + envelope.m_resultId
                    + ".executor-review",
                sourceDocument.m_source,
                envelope,
                ResultSubject(envelope),
                "Executor " + envelope.m_executorReview.m_executorId
                    + " version "
                    + envelope.m_executorReview.m_executorVersion
                    + " was supplied with accepted review "
                    + envelope.m_executorReview.m_reviewId + ".",
                "deployment_executor_review",
                locator,
                "ExecutorReview"));

            AZStd::vector<const AdapterDeploymentExecutionStepResult*> steps;
            steps.reserve(envelope.m_stepResults.size());
            for (const AdapterDeploymentExecutionStepResult& step :
                envelope.m_stepResults)
            {
                steps.push_back(&step);
            }
            AZStd::sort(
                steps.begin(),
                steps.end(),
                [](const AdapterDeploymentExecutionStepResult* left,
                    const AdapterDeploymentExecutionStepResult* right)
                {
                    if (left->m_sequence != right->m_sequence)
                    {
                        return left->m_sequence < right->m_sequence;
                    }
                    return left->m_stepId < right->m_stepId;
                });

            for (const AdapterDeploymentExecutionStepResult* step : steps)
            {
                const AZStd::string sequence =
                    UnsignedString(step->m_sequence);
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.deployment-execution." + envelope.m_resultId
                        + ".step." + sequence,
                    sourceDocument.m_source,
                    envelope,
                    StepSubject(envelope, step->m_stepId),
                    "Executor metadata reported "
                        + ToString(step->m_outcome)
                        + " for deployment work-order step "
                        + step->m_stepId + " ("
                        + ToString(step->m_kind) + ").",
                    "deployment_execution_step_result",
                    locator,
                    "StepResults/" + sequence));
            }

            AZStd::vector<const AdapterDeploymentBackupResult*> backups;
            for (const AdapterDeploymentBackupResult& backup :
                envelope.m_backupResults)
            {
                backups.push_back(&backup);
            }
            AZStd::sort(
                backups.begin(),
                backups.end(),
                [](const AdapterDeploymentBackupResult* left,
                    const AdapterDeploymentBackupResult* right)
                {
                    return left->m_stepId < right->m_stepId;
                });
            for (const AdapterDeploymentBackupResult* backup : backups)
            {
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.deployment-execution." + envelope.m_resultId
                        + ".backup." + backup->m_stepId,
                    sourceDocument.m_source,
                    envelope,
                    StepSubject(envelope, backup->m_stepId),
                    "Backup outcome reported "
                        + ToString(backup->m_outcome)
                        + " for " + backup->m_targetPath
                        + " with backup fingerprint "
                        + backup->m_backupFingerprint + ".",
                    "deployment_backup_result",
                    locator,
                    "BackupResults/" + backup->m_stepId));
            }

            AZStd::vector<const AdapterDeploymentTargetVerification*>
                verifications;
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                verifications.push_back(&verification);
            }
            AZStd::sort(
                verifications.begin(),
                verifications.end(),
                [](const AdapterDeploymentTargetVerification* left,
                    const AdapterDeploymentTargetVerification* right)
                {
                    return left->m_stepId < right->m_stepId;
                });
            for (const AdapterDeploymentTargetVerification* verification :
                verifications)
            {
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.deployment-execution." + envelope.m_resultId
                        + ".verification." + verification->m_stepId,
                    sourceDocument.m_source,
                    envelope,
                    StepSubject(envelope, verification->m_stepId),
                    "Target verification reported "
                        + ToString(verification->m_status)
                        + " for " + verification->m_targetPath
                        + " with observed fingerprint "
                        + verification->m_observedFingerprint + ".",
                    "deployment_target_verification",
                    locator,
                    "TargetVerifications/" + verification->m_stepId));
            }

            AZStd::vector<const AdapterDeploymentRollbackResult*> rollbacks;
            for (const AdapterDeploymentRollbackResult& rollback :
                envelope.m_rollbackResults)
            {
                rollbacks.push_back(&rollback);
            }
            AZStd::sort(
                rollbacks.begin(),
                rollbacks.end(),
                [](const AdapterDeploymentRollbackResult* left,
                    const AdapterDeploymentRollbackResult* right)
                {
                    return left->m_rollbackResultId
                        < right->m_rollbackResultId;
                });
            for (const AdapterDeploymentRollbackResult* rollback : rollbacks)
            {
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.deployment-execution." + envelope.m_resultId
                        + ".rollback." + rollback->m_rollbackResultId,
                    sourceDocument.m_source,
                    envelope,
                    RollbackSubject(
                        envelope,
                        rollback->m_rollbackResultId),
                    "Rollback action " + ToString(rollback->m_action)
                        + " reported " + ToString(rollback->m_outcome)
                        + " for " + rollback->m_targetPath + ".",
                    "deployment_rollback_result",
                    locator,
                    "RollbackResults/"
                        + rollback->m_rollbackResultId));
            }

            AZStd::vector<const AdapterDeploymentExecutionFailure*> failures;
            for (const AdapterDeploymentExecutionFailure& failure :
                envelope.m_failures)
            {
                failures.push_back(&failure);
            }
            AZStd::sort(
                failures.begin(),
                failures.end(),
                [](const AdapterDeploymentExecutionFailure* left,
                    const AdapterDeploymentExecutionFailure* right)
                {
                    return left->m_failureId < right->m_failureId;
                });
            for (const AdapterDeploymentExecutionFailure* failure : failures)
            {
                AZStd::string subject = ResultSubject(envelope);
                if (!failure->m_rollbackResultId.empty())
                {
                    subject = RollbackSubject(
                        envelope,
                        failure->m_rollbackResultId);
                }
                else if (!failure->m_stepId.empty())
                {
                    subject = StepSubject(envelope, failure->m_stepId);
                }
                evidenceDocument.m_evidence.push_back(BuildEvidence(
                    "evidence.deployment-execution." + envelope.m_resultId
                        + ".failure." + failure->m_failureId,
                    sourceDocument.m_source,
                    envelope,
                    subject,
                    "Deployment execution failure "
                        + failure->m_failureId + " reported kind "
                        + ToString(failure->m_kind) + ", code "
                        + failure->m_code + ": " + failure->m_message,
                    "deployment_execution_failure",
                    locator,
                    "Failures/" + failure->m_failureId));
            }

            result.m_sourceDocuments.push_back(
                AZStd::move(sourceDocument));
            result.m_evidenceDocuments.push_back(
                AZStd::move(evidenceDocument));
        }

        void BuildLogEvidence(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result)
        {
            AZStd::vector<const AdapterDeploymentExecutionLogReference*> logs;
            for (const AdapterDeploymentExecutionLogReference& log :
                envelope.m_logReferences)
            {
                logs.push_back(&log);
            }
            AZStd::sort(
                logs.begin(),
                logs.end(),
                [](const AdapterDeploymentExecutionLogReference* left,
                    const AdapterDeploymentExecutionLogReference* right)
                {
                    return left->m_logId < right->m_logId;
                });

            for (const AdapterDeploymentExecutionLogReference* log : logs)
            {
                const AZStd::string sourceId =
                    "source.deployment-execution-log."
                    + envelope.m_resultId + "." + log->m_logId;
                SourceDocument sourceDocument = BuildSourceDocument(
                    sourceId,
                    "Deployment execution log " + log->m_logId,
                    "deployment_execution_log",
                    log->m_reference,
                    log->m_fingerprint,
                    envelope,
                    "text/plain",
                    "Referenced log content is not opened, persisted, or inspected "
                    "by Slice 15.");

                EvidenceDocument evidenceDocument;
                evidenceDocument.m_sourceId = sourceId;
                evidenceDocument.m_sourceFingerprint = log->m_fingerprint;
                evidenceDocument.m_profileId = envelope.m_profileId;
                evidenceDocument.m_gameVersion = envelope.m_gameVersion;
                evidenceDocument.m_branch = envelope.m_branch;

                AZStd::vector<AZStd::string> subjects;
                for (const AZStd::string& stepId : log->m_stepIds)
                {
                    subjects.push_back(StepSubject(envelope, stepId));
                }
                for (const AZStd::string& rollbackId :
                    log->m_rollbackResultIds)
                {
                    subjects.push_back(
                        RollbackSubject(envelope, rollbackId));
                }
                SortUnique(subjects);
                if (subjects.empty())
                {
                    subjects.push_back(ResultSubject(envelope));
                }

                for (size_t index = 0; index < subjects.size(); ++index)
                {
                    evidenceDocument.m_evidence.push_back(BuildEvidence(
                        "evidence.deployment-execution-log."
                            + envelope.m_resultId + "." + log->m_logId
                            + "." + UnsignedString(index + 1),
                        sourceDocument.m_source,
                        envelope,
                        subjects[index],
                        "Deployment " + ToString(log->m_kind)
                            + " log " + log->m_logId
                            + " is referenced with fingerprint "
                            + log->m_fingerprint + ".",
                        "deployment_execution_log_reference",
                        log->m_reference,
                        "LogReferences/" + log->m_logId + "/"
                            + UnsignedString(index + 1)));
                }

                result.m_sourceDocuments.push_back(
                    AZStd::move(sourceDocument));
                result.m_evidenceDocuments.push_back(
                    AZStd::move(evidenceDocument));
            }
        }

        void FinalizeCounts(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterDeploymentExecutionEvidenceReturn& result)
        {
            result.m_stepResultCount =
                static_cast<AZ::u64>(envelope.m_stepResults.size());
            result.m_backupResultCount =
                static_cast<AZ::u64>(envelope.m_backupResults.size());
            result.m_verificationCount =
                static_cast<AZ::u64>(
                    envelope.m_targetVerifications.size());
            result.m_rollbackResultCount =
                static_cast<AZ::u64>(envelope.m_rollbackResults.size());
            result.m_failureCount =
                static_cast<AZ::u64>(envelope.m_failures.size());
            result.m_logReferenceCount =
                static_cast<AZ::u64>(envelope.m_logReferences.size());
            result.m_sourceDocumentCount =
                static_cast<AZ::u64>(result.m_sourceDocuments.size());
            result.m_evidenceRecordCount = 0;
            for (const EvidenceDocument& document :
                result.m_evidenceDocuments)
            {
                result.m_evidenceRecordCount +=
                    static_cast<AZ::u64>(document.m_evidence.size());
            }
        }
    } // namespace

    AdapterDeploymentExecutionEvidenceReturn
    AdapterDeploymentExecutionEvidenceService::BuildEvidenceReturn(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& envelope) const
    {
        AdapterDeploymentExecutionEvidenceReturn result;
        result.m_resultId = envelope.m_resultId;
        result.m_workOrderId = envelope.m_workOrderId;
        result.m_workOrderFingerprint =
            envelope.m_workOrderFingerprint;
        result.m_resultFingerprint = envelope.m_resultFingerprint;

        ValidationFlags flags;
        ValidateWorkOrderReadiness(workOrder, result, flags);
        ValidateExecutorReview(envelope, result, flags);
        ValidateEnvelopeAndWorkOrderBinding(
            workOrder,
            envelope,
            result,
            flags);
        ValidateStepBindings(workOrder, envelope, result, flags);
        ValidateBackupBindings(workOrder, envelope, result, flags);
        ValidateVerificationBindings(workOrder, envelope, result, flags);
        ValidateRollbackBindings(workOrder, envelope, result, flags);
        ValidateFailureAndLogBindings(
            workOrder,
            envelope,
            result,
            flags);

        result.m_status = ResolveStatus(flags);
        result.m_accepted =
            result.m_status
            == AdapterDeploymentExecutionEnvelopeStatus::Accepted;
        SortIssues(result);

        if (result.m_accepted)
        {
            BuildPrimaryEvidence(workOrder, envelope, result);
            BuildLogEvidence(envelope, result);
        }
        FinalizeCounts(envelope, result);
        return result;
    }
} // namespace TaintedGrailModdingSDK
