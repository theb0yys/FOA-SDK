namespace TaintedGrailModdingSDK
{
    namespace
    {
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

        AZStd::string BuildManifestPathIdentity(const AZStd::string& value)
        {
            PackagePathIdentity identity;
            return TryBuildPackagePathIdentity(value, identity)
                ? identity.m_windowsIdentity
                : value;
        }

        bool HasDuplicateOutputPaths(
            const AZStd::vector<AdapterBuildExpectedOutput>& outputs)
        {
            AZStd::vector<AZStd::string> identities;
            for (const AdapterBuildExpectedOutput& output : outputs)
            {
                PackagePathIdentity identity;
                if (!TryBuildPackagePathIdentity(
                        output.m_relativePath,
                        identity))
                {
                    return true;
                }
                if (AZStd::find(
                        identities.begin(),
                        identities.end(),
                        identity.m_windowsIdentity)
                    != identities.end())
                {
                    return true;
                }
                identities.push_back(identity.m_windowsIdentity);
            }
            return false;
        }

        void SortOutputs(
            AZStd::vector<AdapterBuildExpectedOutput>& outputs)
        {
            AZStd::sort(
                outputs.begin(),
                outputs.end(),
                [](const AdapterBuildExpectedOutput& left,
                   const AdapterBuildExpectedOutput& right)
                {
                    const AZStd::string leftIdentity =
                        BuildManifestPathIdentity(left.m_relativePath);
                    const AZStd::string rightIdentity =
                        BuildManifestPathIdentity(right.m_relativePath);
                    if (leftIdentity != rightIdentity)
                    {
                        return leftIdentity < rightIdentity;
                    }
                    if (left.m_role != right.m_role)
                    {
                        return left.m_role < right.m_role;
                    }
                    return left.m_mediaType < right.m_mediaType;
                });
        }
    } // namespace
} // namespace TaintedGrailModdingSDK
