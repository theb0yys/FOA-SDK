namespace TaintedGrailModdingSDK
{
    namespace
    {
        struct VerifierValidationFlags
        {
            bool m_reportNotReady = false;
            bool m_verifierUnreviewed = false;
            bool m_reportBindingMismatch = false;
            bool m_envelopeInvalid = false;
            bool m_checkCoverageIncomplete = false;
            bool m_failureDiagnosticBindingMismatch = false;
            bool m_observationMismatch = false;
        };

        void AddVerifierIssue(
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            bool& flag,
            AZStd::string code,
            AZStd::string message,
            AZStd::string checkId = {},
            AZStd::string failureId = {},
            AZStd::string diagnosticId = {},
            AZStd::string stepId = {})
        {
            flag = true;
            AdapterPostDeploymentVerifierIssue issue;
            issue.m_code = AZStd::move(code);
            issue.m_message = AZStd::move(message);
            issue.m_stepId = AZStd::move(stepId);
            issue.m_checkId = AZStd::move(checkId);
            issue.m_failureId = AZStd::move(failureId);
            issue.m_diagnosticId = AZStd::move(diagnosticId);
            result.m_issues.push_back(AZStd::move(issue));
        }

        AZStd::string UnsignedVerifierString(AZ::u64 value)
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

        void SortUniqueVerifierValues(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        void AppendJsonEscaped(
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
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
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

        void AppendJsonName(AZStd::string& output, const char* name)
        {
            AppendJsonEscaped(output, name);
            output.push_back(':');
        }

        void AppendJsonStringMember(
            AZStd::string& output,
            const char* name,
            const AZStd::string& value,
            bool comma = true)
        {
            AppendJsonName(output, name);
            AppendJsonEscaped(output, value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendJsonUnsignedMember(
            AZStd::string& output,
            const char* name,
            AZ::u64 value,
            bool comma = true)
        {
            AppendJsonName(output, name);
            output += UnsignedVerifierString(value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendJsonBoolMember(
            AZStd::string& output,
            const char* name,
            bool value,
            bool comma = true)
        {
            AppendJsonName(output, name);
            output += value ? "true" : "false";
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendJsonStringArray(
            AZStd::string& output,
            const char* name,
            AZStd::vector<AZStd::string> values,
            bool comma = true)
        {
            SortUniqueVerifierValues(values);
            AppendJsonName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                AppendJsonEscaped(output, values[index]);
            }
            output.push_back(']');
            if (comma)
            {
                output.push_back(',');
            }
        }

        AZStd::string SerializeVerifierInputReport(
            const AdapterPostDeploymentVerificationReport& report)
        {
            AZStd::string output;
            output.push_back('{');
            AppendJsonUnsignedMember(output, "ContractVersion", report.m_contractVersion);
            AppendJsonStringMember(output, "ReportId", report.m_reportId);
            AppendJsonStringMember(output, "ResultId", report.m_resultId);
            AppendJsonStringMember(output, "WorkOrderId", report.m_workOrderId);
            AppendJsonStringMember(
                output,
                "WorkOrderFingerprint",
                report.m_workOrderFingerprint);
            AppendJsonStringMember(
                output,
                "ResultFingerprint",
                report.m_resultFingerprint);
            AppendJsonStringMember(output, "ProfileId", report.m_profileId);
            AppendJsonStringMember(output, "GameVersion", report.m_gameVersion);
            AppendJsonStringMember(output, "Branch", report.m_branch);
            AppendJsonStringMember(output, "RuntimeTarget", report.m_runtimeTarget);
            AppendJsonStringMember(output, "Status", ToString(report.m_status));
            AppendJsonUnsignedMember(
                output,
                "CandidateSourceDocumentCount",
                report.m_candidateSourceDocumentCount);
            AppendJsonUnsignedMember(
                output,
                "CandidateEvidenceRecordCount",
                report.m_candidateEvidenceRecordCount);
            AppendJsonUnsignedMember(output, "StepCount", report.m_stepCount);
            AppendJsonUnsignedMember(
                output,
                "CompletedStepCount",
                report.m_completedStepCount);
            AppendJsonUnsignedMember(
                output,
                "FailedStepCount",
                report.m_failedStepCount);
            AppendJsonUnsignedMember(
                output,
                "IncompleteStepCount",
                report.m_incompleteStepCount);
            AppendJsonUnsignedMember(
                output,
                "BackupResultCount",
                report.m_backupResultCount);
            AppendJsonUnsignedMember(
                output,
                "IncompleteBackupCount",
                report.m_incompleteBackupCount);
            AppendJsonUnsignedMember(
                output,
                "MatchedVerificationCount",
                report.m_matchedVerificationCount);
            AppendJsonUnsignedMember(
                output,
                "MismatchedVerificationCount",
                report.m_mismatchedVerificationCount);
            AppendJsonUnsignedMember(
                output,
                "UncheckedVerificationCount",
                report.m_uncheckedVerificationCount);
            AppendJsonUnsignedMember(
                output,
                "RollbackRequiredCount",
                report.m_rollbackRequiredCount);
            AppendJsonUnsignedMember(
                output,
                "RollbackSucceededCount",
                report.m_rollbackSucceededCount);
            AppendJsonUnsignedMember(
                output,
                "RollbackIncompleteCount",
                report.m_rollbackIncompleteCount);
            AppendJsonUnsignedMember(output, "FailureCount", report.m_failureCount);
            AppendJsonUnsignedMember(
                output,
                "DiagnosticReferenceCount",
                report.m_diagnosticReferenceCount);
            AppendJsonUnsignedMember(
                output,
                "CompatibilityBlockerCount",
                report.m_compatibilityBlockerCount);
            AppendJsonUnsignedMember(
                output,
                "ReleaseBlockerCount",
                report.m_releaseBlockerCount);
            AppendJsonStringArray(
                output,
                "CandidateSourceIds",
                report.m_candidateSourceIds);
            AppendJsonStringArray(
                output,
                "CandidateEvidenceIds",
                report.m_candidateEvidenceIds);
            AppendJsonStringArray(
                output,
                "DiagnosticReferenceIds",
                report.m_diagnosticReferenceIds);

            AZStd::vector<const AdapterPostDeploymentBlocker*> blockers;
            for (const AdapterPostDeploymentBlocker& blocker : report.m_blockers)
            {
                blockers.push_back(&blocker);
            }
            AZStd::sort(
                blockers.begin(),
                blockers.end(),
                [](const AdapterPostDeploymentBlocker* left,
                    const AdapterPostDeploymentBlocker* right)
                {
                    return left->m_blockerId < right->m_blockerId;
                });

            AppendJsonName(output, "Blockers");
            output.push_back('[');
            for (size_t index = 0; index < blockers.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterPostDeploymentBlocker& blocker = *blockers[index];
                output.push_back('{');
                AppendJsonStringMember(output, "BlockerId", blocker.m_blockerId);
                AppendJsonStringMember(output, "Kind", ToString(blocker.m_kind));
                AppendJsonStringMember(
                    output,
                    "Severity",
                    ToString(blocker.m_severity));
                AppendJsonStringMember(output, "Code", blocker.m_code);
                AppendJsonStringMember(output, "SubjectRef", blocker.m_subjectRef);
                AppendJsonStringMember(output, "Message", blocker.m_message);
                AppendJsonStringMember(output, "StepId", blocker.m_stepId);
                AppendJsonStringMember(
                    output,
                    "RollbackResultId",
                    blocker.m_rollbackResultId);
                AppendJsonStringArray(
                    output,
                    "EvidenceIds",
                    blocker.m_evidenceIds);
                AppendJsonStringArray(
                    output,
                    "LogReferenceIds",
                    blocker.m_logReferenceIds);
                AppendJsonBoolMember(
                    output,
                    "BlocksCompatibility",
                    blocker.m_blocksCompatibility);
                AppendJsonBoolMember(
                    output,
                    "BlocksRelease",
                    blocker.m_blocksRelease,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');
            AppendJsonBoolMember(
                output,
                "ExecutionEvidenceAccepted",
                report.m_executionEvidenceAccepted);
            AppendJsonBoolMember(
                output,
                "CompatibilityClear",
                report.m_compatibilityClear);
            AppendJsonBoolMember(
                output,
                "ReleaseBlockerFree",
                report.m_releaseBlockerFree);
            AppendJsonBoolMember(
                output,
                "HumanReviewRequired",
                report.m_humanReviewRequired);
            AppendJsonBoolMember(
                output,
                "VerifierExecuted",
                report.m_verifierExecuted);
            AppendJsonBoolMember(
                output,
                "EvidencePromoted",
                report.m_evidencePromoted);
            AppendJsonBoolMember(
                output,
                "ReleasePublished",
                report.m_releasePublished);
            AppendJsonBoolMember(
                output,
                "LaunchPerformed",
                report.m_launchPerformed);
            AppendJsonBoolMember(
                output,
                "AdapterCalled",
                report.m_adapterCalled,
                false);
            output.push_back('}');
            return output;
        }

    } // namespace
} // namespace TaintedGrailModdingSDK
