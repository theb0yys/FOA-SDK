namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool ScopeCoversPreview(
            AdapterDeploymentConfirmationScope scope,
            const AdapterStagingDeploymentPreview& preview)
        {
            switch (scope)
            {
            case AdapterDeploymentConfirmationScope::AdditionsOnly:
                return preview.m_replacements.empty()
                    && preview.m_removals.empty();
            case AdapterDeploymentConfirmationScope::AdditionsAndReplacements:
                return preview.m_removals.empty();
            case AdapterDeploymentConfirmationScope::FullPreview:
                return true;
            }
            return false;
        }

        bool HasPreflightKind(
            const AZStd::vector<AdapterDeploymentPreflightEvidence>& preflight,
            AdapterDeploymentPreflightKind kind)
        {
            for (const AdapterDeploymentPreflightEvidence& entry : preflight)
            {
                if (entry.m_kind == kind)
                {
                    return true;
                }
            }
            return false;
        }

        size_t CountPreflightKind(
            const AZStd::vector<AdapterDeploymentPreflightEvidence>& preflight,
            AdapterDeploymentPreflightKind kind)
        {
            size_t count = 0;
            for (const AdapterDeploymentPreflightEvidence& entry : preflight)
            {
                if (entry.m_kind == kind)
                {
                    ++count;
                }
            }
            return count;
        }

        void AddStep(
            AdapterDeploymentWorkOrder& workOrder,
            AdapterDeploymentWorkOrderStepKind kind,
            AZStd::string targetPath,
            AZStd::string backupPath,
            AZStd::string previousFingerprint,
            AZStd::string desiredFingerprint,
            AZStd::vector<AZStd::string> evidenceIds,
            AZStd::string description)
        {
            AdapterDeploymentWorkOrderStep step;
            step.m_sequence = static_cast<AZ::u64>(workOrder.m_steps.size() + 1);
            step.m_stepId = workOrder.m_workOrderId
                + ":step:" + ToUnsignedString(step.m_sequence);
            step.m_kind = kind;
            step.m_targetPath = AZStd::move(targetPath);
            step.m_backupPath = AZStd::move(backupPath);
            step.m_previousFingerprint = AZStd::move(previousFingerprint);
            step.m_desiredFingerprint = AZStd::move(desiredFingerprint);
            SortUnique(evidenceIds);
            step.m_evidenceIds = AZStd::move(evidenceIds);
            step.m_description = AZStd::move(description);
            step.m_executionAllowed = false;
            workOrder.m_steps.push_back(AZStd::move(step));
        }

        void AddChecklistItem(
            AdapterDeploymentWorkOrder& workOrder,
            AdapterDeploymentChecklistState state,
            AZStd::string label,
            AZStd::string detail,
            AZStd::vector<AZStd::string> evidenceIds)
        {
            AdapterDeploymentOperatorChecklistItem item;
            item.m_sequence =
                static_cast<AZ::u64>(workOrder.m_checklist.size() + 1);
            item.m_itemId = workOrder.m_workOrderId
                + ":check:" + ToUnsignedString(item.m_sequence);
            item.m_state = state;
            item.m_label = AZStd::move(label);
            item.m_detail = AZStd::move(detail);
            SortUnique(evidenceIds);
            item.m_evidenceIds = AZStd::move(evidenceIds);
            item.m_acknowledgementRecorded = false;
            workOrder.m_checklist.push_back(AZStd::move(item));
        }

        void BuildStepsAndChecklist(
            const AdapterDeploymentWorkOrderRequest& request,
            AdapterDeploymentWorkOrder& workOrder)
        {
            const AZStd::vector<AZStd::string> allEvidence =
                CollectEvidence(request);

            AddStep(
                workOrder,
                AdapterDeploymentWorkOrderStepKind::VerifyPreflight,
                {},
                {},
                {},
                request.m_previewFingerprint,
                allEvidence,
                "Verify the exact preview fingerprint and every required preflight evidence record.");
            AddStep(
                workOrder,
                AdapterDeploymentWorkOrderStepKind::ConfirmMaintenanceWindow,
                request.m_preview.m_targetRoot,
                {},
                {},
                {},
                request.m_maintenanceWindow.m_evidenceIds,
                "Confirm the operator group and exact UTC maintenance window before any later execution review.");

            for (const AdapterDeploymentBackupRequirement& backup :
                request.m_preview.m_backups)
            {
                AddStep(
                    workOrder,
                    AdapterDeploymentWorkOrderStepKind::Backup,
                    backup.m_targetPath,
                    backup.m_backupPath,
                    backup.m_fingerprint,
                    backup.m_fingerprint,
                    request.m_confirmation.m_evidenceIds,
                    "Describe the required pre-change backup. No backup operation is authorized.");
            }

            for (const AdapterDeploymentChange& change :
                request.m_preview.m_additions)
            {
                AddStep(
                    workOrder,
                    AdapterDeploymentWorkOrderStepKind::Add,
                    change.m_targetPath,
                    {},
                    {},
                    change.m_desiredFingerprint,
                    request.m_confirmation.m_evidenceIds,
                    "Describe one exact addition from the reviewed preview. File creation remains prohibited.");
            }
            for (const AdapterDeploymentChange& change :
                request.m_preview.m_replacements)
            {
                AZStd::string backupPath;
                for (const AdapterDeploymentBackupRequirement& backup :
                    request.m_preview.m_backups)
                {
                    if (backup.m_targetEntryId == change.m_targetEntryId)
                    {
                        backupPath = backup.m_backupPath;
                        break;
                    }
                }
                AddStep(
                    workOrder,
                    AdapterDeploymentWorkOrderStepKind::Replace,
                    change.m_targetPath,
                    AZStd::move(backupPath),
                    change.m_previousFingerprint,
                    change.m_desiredFingerprint,
                    request.m_confirmation.m_evidenceIds,
                    "Describe one exact replacement and its backup dependency. Replacement remains prohibited.");
            }
            for (const AdapterDeploymentChange& change :
                request.m_preview.m_removals)
            {
                AZStd::string backupPath;
                for (const AdapterDeploymentBackupRequirement& backup :
                    request.m_preview.m_backups)
                {
                    if (backup.m_targetEntryId == change.m_targetEntryId)
                    {
                        backupPath = backup.m_backupPath;
                        break;
                    }
                }
                AddStep(
                    workOrder,
                    AdapterDeploymentWorkOrderStepKind::Remove,
                    change.m_targetPath,
                    AZStd::move(backupPath),
                    change.m_previousFingerprint,
                    {},
                    request.m_confirmation.m_evidenceIds,
                    "Describe one exact removal and its backup dependency. Deletion remains prohibited.");
            }

            AddStep(
                workOrder,
                AdapterDeploymentWorkOrderStepKind::VerifyDeployment,
                request.m_preview.m_targetRoot,
                {},
                {},
                request.m_previewFingerprint,
                allEvidence,
                "Describe post-change fingerprint verification without reading or hashing target files.");
            AddStep(
                workOrder,
                AdapterDeploymentWorkOrderStepKind::PreserveRollback,
                request.m_preview.m_backupRoot,
                request.m_preview.m_backupRoot,
                {},
                {},
                allEvidence,
                "Preserve the exact typed rollback description for later separately reviewed tooling.");

            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::ContractSatisfied,
                "Exact preview binding",
                "The work order is bound to one ready preview ID and lowercase SHA-256 fingerprint.",
                request.m_confirmation.m_evidenceIds);
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::ContractSatisfied,
                "Confirmation scope",
                "The named reviewer confirmation covers every change kind present in the exact preview.",
                request.m_confirmation.m_evidenceIds);
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::ContractSatisfied,
                "Maintenance window",
                "The deterministic evaluation time is inside the reviewed UTC maintenance window.",
                request.m_maintenanceWindow.m_evidenceIds);
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::ContractSatisfied,
                "Preflight evidence",
                "All required typed preflight evidence records are present, passed, current, and preview-bound.",
                allEvidence);
            if (!request.m_preview.m_backups.empty())
            {
                AddChecklistItem(
                    workOrder,
                    AdapterDeploymentChecklistState::OperatorActionRequired,
                    "Confirm backup destination",
                    "An operator must independently verify backup capacity, isolation, "
                    "and retention before any future execution.",
                    allEvidence);
            }
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::OperatorActionRequired,
                "Review every change",
                "An operator must review additions, replacements, removals, unchanged paths, and exact fingerprints.",
                request.m_confirmation.m_evidenceIds);
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::OperatorActionRequired,
                "Stop on drift",
                "Any identity, path, fingerprint, inventory, scope, time, or evidence "
                "drift invalidates this work order.",
                allEvidence);
            AddChecklistItem(
                workOrder,
                AdapterDeploymentChecklistState::OperatorActionRequired,
                "Retain rollback description",
                "Keep the exact inverse rollback steps available for a separately reviewed executor.",
                allEvidence);
        }

        bool HasExactWorkOrderCoverage(
            const AdapterStagingDeploymentPreview& preview,
            const AdapterDeploymentWorkOrder& workOrder)
        {
            size_t backupSteps = 0;
            size_t addSteps = 0;
            size_t replaceSteps = 0;
            size_t removeSteps = 0;
            for (const AdapterDeploymentWorkOrderStep& step :
                workOrder.m_steps)
            {
                switch (step.m_kind)
                {
                case AdapterDeploymentWorkOrderStepKind::Backup:
                    ++backupSteps;
                    break;
                case AdapterDeploymentWorkOrderStepKind::Add:
                    ++addSteps;
                    break;
                case AdapterDeploymentWorkOrderStepKind::Replace:
                    ++replaceSteps;
                    break;
                case AdapterDeploymentWorkOrderStepKind::Remove:
                    ++removeSteps;
                    break;
                default:
                    break;
                }
                if (step.m_executionAllowed)
                {
                    return false;
                }
            }
            return backupSteps == preview.m_backups.size()
                && addSteps == preview.m_additions.size()
                && replaceSteps == preview.m_replacements.size()
                && removeSteps == preview.m_removals.size()
                && preview.m_rollbackSteps.size()
                    == preview.m_additions.size()
                        + preview.m_replacements.size()
                        + preview.m_removals.size();
        }
    } // namespace

    AdapterDeploymentWorkOrderRegistry& AdapterDeploymentWorkOrderRegistry::Get()
    {
        static AdapterDeploymentWorkOrderRegistry registry;
        return registry;
    }

    bool AdapterDeploymentWorkOrderRegistry::RegisterRequest(
        const AdapterDeploymentWorkOrderRequest& request,
        AZStd::string* error)
    {
        if (!IsStableNamespacedId(request.m_confirmation.m_confirmationId)
            || request.m_preview.m_previewId.empty()
            || !IsSha256Fingerprint(request.m_previewFingerprint))
        {
            if (error)
            {
                *error =
                    "Deployment work-order registration requires stable confirmation "
                    "identity and exact preview fingerprint.";
            }
            return false;
        }
        for (const AdapterDeploymentWorkOrderRequest& existing : m_requests)
        {
            if (existing.m_confirmation.m_confirmationId
                    == request.m_confirmation.m_confirmationId
                || (existing.m_preview.m_previewId == request.m_preview.m_previewId
                    && existing.m_previewFingerprint
                        == request.m_previewFingerprint))
            {
                if (error)
                {
                    *error =
                        "A deployment work-order request for this confirmation or exact preview already exists.";
                }
                return false;
            }
        }
        m_requests.push_back(request);
        if (error)
        {
            error->clear();
        }
        return true;
    }

    void AdapterDeploymentWorkOrderRegistry::Clear()
    {
        m_requests.clear();
    }

    const AZStd::vector<AdapterDeploymentWorkOrderRequest>&
    AdapterDeploymentWorkOrderRegistry::GetRequests() const
    {
        return m_requests;
    }
} // namespace TaintedGrailModdingSDK
