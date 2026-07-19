namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool VerifierDiagnosticContainsCheck(
            const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic,
            const AZStd::string& checkId)
        {
            return AZStd::find(
                       diagnostic.m_checkIds.begin(),
                       diagnostic.m_checkIds.end(),
                       checkId)
                != diagnostic.m_checkIds.end();
        }

        bool IsKnownVerifierFailureKind(
            AdapterPostDeploymentVerifierFailureKind kind)
        {
            switch (kind)
            {
            case AdapterPostDeploymentVerifierFailureKind::Contract:
            case AdapterPostDeploymentVerifierFailureKind::InputBinding:
            case AdapterPostDeploymentVerifierFailureKind::TargetUnavailable:
            case AdapterPostDeploymentVerifierFailureKind::AccessDenied:
            case AdapterPostDeploymentVerifierFailureKind::UnsupportedCheck:
            case AdapterPostDeploymentVerifierFailureKind::Read:
            case AdapterPostDeploymentVerifierFailureKind::Hash:
            case AdapterPostDeploymentVerifierFailureKind::Internal:
                return true;
            case AdapterPostDeploymentVerifierFailureKind::Unknown:
                return false;
            }
            return false;
        }

        bool IsKnownVerifierDiagnosticKind(
            AdapterPostDeploymentVerifierDiagnosticKind kind)
        {
            switch (kind)
            {
            case AdapterPostDeploymentVerifierDiagnosticKind::Verifier:
            case AdapterPostDeploymentVerifierDiagnosticKind::Filesystem:
            case AdapterPostDeploymentVerifierDiagnosticKind::Hash:
            case AdapterPostDeploymentVerifierDiagnosticKind::Binding:
            case AdapterPostDeploymentVerifierDiagnosticKind::Diagnostic:
                return true;
            }
            return false;
        }

        void ValidateVerifierFailureDiagnosticBindings(
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            AZStd::vector<AZStd::string> failureIds;
            AZStd::vector<AZStd::string> diagnosticIds;
            AZStd::vector<AZStd::string> referencedFailureIds;
            AZStd::vector<AZStd::string> referencedDiagnosticIds;

            for (const AdapterPostDeploymentVerifierFailure& failure :
                 envelope.m_failures)
            {
                failureIds.push_back(failure.m_failureId);
                const AdapterPostDeploymentVerifierCheckResult* check =
                    FindVerifierCheck(envelope, failure.m_checkId);
                AZStd::vector<AZStd::string> failureDiagnosticIds =
                    failure.m_diagnosticReferenceIds;
                AZStd::sort(
                    failureDiagnosticIds.begin(),
                    failureDiagnosticIds.end());
                const bool duplicateDiagnosticReferences = AZStd::adjacent_find(
                        failureDiagnosticIds.begin(),
                        failureDiagnosticIds.end())
                    != failureDiagnosticIds.end();
                if (!IsAdapterPostDeploymentVerifierStableId(failure.m_failureId)
                    || !IsKnownVerifierFailureKind(failure.m_kind)
                    || !check
                    || failure.m_code.empty()
                    || failure.m_message.empty()
                    || failure.m_diagnosticReferenceIds.empty()
                    || duplicateDiagnosticReferences)
                {
                    AddVerifierIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "post_deployment_verifier.failure_invalid",
                        "A verifier failure requires stable identity, one recognised non-"
                        "unknown failure kind, one known check, non-empty code and message, "
                        "and at least one diagnostic reference.",
                        failure.m_checkId,
                        failure.m_failureId);
                }
                for (const AZStd::string& diagnosticId :
                     failure.m_diagnosticReferenceIds)
                {
                    referencedDiagnosticIds.push_back(diagnosticId);
                    const AdapterPostDeploymentVerifierDiagnosticReference* diagnostic =
                        FindVerifierDiagnostic(envelope, diagnosticId);
                    if (!diagnostic
                        || !VerifierDiagnosticContainsCheck(
                            *diagnostic,
                            failure.m_checkId))
                    {
                        AddVerifierIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "post_deployment_verifier.failure_diagnostic_mismatch",
                            "A failure diagnostic must exist and bind to the same exact "
                            "verifier check.",
                            failure.m_checkId,
                            failure.m_failureId,
                            diagnosticId);
                    }
                }
            }

            for (const AdapterPostDeploymentVerifierDiagnosticReference& diagnostic :
                 envelope.m_diagnosticReferences)
            {
                diagnosticIds.push_back(diagnostic.m_diagnosticId);
                AZStd::vector<AZStd::string> checkIds = diagnostic.m_checkIds;
                AZStd::sort(checkIds.begin(), checkIds.end());
                const bool duplicateChecks = AZStd::adjacent_find(
                        checkIds.begin(),
                        checkIds.end())
                    != checkIds.end();
                bool invalidCheck = checkIds.empty();
                for (const AZStd::string& checkId : checkIds)
                {
                    invalidCheck = invalidCheck
                        || FindVerifierCheck(envelope, checkId) == nullptr;
                }
                if (!IsAdapterPostDeploymentVerifierStableId(
                        diagnostic.m_diagnosticId)
                    || !IsKnownVerifierDiagnosticKind(diagnostic.m_kind)
                    || !IsAdapterPostDeploymentVerifierDiagnosticReference(
                        diagnostic.m_reference)
                    || !IsAdapterPostDeploymentVerifierFingerprint(
                        diagnostic.m_fingerprint)
                    || duplicateChecks
                    || invalidCheck)
                {
                    AddVerifierIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "post_deployment_verifier.diagnostic_invalid",
                        "A verifier diagnostic requires stable identity, safe relative "
                        "reference, lowercase SHA-256 fingerprint, and unique known checks.",
                        {},
                        {},
                        diagnostic.m_diagnosticId);
                }
            }

            for (const AdapterPostDeploymentVerifierCheckResult& check :
                 envelope.m_checkResults)
            {
                AZStd::vector<AZStd::string> checkFailureIds = check.m_failureIds;
                AZStd::vector<AZStd::string> checkDiagnosticIds =
                    check.m_diagnosticReferenceIds;
                AZStd::sort(checkFailureIds.begin(), checkFailureIds.end());
                AZStd::sort(checkDiagnosticIds.begin(), checkDiagnosticIds.end());
                if (AZStd::adjacent_find(
                        checkFailureIds.begin(),
                        checkFailureIds.end())
                        != checkFailureIds.end()
                    || AZStd::adjacent_find(
                           checkDiagnosticIds.begin(),
                           checkDiagnosticIds.end())
                        != checkDiagnosticIds.end())
                {
                    AddVerifierIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "post_deployment_verifier.duplicate_check_reference",
                        "Failure and diagnostic references on one verifier check must be "
                        "unique.",
                        check.m_checkId);
                }
                for (const AZStd::string& failureId : check.m_failureIds)
                {
                    referencedFailureIds.push_back(failureId);
                    const AdapterPostDeploymentVerifierFailure* failure =
                        FindVerifierFailure(envelope, failureId);
                    if (!failure || failure->m_checkId != check.m_checkId)
                    {
                        AddVerifierIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "post_deployment_verifier.check_failure_mismatch",
                            "A check failure reference must exist and bind back to the same "
                            "exact verifier check.",
                            check.m_checkId,
                            failureId);
                    }
                }
                for (const AZStd::string& diagnosticId :
                     check.m_diagnosticReferenceIds)
                {
                    referencedDiagnosticIds.push_back(diagnosticId);
                    const AdapterPostDeploymentVerifierDiagnosticReference* diagnostic =
                        FindVerifierDiagnostic(envelope, diagnosticId);
                    if (!diagnostic
                        || !VerifierDiagnosticContainsCheck(
                            *diagnostic,
                            check.m_checkId))
                    {
                        AddVerifierIssue(
                            result,
                            flags.m_failureDiagnosticBindingMismatch,
                            "post_deployment_verifier.check_diagnostic_mismatch",
                            "A check diagnostic reference must exist and bind back to the "
                            "same exact verifier check.",
                            check.m_checkId,
                            {},
                            diagnosticId);
                    }
                }
            }

            AZStd::sort(failureIds.begin(), failureIds.end());
            AZStd::sort(diagnosticIds.begin(), diagnosticIds.end());
            AZStd::sort(referencedFailureIds.begin(), referencedFailureIds.end());
            AZStd::sort(referencedDiagnosticIds.begin(), referencedDiagnosticIds.end());
            if (AZStd::adjacent_find(failureIds.begin(), failureIds.end())
                != failureIds.end())
            {
                AddVerifierIssue(
                    result,
                    flags.m_failureDiagnosticBindingMismatch,
                    "post_deployment_verifier.duplicate_failure_id",
                    "Verifier failure identities must be unique.");
            }
            if (AZStd::adjacent_find(diagnosticIds.begin(), diagnosticIds.end())
                != diagnosticIds.end())
            {
                AddVerifierIssue(
                    result,
                    flags.m_failureDiagnosticBindingMismatch,
                    "post_deployment_verifier.duplicate_diagnostic_id",
                    "Verifier diagnostic identities must be unique.");
            }

            SortUniqueVerifierValues(referencedFailureIds);
            SortUniqueVerifierValues(referencedDiagnosticIds);
            for (const AZStd::string& failureId : failureIds)
            {
                if (AZStd::find(
                        referencedFailureIds.begin(),
                        referencedFailureIds.end(),
                        failureId)
                    == referencedFailureIds.end())
                {
                    AddVerifierIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "post_deployment_verifier.orphan_failure",
                        "Every verifier failure must be referenced by its exact check.",
                        {},
                        failureId);
                }
            }
            for (const AZStd::string& diagnosticId : diagnosticIds)
            {
                if (AZStd::find(
                        referencedDiagnosticIds.begin(),
                        referencedDiagnosticIds.end(),
                        diagnosticId)
                    == referencedDiagnosticIds.end())
                {
                    AddVerifierIssue(
                        result,
                        flags.m_failureDiagnosticBindingMismatch,
                        "post_deployment_verifier.orphan_diagnostic",
                        "Every verifier diagnostic must be referenced by a check or "
                        "same-check failure.",
                        {},
                        {},
                        diagnosticId);
                }
            }
        }

        AdapterPostDeploymentVerifierEnvelopeStatus ResolveVerifierStatus(
            const VerifierValidationFlags& flags)
        {
            if (flags.m_reportNotReady)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::ReportNotReady;
            }
            if (flags.m_verifierUnreviewed)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::VerifierUnreviewed;
            }
            if (flags.m_reportBindingMismatch)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::
                    ReportBindingMismatch;
            }
            if (flags.m_envelopeInvalid)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::EnvelopeInvalid;
            }
            if (flags.m_checkCoverageIncomplete)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::
                    CheckCoverageIncomplete;
            }
            if (flags.m_failureDiagnosticBindingMismatch)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::
                    FailureDiagnosticBindingMismatch;
            }
            if (flags.m_observationMismatch)
            {
                return AdapterPostDeploymentVerifierEnvelopeStatus::ObservationMismatch;
            }
            return AdapterPostDeploymentVerifierEnvelopeStatus::Accepted;
        }

        bool VerifierContractIsValid(const VerifierValidationFlags& flags)
        {
            return !flags.m_reportNotReady
                && !flags.m_verifierUnreviewed
                && !flags.m_reportBindingMismatch
                && !flags.m_envelopeInvalid
                && !flags.m_checkCoverageIncomplete
                && !flags.m_failureDiagnosticBindingMismatch;
        }

        void SortVerifierIssues(
            AdapterPostDeploymentVerifierEvidenceReturn& result)
        {
            AZStd::sort(
                result.m_issues.begin(),
                result.m_issues.end(),
                [](const AdapterPostDeploymentVerifierIssue& left,
                    const AdapterPostDeploymentVerifierIssue& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_stepId != right.m_stepId)
                    {
                        return left.m_stepId < right.m_stepId;
                    }
                    if (left.m_checkId != right.m_checkId)
                    {
                        return left.m_checkId < right.m_checkId;
                    }
                    if (left.m_failureId != right.m_failureId)
                    {
                        return left.m_failureId < right.m_failureId;
                    }
                    if (left.m_diagnosticId != right.m_diagnosticId)
                    {
                        return left.m_diagnosticId < right.m_diagnosticId;
                    }
                    return left.m_message < right.m_message;
                });
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
