/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "AdapterDeploymentExecutionEvidenceService.h"
#include "AdapterDeploymentExecutionResultCanonical.h"
#include "AdapterPostDeploymentVerificationService.h"
#include "AdapterPostDeploymentVerifierEvidenceService.h"
#include "AdapterPostDeploymentVerifierResultCanonical.h"
#include "AdapterReleaseArtifactProvenanceService.h"
#include "AdapterVerifierEvidenceReconciliationService.h"
#include "CanonicalFingerprint.h"
#include "SourceEvidenceRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK::Test
{
    inline AZStd::string FixtureFingerprint(char value)
    {
        return "sha256:" + AZStd::string(64, value);
    }

    inline AZStd::string FixtureUnsigned(AZ::u64 value)
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

    inline void FixtureSortUnique(AZStd::vector<AZStd::string>& values)
    {
        AZStd::sort(values.begin(), values.end());
        values.erase(AZStd::unique(values.begin(), values.end()), values.end());
    }

    struct AdapterResearchPipelineFixture
    {
        AdapterDeploymentWorkOrder m_workOrder;
        AdapterDeploymentExecutionResultEnvelope m_executionEnvelope;
        AdapterDeploymentExecutionEvidenceReturn m_executionEvidence;
        AdapterPostDeploymentVerificationReport m_report;
        SourceEvidenceRegistry m_sourceRegistry;
        AdapterPostDeploymentVerifierResultEnvelope m_verifierEnvelope;
        AdapterPostDeploymentVerifierEvidenceReturn m_verifierEvidence;
        AdapterVerifierEvidenceReconciliationRequest m_reconciliationRequest;
        AdapterVerifierEvidenceReconciliationResult m_reconciliation;
        AdapterPackageAssemblyPreview m_packagePreview;
        AdapterReleaseArtifactRequest m_releaseRequest;
        AdapterReleaseArtifactResult m_releaseArtifact;

        static AdapterDeploymentWorkOrderStep MakeWorkOrderStep(
            AZ::u64 sequence,
            AdapterDeploymentWorkOrderStepKind kind,
            AZStd::string targetPath = {},
            AZStd::string backupPath = {},
            AZStd::string previousFingerprint = {},
            AZStd::string desiredFingerprint = {})
        {
            AdapterDeploymentWorkOrderStep step;
            step.m_sequence = sequence;
            step.m_stepId = "deployment.work-order:owner.pipeline:step:"
                + FixtureUnsigned(sequence);
            step.m_kind = kind;
            step.m_targetPath = AZStd::move(targetPath);
            step.m_backupPath = AZStd::move(backupPath);
            step.m_previousFingerprint = AZStd::move(previousFingerprint);
            step.m_desiredFingerprint = AZStd::move(desiredFingerprint);
            step.m_evidenceIds = {
                "owner.evidence.work-order-step." + FixtureUnsigned(sequence),
            };
            step.m_description = "Synthetic exact-bound pipeline test step.";
            step.m_executionAllowed = false;
            return step;
        }

        static AdapterDeploymentWorkOrder BuildWorkOrder()
        {
            AdapterDeploymentWorkOrder workOrder;
            workOrder.m_workOrderId = "deployment.work-order:owner.pipeline";
            workOrder.m_previewId = "deploymentpreview:owner.pipeline";
            workOrder.m_previewFingerprint = FixtureFingerprint('a');
            workOrder.m_packId = "owner.pipeline";
            workOrder.m_targetInventoryId = "owner.target-inventory";
            workOrder.m_confirmationId = "owner.confirmation";
            workOrder.m_confirmationScope =
                AdapterDeploymentConfirmationScope::FullPreview;
            workOrder.m_reviewer = "pipeline-reviewer";
            workOrder.m_evaluatedAtUtc = "2026-07-19T13:00:00Z";
            workOrder.m_confirmationExpiresAtUtc = "2026-07-19T14:00:00Z";
            workOrder.m_maintenanceWindowId = "owner.window";
            workOrder.m_maintenanceStartAtUtc = "2026-07-19T12:30:00Z";
            workOrder.m_maintenanceEndAtUtc = "2026-07-19T13:30:00Z";
            workOrder.m_operatorGroup = "pipeline-operators";
            workOrder.m_status = AdapterDeploymentWorkOrderStatus::ReviewReady;
            workOrder.m_steps = {
                MakeWorkOrderStep(
                    1,
                    AdapterDeploymentWorkOrderStepKind::VerifyPreflight,
                    {},
                    {},
                    {},
                    FixtureFingerprint('a')),
                MakeWorkOrderStep(
                    2,
                    AdapterDeploymentWorkOrderStepKind::ConfirmMaintenanceWindow,
                    "BepInEx/plugins/owner.pipeline"),
                MakeWorkOrderStep(
                    3,
                    AdapterDeploymentWorkOrderStepKind::Backup,
                    "BepInEx/plugins/owner.pipeline/old.dll",
                    "Backups/owner.pipeline/old.dll",
                    FixtureFingerprint('1'),
                    FixtureFingerprint('1')),
                MakeWorkOrderStep(
                    4,
                    AdapterDeploymentWorkOrderStepKind::Add,
                    "BepInEx/plugins/owner.pipeline/new.dll",
                    {},
                    {},
                    FixtureFingerprint('2')),
                MakeWorkOrderStep(
                    5,
                    AdapterDeploymentWorkOrderStepKind::Replace,
                    "BepInEx/plugins/owner.pipeline/replace.dll",
                    "Backups/owner.pipeline/replace.dll",
                    FixtureFingerprint('3'),
                    FixtureFingerprint('4')),
                MakeWorkOrderStep(
                    6,
                    AdapterDeploymentWorkOrderStepKind::Remove,
                    "BepInEx/plugins/owner.pipeline/old.dll",
                    "Backups/owner.pipeline/old.dll",
                    FixtureFingerprint('1'),
                    {}),
                MakeWorkOrderStep(
                    7,
                    AdapterDeploymentWorkOrderStepKind::VerifyDeployment,
                    "BepInEx/plugins/owner.pipeline",
                    {},
                    {},
                    FixtureFingerprint('a')),
                MakeWorkOrderStep(
                    8,
                    AdapterDeploymentWorkOrderStepKind::PreserveRollback,
                    "Backups/owner.pipeline",
                    "Backups/owner.pipeline"),
            };
            AdapterDeploymentWorkOrderService service;
            workOrder.m_canonicalJson =
                service.SerializeCanonicalWorkOrder(workOrder);
            return workOrder;
        }

        static AdapterDeploymentExecutionStepResult MakeStepResult(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentExecutionStepResult result;
            result.m_stepId = step.m_stepId;
            result.m_sequence = step.m_sequence;
            result.m_kind = step.m_kind;
            result.m_targetPath = step.m_targetPath;
            result.m_backupPath = step.m_backupPath;
            result.m_previousFingerprint = step.m_previousFingerprint;
            result.m_desiredFingerprint = step.m_desiredFingerprint;
            result.m_outcome = AdapterDeploymentExecutionOutcome::Succeeded;
            result.m_attempted = true;
            if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                || step.m_kind == AdapterDeploymentWorkOrderStepKind::Replace)
            {
                result.m_targetPresent = true;
                result.m_observedFingerprint = step.m_desiredFingerprint;
            }
            else if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Backup)
            {
                result.m_targetPresent = true;
                result.m_observedFingerprint = step.m_previousFingerprint;
            }
            return result;
        }

        static AdapterDeploymentTargetVerification MakeVerification(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentTargetVerification verification;
            verification.m_stepId = step.m_stepId;
            verification.m_targetPath = step.m_targetPath;
            verification.m_expectedPresent =
                step.m_kind != AdapterDeploymentWorkOrderStepKind::Remove;
            verification.m_expectedFingerprint = verification.m_expectedPresent
                ? step.m_desiredFingerprint
                : AZStd::string{};
            verification.m_observedPresent = verification.m_expectedPresent;
            verification.m_observedFingerprint =
                verification.m_expectedFingerprint;
            verification.m_status = AdapterDeploymentVerificationStatus::Matched;
            verification.m_checkedAtUtc = "2026-07-19T14:05:00Z";
            return verification;
        }

        static AdapterDeploymentRollbackResult MakeRollback(
            const AdapterDeploymentWorkOrderStep& step)
        {
            AdapterDeploymentRollbackResult rollback;
            rollback.m_rollbackResultId = "deployment.rollback-result:owner.pipeline."
                + FixtureUnsigned(step.m_sequence);
            rollback.m_sourceStepId = step.m_stepId;
            rollback.m_targetPath = step.m_targetPath;
            rollback.m_outcome =
                AdapterDeploymentExecutionOutcome::NotAttempted;
            rollback.m_attempted = false;
            if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add)
            {
                rollback.m_action = AdapterDeploymentRollbackAction::RemoveAdded;
                rollback.m_expectedDeployedFingerprint =
                    step.m_desiredFingerprint;
            }
            else if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Replace)
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RestoreReplaced;
                rollback.m_backupPath = step.m_backupPath;
                rollback.m_expectedDeployedFingerprint =
                    step.m_desiredFingerprint;
                rollback.m_restoreFingerprint = step.m_previousFingerprint;
            }
            else
            {
                rollback.m_action =
                    AdapterDeploymentRollbackAction::RestoreRemoved;
                rollback.m_backupPath = step.m_backupPath;
                rollback.m_restoreFingerprint = step.m_previousFingerprint;
            }
            return rollback;
        }

        static AdapterDeploymentExecutionResultEnvelope BuildExecutionEnvelope(
            const AdapterDeploymentWorkOrder& workOrder)
        {
            AdapterDeploymentExecutionResultEnvelope envelope;
            envelope.m_resultId = "deployment.result:owner.pipeline";
            envelope.m_workOrderId = workOrder.m_workOrderId;
            envelope.m_workOrderCanonicalJson = workOrder.m_canonicalJson;
            envelope.m_workOrderFingerprint =
                CalculateCanonicalSha256(workOrder.m_canonicalJson);
            envelope.m_previewId = workOrder.m_previewId;
            envelope.m_previewFingerprint = workOrder.m_previewFingerprint;
            envelope.m_packId = workOrder.m_packId;
            envelope.m_targetInventoryId = workOrder.m_targetInventoryId;
            envelope.m_profileId = "owner.profile";
            envelope.m_gameVersion = "1.0.0";
            envelope.m_branch = "release";
            envelope.m_runtimeTarget = "Mono";
            envelope.m_capturedAtUtc = "2026-07-19T14:10:00Z";
            envelope.m_executorReview.m_reviewId = "owner.executor-review";
            envelope.m_executorReview.m_executorId =
                "owner.deployment-executor";
            envelope.m_executorReview.m_executorVersion = "1.0.0";
            envelope.m_executorReview.m_executorFingerprint =
                FixtureFingerprint('d');
            envelope.m_executorReview.m_decision =
                AdapterDeploymentExecutorReviewDecision::Accepted;
            envelope.m_executorReview.m_reviewer = "executor-reviewer";
            envelope.m_executorReview.m_evidenceIds = {
                "owner.evidence.executor-review",
            };
            envelope.m_executorReview.m_reviewedAtUtc =
                "2026-07-19T13:30:00Z";

            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                envelope.m_stepResults.push_back(MakeStepResult(step));
                if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Backup)
                {
                    AdapterDeploymentBackupResult backup;
                    backup.m_stepId = step.m_stepId;
                    backup.m_targetPath = step.m_targetPath;
                    backup.m_backupPath = step.m_backupPath;
                    backup.m_sourceFingerprint = step.m_previousFingerprint;
                    backup.m_backupFingerprint = step.m_previousFingerprint;
                    backup.m_outcome =
                        AdapterDeploymentExecutionOutcome::Succeeded;
                    backup.m_attempted = true;
                    envelope.m_backupResults.push_back(AZStd::move(backup));
                }
                if (step.m_kind == AdapterDeploymentWorkOrderStepKind::Add
                    || step.m_kind == AdapterDeploymentWorkOrderStepKind::Replace
                    || step.m_kind == AdapterDeploymentWorkOrderStepKind::Remove)
                {
                    envelope.m_targetVerifications.push_back(
                        MakeVerification(step));
                    envelope.m_rollbackResults.push_back(MakeRollback(step));
                }
            }
            envelope.m_resultFingerprint =
                CalculateDeploymentExecutionResultFingerprint(envelope);
            return envelope;
        }

        static SourceEvidenceRegistry BuildVerifierReviewRegistry(
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AZStd::string& verifierId,
            const AZStd::string& evidenceId)
        {
            SourceEvidenceRegistry registry;
            SourceRecord source;
            source.m_sourceId = "owner.verifier-review-source";
            source.m_title = "Independent verifier review source";
            source.m_sourceKind = "verifier_review";
            source.m_locator = "research/verifier-review.json";
            source.m_fingerprint = FixtureFingerprint('e');
            source.m_profileId = executionEnvelope.m_profileId;
            source.m_gameVersion = executionEnvelope.m_gameVersion;
            source.m_branch = executionEnvelope.m_branch;
            source.m_runtimeTarget = executionEnvelope.m_runtimeTarget;
            source.m_toolName = "review-registry";
            source.m_toolVersion = "1.0.0";
            source.m_importerId = "owner.verifier-review-importer";
            source.m_importerVersion = "1.0.0";
            source.m_capturedAt = "2026-07-19T12:00:00Z";
            source.m_importedAt = "2026-07-19T12:10:00Z";
            source.m_mediaType = "application/json";
            source.m_byteSize = 512;
            source.m_importStatus = "imported";
            AZStd::string error;
            registry.RegisterSource(source, &error);

            EvidenceRecord evidence;
            evidence.m_evidenceId = evidenceId;
            evidence.m_sourceId = source.m_sourceId;
            evidence.m_sourceFingerprint = source.m_fingerprint;
            evidence.m_profileId = source.m_profileId;
            evidence.m_gameVersion = source.m_gameVersion;
            evidence.m_branch = source.m_branch;
            evidence.m_subjectRef = "post-deployment-verifier:" + verifierId;
            evidence.m_claim =
                "The named verifier identity and capabilities were reviewed for the exact active profile.";
            evidence.m_evidenceKind = "verifier_identity_review";
            evidence.m_confidence = "high";
            evidence.m_locator = source.m_locator;
            evidence.m_recordPath = "VerifierReview";
            evidence.m_extractedAt = "2026-07-19T12:05:00Z";
            registry.RegisterEvidence(evidence, &error);
            return registry;
        }

        static AdapterPostDeploymentVerifierResultEnvelope BuildVerifierEnvelope(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const AZStd::string& reviewEvidenceId)
        {
            AdapterPostDeploymentVerifierEvidenceService service;
            AdapterPostDeploymentVerifierResultEnvelope envelope;
            envelope.m_verifierResultId = "owner.verifier-result";
            envelope.m_reportId = report.m_reportId;
            envelope.m_reportStatus = report.m_status;
            envelope.m_reportCanonicalJson =
                service.SerializeCanonicalReport(report);
            envelope.m_resultId = executionEnvelope.m_resultId;
            envelope.m_workOrderId = workOrder.m_workOrderId;
            envelope.m_workOrderFingerprint =
                executionEnvelope.m_workOrderFingerprint;
            envelope.m_executionResultFingerprint =
                executionEnvelope.m_resultFingerprint;
            envelope.m_profileId = executionEnvelope.m_profileId;
            envelope.m_gameVersion = executionEnvelope.m_gameVersion;
            envelope.m_branch = executionEnvelope.m_branch;
            envelope.m_runtimeTarget = executionEnvelope.m_runtimeTarget;
            envelope.m_capturedAtUtc = "2026-07-19T14:20:00Z";
            envelope.m_verifierReview.m_reviewId = "owner.verifier-review";
            envelope.m_verifierReview.m_verifierId = "owner.verifier";
            envelope.m_verifierReview.m_verifierVersion = "1.0.0";
            envelope.m_verifierReview.m_verifierFingerprint =
                FixtureFingerprint('f');
            envelope.m_verifierReview.m_decision =
                AdapterPostDeploymentVerifierReviewDecision::Accepted;
            envelope.m_verifierReview.m_reviewer = "verifier-reviewer";
            envelope.m_verifierReview.m_capabilities = {
                AdapterPostDeploymentVerifierCapability::TargetPresence,
                AdapterPostDeploymentVerifierCapability::TargetFingerprint,
                AdapterPostDeploymentVerifierCapability::TargetAbsence,
            };
            envelope.m_verifierReview.m_evidenceIds = { reviewEvidenceId };
            envelope.m_verifierReview.m_reviewedAtUtc =
                "2026-07-19T13:45:00Z";

            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (step.m_kind != AdapterDeploymentWorkOrderStepKind::Add
                    && step.m_kind != AdapterDeploymentWorkOrderStepKind::Replace
                    && step.m_kind != AdapterDeploymentWorkOrderStepKind::Remove)
                {
                    continue;
                }
                AdapterPostDeploymentVerifierCheckResult check;
                check.m_checkId = "owner.verifier-check."
                    + FixtureUnsigned(step.m_sequence);
                check.m_sequence = step.m_sequence;
                check.m_stepId = step.m_stepId;
                check.m_targetPath = step.m_targetPath;
                check.m_expectedPresent =
                    step.m_kind != AdapterDeploymentWorkOrderStepKind::Remove;
                check.m_expectedFingerprint = check.m_expectedPresent
                    ? step.m_desiredFingerprint
                    : AZStd::string{};
                check.m_observedPresent = check.m_expectedPresent;
                check.m_observedFingerprint = check.m_expectedFingerprint;
                check.m_attempted = true;
                check.m_observationRecorded = true;
                check.m_outcome =
                    AdapterPostDeploymentVerifierCheckOutcome::Matched;
                check.m_checkedAtUtc = "2026-07-19T14:15:00Z";
                envelope.m_checkResults.push_back(AZStd::move(check));
            }
            envelope.m_resultFingerprint =
                CalculatePostDeploymentVerifierResultFingerprint(envelope);
            return envelope;
        }

        static void GatherReconciliationCandidateIds(
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence,
            AZStd::vector<AZStd::string>& sourceIds,
            AZStd::vector<AZStd::string>& evidenceIds)
        {
            sourceIds = report.m_candidateSourceIds;
            evidenceIds = report.m_candidateEvidenceIds;
            for (const SourceDocument& document : verifierEvidence.m_sourceDocuments)
            {
                sourceIds.push_back(document.m_source.m_sourceId);
            }
            for (const EvidenceDocument& document : verifierEvidence.m_evidenceDocuments)
            {
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    evidenceIds.push_back(evidence.m_evidenceId);
                }
            }
            FixtureSortUnique(sourceIds);
            FixtureSortUnique(evidenceIds);
        }

        static AdapterVerifierEvidenceReconciliationRequest BuildReconciliationRequest(
            const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
            const AdapterPostDeploymentVerificationReport& report,
            const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope,
            const AdapterPostDeploymentVerifierEvidenceReturn& verifierEvidence)
        {
            AdapterVerifierEvidenceReconciliationRequest request;
            request.m_reconciliationId = "owner.reconciliation";
            request.m_evaluatedAtUtc = "2026-07-19T14:30:00Z";
            AdapterVerifierReleaseReview& review = request.m_releaseReview;
            review.m_reviewId = "owner.release-review";
            review.m_reportId = report.m_reportId;
            review.m_reportCanonicalJson = verifierEnvelope.m_reportCanonicalJson;
            review.m_verifierResultId = verifierEnvelope.m_verifierResultId;
            review.m_workOrderFingerprint =
                executionEnvelope.m_workOrderFingerprint;
            review.m_executionResultFingerprint =
                executionEnvelope.m_resultFingerprint;
            review.m_verifierResultFingerprint =
                verifierEnvelope.m_resultFingerprint;
            GatherReconciliationCandidateIds(
                report,
                verifierEvidence,
                review.m_candidateSourceIds,
                review.m_candidateEvidenceIds);
            review.m_decision = AdapterVerifierReleaseReviewDecision::Approve;
            review.m_reviewer = "release-reviewer";
            if (!review.m_candidateEvidenceIds.empty())
            {
                review.m_evidenceIds.push_back(
                    review.m_candidateEvidenceIds.front());
            }
            review.m_reviewedAtUtc = "2026-07-19T14:25:00Z";
            review.m_rationale =
                "All exact-bound verifier observations match and no upstream blocker remains.";
            return request;
        }

        static AdapterPackageAssemblyPreview BuildPackagePreview(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation)
        {
            AdapterPackageAssemblyPreview preview;
            preview.m_previewId = "packagepreview:owner.pipeline";
            preview.m_manifestId = "buildmanifest:owner.pipeline";
            preview.m_manifestFingerprint = FixtureFingerprint('7');
            preview.m_packId = reconciliation.m_envelope.m_packId;
            preview.m_packVersion = "1.0.0";
            preview.m_adapterId = "owner.adapter";
            preview.m_adapterVersion = "1.0.0";
            preview.m_planId = "workorder.plan:owner.pipeline";
            preview.m_inventoryId = "owner.package-inventory";
            preview.m_packageRoot = "BepInEx/plugins/owner.pipeline";
            preview.m_status = AdapterPackageAssemblyPreviewStatus::Ready;
            preview.m_canonicalJson =
                "{\"PreviewId\":\"packagepreview:owner.pipeline\","
                "\"AssemblyAllowed\":false,\"ArchiveAllowed\":false,"
                "\"DeploymentAllowed\":false}";

            AdapterPackageLayoutEntry entry;
            entry.m_inventoryEntryId = "owner.package-entry";
            entry.m_stagingPath = "Staging/owner.pipeline.dll";
            entry.m_packagePath =
                "BepInEx/plugins/owner.pipeline/owner.pipeline.dll";
            entry.m_role = "plugin_binary";
            entry.m_mediaType =
                "application/vnd.microsoft.portable-executable";
            entry.m_outputDigest = FixtureFingerprint('8');
            entry.m_byteSize = 2048;
            entry.m_projectOwned = true;
            entry.m_redistributable = true;
            preview.m_layout.push_back(AZStd::move(entry));
            return preview;
        }

        static AdapterReleaseArtifactRequest BuildReleaseRequest(
            const AdapterVerifierEvidenceReconciliationResult& reconciliation,
            const AdapterPackageAssemblyPreview& packagePreview)
        {
            AdapterReleaseArtifactRequest request;
            request.m_artifactId = "owner.release-artifact";
            request.m_evaluatedAtUtc = "2026-07-19T15:00:00Z";
            request.m_reconciliationCanonicalJson =
                reconciliation.m_envelope.m_canonicalJson;
            request.m_packagePreviewCanonicalJson =
                packagePreview.m_canonicalJson;

            const AZStd::string evidenceId =
                reconciliation.m_envelope.m_inputCandidateEvidenceIds.empty()
                ? AZStd::string("owner.evidence.release")
                : reconciliation.m_envelope.m_inputCandidateEvidenceIds.front();
            for (size_t index = 0; index < packagePreview.m_layout.size(); ++index)
            {
                const AdapterPackageLayoutEntry& entry =
                    packagePreview.m_layout[index];
                const AZStd::string suffix = FixtureUnsigned(index + 1);
                AdapterReleaseArtifactContent content;
                content.m_contentId = "owner.release-content." + suffix;
                content.m_packageEntryId = entry.m_inventoryEntryId;
                content.m_packagePath = entry.m_packagePath;
                content.m_role = entry.m_role;
                content.m_mediaType = entry.m_mediaType;
                content.m_byteSize = entry.m_byteSize;
                content.m_checksumAlgorithm =
                    AdapterReleaseChecksumAlgorithm::Sha256;
                content.m_expectedChecksum = entry.m_outputDigest;
                content.m_provenanceIds = {
                    "owner.provenance." + suffix,
                };
                content.m_legalDispositionId =
                    "owner.legal-disposition." + suffix;
                request.m_contents.push_back(content);

                AdapterReleaseProvenanceRecord provenance;
                provenance.m_provenanceId = content.m_provenanceIds.front();
                provenance.m_contentId = content.m_contentId;
                provenance.m_subjectRef =
                    "release-content:" + content.m_contentId;
                provenance.m_sourceKind = "reviewed_package_layout";
                provenance.m_sourceId = "owner.release-source." + suffix;
                provenance.m_sourceFingerprint = entry.m_outputDigest;
                provenance.m_evidenceIds = { evidenceId };
                provenance.m_capturedAtUtc = "2026-07-19T14:40:00Z";
                provenance.m_limitations =
                    "Metadata declaration only; no file was opened or hashed.";
                request.m_provenance.push_back(AZStd::move(provenance));

                AdapterReleaseLegalDispositionRecord disposition;
                disposition.m_dispositionId = content.m_legalDispositionId;
                disposition.m_contentId = content.m_contentId;
                disposition.m_disposition =
                    AdapterReleaseLegalDisposition::Approved;
                disposition.m_reviewer = "legal-reviewer";
                disposition.m_evidenceIds = { evidenceId };
                disposition.m_reviewedAtUtc = "2026-07-19T14:45:00Z";
                disposition.m_rationale =
                    "The synthetic package entry is project-owned and redistributable.";
                request.m_legalDispositions.push_back(AZStd::move(disposition));
            }

            request.m_signingIntent.m_intentId = "owner.signing-intent";
            request.m_signingIntent.m_decision =
                AdapterReleaseSigningIntentDecision::Unsigned;
            request.m_signingIntent.m_identityKind =
                AdapterReleaseSigningIdentityKind::None;
            request.m_signingIntent.m_reviewer = "signing-reviewer";
            request.m_signingIntent.m_evidenceIds = { evidenceId };
            request.m_signingIntent.m_reviewedAtUtc =
                "2026-07-19T14:50:00Z";
            request.m_signingIntent.m_rationale =
                "The synthetic test artifact is intentionally unsigned.";

            AdapterReleasePublicationTarget target;
            target.m_targetId = "owner.publication-target";
            target.m_kind = AdapterReleasePublicationTargetKind::GitHubRelease;
            target.m_locator = "github.com/theb0yys/o3de/releases";
            target.m_channel = "test";
            target.m_reviewer = "publication-reviewer";
            target.m_evidenceIds = { evidenceId };
            target.m_reviewedAtUtc = "2026-07-19T14:55:00Z";
            target.m_rationale =
                "Reviewed metadata declaration only; no service is contacted.";
            request.m_publicationTargets.push_back(AZStd::move(target));
            return request;
        }

        AdapterResearchPipelineFixture()
        {
            m_workOrder = BuildWorkOrder();
            m_executionEnvelope = BuildExecutionEnvelope(m_workOrder);
            AdapterDeploymentExecutionEvidenceService executionService;
            m_executionEvidence = executionService.BuildEvidenceReturn(
                m_workOrder,
                m_executionEnvelope);

            AdapterPostDeploymentVerificationService reportService;
            m_report = reportService.BuildReport(
                m_workOrder,
                m_executionEnvelope,
                m_executionEvidence);

            const AZStd::string reviewEvidenceId =
                "owner.evidence.verifier-review";
            m_sourceRegistry = BuildVerifierReviewRegistry(
                m_executionEnvelope,
                "owner.verifier",
                reviewEvidenceId);
            m_verifierEnvelope = BuildVerifierEnvelope(
                m_workOrder,
                m_executionEnvelope,
                m_report,
                reviewEvidenceId);
            AdapterPostDeploymentVerifierEvidenceService verifierService;
            m_verifierEvidence = verifierService.BuildEvidenceReturn(
                m_workOrder,
                m_executionEnvelope,
                m_report,
                m_sourceRegistry,
                m_verifierEnvelope);

            m_reconciliationRequest = BuildReconciliationRequest(
                m_executionEnvelope,
                m_report,
                m_verifierEnvelope,
                m_verifierEvidence);
            AdapterVerifierEvidenceReconciliationService reconciliationService;
            m_reconciliation = reconciliationService.BuildReconciliation(
                m_workOrder,
                m_executionEnvelope,
                m_report,
                m_verifierEnvelope,
                m_verifierEvidence,
                m_reconciliationRequest);

            m_packagePreview = BuildPackagePreview(m_reconciliation);
            m_releaseRequest = BuildReleaseRequest(
                m_reconciliation,
                m_packagePreview);
            AdapterReleaseArtifactProvenanceService releaseService;
            m_releaseArtifact = releaseService.BuildEnvelope(
                m_reconciliation,
                m_packagePreview,
                m_releaseRequest);
        }
    };
} // namespace TaintedGrailModdingSDK::Test
