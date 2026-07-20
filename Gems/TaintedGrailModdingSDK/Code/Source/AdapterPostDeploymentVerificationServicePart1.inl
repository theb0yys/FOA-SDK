/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionResultCanonical.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

namespace TaintedGrailModdingSDK
{
    namespace
    {
        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        AZStd::string StepSubject(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            return "deployment-work-order:" + envelope.m_workOrderId
                + ":step:" + stepId;
        }

        AZStd::string RollbackSubject(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& rollbackResultId)
        {
            return "deployment-work-order:" + envelope.m_workOrderId
                + ":rollback:" + rollbackResultId;
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

        AZStd::string ResultLocator(
            const AdapterDeploymentExecutionResultEnvelope& envelope)
        {
            return "deployment-results/" + envelope.m_resultId + ".json";
        }

        bool IsMutationStep(AdapterDeploymentWorkOrderStepKind kind)
        {
            return kind == AdapterDeploymentWorkOrderStepKind::Add
                || kind == AdapterDeploymentWorkOrderStepKind::Replace
                || kind == AdapterDeploymentWorkOrderStepKind::Remove;
        }

        const AdapterDeploymentExecutionStepResult* FindStepResult(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentExecutionStepResult& step :
                envelope.m_stepResults)
            {
                if (step.m_stepId == stepId)
                {
                    return &step;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentTargetVerification* FindVerification(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentTargetVerification& verification :
                envelope.m_targetVerifications)
            {
                if (verification.m_stepId == stepId)
                {
                    return &verification;
                }
            }
            return nullptr;
        }

        const AdapterDeploymentRollbackResult* FindRollback(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::string& stepId)
        {
            for (const AdapterDeploymentRollbackResult& rollback :
                envelope.m_rollbackResults)
            {
                if (rollback.m_sourceStepId == stepId)
                {
                    return &rollback;
                }
            }
            return nullptr;
        }

        const SourceDocument* FindSourceDocument(
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            const AZStd::string& sourceId)
        {
            for (const SourceDocument& document :
                evidenceReturn.m_sourceDocuments)
            {
                if (document.m_source.m_sourceId == sourceId)
                {
                    return &document;
                }
            }
            return nullptr;
        }

        AZStd::vector<AZStd::string> EvidenceIdsForSubject(
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            const AZStd::string& subjectRef)
        {
            AZStd::vector<AZStd::string> result;
            for (const EvidenceDocument& document :
                evidenceReturn.m_evidenceDocuments)
            {
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    if (evidence.m_subjectRef == subjectRef)
                    {
                        result.push_back(evidence.m_evidenceId);
                    }
                }
            }
            SortUnique(result);
            return result;
        }

        AZStd::vector<AZStd::string> FailureLogIds(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::vector<AZStd::string>& failureIds)
        {
            AZStd::vector<AZStd::string> result;
            for (const AZStd::string& failureId : failureIds)
            {
                for (const AdapterDeploymentExecutionFailure& failure :
                    envelope.m_failures)
                {
                    if (failure.m_failureId == failureId)
                    {
                        result.insert(
                            result.end(),
                            failure.m_logReferenceIds.begin(),
                            failure.m_logReferenceIds.end());
                    }
                }
            }
            SortUnique(result);
            return result;
        }

        AZStd::vector<AZStd::string> CombinedLogIds(
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AZStd::vector<AZStd::string>& directLogIds,
            const AZStd::vector<AZStd::string>& failureIds)
        {
            AZStd::vector<AZStd::string> result = directLogIds;
            const AZStd::vector<AZStd::string> failureLogIds =
                FailureLogIds(envelope, failureIds);
            result.insert(
                result.end(),
                failureLogIds.begin(),
                failureLogIds.end());
            SortUnique(result);
            return result;
        }

        AdapterPostDeploymentBlocker* FindBlocker(
            AdapterPostDeploymentVerificationReport& report,
            const AZStd::string& blockerId)
        {
            for (AdapterPostDeploymentBlocker& blocker : report.m_blockers)
            {
                if (blocker.m_blockerId == blockerId)
                {
                    return &blocker;
                }
            }
            return nullptr;
        }

        void AddBlocker(
            AdapterPostDeploymentVerificationReport& report,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            AdapterPostDeploymentBlockerKind kind,
            const AZStd::string& identity,
            AZStd::string code,
            AZStd::string subjectRef,
            AZStd::string message,
            AZStd::string stepId = {},
            AZStd::string rollbackResultId = {},
            AZStd::vector<AZStd::string> evidenceIds = {},
            AZStd::vector<AZStd::string> logReferenceIds = {},
            bool blocksCompatibility = false,
            bool blocksRelease = true)
        {
            const AZStd::string stableIdentity =
                identity.empty() ? "report" : identity;
            const AZStd::string blockerId =
                "blocker.post-deployment." + envelope.m_resultId + "."
                + ToString(kind) + "." + stableIdentity;

            SortUnique(evidenceIds);
            SortUnique(logReferenceIds);
            if (AdapterPostDeploymentBlocker* existing =
                    FindBlocker(report, blockerId))
            {
                existing->m_evidenceIds.insert(
                    existing->m_evidenceIds.end(),
                    evidenceIds.begin(),
                    evidenceIds.end());
                existing->m_logReferenceIds.insert(
                    existing->m_logReferenceIds.end(),
                    logReferenceIds.begin(),
                    logReferenceIds.end());
                SortUnique(existing->m_evidenceIds);
                SortUnique(existing->m_logReferenceIds);
                existing->m_blocksCompatibility =
                    existing->m_blocksCompatibility || blocksCompatibility;
                existing->m_blocksRelease =
                    existing->m_blocksRelease || blocksRelease;
                return;
            }

            AdapterPostDeploymentBlocker blocker;
            blocker.m_blockerId = blockerId;
            blocker.m_kind = kind;
            blocker.m_severity = AdapterPostDeploymentBlockerSeverity::Blocking;
            blocker.m_code = AZStd::move(code);
            blocker.m_subjectRef = AZStd::move(subjectRef);
            blocker.m_message = AZStd::move(message);
            blocker.m_stepId = AZStd::move(stepId);
            blocker.m_rollbackResultId = AZStd::move(rollbackResultId);
            blocker.m_evidenceIds = AZStd::move(evidenceIds);
            blocker.m_logReferenceIds = AZStd::move(logReferenceIds);
            blocker.m_blocksCompatibility = blocksCompatibility;
            blocker.m_blocksRelease = blocksRelease;
            report.m_blockers.push_back(AZStd::move(blocker));
        }

        bool HasExactWorkOrderBindingEvidence(
            const EvidenceRecord& evidence,
            const SourceRecord& source,
            const AdapterDeploymentExecutionResultEnvelope& envelope)
        {
            const AZStd::string expectedClaim =
                "Deployment result binds exactly to review-ready work order "
                + envelope.m_workOrderId + " with declared fingerprint "
                + envelope.m_workOrderFingerprint + ".";
            return evidence.m_evidenceKind == "deployment_work_order_binding"
                && evidence.m_subjectRef == WorkOrderSubject(envelope)
                && evidence.m_claim == expectedClaim
                && evidence.m_locator == ResultLocator(envelope)
                && evidence.m_recordPath == "WorkOrderBinding"
                && evidence.m_extractedAt == envelope.m_capturedAtUtc
                && evidence.m_sourceId == source.m_sourceId
                && evidence.m_sourceFingerprint == source.m_fingerprint;
        }

        bool HasExactExecutorReviewEvidence(
            const EvidenceRecord& evidence,
            const SourceRecord& source,
            const AdapterDeploymentExecutionResultEnvelope& envelope)
        {
            const AZStd::string expectedClaim =
                "Executor " + envelope.m_executorReview.m_executorId
                + " version " + envelope.m_executorReview.m_executorVersion
                + " was supplied with accepted review "
                + envelope.m_executorReview.m_reviewId + ".";
            return evidence.m_evidenceKind == "deployment_executor_review"
                && evidence.m_subjectRef == ResultSubject(envelope)
                && evidence.m_claim == expectedClaim
                && evidence.m_locator == ResultLocator(envelope)
                && evidence.m_recordPath == "ExecutorReview"
                && evidence.m_extractedAt == envelope.m_capturedAtUtc
                && evidence.m_sourceId == source.m_sourceId
                && evidence.m_sourceFingerprint == source.m_fingerprint;
        }

        bool ValidateEvidenceReturnBinding(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& envelope,
            const AdapterDeploymentExecutionEvidenceReturn& evidenceReturn,
            AdapterPostDeploymentVerificationReport& report)
        {
            bool valid = true;
            if (workOrder.m_status
                    != AdapterDeploymentWorkOrderStatus::ReviewReady
                || workOrder.m_workOrderId != envelope.m_workOrderId
                || workOrder.m_canonicalJson
                    != envelope.m_workOrderCanonicalJson
                || !CanonicalSha256Matches(
                    workOrder.m_canonicalJson,
                    envelope.m_workOrderFingerprint)
                || !DeploymentExecutionResultFingerprintMatches(envelope)
                || workOrder.m_previewId != envelope.m_previewId
                || workOrder.m_previewFingerprint
                    != envelope.m_previewFingerprint
                || workOrder.m_packId != envelope.m_packId
                || workOrder.m_targetInventoryId
                    != envelope.m_targetInventoryId
                || workOrder.m_executionAllowed
                || workOrder.m_copyAllowed
                || workOrder.m_deleteAllowed
                || workOrder.m_backupAllowed
                || workOrder.m_restoreAllowed
                || workOrder.m_deploymentAllowed
                || workOrder.m_launchAllowed)
            {
                valid = false;
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                    "current-work-order",
                    "post_deployment.work_order_binding_mismatch",
                    ResultSubject(envelope),
                    "The report requires the exact current review-ready work order, "
                    "canonical JSON and derived fingerprints, preview, pack, target "
                    "inventory, and all execution and mutation permissions false.");
            }
            if (!evidenceReturn.m_accepted
                || evidenceReturn.m_status
                    != AdapterDeploymentExecutionEnvelopeStatus::Accepted)
            {
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::ExecutionEvidenceRejected,
                    "contract",
                    "post_deployment.execution_evidence_rejected",
                    ResultSubject(envelope),
                    "Post-deployment reporting requires one accepted execution-result "
                    "evidence return. Rejected metadata is not interpreted as deployment "
                    "or compatibility evidence.");
                return false;
            }

            if (evidenceReturn.m_resultId != envelope.m_resultId
                || evidenceReturn.m_workOrderId != envelope.m_workOrderId
                || evidenceReturn.m_workOrderFingerprint
                    != envelope.m_workOrderFingerprint
                || evidenceReturn.m_resultFingerprint
                    != envelope.m_resultFingerprint
                || evidenceReturn.m_stepResultCount
                    != static_cast<AZ::u64>(envelope.m_stepResults.size())
                || evidenceReturn.m_backupResultCount
                    != static_cast<AZ::u64>(envelope.m_backupResults.size())
                || evidenceReturn.m_verificationCount
                    != static_cast<AZ::u64>(
                        envelope.m_targetVerifications.size())
                || evidenceReturn.m_rollbackResultCount
                    != static_cast<AZ::u64>(envelope.m_rollbackResults.size())
                || evidenceReturn.m_failureCount
                    != static_cast<AZ::u64>(envelope.m_failures.size())
                || evidenceReturn.m_logReferenceCount
                    != static_cast<AZ::u64>(envelope.m_logReferences.size()))
            {
                valid = false;
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                    "result-binding",
                    "post_deployment.evidence_binding_mismatch",
                    ResultSubject(envelope),
                    "The candidate evidence return does not preserve the exact result, "
                    "work-order, fingerprint, or typed result counts.");
            }

            AZ::u64 evidenceRecordCount = 0;
            AZStd::vector<AZStd::string> sourceIds;
            for (const SourceDocument& document :
                evidenceReturn.m_sourceDocuments)
            {
                const SourceRecord& source = document.m_source;
                sourceIds.push_back(source.m_sourceId);
                report.m_candidateSourceIds.push_back(source.m_sourceId);
                if (!IsStableContractId(source.m_sourceId)
                    || !IsSha256Fingerprint(source.m_fingerprint)
                    || source.m_profileId != envelope.m_profileId
                    || source.m_gameVersion != envelope.m_gameVersion
                    || source.m_branch != envelope.m_branch
                    || source.m_runtimeTarget != envelope.m_runtimeTarget
                    || source.m_importStatus != "contract_validated"
                    || source.m_locator.empty()
                    || !IsStrictUtcTimestamp(source.m_capturedAt)
                    || source.m_capturedAt != envelope.m_capturedAtUtc)
                {
                    valid = false;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                        source.m_sourceId.empty() ? "source" : source.m_sourceId,
                        "post_deployment.source_binding_mismatch",
                        ResultSubject(envelope),
                        "A candidate source document does not preserve stable identity, "
                        "SHA-256 fingerprint, exact context, contract-validated import "
                        "status, safe locator, or capture time.");
                }
            }
            AZStd::sort(sourceIds.begin(), sourceIds.end());
            if (AZStd::adjacent_find(sourceIds.begin(), sourceIds.end())
                != sourceIds.end())
            {
                valid = false;
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                    "duplicate-source",
                    "post_deployment.duplicate_candidate_source",
                    ResultSubject(envelope),
                    "Candidate source document identities must be unique.");
            }

            AZStd::vector<AZStd::string> evidenceIds;
            bool hasWorkOrderBinding = false;
            bool hasExecutorReview = false;
            for (const EvidenceDocument& document :
                evidenceReturn.m_evidenceDocuments)
            {
                const SourceDocument* source =
                    FindSourceDocument(evidenceReturn, document.m_sourceId);
                if (!source
                    || document.m_sourceFingerprint
                        != source->m_source.m_fingerprint
                    || document.m_profileId != envelope.m_profileId
                    || document.m_gameVersion != envelope.m_gameVersion
                    || document.m_branch != envelope.m_branch)
                {
                    valid = false;
                    AddBlocker(
                        report,
                        envelope,
                        AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                        document.m_sourceId.empty()
                            ? "evidence-document"
                            : document.m_sourceId,
                        "post_deployment.evidence_document_binding_mismatch",
                        ResultSubject(envelope),
                        "A candidate evidence document does not bind to a known candidate "
                        "source and the exact deployment result context.");
                }

                evidenceRecordCount +=
                    static_cast<AZ::u64>(document.m_evidence.size());
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    evidenceIds.push_back(evidence.m_evidenceId);
                    report.m_candidateEvidenceIds.push_back(evidence.m_evidenceId);
                    bool specialBindingMismatch = false;
                    if (source)
                    {
                        const bool exactWorkOrderBinding =
                            HasExactWorkOrderBindingEvidence(
                                evidence,
                                source->m_source,
                                envelope);
                        const bool exactExecutorReview =
                            HasExactExecutorReviewEvidence(
                                evidence,
                                source->m_source,
                                envelope);
                        hasWorkOrderBinding = hasWorkOrderBinding
                            || exactWorkOrderBinding;
                        hasExecutorReview = hasExecutorReview
                            || exactExecutorReview;
                        specialBindingMismatch =
                            (evidence.m_evidenceKind
                                    == "deployment_work_order_binding"
                                && !exactWorkOrderBinding)
                            || (evidence.m_evidenceKind
                                    == "deployment_executor_review"
                                && !exactExecutorReview);
                    }
                    else if (evidence.m_evidenceKind
                            == "deployment_work_order_binding"
                        || evidence.m_evidenceKind
                            == "deployment_executor_review")
                    {
                        specialBindingMismatch = true;
                    }
                    if (!IsStableContractId(evidence.m_evidenceId)
                        || evidence.m_sourceId != document.m_sourceId
                        || evidence.m_sourceFingerprint
                            != document.m_sourceFingerprint
                        || !IsSha256Fingerprint(evidence.m_sourceFingerprint)
                        || evidence.m_profileId != envelope.m_profileId
                        || evidence.m_gameVersion != envelope.m_gameVersion
                        || evidence.m_branch != envelope.m_branch
                        || evidence.m_confidence != "unrated"
                        || evidence.m_subjectRef.empty()
                        || evidence.m_claim.empty()
                        || evidence.m_evidenceKind.empty()
                        || evidence.m_locator.empty()
                        || evidence.m_recordPath.empty()
                        || !IsStrictUtcTimestamp(evidence.m_extractedAt)
                        || evidence.m_extractedAt != envelope.m_capturedAtUtc
                        || specialBindingMismatch)
                    {
                        valid = false;
                        AddBlocker(
                            report,
                            envelope,
                            AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                            evidence.m_evidenceId.empty()
                                ? "evidence-record"
                                : evidence.m_evidenceId,
                            "post_deployment.evidence_record_binding_mismatch",
                            ResultSubject(envelope),
                            "A candidate evidence record does not preserve stable identity, "
                            "source SHA-256 binding, exact context, claim, kind, locator, "
                            "record path, or real UTC extraction time.");
                    }
                }
            }
            AZStd::sort(evidenceIds.begin(), evidenceIds.end());
            if (AZStd::adjacent_find(evidenceIds.begin(), evidenceIds.end())
                != evidenceIds.end())
            {
                valid = false;
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::EvidenceBindingMismatch,
                    "duplicate-evidence",
                    "post_deployment.duplicate_candidate_evidence",
                    ResultSubject(envelope),
                    "Candidate evidence record identities must be unique.");
            }

            if (evidenceReturn.m_sourceDocumentCount
                    != static_cast<AZ::u64>(
                        evidenceReturn.m_sourceDocuments.size())
                || evidenceReturn.m_evidenceRecordCount != evidenceRecordCount
                || evidenceReturn.m_sourceDocuments.empty()
                || evidenceRecordCount == 0
                || !hasWorkOrderBinding
                || !hasExecutorReview)
            {
                valid = false;
                AddBlocker(
                    report,
                    envelope,
                    AdapterPostDeploymentBlockerKind::CandidateEvidenceMissing,
                    "candidate-set",
                    "post_deployment.candidate_evidence_missing",
                    ResultSubject(envelope),
                    "The accepted result must return a complete counted candidate source "
                    "and evidence set containing exact-subject, exact-claim work-order "
                    "binding and executor-review evidence.");
            }

            SortUnique(report.m_candidateSourceIds);
            SortUnique(report.m_candidateEvidenceIds);
            report.m_candidateSourceDocumentCount =
                static_cast<AZ::u64>(report.m_candidateSourceIds.size());
            report.m_candidateEvidenceRecordCount =
                static_cast<AZ::u64>(report.m_candidateEvidenceIds.size());
            return valid;
        }

    } // namespace
} // namespace TaintedGrailModdingSDK
