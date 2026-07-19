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

        constexpr EnumName<AdapterBuildManifestReviewDecision> ReviewDecisions[] = {
            { AdapterBuildManifestReviewDecision::Unknown, "unknown" },
            { AdapterBuildManifestReviewDecision::Accepted, "accepted" },
            { AdapterBuildManifestReviewDecision::Rejected, "rejected" },
        };

        constexpr EnumName<AdapterPackageAssemblyPreviewStatus> PreviewStatuses[] = {
            { AdapterPackageAssemblyPreviewStatus::Ready, "ready" },
            { AdapterPackageAssemblyPreviewStatus::ManifestNotReady, "manifest_not_ready" },
            { AdapterPackageAssemblyPreviewStatus::ManifestUnreviewed, "manifest_unreviewed" },
            { AdapterPackageAssemblyPreviewStatus::InventoryBindingMismatch,
                "inventory_binding_mismatch" },
            { AdapterPackageAssemblyPreviewStatus::InventoryUntrusted, "inventory_untrusted" },
            { AdapterPackageAssemblyPreviewStatus::OutputMissing, "output_missing" },
            { AdapterPackageAssemblyPreviewStatus::FingerprintMissing, "fingerprint_missing" },
            { AdapterPackageAssemblyPreviewStatus::PathInvalid, "path_invalid" },
            { AdapterPackageAssemblyPreviewStatus::Collision, "collision" },
            { AdapterPackageAssemblyPreviewStatus::RedistributionBlocked,
                "redistribution_blocked" },
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
            return IsSafePackageRelativePath(value);
        }

        bool PathIsInsideRoot(
            const AZStd::string& root,
            const AZStd::string& path)
        {
            return IsPackagePathInsideRoot(root, path);
        }

        bool PackagePathsEqual(
            const AZStd::string& left,
            const AZStd::string& right)
        {
            PackagePathIdentity leftIdentity;
            PackagePathIdentity rightIdentity;
            return TryBuildPackagePathIdentity(left, leftIdentity)
                && TryBuildPackagePathIdentity(right, rightIdentity)
                && leftIdentity.m_windowsIdentity
                    == rightIdentity.m_windowsIdentity;
        }

        AZStd::string PackagePathSortIdentity(const AZStd::string& path)
        {
            PackagePathIdentity identity;
            return TryBuildPackagePathIdentity(path, identity)
                ? identity.m_windowsIdentity
                : path;
        }

        template<class ValueType>
        bool ContainsValue(
            const AZStd::vector<ValueType>& values,
            const ValueType& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        void AddBlocker(
            AZStd::vector<AdapterPackageAssemblyBlocker>& blockers,
            AZStd::string code,
            AZStd::string packagePath,
            AZStd::string reason)
        {
            for (const AdapterPackageAssemblyBlocker& blocker : blockers)
            {
                if (blocker.m_code == code
                    && PackagePathsEqual(blocker.m_packagePath, packagePath)
                    && blocker.m_reason == reason)
                {
                    return;
                }
            }
            AdapterPackageAssemblyBlocker blocker;
            blocker.m_code = AZStd::move(code);
            blocker.m_packagePath = AZStd::move(packagePath);
            blocker.m_reason = AZStd::move(reason);
            blockers.push_back(AZStd::move(blocker));
        }

        void AddOmission(
            AZStd::vector<AdapterPackageAssemblyOmission>& omissions,
            const AdapterBuildExpectedOutput& output,
            AZStd::string reason)
        {
            AdapterPackageAssemblyOmission omission;
            omission.m_expectedPath = output.m_relativePath;
            omission.m_role = output.m_role;
            omission.m_reason = AZStd::move(reason);
            omissions.push_back(AZStd::move(omission));
        }

        const AdapterBuildExpectedOutput* FindExpectedOutput(
            const AdapterBuildManifest& manifest,
            const AZStd::string& packagePath)
        {
            for (const AdapterBuildExpectedOutput& output : manifest.m_expectedOutputs)
            {
                if (PackagePathsEqual(output.m_relativePath, packagePath))
                {
                    return &output;
                }
            }
            return nullptr;
        }

        AZStd::vector<const AdapterStagingInventoryEntry*> FindIncludedEntries(
            const AdapterStagingInventory& inventory,
            const AZStd::string& packagePath)
        {
            AZStd::vector<const AdapterStagingInventoryEntry*> matches;
            for (const AdapterStagingInventoryEntry& entry : inventory.m_entries)
            {
                if (entry.m_includeInPackage
                    && PackagePathsEqual(entry.m_packagePath, packagePath))
                {
                    matches.push_back(&entry);
                }
            }
            return matches;
        }

        void SortPreviewCollections(AdapterPackageAssemblyPreview& preview)
        {
            AZStd::sort(
                preview.m_layout.begin(),
                preview.m_layout.end(),
                [](const AdapterPackageLayoutEntry& left,
                    const AdapterPackageLayoutEntry& right)
                {
                    const AZStd::string leftPath =
                        PackagePathSortIdentity(left.m_packagePath);
                    const AZStd::string rightPath =
                        PackagePathSortIdentity(right.m_packagePath);
                    if (leftPath != rightPath)
                    {
                        return leftPath < rightPath;
                    }
                    return PackagePathSortIdentity(left.m_stagingPath)
                        < PackagePathSortIdentity(right.m_stagingPath);
                });
            AZStd::sort(
                preview.m_omissions.begin(),
                preview.m_omissions.end(),
                [](const AdapterPackageAssemblyOmission& left,
                    const AdapterPackageAssemblyOmission& right)
                {
                    const AZStd::string leftPath =
                        PackagePathSortIdentity(left.m_expectedPath);
                    const AZStd::string rightPath =
                        PackagePathSortIdentity(right.m_expectedPath);
                    if (leftPath != rightPath)
                    {
                        return leftPath < rightPath;
                    }
                    return left.m_reason < right.m_reason;
                });
            AZStd::sort(
                preview.m_collisions.begin(),
                preview.m_collisions.end(),
                [](const AdapterPackageAssemblyCollision& left,
                    const AdapterPackageAssemblyCollision& right)
                {
                    return PackagePathSortIdentity(left.m_packagePath)
                        < PackagePathSortIdentity(right.m_packagePath);
                });
            for (AdapterPackageAssemblyCollision& collision : preview.m_collisions)
            {
                AZStd::sort(
                    collision.m_inventoryEntryIds.begin(),
                    collision.m_inventoryEntryIds.end());
            }
            AZStd::sort(
                preview.m_blockers.begin(),
                preview.m_blockers.end(),
                [](const AdapterPackageAssemblyBlocker& left,
                    const AdapterPackageAssemblyBlocker& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    const AZStd::string leftPath =
                        PackagePathSortIdentity(left.m_packagePath);
                    const AZStd::string rightPath =
                        PackagePathSortIdentity(right.m_packagePath);
                    if (leftPath != rightPath)
                    {
                        return leftPath < rightPath;
                    }
                    return left.m_reason < right.m_reason;
                });
        }
    } // namespace

    AZStd::string ToString(AdapterBuildManifestReviewDecision decision)
    {
        return EnumToString(decision, ReviewDecisions);
    }

    AZStd::string ToString(AdapterPackageAssemblyPreviewStatus status)
    {
        return EnumToString(status, PreviewStatuses);
    }

    bool TryParseAdapterBuildManifestReviewDecision(
        const AZStd::string& value,
        AdapterBuildManifestReviewDecision& decision)
    {
        return TryParseEnum(value, decision, ReviewDecisions);
    }

    bool TryParseAdapterPackageAssemblyPreviewStatus(
        const AZStd::string& value,
        AdapterPackageAssemblyPreviewStatus& status)
    {
        return TryParseEnum(value, status, PreviewStatuses);
    }
} // namespace TaintedGrailModdingSDK
