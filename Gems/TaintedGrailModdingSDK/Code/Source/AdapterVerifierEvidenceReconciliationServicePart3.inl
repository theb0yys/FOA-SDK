namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AppendReconciliationJsonEscaped(
            AZStd::string& output,
            const AZStd::string& value)
        {
            constexpr char HexDigits[] = "0123456789abcdef";
            output.push_back('"');
            for (char character : value)
            {
                const unsigned char byte =
                    static_cast<unsigned char>(character);
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

        void AppendReconciliationJsonName(
            AZStd::string& output,
            const char* name)
        {
            AppendReconciliationJsonEscaped(output, name);
            output.push_back(':');
        }

        void AppendReconciliationJsonString(
            AZStd::string& output,
            const char* name,
            const AZStd::string& value,
            bool comma = true)
        {
            AppendReconciliationJsonName(output, name);
            AppendReconciliationJsonEscaped(output, value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendReconciliationJsonUnsigned(
            AZStd::string& output,
            const char* name,
            AZ::u64 value,
            bool comma = true)
        {
            AppendReconciliationJsonName(output, name);
            output += ReconciliationUnsignedString(value);
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendReconciliationJsonBool(
            AZStd::string& output,
            const char* name,
            bool value,
            bool comma = true)
        {
            AppendReconciliationJsonName(output, name);
            output += value ? "true" : "false";
            if (comma)
            {
                output.push_back(',');
            }
        }

        void AppendReconciliationJsonStringArray(
            AZStd::string& output,
            const char* name,
            AZStd::vector<AZStd::string> values,
            bool comma = true)
        {
            SortUniqueReconciliationValues(values);
            AppendReconciliationJsonName(output, name);
            output.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                AppendReconciliationJsonEscaped(output, values[index]);
            }
            output.push_back(']');
            if (comma)
            {
                output.push_back(',');
            }
        }

        AZStd::string SerializeReconciliationEnvelope(
            const AdapterVerifierEvidenceReconciliationEnvelope& envelope)
        {
            AZStd::string output;
            output.push_back('{');
            AppendReconciliationJsonUnsigned(
                output,
                "ContractVersion",
                envelope.m_contractVersion);
            AppendReconciliationJsonString(
                output,
                "ReconciliationId",
                envelope.m_reconciliationId);
            AppendReconciliationJsonString(
                output,
                "ReportId",
                envelope.m_reportId);
            AppendReconciliationJsonString(
                output,
                "ReportStatus",
                ToString(envelope.m_reportStatus));
            AppendReconciliationJsonString(
                output,
                "ReportCanonicalJson",
                envelope.m_reportCanonicalJson);
            AppendReconciliationJsonString(
                output,
                "VerifierResultId",
                envelope.m_verifierResultId);
            AppendReconciliationJsonString(
                output,
                "VerifierEvidenceStatus",
                ToString(envelope.m_verifierEvidenceStatus));
            AppendReconciliationJsonString(
                output,
                "ExecutionResultId",
                envelope.m_executionResultId);
            AppendReconciliationJsonString(
                output,
                "WorkOrderId",
                envelope.m_workOrderId);
            AppendReconciliationJsonString(
                output,
                "WorkOrderFingerprint",
                envelope.m_workOrderFingerprint);
            AppendReconciliationJsonString(
                output,
                "ExecutionResultFingerprint",
                envelope.m_executionResultFingerprint);
            AppendReconciliationJsonString(
                output,
                "VerifierResultFingerprint",
                envelope.m_verifierResultFingerprint);
            AppendReconciliationJsonString(
                output,
                "ProfileId",
                envelope.m_profileId);
            AppendReconciliationJsonString(
                output,
                "GameVersion",
                envelope.m_gameVersion);
            AppendReconciliationJsonString(output, "Branch", envelope.m_branch);
            AppendReconciliationJsonString(
                output,
                "RuntimeTarget",
                envelope.m_runtimeTarget);
            AppendReconciliationJsonString(output, "PackId", envelope.m_packId);
            AppendReconciliationJsonString(
                output,
                "PreviewId",
                envelope.m_previewId);
            AppendReconciliationJsonString(
                output,
                "PreviewFingerprint",
                envelope.m_previewFingerprint);
            AppendReconciliationJsonString(
                output,
                "TargetInventoryId",
                envelope.m_targetInventoryId);
            AppendReconciliationJsonString(
                output,
                "EvaluatedAtUtc",
                envelope.m_evaluatedAtUtc);
            AppendReconciliationJsonString(
                output,
                "Status",
                ToString(envelope.m_status));
            AppendReconciliationJsonString(
                output,
                "CompatibilityAssessment",
                ToString(envelope.m_compatibilityAssessment));
            AppendReconciliationJsonString(
                output,
                "ReleaseDecision",
                ToString(envelope.m_releaseDecision));
            AppendReconciliationJsonString(
                output,
                "HumanReviewState",
                ToString(envelope.m_humanReviewState));
            AppendReconciliationJsonUnsigned(
                output,
                "ReportBlockerCount",
                envelope.m_reportBlockerCount);
            AppendReconciliationJsonUnsigned(
                output,
                "VerifierCheckCount",
                envelope.m_verifierCheckCount);
            AppendReconciliationJsonUnsigned(
                output,
                "FindingCount",
                envelope.m_findingCount);
            AppendReconciliationJsonUnsigned(
                output,
                "CompatibilityBlockerCount",
                envelope.m_compatibilityBlockerCount);
            AppendReconciliationJsonUnsigned(
                output,
                "ReleaseBlockerCount",
                envelope.m_releaseBlockerCount);
            AppendReconciliationJsonUnsigned(
                output,
                "RequiredDispositionCount",
                envelope.m_requiredDispositionCount);
            AppendReconciliationJsonUnsigned(
                output,
                "CompletedDispositionCount",
                envelope.m_completedDispositionCount);
            AppendReconciliationJsonStringArray(
                output,
                "InputCandidateSourceIds",
                envelope.m_inputCandidateSourceIds);
            AppendReconciliationJsonStringArray(
                output,
                "InputCandidateEvidenceIds",
                envelope.m_inputCandidateEvidenceIds);

            AppendReconciliationJsonName(output, "ReleaseReview");
            output.push_back('{');
            AppendReconciliationJsonString(
                output,
                "ReviewId",
                envelope.m_releaseReview.m_reviewId);
            AppendReconciliationJsonString(
                output,
                "ReportId",
                envelope.m_releaseReview.m_reportId);
            AppendReconciliationJsonString(
                output,
                "ReportCanonicalJson",
                envelope.m_releaseReview.m_reportCanonicalJson);
            AppendReconciliationJsonString(
                output,
                "VerifierResultId",
                envelope.m_releaseReview.m_verifierResultId);
            AppendReconciliationJsonString(
                output,
                "WorkOrderFingerprint",
                envelope.m_releaseReview.m_workOrderFingerprint);
            AppendReconciliationJsonString(
                output,
                "ExecutionResultFingerprint",
                envelope.m_releaseReview.m_executionResultFingerprint);
            AppendReconciliationJsonString(
                output,
                "VerifierResultFingerprint",
                envelope.m_releaseReview.m_verifierResultFingerprint);
            AppendReconciliationJsonStringArray(
                output,
                "CandidateSourceIds",
                envelope.m_releaseReview.m_candidateSourceIds);
            AppendReconciliationJsonStringArray(
                output,
                "CandidateEvidenceIds",
                envelope.m_releaseReview.m_candidateEvidenceIds);
            AppendReconciliationJsonString(
                output,
                "Decision",
                ToString(envelope.m_releaseReview.m_decision));
            AppendReconciliationJsonString(
                output,
                "Reviewer",
                envelope.m_releaseReview.m_reviewer);
            AppendReconciliationJsonStringArray(
                output,
                "EvidenceIds",
                envelope.m_releaseReview.m_evidenceIds);
            AppendReconciliationJsonString(
                output,
                "ReviewedAtUtc",
                envelope.m_releaseReview.m_reviewedAtUtc);
            AppendReconciliationJsonString(
                output,
                "Rationale",
                envelope.m_releaseReview.m_rationale);

            AZStd::vector<const AdapterVerifierFindingDisposition*> dispositions;
            for (const AdapterVerifierFindingDisposition& disposition :
                 envelope.m_releaseReview.m_dispositions)
            {
                dispositions.push_back(&disposition);
            }
            AZStd::sort(
                dispositions.begin(),
                dispositions.end(),
                [](const AdapterVerifierFindingDisposition* left,
                    const AdapterVerifierFindingDisposition* right)
                {
                    return left->m_findingId < right->m_findingId;
                });
            AppendReconciliationJsonName(output, "Dispositions");
            output.push_back('[');
            for (size_t index = 0; index < dispositions.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterVerifierFindingDisposition& disposition =
                    *dispositions[index];
                output.push_back('{');
                AppendReconciliationJsonString(
                    output,
                    "FindingId",
                    disposition.m_findingId);
                AppendReconciliationJsonString(
                    output,
                    "Decision",
                    ToString(disposition.m_decision));
                AppendReconciliationJsonString(
                    output,
                    "Rationale",
                    disposition.m_rationale);
                AppendReconciliationJsonStringArray(
                    output,
                    "EvidenceIds",
                    disposition.m_evidenceIds,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back('}');
            output.push_back(',');

            AppendReconciliationJsonName(output, "Findings");
            output.push_back('[');
            for (size_t index = 0; index < envelope.m_findings.size(); ++index)
            {
                if (index != 0)
                {
                    output.push_back(',');
                }
                const AdapterVerifierReconciliationFinding& finding =
                    envelope.m_findings[index];
                output.push_back('{');
                AppendReconciliationJsonString(
                    output,
                    "FindingId",
                    finding.m_findingId);
                AppendReconciliationJsonString(
                    output,
                    "Kind",
                    ToString(finding.m_kind));
                AppendReconciliationJsonString(
                    output,
                    "Relationship",
                    ToString(finding.m_relationship));
                AppendReconciliationJsonString(
                    output,
                    "SubjectRef",
                    finding.m_subjectRef);
                AppendReconciliationJsonString(
                    output,
                    "ReportBlockerId",
                    finding.m_reportBlockerId);
                AppendReconciliationJsonString(
                    output,
                    "CheckId",
                    finding.m_checkId);
                AppendReconciliationJsonString(
                    output,
                    "StepId",
                    finding.m_stepId);
                AppendReconciliationJsonString(
                    output,
                    "Message",
                    finding.m_message);
                AppendReconciliationJsonStringArray(
                    output,
                    "EvidenceIds",
                    finding.m_evidenceIds);
                AppendReconciliationJsonStringArray(
                    output,
                    "DiagnosticReferenceIds",
                    finding.m_diagnosticReferenceIds);
                AppendReconciliationJsonBool(
                    output,
                    "ExistingBlockerPreserved",
                    finding.m_existingBlockerPreserved);
                AppendReconciliationJsonBool(
                    output,
                    "BlocksCompatibility",
                    finding.m_blocksCompatibility);
                AppendReconciliationJsonBool(
                    output,
                    "BlocksRelease",
                    finding.m_blocksRelease);
                AppendReconciliationJsonBool(
                    output,
                    "RequiresHumanDisposition",
                    finding.m_requiresHumanDisposition,
                    false);
                output.push_back('}');
            }
            output.push_back(']');
            output.push_back(',');
            AppendReconciliationJsonBool(
                output,
                "HumanReviewRequired",
                envelope.m_humanReviewRequired);
            AppendReconciliationJsonBool(
                output,
                "VerifierExecuted",
                envelope.m_verifierExecuted);
            AppendReconciliationJsonBool(
                output,
                "TargetAccessed",
                envelope.m_targetAccessed);
            AppendReconciliationJsonBool(
                output,
                "FilesMutated",
                envelope.m_filesMutated);
            AppendReconciliationJsonBool(
                output,
                "EvidencePromoted",
                envelope.m_evidencePromoted);
            AppendReconciliationJsonBool(
                output,
                "ArchiveAssembled",
                envelope.m_archiveAssembled);
            AppendReconciliationJsonBool(
                output,
                "ArchiveSigned",
                envelope.m_archiveSigned);
            AppendReconciliationJsonBool(
                output,
                "ReleasePublished",
                envelope.m_releasePublished);
            AppendReconciliationJsonBool(
                output,
                "LaunchPerformed",
                envelope.m_launchPerformed);
            AppendReconciliationJsonBool(
                output,
                "AdapterCalled",
                envelope.m_adapterCalled,
                false);
            output.push_back('}');
            return output;
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
