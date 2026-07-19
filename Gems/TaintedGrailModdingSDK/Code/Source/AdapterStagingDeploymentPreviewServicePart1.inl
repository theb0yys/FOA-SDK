namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class EnumType>
        struct EnumName
        {
            EnumType m_value;
            const char* m_name;
        };

        constexpr EnumName<AdapterDeploymentTargetReviewDecision> TargetReviewDecisions[] = {
            { AdapterDeploymentTargetReviewDecision::Unknown, "unknown" },
            { AdapterDeploymentTargetReviewDecision::Accepted, "accepted" },
            { AdapterDeploymentTargetReviewDecision::Rejected, "rejected" },
        };

        constexpr EnumName<AdapterDeploymentChangeKind> DeploymentChangeKinds[] = {
            { AdapterDeploymentChangeKind::Add, "add" },
            { AdapterDeploymentChangeKind::Replace, "replace" },
            { AdapterDeploymentChangeKind::Remove, "remove" },
            { AdapterDeploymentChangeKind::Unchanged, "unchanged" },
            { AdapterDeploymentChangeKind::Conflict, "conflict" },
        };

        constexpr EnumName<AdapterDeploymentRollbackAction> RollbackActions[] = {
            { AdapterDeploymentRollbackAction::RemoveAdded, "remove_added" },
            { AdapterDeploymentRollbackAction::RestoreReplaced, "restore_replaced" },
            { AdapterDeploymentRollbackAction::RestoreRemoved, "restore_removed" },
        };

        constexpr EnumName<AdapterStagingDeploymentPreviewStatus> DeploymentPreviewStatuses[] = {
            { AdapterStagingDeploymentPreviewStatus::Ready, "ready" },
            { AdapterStagingDeploymentPreviewStatus::PackageNotReady, "package_not_ready" },
            { AdapterStagingDeploymentPreviewStatus::TargetUnreviewed, "target_unreviewed" },
            { AdapterStagingDeploymentPreviewStatus::InventoryBindingMismatch,
                "inventory_binding_mismatch" },
            { AdapterStagingDeploymentPreviewStatus::InventoryUntrusted, "inventory_untrusted" },
            { AdapterStagingDeploymentPreviewStatus::PathInvalid, "path_invalid" },
            { AdapterStagingDeploymentPreviewStatus::Conflict, "conflict" },
            { AdapterStagingDeploymentPreviewStatus::BackupIncomplete, "backup_incomplete" },
            { AdapterStagingDeploymentPreviewStatus::RollbackIncomplete, "rollback_incomplete" },
        };

        template<class EnumType, size_t Count>
        AZStd::string EnumToString(
            EnumType value,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (name.m_value == value)
                {
                    return name.m_name;
                }
            }
            return "unknown";
        }

        template<class EnumType, size_t Count>
        bool TryParseEnum(
            const AZStd::string& value,
            EnumType& result,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (value == name.m_name)
                {
                    result = name.m_value;
                    return true;
                }
            }
            return false;
        }

        bool IsLowerHex(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'a' && value <= 'f');
        }

        bool IsLowerAlphaNumeric(char value)
        {
            return (value >= 'a' && value <= 'z')
                || (value >= '0' && value <= '9');
        }

        bool IsSha256Fingerprint(const AZStd::string& value)
        {
            constexpr size_t PrefixLength = 7;
            constexpr size_t DigestLength = 64;
            if (value.size() != PrefixLength + DigestLength
                || value.substr(0, PrefixLength) != "sha256:")
            {
                return false;
            }
            for (size_t index = PrefixLength; index < value.size(); ++index)
            {
                if (!IsLowerHex(value[index]))
                {
                    return false;
                }
            }
            return true;
        }

        bool IsStableNamespacedId(const AZStd::string& value)
        {
            if (value.size() < 3 || value.find('.') == AZStd::string::npos
                || !IsLowerAlphaNumeric(value.front())
                || !IsLowerAlphaNumeric(value.back()))
            {
                return false;
            }
            for (char character : value)
            {
                if (!IsLowerAlphaNumeric(character)
                    && character != '.'
                    && character != '-'
                    && character != '_')
                {
                    return false;
                }
            }
            return true;
        }

        bool IsSafeRelativePath(const AZStd::string& value)
        {
            if (value.empty() || value.front() == '/' || value.front() == '\\')
            {
                return false;
            }
            if (value.size() > 1 && value[1] == ':')
            {
                return false;
            }

            size_t start = 0;
            while (start <= value.size())
            {
                const size_t end = value.find('/', start);
                const size_t length = end == AZStd::string::npos
                    ? value.size() - start
                    : end - start;
                if (length == 0)
                {
                    return false;
                }
                const AZStd::string component = value.substr(start, length);
                if (component == "." || component == "..")
                {
                    return false;
                }
                for (char character : component)
                {
                    const unsigned char byte = static_cast<unsigned char>(character);
                    if (byte <= 0x20 || byte == 0x7f
                        || character == '\\' || character == ':')
                    {
                        return false;
                    }
                }
                if (end == AZStd::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return true;
        }

        bool PathIsInsideRoot(
            const AZStd::string& root,
            const AZStd::string& path)
        {
            if (!IsSafeRelativePath(root) || !IsSafeRelativePath(path))
            {
                return false;
            }
            if (path == root)
            {
                return true;
            }
            AZStd::string normalizedRoot = root;
            if (normalizedRoot.back() != '/')
            {
                normalizedRoot.push_back('/');
            }
            return path.size() > normalizedRoot.size()
                && path.substr(0, normalizedRoot.size()) == normalizedRoot;
        }

        template<class ValueType>
        bool ContainsValue(
            const AZStd::vector<ValueType>& values,
            const ValueType& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        void AddBlocker(
            AZStd::vector<AdapterDeploymentPreviewBlocker>& blockers,
            AZStd::string code,
            AZStd::string targetPath,
            AZStd::string reason)
        {
            for (const AdapterDeploymentPreviewBlocker& blocker : blockers)
            {
                if (blocker.m_code == code
                    && blocker.m_targetPath == targetPath
                    && blocker.m_reason == reason)
                {
                    return;
                }
            }
            AdapterDeploymentPreviewBlocker blocker;
            blocker.m_code = AZStd::move(code);
            blocker.m_targetPath = AZStd::move(targetPath);
            blocker.m_reason = AZStd::move(reason);
            blockers.push_back(AZStd::move(blocker));
        }

        void AddConflict(
            AdapterStagingDeploymentPreview& preview,
            const AZStd::string& targetPath,
            const AZStd::string& packageEntryId,
            AZStd::vector<AZStd::string> targetEntryIds,
            AZStd::string reason)
        {
            SortUnique(targetEntryIds);
            for (const AdapterDeploymentConflict& conflict : preview.m_conflicts)
            {
                if (conflict.m_targetPath == targetPath
                    && conflict.m_packageEntryId == packageEntryId
                    && conflict.m_targetEntryIds == targetEntryIds
                    && conflict.m_reason == reason)
                {
                    return;
                }
            }
            AdapterDeploymentConflict conflict;
            conflict.m_targetPath = targetPath;
            conflict.m_packageEntryId = packageEntryId;
            conflict.m_targetEntryIds = AZStd::move(targetEntryIds);
            conflict.m_reason = AZStd::move(reason);
            preview.m_conflicts.push_back(AZStd::move(conflict));
        }

        AZStd::vector<const AdapterDeploymentTargetEntry*> FindTargetEntries(
            const AdapterDeploymentTargetInventory& inventory,
            const AZStd::string& targetPath)
        {
            AZStd::vector<const AdapterDeploymentTargetEntry*> result;
            for (const AdapterDeploymentTargetEntry& entry : inventory.m_entries)
            {
                if (entry.m_targetPath == targetPath)
                {
                    result.push_back(&entry);
                }
            }
            return result;
        }

        const AdapterPackageLayoutEntry* FindPackageEntry(
            const AdapterPackageAssemblyPreview& packagePreview,
            const AZStd::string& packagePath)
        {
            for (const AdapterPackageLayoutEntry& entry : packagePreview.m_layout)
            {
                if (entry.m_packagePath == packagePath)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        AZStd::string MakeChangeId(
            AdapterDeploymentChangeKind kind,
            const AZStd::string& targetPath)
        {
            return AZStd::string::format(
                "deployment.change:%s:%s",
                ToString(kind).c_str(),
                targetPath.c_str());
        }

        AZStd::string MakeBackupPath(
            const AZStd::string& backupRoot,
            const AZStd::string& targetPath)
        {
            return backupRoot + "/" + targetPath;
        }

        AdapterDeploymentChange MakeChange(
            AdapterDeploymentChangeKind kind,
            const AdapterPackageLayoutEntry* packageEntry,
            const AdapterDeploymentTargetEntry* targetEntry,
            const AZStd::string& targetPath,
            AZStd::string reason)
        {
            AdapterDeploymentChange change;
            change.m_kind = kind;
            change.m_changeId = MakeChangeId(kind, targetPath);
            change.m_targetPath = targetPath;
            change.m_reason = AZStd::move(reason);
            if (packageEntry)
            {
                change.m_packageEntryId = packageEntry->m_inventoryEntryId;
                change.m_packagePath = packageEntry->m_packagePath;
                change.m_role = packageEntry->m_role;
                change.m_mediaType = packageEntry->m_mediaType;
                change.m_desiredFingerprint = packageEntry->m_outputDigest;
                change.m_desiredByteSize = packageEntry->m_byteSize;
            }
            if (targetEntry)
            {
                change.m_targetEntryId = targetEntry->m_entryId;
                if (change.m_role.empty())
                {
                    change.m_role = targetEntry->m_role;
                }
                if (change.m_mediaType.empty())
                {
                    change.m_mediaType = targetEntry->m_mediaType;
                }
                change.m_previousFingerprint = targetEntry->m_fingerprint;
                change.m_previousByteSize = targetEntry->m_byteSize;
            }
            return change;
        }

        void SortChanges(AZStd::vector<AdapterDeploymentChange>& changes)
        {
            AZStd::sort(
                changes.begin(),
                changes.end(),
                [](const AdapterDeploymentChange& left,
                    const AdapterDeploymentChange& right)
                {
                    if (left.m_targetPath != right.m_targetPath)
                    {
                        return left.m_targetPath < right.m_targetPath;
                    }
                    return left.m_changeId < right.m_changeId;
                });
        }

        void SortPreviewCollections(AdapterStagingDeploymentPreview& preview)
        {
            SortChanges(preview.m_additions);
            SortChanges(preview.m_replacements);
            SortChanges(preview.m_removals);
            SortChanges(preview.m_unchanged);
            AZStd::sort(
                preview.m_conflicts.begin(),
                preview.m_conflicts.end(),
                [](const AdapterDeploymentConflict& left,
                    const AdapterDeploymentConflict& right)
                {
                    if (left.m_targetPath != right.m_targetPath)
                    {
                        return left.m_targetPath < right.m_targetPath;
                    }
                    return left.m_reason < right.m_reason;
                });
            for (AdapterDeploymentConflict& conflict : preview.m_conflicts)
            {
                SortUnique(conflict.m_targetEntryIds);
            }
            AZStd::sort(
                preview.m_backups.begin(),
                preview.m_backups.end(),
                [](const AdapterDeploymentBackupRequirement& left,
                    const AdapterDeploymentBackupRequirement& right)
                {
                    if (left.m_targetPath != right.m_targetPath)
                    {
                        return left.m_targetPath < right.m_targetPath;
                    }
                    return left.m_backupId < right.m_backupId;
                });
            AZStd::sort(
                preview.m_rollbackSteps.begin(),
                preview.m_rollbackSteps.end(),
                [](const AdapterDeploymentRollbackStep& left,
                    const AdapterDeploymentRollbackStep& right)
                {
                    if (left.m_sequence != right.m_sequence)
                    {
                        return left.m_sequence < right.m_sequence;
                    }
                    return left.m_stepId < right.m_stepId;
                });
            AZStd::sort(
                preview.m_blockers.begin(),
                preview.m_blockers.end(),
                [](const AdapterDeploymentPreviewBlocker& left,
                    const AdapterDeploymentPreviewBlocker& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_targetPath != right.m_targetPath)
                    {
                        return left.m_targetPath < right.m_targetPath;
                    }
                    return left.m_reason < right.m_reason;
                });
        }
    } // namespace

    AZStd::string ToString(AdapterDeploymentTargetReviewDecision decision)
    {
        return EnumToString(decision, TargetReviewDecisions);
    }

    AZStd::string ToString(AdapterDeploymentChangeKind kind)
    {
        return EnumToString(kind, DeploymentChangeKinds);
    }

    AZStd::string ToString(AdapterDeploymentRollbackAction action)
    {
        return EnumToString(action, RollbackActions);
    }

    AZStd::string ToString(AdapterStagingDeploymentPreviewStatus status)
    {
        return EnumToString(status, DeploymentPreviewStatuses);
    }

    bool TryParseAdapterDeploymentTargetReviewDecision(
        const AZStd::string& value,
        AdapterDeploymentTargetReviewDecision& decision)
    {
        return TryParseEnum(value, decision, TargetReviewDecisions);
    }

    bool TryParseAdapterDeploymentChangeKind(
        const AZStd::string& value,
        AdapterDeploymentChangeKind& kind)
    {
        return TryParseEnum(value, kind, DeploymentChangeKinds);
    }

    bool TryParseAdapterDeploymentRollbackAction(
        const AZStd::string& value,
        AdapterDeploymentRollbackAction& action)
    {
        return TryParseEnum(value, action, RollbackActions);
    }

    bool TryParseAdapterStagingDeploymentPreviewStatus(
        const AZStd::string& value,
        AdapterStagingDeploymentPreviewStatus& status)
    {
        return TryParseEnum(value, status, DeploymentPreviewStatuses);
    }

    AdapterStagingDeploymentPreviewRegistry& AdapterStagingDeploymentPreviewRegistry::Get()
    {
        static AdapterStagingDeploymentPreviewRegistry registry;
        return registry;
    }

    bool AdapterStagingDeploymentPreviewRegistry::RegisterRequest(
        const AdapterStagingDeploymentPreviewRequest& request,
        AZStd::string* error)
    {
        if (request.m_packagePreview.m_previewId.empty()
            || request.m_targetInventory.m_inventoryId.empty())
        {
            if (error)
            {
                *error = "A package-preview ID and target-inventory ID are required.";
            }
            return false;
        }
        for (const AdapterStagingDeploymentPreviewRequest& existing : m_requests)
        {
            if (existing.m_packagePreview.m_previewId
                    == request.m_packagePreview.m_previewId
                && existing.m_targetInventory.m_inventoryId
                    == request.m_targetInventory.m_inventoryId)
            {
                if (error)
                {
                    *error = "A staging/deployment preview request for this package and target inventory already exists.";
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

    void AdapterStagingDeploymentPreviewRegistry::Clear()
    {
        m_requests.clear();
    }

    const AZStd::vector<AdapterStagingDeploymentPreviewRequest>&
    AdapterStagingDeploymentPreviewRegistry::GetRequests() const
    {
        return m_requests;
    }
} // namespace TaintedGrailModdingSDK
