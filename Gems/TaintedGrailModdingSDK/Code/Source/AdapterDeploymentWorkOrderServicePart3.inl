namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string EscapeJson(const AZStd::string& value)
        {
            constexpr char HexDigits[] = "0123456789abcdef";
            AZStd::string escaped;
            escaped.reserve(value.size() + 8);
            for (char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                switch (character)
                {
                case '"':
                    escaped += "\\\"";
                    break;
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\b':
                    escaped += "\\b";
                    break;
                case '\f':
                    escaped += "\\f";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    if (byte < 0x20)
                    {
                        escaped += "\\u00";
                        escaped.push_back(HexDigits[(byte >> 4) & 0x0f]);
                        escaped.push_back(HexDigits[byte & 0x0f]);
                    }
                    else
                    {
                        escaped.push_back(character);
                    }
                    break;
                }
            }
            return escaped;
        }

        void WriteJsonString(std::ostringstream& stream, const AZStd::string& value)
        {
            stream << '"' << EscapeJson(value).c_str() << '"';
        }

        void WriteStringArray(
            std::ostringstream& stream,
            const AZStd::vector<AZStd::string>& values)
        {
            stream << '[';
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index > 0)
                {
                    stream << ',';
                }
                WriteJsonString(stream, values[index]);
            }
            stream << ']';
        }

        void WriteJsonBool(std::ostringstream& stream, bool value)
        {
            stream << (value ? "true" : "false");
        }

        void AddBlockedChecklist(AdapterDeploymentWorkOrder& workOrder)
        {
            AZStd::vector<AZStd::string> blockerCodes;
            for (const AdapterDeploymentWorkOrderBlocker& blocker :
                 workOrder.m_blockers)
            {
                blockerCodes.push_back(blocker.m_code);
            }
            SortUnique(blockerCodes);
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::Blocked,
                "Resolve contract blockers",
                "Operator review cannot begin until every fail-closed confirmation, "
                "time, evidence, and work-order blocker is resolved.",
                AZStd::move(blockerCodes));
        }
    } // namespace

    AdapterDeploymentWorkOrder AdapterDeploymentWorkOrderService::BuildWorkOrder(
        const AdapterDeploymentWorkOrderRequest& request) const
    {
        AdapterDeploymentWorkOrder workOrder;
        workOrder.m_previewId = request.m_preview.m_previewId;
        workOrder.m_previewFingerprint = request.m_previewFingerprint;
        workOrder.m_packId = request.m_preview.m_packId;
        workOrder.m_targetInventoryId =
            request.m_preview.m_targetInventoryId;
        workOrder.m_confirmationId =
            request.m_confirmation.m_confirmationId;
        workOrder.m_confirmationScope = request.m_confirmation.m_scope;
        workOrder.m_reviewer = request.m_confirmation.m_reviewer;
        workOrder.m_evaluatedAtUtc = request.m_evaluatedAtUtc;
        workOrder.m_confirmationExpiresAtUtc =
            request.m_confirmation.m_expiresAtUtc;
        workOrder.m_maintenanceWindowId =
            request.m_maintenanceWindow.m_windowId;
        workOrder.m_maintenanceStartAtUtc =
            request.m_maintenanceWindow.m_startAtUtc;
        workOrder.m_maintenanceEndAtUtc =
            request.m_maintenanceWindow.m_endAtUtc;
        workOrder.m_operatorGroup =
            request.m_maintenanceWindow.m_operatorGroup;
        workOrder.m_executionAllowed = false;
        workOrder.m_copyAllowed = false;
        workOrder.m_deleteAllowed = false;
        workOrder.m_backupAllowed = false;
        workOrder.m_restoreAllowed = false;
        workOrder.m_deploymentAllowed = false;
        workOrder.m_launchAllowed = false;
        workOrder.m_workOrderId =
            "deploymentworkorder:" + workOrder.m_packId
            + ":" + workOrder.m_previewId
            + ":" + workOrder.m_confirmationId;

        bool previewNotReady = false;
        bool confirmationMissing = false;
        bool confirmationRejected = false;
        bool confirmationBindingMismatch = false;
        bool scopeMismatch = false;
        bool confirmationExpired = false;
        bool maintenanceWindowInvalid = false;
        bool outsideMaintenanceWindow = false;
        bool preflightMissing = false;
        bool preflightFailed = false;
        bool workOrderIncomplete = false;

        if (request.m_preview.m_status
                != AdapterStagingDeploymentPreviewStatus::Ready
            || request.m_preview.m_previewId.empty()
            || request.m_preview.m_canonicalJson.empty()
            || !request.m_preview.m_conflicts.empty()
            || !request.m_preview.m_blockers.empty()
            || request.m_preview.m_stagingMutationAllowed
            || request.m_preview.m_deploymentMutationAllowed
            || request.m_preview.m_rollbackExecutionAllowed
            || request.m_preview.m_launchAllowed
            || !IsSha256Fingerprint(request.m_previewFingerprint)
            || !CanonicalSha256Matches(
                request.m_preview.m_canonicalJson,
                request.m_previewFingerprint))
        {
            previewNotReady = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.preview_not_ready",
                workOrder.m_previewId,
                "An exact ready staging/deployment preview with a SHA-256 fingerprint "
                "derived from its canonical JSON and all mutation and launch permissions "
                "false is required.");
        }

        const AdapterDeploymentConfirmation& confirmation =
            request.m_confirmation;
        if (!IsStableNamespacedId(confirmation.m_confirmationId)
            || confirmation.m_decision
                == AdapterDeploymentConfirmationDecision::Unknown
            || confirmation.m_reviewer.empty()
            || confirmation.m_evidenceIds.empty()
            || confirmation.m_issuedAtUtc.empty()
            || confirmation.m_expiresAtUtc.empty())
        {
            confirmationMissing = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.confirmation_missing",
                confirmation.m_confirmationId,
                "A named evidence-backed explicit confirmation with issue and expiry times is required.");
        }
        if (confirmation.m_decision
            == AdapterDeploymentConfirmationDecision::Rejected)
        {
            confirmationRejected = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.confirmation_rejected",
                confirmation.m_confirmationId,
                "The exact deployment confirmation was rejected.");
        }
        if (confirmation.m_previewId != request.m_preview.m_previewId
            || confirmation.m_previewFingerprint
                != request.m_previewFingerprint
            || !IsSha256Fingerprint(confirmation.m_previewFingerprint))
        {
            confirmationBindingMismatch = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.confirmation_binding_mismatch",
                confirmation.m_confirmationId,
                "The confirmation must bind to the exact ready preview ID and derived fingerprint.");
        }
        if (!ScopeCoversPreview(confirmation.m_scope, request.m_preview))
        {
            scopeMismatch = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.scope_mismatch",
                confirmation.m_confirmationId,
                "The typed confirmation scope does not cover every addition, replacement, "
                "and removal in the exact preview.");
        }
        if (!IsUtcTimestamp(confirmation.m_issuedAtUtc)
            || !IsUtcTimestamp(confirmation.m_expiresAtUtc)
            || !IsUtcTimestamp(request.m_evaluatedAtUtc)
            || confirmation.m_issuedAtUtc > confirmation.m_expiresAtUtc
            || request.m_evaluatedAtUtc < confirmation.m_issuedAtUtc
            || request.m_evaluatedAtUtc > confirmation.m_expiresAtUtc)
        {
            confirmationExpired = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.confirmation_expired",
                confirmation.m_confirmationId,
                "Confirmation times and evaluation time must use exact valid UTC seconds and "
                "the evaluation must occur before expiry.");
        }

        const AdapterDeploymentMaintenanceWindow& window =
            request.m_maintenanceWindow;
        if (!IsStableNamespacedId(window.m_windowId)
            || window.m_previewId != request.m_preview.m_previewId
            || window.m_previewFingerprint != request.m_previewFingerprint
            || !IsSha256Fingerprint(window.m_previewFingerprint)
            || !IsUtcTimestamp(window.m_startAtUtc)
            || !IsUtcTimestamp(window.m_endAtUtc)
            || window.m_startAtUtc >= window.m_endAtUtc
            || window.m_operatorGroup.empty()
            || window.m_evidenceIds.empty())
        {
            maintenanceWindowInvalid = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.maintenance_window_invalid",
                window.m_windowId,
                "The maintenance window must be evidence-backed, preview-bound, non-empty, "
                "and expressed as a valid increasing UTC interval.");
        }
        else if (!IsUtcTimestamp(request.m_evaluatedAtUtc)
            || request.m_evaluatedAtUtc < window.m_startAtUtc
            || request.m_evaluatedAtUtc > window.m_endAtUtc)
        {
            outsideMaintenanceWindow = true;
            AddBlocker(
                workOrder,
                "deployment_work_order.outside_maintenance_window",
                window.m_windowId,
                "The deterministic evaluation time is outside the reviewed maintenance window.");
        }

        AZStd::vector<AdapterDeploymentPreflightKind> requiredKinds = {
            AdapterDeploymentPreflightKind::PackageIntegrity,
            AdapterDeploymentPreflightKind::TargetInventory,
            AdapterDeploymentPreflightKind::RollbackReadiness,
            AdapterDeploymentPreflightKind::OperatorReadiness,
        };
        if (!request.m_preview.m_backups.empty())
        {
            requiredKinds.push_back(
                AdapterDeploymentPreflightKind::BackupReadiness);
        }
        for (AdapterDeploymentPreflightKind kind : requiredKinds)
        {
            if (!HasPreflightKind(request.m_preflightEvidence, kind))
            {
                preflightMissing = true;
                AddBlocker(
                    workOrder,
                    "deployment_work_order.preflight_missing",
                    ToString(kind),
                    "A required typed preflight evidence record is missing.");
            }
            else if (CountPreflightKind(request.m_preflightEvidence, kind) != 1)
            {
                preflightFailed = true;
                AddBlocker(
                    workOrder,
                    "deployment_work_order.preflight_duplicate",
                    ToString(kind),
                    "Every required preflight kind must appear exactly once.");
            }
        }
        for (const AdapterDeploymentPreflightEvidence& preflight :
             request.m_preflightEvidence)
        {
            if (!IsStableNamespacedId(preflight.m_preflightId)
                || preflight.m_previewId != request.m_preview.m_previewId
                || preflight.m_previewFingerprint
                    != request.m_previewFingerprint
                || !IsSha256Fingerprint(preflight.m_previewFingerprint)
                || preflight.m_status
                    != AdapterDeploymentPreflightStatus::Passed
                || !IsUtcTimestamp(preflight.m_checkedAtUtc)
                || preflight.m_checkedAtUtc < confirmation.m_issuedAtUtc
                || preflight.m_checkedAtUtc > request.m_evaluatedAtUtc
                || preflight.m_checker.empty()
                || preflight.m_evidenceIds.empty())
            {
                preflightFailed = true;
                AddBlocker(
                    workOrder,
                    "deployment_work_order.preflight_failed",
                    preflight.m_preflightId,
                    "Preflight evidence must be passed, exact-preview-bound, evidence-backed, "
                    "and checked between confirmation issue and evaluation.");
            }
        }

        if (!previewNotReady
            && !confirmationMissing
            && !confirmationRejected
            && !confirmationBindingMismatch
            && !scopeMismatch
            && !confirmationExpired
            && !maintenanceWindowInvalid
            && !outsideMaintenanceWindow
            && !preflightMissing
            && !preflightFailed)
        {
            BuildStepsAndChecklist(request, workOrder);
            if (!HasExactWorkOrderCoverage(request.m_preview, workOrder))
            {
                workOrderIncomplete = true;
                AddBlocker(
                    workOrder,
                    "deployment_work_order.incomplete",
                    workOrder.m_previewId,
                    "Every preview addition, replacement, removal, backup, and rollback "
                    "inverse must have exact work-order coverage.");
            }
        }

        if (previewNotReady)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::PreviewNotReady;
        }
        else if (confirmationMissing)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ConfirmationMissing;
        }
        else if (confirmationRejected)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ConfirmationRejected;
        }
        else if (confirmationBindingMismatch)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ConfirmationBindingMismatch;
        }
        else if (scopeMismatch)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ScopeMismatch;
        }
        else if (confirmationExpired)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ConfirmationExpired;
        }
        else if (maintenanceWindowInvalid)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::MaintenanceWindowInvalid;
        }
        else if (outsideMaintenanceWindow)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::OutsideMaintenanceWindow;
        }
        else if (preflightMissing)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::PreflightMissing;
        }
        else if (preflightFailed)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::PreflightFailed;
        }
        else if (workOrderIncomplete)
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::WorkOrderIncomplete;
        }
        else
        {
            workOrder.m_status =
                AdapterDeploymentWorkOrderStatus::ReviewReady;
        }

        if (workOrder.m_checklist.empty())
        {
            AddBlockedChecklist(workOrder);
        }
        SortWorkOrderCollections(workOrder);
        workOrder.m_canonicalJson = SerializeCanonicalWorkOrder(workOrder);
        return workOrder;
    }

    AZStd::string AdapterDeploymentWorkOrderService::SerializeCanonicalWorkOrder(
        const AdapterDeploymentWorkOrder& workOrder) const
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << '{';
        stream << "\"FormatVersion\":" << workOrder.m_formatVersion;
        stream << ",\"WorkOrderId\":";
        WriteJsonString(stream, workOrder.m_workOrderId);
        stream << ",\"PreviewId\":";
        WriteJsonString(stream, workOrder.m_previewId);
        stream << ",\"PreviewFingerprint\":";
        WriteJsonString(stream, workOrder.m_previewFingerprint);
        stream << ",\"PackId\":";
        WriteJsonString(stream, workOrder.m_packId);
        stream << ",\"TargetInventoryId\":";
        WriteJsonString(stream, workOrder.m_targetInventoryId);
        stream << ",\"ConfirmationId\":";
        WriteJsonString(stream, workOrder.m_confirmationId);
        stream << ",\"ConfirmationScope\":";
        WriteJsonString(stream, ToString(workOrder.m_confirmationScope));
        stream << ",\"Reviewer\":";
        WriteJsonString(stream, workOrder.m_reviewer);
        stream << ",\"EvaluatedAtUtc\":";
        WriteJsonString(stream, workOrder.m_evaluatedAtUtc);
        stream << ",\"ConfirmationExpiresAtUtc\":";
        WriteJsonString(stream, workOrder.m_confirmationExpiresAtUtc);
        stream << ",\"MaintenanceWindowId\":";
        WriteJsonString(stream, workOrder.m_maintenanceWindowId);
        stream << ",\"MaintenanceStartAtUtc\":";
        WriteJsonString(stream, workOrder.m_maintenanceStartAtUtc);
        stream << ",\"MaintenanceEndAtUtc\":";
        WriteJsonString(stream, workOrder.m_maintenanceEndAtUtc);
        stream << ",\"OperatorGroup\":";
        WriteJsonString(stream, workOrder.m_operatorGroup);
        stream << ",\"Status\":";
        WriteJsonString(stream, ToString(workOrder.m_status));
        stream << ",\"Steps\":[";
        for (size_t index = 0; index < workOrder.m_steps.size(); ++index)
        {
            if (index > 0)
            {
                stream << ',';
            }
            const AdapterDeploymentWorkOrderStep& step =
                workOrder.m_steps[index];
            stream << '{';
            stream << "\"Sequence\":" << step.m_sequence;
            stream << ",\"StepId\":";
            WriteJsonString(stream, step.m_stepId);
            stream << ",\"Kind\":";
            WriteJsonString(stream, ToString(step.m_kind));
            stream << ",\"TargetPath\":";
            WriteJsonString(stream, step.m_targetPath);
            stream << ",\"BackupPath\":";
            WriteJsonString(stream, step.m_backupPath);
            stream << ",\"PreviousFingerprint\":";
            WriteJsonString(stream, step.m_previousFingerprint);
            stream << ",\"DesiredFingerprint\":";
            WriteJsonString(stream, step.m_desiredFingerprint);
            stream << ",\"EvidenceIds\":";
            WriteStringArray(stream, step.m_evidenceIds);
            stream << ",\"Description\":";
            WriteJsonString(stream, step.m_description);
            stream << ",\"ExecutionAllowed\":";
            WriteJsonBool(stream, step.m_executionAllowed);
            stream << '}';
        }
        stream << ']';
        stream << ",\"Checklist\":[";
        for (size_t index = 0; index < workOrder.m_checklist.size(); ++index)
        {
            if (index > 0)
            {
                stream << ',';
            }
            const AdapterDeploymentOperatorChecklistItem& item =
                workOrder.m_checklist[index];
            stream << '{';
            stream << "\"Sequence\":" << item.m_sequence;
            stream << ",\"ItemId\":";
            WriteJsonString(stream, item.m_itemId);
            stream << ",\"State\":";
            WriteJsonString(stream, ToString(item.m_state));
            stream << ",\"Label\":";
            WriteJsonString(stream, item.m_label);
            stream << ",\"Detail\":";
            WriteJsonString(stream, item.m_detail);
            stream << ",\"EvidenceIds\":";
            WriteStringArray(stream, item.m_evidenceIds);
            stream << ",\"AcknowledgementRecorded\":";
            WriteJsonBool(stream, item.m_acknowledgementRecorded);
            stream << '}';
        }
        stream << ']';
        stream << ",\"Blockers\":[";
        for (size_t index = 0; index < workOrder.m_blockers.size(); ++index)
        {
            if (index > 0)
            {
                stream << ',';
            }
            const AdapterDeploymentWorkOrderBlocker& blocker =
                workOrder.m_blockers[index];
            stream << '{';
            stream << "\"Code\":";
            WriteJsonString(stream, blocker.m_code);
            stream << ",\"Subject\":";
            WriteJsonString(stream, blocker.m_subject);
            stream << ",\"Reason\":";
            WriteJsonString(stream, blocker.m_reason);
            stream << '}';
        }
        stream << ']';
        stream << ",\"ExecutionAllowed\":";
        WriteJsonBool(stream, workOrder.m_executionAllowed);
        stream << ",\"CopyAllowed\":";
        WriteJsonBool(stream, workOrder.m_copyAllowed);
        stream << ",\"DeleteAllowed\":";
        WriteJsonBool(stream, workOrder.m_deleteAllowed);
        stream << ",\"BackupAllowed\":";
        WriteJsonBool(stream, workOrder.m_backupAllowed);
        stream << ",\"RestoreAllowed\":";
        WriteJsonBool(stream, workOrder.m_restoreAllowed);
        stream << ",\"DeploymentAllowed\":";
        WriteJsonBool(stream, workOrder.m_deploymentAllowed);
        stream << ",\"LaunchAllowed\":";
        WriteJsonBool(stream, workOrder.m_launchAllowed);
        stream << '}';
        return stream.str().c_str();
    }
} // namespace TaintedGrailModdingSDK
