namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool VerifierOutcomeShapeIsValid(
            const AdapterPostDeploymentVerifierCheckResult& check)
        {
            const bool checkedAtValid =
                IsAdapterPostDeploymentVerifierUtcTimestamp(check.m_checkedAtUtc);
            const bool observedShapeValid = check.m_observationRecorded
                ? (check.m_observedPresent
                    ? IsAdapterPostDeploymentVerifierFingerprint(
                        check.m_observedFingerprint)
                    : check.m_observedFingerprint.empty())
                : !check.m_observedPresent
                    && check.m_observedFingerprint.empty();
            if (!observedShapeValid)
            {
                return false;
            }

            switch (check.m_outcome)
            {
            case AdapterPostDeploymentVerifierCheckOutcome::NotRun:
                return !check.m_attempted
                    && !check.m_observationRecorded
                    && check.m_checkedAtUtc.empty()
                    && check.m_failureIds.empty()
                    && check.m_diagnosticReferenceIds.empty();
            case AdapterPostDeploymentVerifierCheckOutcome::Matched:
            case AdapterPostDeploymentVerifierCheckOutcome::Mismatched:
                return check.m_attempted
                    && check.m_observationRecorded
                    && checkedAtValid
                    && check.m_failureIds.empty();
            case AdapterPostDeploymentVerifierCheckOutcome::Failed:
            case AdapterPostDeploymentVerifierCheckOutcome::Inconclusive:
                return check.m_attempted
                    && checkedAtValid
                    && !check.m_failureIds.empty();
            }
            return false;
        }

        void ValidateVerifierCheckCoverage(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            AZStd::vector<const AdapterDeploymentWorkOrderStep*> expectedSteps;
            for (const AdapterDeploymentWorkOrderStep& step : workOrder.m_steps)
            {
                if (IsVerifierMutationStep(step.m_kind))
                {
                    expectedSteps.push_back(&step);
                }
            }

            if (envelope.m_checkResults.size() != expectedSteps.size())
            {
                AddVerifierIssue(
                    result,
                    flags.m_checkCoverageIncomplete,
                    "post_deployment_verifier.check_count_mismatch",
                    "The verifier envelope must contain exactly one check for every "
                    "canonical add, replace, and remove work-order step.");
            }

            AZStd::vector<AZStd::string> checkIds;
            AZStd::vector<AZStd::string> checkedStepIds;
            for (const AdapterPostDeploymentVerifierCheckResult& check :
                 envelope.m_checkResults)
            {
                checkIds.push_back(check.m_checkId);
                checkedStepIds.push_back(check.m_stepId);
                const AdapterDeploymentWorkOrderStep* step =
                    FindVerifierWorkOrderStep(workOrder, check.m_stepId);
                if (!IsAdapterPostDeploymentVerifierStableId(check.m_checkId)
                    || !step
                    || !IsVerifierMutationStep(step->m_kind))
                {
                    AddVerifierIssue(
                        result,
                        flags.m_checkCoverageIncomplete,
                        "post_deployment_verifier.check_unknown",
                        "A verifier check must use stable identity and bind to one exact "
                        "canonical add, replace, or remove work-order step.",
                        check.m_checkId);
                    continue;
                }

                const bool expectedPresent = step->m_kind
                    != AdapterDeploymentWorkOrderStepKind::Remove;
                const AZStd::string expectedFingerprint = expectedPresent
                    ? step->m_desiredFingerprint
                    : AZStd::string{};
                if (check.m_sequence != step->m_sequence
                    || check.m_targetPath != step->m_targetPath
                    || check.m_expectedPresent != expectedPresent
                    || check.m_expectedFingerprint != expectedFingerprint
                    || (expectedPresent
                        && !IsAdapterPostDeploymentVerifierFingerprint(
                            check.m_expectedFingerprint)))
                {
                    AddVerifierIssue(
                        result,
                        flags.m_checkCoverageIncomplete,
                        "post_deployment_verifier.check_binding_mismatch",
                        "The verifier check sequence, step, target path, expected presence, "
                        "or expected fingerprint does not match the exact work-order state.",
                        check.m_checkId);
                }

                if (!VerifierOutcomeShapeIsValid(check)
                    || (!check.m_checkedAtUtc.empty()
                        && check.m_checkedAtUtc > envelope.m_capturedAtUtc))
                {
                    AddVerifierIssue(
                        result,
                        flags.m_envelopeInvalid,
                        "post_deployment_verifier.check_outcome_invalid",
                        "The attempted, observation, timestamp, failure, and diagnostic "
                        "shape does not match the typed verifier check outcome.",
                        check.m_checkId);
                    continue;
                }

                switch (check.m_outcome)
                {
                case AdapterPostDeploymentVerifierCheckOutcome::NotRun:
                    AddVerifierIssue(
                        result,
                        flags.m_observationMismatch,
                        "post_deployment_verifier.check_not_run",
                        "The required check is present and exactly bound but was not run. "
                        "This remains structurally valid adverse evidence and never becomes "
                        "an accepted all-matched verifier result.",
                        check.m_checkId);
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Matched:
                    if (check.m_observedPresent != check.m_expectedPresent
                        || check.m_observedFingerprint
                            != check.m_expectedFingerprint)
                    {
                        AddVerifierIssue(
                            result,
                            flags.m_envelopeInvalid,
                            "post_deployment_verifier.matched_observation_invalid",
                            "A matched check must report the exact expected target presence "
                            "and fingerprint.",
                            check.m_checkId);
                    }
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Mismatched:
                    if (check.m_observedPresent == check.m_expectedPresent
                        && check.m_observedFingerprint
                            == check.m_expectedFingerprint)
                    {
                        AddVerifierIssue(
                            result,
                            flags.m_envelopeInvalid,
                            "post_deployment_verifier.mismatch_not_observed",
                            "A mismatched check must report an observation that differs from "
                            "the exact expected target state.",
                            check.m_checkId);
                    }
                    else
                    {
                        AddVerifierIssue(
                            result,
                            flags.m_observationMismatch,
                            "post_deployment_verifier.observation_mismatch",
                            "The independently supplied observation differs from the exact "
                            "expected deployed target state.",
                            check.m_checkId);
                    }
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Failed:
                    AddVerifierIssue(
                        result,
                        flags.m_observationMismatch,
                        "post_deployment_verifier.check_failed",
                        "The independently supplied verifier check reported failed.",
                        check.m_checkId);
                    break;
                case AdapterPostDeploymentVerifierCheckOutcome::Inconclusive:
                    AddVerifierIssue(
                        result,
                        flags.m_observationMismatch,
                        "post_deployment_verifier.check_inconclusive",
                        "The independently supplied verifier check was inconclusive.",
                        check.m_checkId);
                    break;
                }
            }

            AZStd::sort(checkIds.begin(), checkIds.end());
            if (AZStd::adjacent_find(checkIds.begin(), checkIds.end())
                != checkIds.end())
            {
                AddVerifierIssue(
                    result,
                    flags.m_checkCoverageIncomplete,
                    "post_deployment_verifier.duplicate_check_id",
                    "Verifier check identities must be unique.");
            }

            AZStd::sort(checkedStepIds.begin(), checkedStepIds.end());
            if (AZStd::adjacent_find(
                    checkedStepIds.begin(),
                    checkedStepIds.end())
                != checkedStepIds.end())
            {
                AddVerifierIssue(
                    result,
                    flags.m_checkCoverageIncomplete,
                    "post_deployment_verifier.duplicate_step_check",
                    "Every mutation step must have exactly one independent verifier check.");
            }

            for (const AdapterDeploymentWorkOrderStep* step : expectedSteps)
            {
                if (AZStd::find(
                        checkedStepIds.begin(),
                        checkedStepIds.end(),
                        step->m_stepId)
                    == checkedStepIds.end())
                {
                    AddVerifierIssue(
                        result,
                        flags.m_checkCoverageIncomplete,
                        "post_deployment_verifier.missing_step_check",
                        "A canonical mutation step has no independent verifier check.",
                        {},
                        {},
                        {},
                        step->m_stepId);
                }
            }
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
