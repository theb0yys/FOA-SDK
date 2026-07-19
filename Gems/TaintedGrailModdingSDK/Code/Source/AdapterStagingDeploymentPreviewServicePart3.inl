namespace TaintedGrailModdingSDK
{
    namespace
    {
        AZStd::string EscapeJson(const AZStd::string& value)
        {
            AZStd::string escaped;
            escaped.reserve(value.size() + 8);
            for (char character : value)
            {
                switch (character)
                {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
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
                    if (static_cast<unsigned char>(character) < 0x20)
                    {
                        const char hex[] = "0123456789abcdef";
                        escaped += "\\u00";
                        escaped.push_back(hex[(character >> 4) & 0x0f]);
                        escaped.push_back(hex[character & 0x0f]);
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

        void AppendJsonString(
            std::ostringstream& stream,
            const char* name,
            const AZStd::string& value,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << '"' << name << "\":\"" << EscapeJson(value).c_str() << '"';
        }

        void AppendJsonBool(
            std::ostringstream& stream,
            const char* name,
            bool value,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << '"' << name << "\":" << (value ? "true" : "false");
        }

        void AppendJsonUnsigned(
            std::ostringstream& stream,
            const char* name,
            AZ::u64 value,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << '"' << name << "\":"
                   << static_cast<unsigned long long>(value);
        }

        void AppendChangeObject(
            std::ostringstream& stream,
            const AdapterDeploymentChange& change)
        {
            stream << '{';
            bool first = true;
            AppendJsonString(stream, "ChangeId", change.m_changeId, first);
            AppendJsonString(stream, "Kind", ToString(change.m_kind), first);
            AppendJsonString(stream, "PackageEntryId", change.m_packageEntryId, first);
            AppendJsonString(stream, "TargetEntryId", change.m_targetEntryId, first);
            AppendJsonString(stream, "PackagePath", change.m_packagePath, first);
            AppendJsonString(stream, "TargetPath", change.m_targetPath, first);
            AppendJsonString(stream, "Role", change.m_role, first);
            AppendJsonString(stream, "MediaType", change.m_mediaType, first);
            AppendJsonString(stream, "PreviousFingerprint", change.m_previousFingerprint, first);
            AppendJsonString(stream, "DesiredFingerprint", change.m_desiredFingerprint, first);
            AppendJsonUnsigned(stream, "PreviousByteSize", change.m_previousByteSize, first);
            AppendJsonUnsigned(stream, "DesiredByteSize", change.m_desiredByteSize, first);
            AppendJsonBool(stream, "BackupRequired", change.m_backupRequired, first);
            AppendJsonString(stream, "Reason", change.m_reason, first);
            stream << '}';
        }

        void AppendChangeArray(
            std::ostringstream& stream,
            const char* name,
            const AZStd::vector<AdapterDeploymentChange>& changes,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << '"' << name << "\":[";
            for (size_t index = 0; index < changes.size(); ++index)
            {
                if (index > 0)
                {
                    stream << ',';
                }
                AppendChangeObject(stream, changes[index]);
            }
            stream << ']';
        }

        void AppendStringArray(
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
                stream << '"' << EscapeJson(values[index]).c_str() << '"';
            }
            stream << ']';
        }

        void AppendConflicts(
            std::ostringstream& stream,
            const AZStd::vector<AdapterDeploymentConflict>& conflicts,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << "\"Conflicts\":[";
            for (size_t index = 0; index < conflicts.size(); ++index)
            {
                if (index > 0)
                {
                    stream << ',';
                }
                const AdapterDeploymentConflict& conflict = conflicts[index];
                stream << '{';
                bool objectFirst = true;
                AppendJsonString(stream, "TargetPath", conflict.m_targetPath, objectFirst);
                AppendJsonString(stream, "PackageEntryId", conflict.m_packageEntryId, objectFirst);
                if (!objectFirst)
                {
                    stream << ',';
                }
                objectFirst = false;
                stream << "\"TargetEntryIds\":";
                AppendStringArray(stream, conflict.m_targetEntryIds);
                AppendJsonString(stream, "Reason", conflict.m_reason, objectFirst);
                stream << '}';
            }
            stream << ']';
        }

        void AppendBackups(
            std::ostringstream& stream,
            const AZStd::vector<AdapterDeploymentBackupRequirement>& backups,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << "\"Backups\":[";
            for (size_t index = 0; index < backups.size(); ++index)
            {
                if (index > 0)
                {
                    stream << ',';
                }
                const AdapterDeploymentBackupRequirement& backup = backups[index];
                stream << '{';
                bool objectFirst = true;
                AppendJsonString(stream, "BackupId", backup.m_backupId, objectFirst);
                AppendJsonString(stream, "TargetEntryId", backup.m_targetEntryId, objectFirst);
                AppendJsonString(stream, "TargetPath", backup.m_targetPath, objectFirst);
                AppendJsonString(stream, "BackupPath", backup.m_backupPath, objectFirst);
                AppendJsonString(stream, "Fingerprint", backup.m_fingerprint, objectFirst);
                AppendJsonUnsigned(stream, "ByteSize", backup.m_byteSize, objectFirst);
                AppendJsonString(stream, "Reason", backup.m_reason, objectFirst);
                stream << '}';
            }
            stream << ']';
        }

        void AppendRollbackSteps(
            std::ostringstream& stream,
            const AZStd::vector<AdapterDeploymentRollbackStep>& steps,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << "\"RollbackSteps\":[";
            for (size_t index = 0; index < steps.size(); ++index)
            {
                if (index > 0)
                {
                    stream << ',';
                }
                const AdapterDeploymentRollbackStep& step = steps[index];
                stream << '{';
                bool objectFirst = true;
                AppendJsonUnsigned(stream, "Sequence", step.m_sequence, objectFirst);
                AppendJsonString(stream, "StepId", step.m_stepId, objectFirst);
                AppendJsonString(stream, "Action", ToString(step.m_action), objectFirst);
                AppendJsonString(stream, "TargetPath", step.m_targetPath, objectFirst);
                AppendJsonString(stream, "BackupPath", step.m_backupPath, objectFirst);
                AppendJsonString(
                    stream,
                    "ExpectedDeployedFingerprint",
                    step.m_expectedDeployedFingerprint,
                    objectFirst);
                AppendJsonString(
                    stream,
                    "RestoreFingerprint",
                    step.m_restoreFingerprint,
                    objectFirst);
                AppendJsonString(stream, "Reason", step.m_reason, objectFirst);
                stream << '}';
            }
            stream << ']';
        }

        void AppendBlockers(
            std::ostringstream& stream,
            const AZStd::vector<AdapterDeploymentPreviewBlocker>& blockers,
            bool& first)
        {
            if (!first)
            {
                stream << ',';
            }
            first = false;
            stream << "\"Blockers\":[";
            for (size_t index = 0; index < blockers.size(); ++index)
            {
                if (index > 0)
                {
                    stream << ',';
                }
                const AdapterDeploymentPreviewBlocker& blocker = blockers[index];
                stream << '{';
                bool objectFirst = true;
                AppendJsonString(stream, "Code", blocker.m_code, objectFirst);
                AppendJsonString(stream, "TargetPath", blocker.m_targetPath, objectFirst);
                AppendJsonString(stream, "Reason", blocker.m_reason, objectFirst);
                stream << '}';
            }
            stream << ']';
        }
    } // namespace

    AZStd::string AdapterStagingDeploymentPreviewService::SerializeCanonicalPreview(
        const AdapterStagingDeploymentPreview& preview) const
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << '{';
        bool first = true;
        AppendJsonUnsigned(stream, "FormatVersion", preview.m_formatVersion, first);
        AppendJsonString(stream, "PreviewId", preview.m_previewId, first);
        AppendJsonString(stream, "PackagePreviewId", preview.m_packagePreviewId, first);
        AppendJsonString(
            stream,
            "PackagePreviewFingerprint",
            preview.m_packagePreviewFingerprint,
            first);
        AppendJsonString(stream, "TargetInventoryId", preview.m_targetInventoryId, first);
        AppendJsonString(
            stream,
            "TargetInventoryFingerprint",
            preview.m_targetInventoryFingerprint,
            first);
        AppendJsonString(stream, "PackId", preview.m_packId, first);
        AppendJsonString(stream, "PackageRoot", preview.m_packageRoot, first);
        AppendJsonString(stream, "TargetRoot", preview.m_targetRoot, first);
        AppendJsonString(stream, "BackupRoot", preview.m_backupRoot, first);
        AppendJsonString(stream, "Status", ToString(preview.m_status), first);
        AppendChangeArray(stream, "Additions", preview.m_additions, first);
        AppendChangeArray(stream, "Replacements", preview.m_replacements, first);
        AppendChangeArray(stream, "Removals", preview.m_removals, first);
        AppendChangeArray(stream, "Unchanged", preview.m_unchanged, first);
        AppendConflicts(stream, preview.m_conflicts, first);
        AppendBackups(stream, preview.m_backups, first);
        AppendRollbackSteps(stream, preview.m_rollbackSteps, first);
        AppendBlockers(stream, preview.m_blockers, first);
        AppendJsonBool(
            stream,
            "StagingMutationAllowed",
            preview.m_stagingMutationAllowed,
            first);
        AppendJsonBool(
            stream,
            "DeploymentMutationAllowed",
            preview.m_deploymentMutationAllowed,
            first);
        AppendJsonBool(
            stream,
            "RollbackExecutionAllowed",
            preview.m_rollbackExecutionAllowed,
            first);
        AppendJsonBool(stream, "LaunchAllowed", preview.m_launchAllowed, first);
        stream << '}';
        return stream.str().c_str();
    }
} // namespace TaintedGrailModdingSDK
