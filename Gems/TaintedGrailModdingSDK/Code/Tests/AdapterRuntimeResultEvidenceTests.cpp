/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterRuntimeResultCanonical.h"
#include "AdapterRuntimeResultEvidenceService.h"
#include "CanonicalFingerprint.h"

#include <AzTest/AzTest.h>

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string Fingerprint(char value)
        {
            return "sha256:" + AZStd::string(64, value);
        }

        AdapterWorkOrderStep MakePlanStep(
            AZ::u64 sequence,
            const AZStd::string& capability,
            const AZStd::string& subjectKind,
            const AZStd::string& subjectId,
            const AZStd::string& subjectRef)
        {
            AdapterWorkOrderStep step;
            step.m_stepId =
                "workorder.plan:preview:step:" + capability + ":" + subjectId;
            step.m_sequence = sequence;
            step.m_capability = capability;
            step.m_subjectKind = subjectKind;
            step.m_subjectId = subjectId;
            step.m_subjectRef = subjectRef;
            step.m_executionAllowed = false;
            return step;
        }

        AdapterWorkOrderPlan MakePlan()
        {
            AdapterWorkOrderPlan plan;
            plan.m_planId =
                "workorder.plan:preview.pack@1.0.0:preview.adapter@1.0.0:preview.profile";
            plan.m_packId = "preview.pack";
            plan.m_packVersion = "1.0.0";
            plan.m_adapterId = "preview.adapter";
            plan.m_adapterVersion = "1.0.0";
            plan.m_requiredAdapterVersion = "1.0.0";
            plan.m_profileId = "preview.profile";
            plan.m_gameVersion = "preview-build-0";
            plan.m_branch = "preview";
            plan.m_runtimeTarget = "Mono";
            plan.m_steps = {
                MakePlanStep(
                    1,
                    "item_grant",
                    "record",
                    "preview.item.one",
                    "subject:preview:item:one"),
                MakePlanStep(
                    2,
                    "cleanup",
                    "pack",
                    "preview.pack",
                    "pack:preview.pack"),
                MakePlanStep(
                    3,
                    "rollback",
                    "pack",
                    "preview.pack",
                    "pack:preview.pack"),
            };
            plan.m_canonicalJson =
                "{\"PlanId\":\"workorder.plan:preview.pack@1.0.0:preview.adapter@1.0.0:preview.profile\","
                "\"ExecutionAllowed\":false}";
            return plan;
        }

        AdapterRuntimeStepResult MakeSucceededResult(
            const AdapterWorkOrderStep& planStep,
            char fingerprintValue)
        {
            AdapterRuntimeStepResult result;
            result.m_stepId = planStep.m_stepId;
            result.m_sequence = planStep.m_sequence;
            result.m_capability = planStep.m_capability;
            result.m_subjectKind = planStep.m_subjectKind;
            result.m_subjectId = planStep.m_subjectId;
            result.m_subjectRef = planStep.m_subjectRef;
            result.m_outcome = AdapterRuntimeOutcome::Succeeded;
            result.m_outputFingerprint = Fingerprint(fingerprintValue);
            result.m_attempted = true;
            return result;
        }

        void RefreshFingerprint(AdapterRuntimeResultEnvelope& envelope)
        {
            envelope.m_resultFingerprint =
                CalculateRuntimeResultFingerprint(envelope);
        }

        AdapterRuntimeResultEnvelope MakeEnvelope(
            const AdapterWorkOrderPlan& plan)
        {
            AdapterRuntimeResultEnvelope envelope;
            envelope.m_resultId = "preview.runtime.result";
            envelope.m_planId = plan.m_planId;
            envelope.m_planCanonicalJson = plan.m_canonicalJson;
            envelope.m_planFingerprint =
                CalculateCanonicalSha256(plan.m_canonicalJson);
            envelope.m_packId = plan.m_packId;
            envelope.m_packVersion = plan.m_packVersion;
            envelope.m_adapterId = plan.m_adapterId;
            envelope.m_adapterVersion = plan.m_adapterVersion;
            envelope.m_profileId = plan.m_profileId;
            envelope.m_gameVersion = plan.m_gameVersion;
            envelope.m_branch = plan.m_branch;
            envelope.m_runtimeTarget = plan.m_runtimeTarget;
            envelope.m_capturedAt = "2026-07-19T00:00:00Z";
            envelope.m_stepResults = {
                MakeSucceededResult(plan.m_steps[0], 'c'),
                MakeSucceededResult(plan.m_steps[1], 'd'),
                MakeSucceededResult(plan.m_steps[2], 'e'),
            };

            AdapterRuntimeLogReference adapterLog;
            adapterLog.m_logId = "preview.log.adapter";
            adapterLog.m_kind = AdapterRuntimeLogKind::Adapter;
            adapterLog.m_reference = "logs/adapter.log";
            adapterLog.m_fingerprint = Fingerprint('f');
            adapterLog.m_stepIds = { plan.m_steps[0].m_stepId };
            envelope.m_logReferences.push_back(adapterLog);
            envelope.m_stepResults[0].m_logReferenceIds = {
                adapterLog.m_logId,
            };

            envelope.m_cleanupResult.m_stepId = plan.m_steps[1].m_stepId;
            envelope.m_cleanupResult.m_outcome =
                AdapterRuntimeOutcome::Succeeded;
            envelope.m_cleanupResult.m_outputFingerprint =
                envelope.m_stepResults[1].m_outputFingerprint;

            envelope.m_rollbackResult.m_stepId = plan.m_steps[2].m_stepId;
            envelope.m_rollbackResult.m_outcome =
                AdapterRuntimeOutcome::Succeeded;
            envelope.m_rollbackResult.m_outputFingerprint =
                envelope.m_stepResults[2].m_outputFingerprint;
            RefreshFingerprint(envelope);
            return envelope;
        }

        bool HasIssue(
            const AdapterRuntimeEvidenceReturn& result,
            const AZStd::string& code)
        {
            for (const AdapterRuntimeResultIssue& issue : result.m_issues)
            {
                if (issue.m_code == code)
                {
                    return true;
                }
            }
            return false;
        }

        bool HasEvidenceKind(
            const AdapterRuntimeEvidenceReturn& result,
            const AZStd::string& evidenceKind)
        {
            for (const EvidenceDocument& document : result.m_evidenceDocuments)
            {
                for (const EvidenceRecord& evidence : document.m_evidence)
                {
                    if (evidence.m_evidenceKind == evidenceKind)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    } // namespace

    TEST(
        AdapterRuntimeResultEvidenceTests,
        TypedOutcomeFailureAndLogVocabulariesAreStrict)
    {
        AdapterRuntimeOutcome outcome;
        AdapterRuntimeFailureKind failureKind;
        AdapterRuntimeLogKind logKind;
        EXPECT_TRUE(TryParseAdapterRuntimeOutcome("not_attempted", outcome));
        EXPECT_TRUE(TryParseAdapterRuntimeOutcome("succeeded", outcome));
        EXPECT_TRUE(TryParseAdapterRuntimeOutcome("failed", outcome));
        EXPECT_TRUE(TryParseAdapterRuntimeOutcome("skipped", outcome));
        EXPECT_FALSE(TryParseAdapterRuntimeOutcome("success", outcome));
        EXPECT_TRUE(TryParseAdapterRuntimeFailureKind("cleanup", failureKind));
        EXPECT_TRUE(TryParseAdapterRuntimeFailureKind("rollback", failureKind));
        EXPECT_FALSE(TryParseAdapterRuntimeFailureKind("unknown", failureKind));
        EXPECT_FALSE(TryParseAdapterRuntimeFailureKind("crashed", failureKind));
        EXPECT_TRUE(TryParseAdapterRuntimeLogKind("adapter", logKind));
        EXPECT_TRUE(TryParseAdapterRuntimeLogKind("diagnostic", logKind));
        EXPECT_FALSE(TryParseAdapterRuntimeLogKind("stdout", logKind));
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        RegistryRejectsCallerSelectedAndDuplicateFingerprints)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEnvelope envelope = MakeEnvelope(plan);
        AdapterRuntimeResultRegistry registry;
        AZStd::string error;

        envelope.m_resultFingerprint = Fingerprint('9');
        EXPECT_FALSE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_NE(error.find("deterministic canonical"), AZStd::string::npos);

        envelope = MakeEnvelope(plan);
        ASSERT_TRUE(registry.RegisterEnvelope(envelope, &error)) << error.c_str();
        EXPECT_FALSE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_NE(error.find("already registered"), AZStd::string::npos);
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        PlanFingerprintMustHashExactCanonicalJson)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEnvelope envelope = MakeEnvelope(plan);
        envelope.m_planFingerprint = Fingerprint('a');
        RefreshFingerprint(envelope);

        AdapterRuntimeResultRegistry registry;
        AZStd::string error;
        EXPECT_FALSE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_NE(error.find("canonical plan JSON"), AZStd::string::npos);
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        ExactAttemptedPlanProducesCandidateEvidenceOnly)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        const AdapterRuntimeResultEnvelope envelope = MakeEnvelope(plan);
        AdapterRuntimeResultEvidenceService service;
        const AdapterRuntimeEvidenceReturn result =
            service.BuildEvidenceReturn(plan, envelope);

        ASSERT_TRUE(result.m_accepted);
        EXPECT_TRUE(result.m_issues.empty());
        EXPECT_EQ(result.m_stepResultCount, 3);
        EXPECT_EQ(result.m_failureCount, 0);
        EXPECT_EQ(result.m_logReferenceCount, 1);
        EXPECT_EQ(result.m_sourceDocumentCount, 2);
        EXPECT_GT(result.m_evidenceRecordCount, 3);
        EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_step_result"));
        EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_cleanup_result"));
        EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_rollback_result"));
        EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_plan_binding"));
        EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_log_reference"));
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        FailedStepAndTypedFailureReturnAsNewEvidence)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEnvelope envelope = MakeEnvelope(plan);
        AdapterRuntimeFailure failure;
        failure.m_failureId = "preview.failure.item-grant";
        failure.m_kind = AdapterRuntimeFailureKind::Capability;
        failure.m_code = "item_grant_failed";
        failure.m_message = "The adapter reported a synthetic test failure.";
        failure.m_stepId = plan.m_steps[0].m_stepId;
        failure.m_logReferenceIds = { "preview.log.adapter" };
        envelope.m_failures.push_back(failure);
        envelope.m_stepResults[0].m_outcome = AdapterRuntimeOutcome::Failed;
        envelope.m_stepResults[0].m_failureIds = { failure.m_failureId };
        envelope.m_stepResults[0].m_outputFingerprint.clear();
        RefreshFingerprint(envelope);

        AdapterRuntimeResultEvidenceService service;
        const AdapterRuntimeEvidenceReturn result =
            service.BuildEvidenceReturn(plan, envelope);
        ASSERT_TRUE(result.m_accepted);
        EXPECT_EQ(result.m_failureCount, 1);
        EXPECT_TRUE(HasEvidenceKind(result, "adapter_runtime_failure"));
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        UnknownOrMissingStepIdentityFailsClosed)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEvidenceService service;

        AdapterRuntimeResultEnvelope unknown = MakeEnvelope(plan);
        unknown.m_stepResults[0].m_stepId =
            "workorder.plan:preview:step:item_grant:unknown";
        unknown.m_logReferences[0].m_stepIds = {
            unknown.m_stepResults[0].m_stepId,
        };
        RefreshFingerprint(unknown);
        const AdapterRuntimeEvidenceReturn unknownResult =
            service.BuildEvidenceReturn(plan, unknown);
        EXPECT_FALSE(unknownResult.m_accepted);
        EXPECT_TRUE(HasIssue(unknownResult, "runtime_result.step_missing"));
        EXPECT_TRUE(HasIssue(unknownResult, "runtime_result.step_unknown"));

        AdapterRuntimeResultEnvelope missing = MakeEnvelope(plan);
        missing.m_stepResults.pop_back();
        missing.m_rollbackResult.m_stepId = missing.m_stepResults.back().m_stepId;
        RefreshFingerprint(missing);
        const AdapterRuntimeEvidenceReturn missingResult =
            service.BuildEvidenceReturn(plan, missing);
        EXPECT_FALSE(missingResult.m_accepted);
        EXPECT_TRUE(
            HasIssue(missingResult, "runtime_result.step_count_mismatch"));
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        OutcomeAndFailureShapeIsValidatedBeforeEvidenceReturn)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEnvelope envelope = MakeEnvelope(plan);
        envelope.m_stepResults[0].m_outcome = AdapterRuntimeOutcome::Failed;
        envelope.m_stepResults[0].m_failureIds.clear();
        envelope.m_stepResults[0].m_outputFingerprint.clear();
        RefreshFingerprint(envelope);

        AdapterRuntimeResultRegistry registry;
        AZStd::string error;
        EXPECT_FALSE(registry.RegisterEnvelope(envelope, &error));
        EXPECT_NE(error.find("require failures"), AZStd::string::npos);
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        CleanupAndRollbackSummariesMustMatchTheirStepResults)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEnvelope envelope = MakeEnvelope(plan);
        envelope.m_cleanupResult.m_outputFingerprint = Fingerprint('9');
        RefreshFingerprint(envelope);

        AdapterRuntimeResultEvidenceService service;
        const AdapterRuntimeEvidenceReturn result =
            service.BuildEvidenceReturn(plan, envelope);
        EXPECT_FALSE(result.m_accepted);
        EXPECT_TRUE(HasIssue(result, "runtime_result.cleanup_mismatch"));
        EXPECT_TRUE(result.m_sourceDocuments.empty());
        EXPECT_TRUE(result.m_evidenceDocuments.empty());
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        ImpossibleUtcDateAndUnknownFailureKindAreRejected)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultRegistry registry;
        AZStd::string error;

        AdapterRuntimeResultEnvelope impossible = MakeEnvelope(plan);
        impossible.m_capturedAt = "2026-02-31T12:00:00Z";
        RefreshFingerprint(impossible);
        EXPECT_FALSE(registry.RegisterEnvelope(impossible, &error));
        EXPECT_NE(error.find("UTC capture time"), AZStd::string::npos);

        AdapterRuntimeResultEnvelope unknown = MakeEnvelope(plan);
        AdapterRuntimeFailure failure;
        failure.m_failureId = "preview.failure.unknown";
        failure.m_kind = AdapterRuntimeFailureKind::Unknown;
        failure.m_code = "unknown_failure";
        failure.m_message = "Unknown classifications are not accepted.";
        failure.m_stepId = plan.m_steps[0].m_stepId;
        unknown.m_failures.push_back(failure);
        unknown.m_stepResults[0].m_outcome = AdapterRuntimeOutcome::Failed;
        unknown.m_stepResults[0].m_failureIds = { failure.m_failureId };
        unknown.m_stepResults[0].m_outputFingerprint.clear();
        RefreshFingerprint(unknown);
        EXPECT_FALSE(registry.RegisterEnvelope(unknown, &error));
        EXPECT_NE(error.find("known kind"), AZStd::string::npos);
    }

    TEST(
        AdapterRuntimeResultEvidenceTests,
        CanonicalPayloadIsOrderIndependentButContentSensitive)
    {
        const AdapterWorkOrderPlan plan = MakePlan();
        AdapterRuntimeResultEnvelope first = MakeEnvelope(plan);
        AdapterRuntimeResultEnvelope reordered = first;
        AZStd::reverse(
            reordered.m_stepResults.begin(),
            reordered.m_stepResults.end());
        EXPECT_EQ(
            SerializeCanonicalRuntimeResult(first),
            SerializeCanonicalRuntimeResult(reordered));

        reordered.m_adapterVersion = "1.0.1";
        EXPECT_NE(
            SerializeCanonicalRuntimeResult(first),
            SerializeCanonicalRuntimeResult(reordered));
    }
} // namespace TaintedGrailModdingSDK
