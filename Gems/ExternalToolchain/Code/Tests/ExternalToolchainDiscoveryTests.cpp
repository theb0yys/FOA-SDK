/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainConfigurationService.h"
#include "ExternalToolchainDiscoveryService.h"

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

namespace ExternalToolchain
{
    namespace
    {
        class FakeSettingsSource final
            : public ExternalToolchainSettingsSource
        {
        public:
            bool GetString(
                const AZStd::string& path,
                AZStd::string& value) const override
            {
                const auto found = m_strings.find(path);
                if (found == m_strings.end())
                {
                    return false;
                }
                value = found->second;
                return true;
            }

            bool GetBool(
                const AZStd::string& path,
                bool& value) const override
            {
                const auto found = m_bools.find(path);
                if (found == m_bools.end())
                {
                    return false;
                }
                value = found->second;
                return true;
            }

            bool GetUInt64(
                const AZStd::string& path,
                AZ::u64& value) const override
            {
                const auto found = m_uint64.find(path);
                if (found == m_uint64.end())
                {
                    return false;
                }
                value = found->second;
                return true;
            }

            AZStd::unordered_map<AZStd::string, AZStd::string> m_strings;
            AZStd::unordered_map<AZStd::string, bool> m_bools;
            AZStd::unordered_map<AZStd::string, AZ::u64> m_uint64;
        };

        class FakePathProbe final
            : public ExternalToolPathProbe
        {
        public:
            ExternalToolPathObservation Inspect(
                const AZStd::string& path) const override
            {
                ++m_inspectionCount;
                const auto found = m_observations.find(path);
                if (found != m_observations.end())
                {
                    return found->second;
                }
                return ExternalToolPathObservation{
                    false,
                    false,
                    "Path does not exist." };
            }

            mutable AZ::u64 m_inspectionCount = 0;
            AZStd::unordered_map<AZStd::string, ExternalToolPathObservation>
                m_observations;
        };

        ExternalToolProviderDescriptor MakeProvider(
            AZStd::string providerId = "fixture.discovery")
        {
            ExternalToolConfigurationDescriptor path;
            path.m_key = "executable-path";
            path.m_displayName = "Executable path";
            path.m_kind = ConfigurationValueKind::Path;
            path.m_defaultValue = "C:/Tools/tool.exe";
            path.m_required = true;

            ExternalToolConfigurationDescriptor version;
            version.m_key = "tool-version";
            version.m_displayName = "Tool version";
            version.m_kind = ConfigurationValueKind::SemanticVersion;
            version.m_defaultValue = "3.2.0";

            ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = "configured-executable";
            probe.m_kind = DiscoveryProbeKind::File;
            probe.m_pathConfigurationKey = path.m_key;
            probe.m_versionConfigurationKey = version.m_key;
            probe.m_minimumSupportedVersion = "3.0.0";
            probe.m_maximumSupportedVersion = "4.0.0";
            probe.m_platforms = { "windows" };

            ExternalToolProviderDescriptor provider;
            provider.m_providerId = AZStd::move(providerId);
            provider.m_platforms = { "windows", "linux" };
            provider.m_configuration = { path, version };
            provider.m_discoveryProbes = { probe };
            provider.m_enabledByDefault = true;
            return provider;
        }

        ExternalToolDiscoveryResult RefreshOne(
            const ExternalToolProviderDescriptor& provider,
            FakeSettingsSource& settings,
            FakePathProbe& pathProbe,
            ProviderOperationResult* operation = nullptr)
        {
            ExternalToolchainConfigurationService configuration(settings);
            ExternalToolchainDiscoveryService discovery(pathProbe, settings);
            const ProviderOperationResult result = discovery.Refresh(
                { provider },
                configuration,
                "windows");
            if (operation)
            {
                *operation = result;
            }
            EXPECT_TRUE(result.m_success);
            EXPECT_EQ(discovery.GetResults().size(), 1);
            return discovery.GetResults().front();
        }
    } // namespace

    TEST(ExternalToolchainDiscoveryTests, ExistingConfiguredFileIsInstalled)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        paths.m_observations["C:/Tools/tool.exe"] = {
            true,
            false,
            "File exists." };

        const ExternalToolDiscoveryResult result =
            RefreshOne(MakeProvider(), settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Installed);
        EXPECT_EQ(result.m_selectedPath, "C:/Tools/tool.exe");
        EXPECT_EQ(result.m_selectedVersion, "3.2.0");
    }

    TEST(ExternalToolchainDiscoveryTests, MissingPathIsNotInstalled)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        const ExternalToolDiscoveryResult result =
            RefreshOne(MakeProvider(), settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::NotInstalled);
    }

    TEST(ExternalToolchainDiscoveryTests, WrongPathKindIsMisconfigured)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        paths.m_observations["C:/Tools/tool.exe"] = {
            true,
            true,
            "Directory exists." };
        const ExternalToolDiscoveryResult result =
            RefreshOne(MakeProvider(), settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Misconfigured);
    }

    TEST(ExternalToolchainDiscoveryTests, UnsupportedConfiguredVersionIsReported)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_configuration[1].m_defaultValue = "5.0.0";
        paths.m_observations["C:/Tools/tool.exe"] = {
            true,
            false,
            "File exists." };
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::UnsupportedVersion);
    }

    TEST(ExternalToolchainDiscoveryTests, DisabledProviderIsNotInspected)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_enabledByDefault = false;
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Disabled);
        EXPECT_EQ(paths.m_inspectionCount, 0);
    }

    TEST(ExternalToolchainDiscoveryTests, UnsupportedPlatformIsNotInspected)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_platforms = { "linux" };
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::UnsupportedPlatform);
        EXPECT_EQ(paths.m_inspectionCount, 0);
    }

    TEST(ExternalToolchainDiscoveryTests, RequiredEmptyPathIsMisconfigured)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_configuration[0].m_defaultValue.clear();
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Misconfigured);
        EXPECT_EQ(paths.m_inspectionCount, 0);
    }

    TEST(ExternalToolchainDiscoveryTests, NetworkPathIsRejectedWithoutInspection)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_configuration[0].m_defaultValue = "//server/share/tool.exe";
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Misconfigured);
        EXPECT_EQ(paths.m_inspectionCount, 0);
    }

    TEST(ExternalToolchainDiscoveryTests, ParentTraversalIsRejectedWithoutInspection)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_configuration[0].m_defaultValue = "C:/Tools/../tool.exe";
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Misconfigured);
        EXPECT_EQ(paths.m_inspectionCount, 0);
    }

    TEST(ExternalToolchainDiscoveryTests, DuplicateNormalizedCandidatePathIsInspectedOnce)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();

        ExternalToolConfigurationDescriptor alternatePath =
            provider.m_configuration.front();
        alternatePath.m_key = "alternate-path";
        alternatePath.m_displayName = "Alternate path";
        alternatePath.m_defaultValue = "c:/tools/tool.exe";
        provider.m_configuration.push_back(alternatePath);

        ExternalToolDiscoveryProbeDescriptor alternateProbe =
            provider.m_discoveryProbes.front();
        alternateProbe.m_probeId = "alternate-executable";
        alternateProbe.m_pathConfigurationKey = alternatePath.m_key;
        provider.m_discoveryProbes.push_back(alternateProbe);

        paths.m_observations["C:/Tools/tool.exe"] = {
            true,
            false,
            "File exists." };
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Installed);
        EXPECT_EQ(paths.m_inspectionCount, 1);
        EXPECT_EQ(result.m_candidates.size(), 1);
    }

    TEST(ExternalToolchainDiscoveryTests, MultipleCompatibleInstallationsAreAmbiguous)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();

        ExternalToolConfigurationDescriptor alternatePath =
            provider.m_configuration.front();
        alternatePath.m_key = "alternate-path";
        alternatePath.m_displayName = "Alternate path";
        alternatePath.m_defaultValue = "C:/Other/tool.exe";
        provider.m_configuration.push_back(alternatePath);

        ExternalToolDiscoveryProbeDescriptor alternateProbe =
            provider.m_discoveryProbes.front();
        alternateProbe.m_probeId = "alternate-executable";
        alternateProbe.m_pathConfigurationKey = alternatePath.m_key;
        provider.m_discoveryProbes.push_back(alternateProbe);

        paths.m_observations["C:/Tools/tool.exe"] = {
            true,
            false,
            "File exists." };
        paths.m_observations["C:/Other/tool.exe"] = {
            true,
            false,
            "File exists." };
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Ambiguous);
        EXPECT_TRUE(result.m_selectedPath.empty());
        EXPECT_EQ(paths.m_inspectionCount, 2);
    }

    TEST(ExternalToolchainDiscoveryTests, FailedRequiredProbeBlocksOptionalInstalledCandidate)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolProviderDescriptor provider = MakeProvider();
        provider.m_configuration[0].m_defaultValue = "C:/Missing/tool.exe";

        ExternalToolConfigurationDescriptor optionalPath =
            provider.m_configuration.front();
        optionalPath.m_key = "optional-path";
        optionalPath.m_displayName = "Optional path";
        optionalPath.m_defaultValue = "C:/Other/tool.exe";
        optionalPath.m_required = false;
        provider.m_configuration.push_back(optionalPath);

        ExternalToolDiscoveryProbeDescriptor optionalProbe =
            provider.m_discoveryProbes.front();
        optionalProbe.m_probeId = "optional-executable";
        optionalProbe.m_pathConfigurationKey = optionalPath.m_key;
        optionalProbe.m_required = false;
        provider.m_discoveryProbes.push_back(optionalProbe);

        paths.m_observations["C:/Other/tool.exe"] = {
            true,
            false,
            "File exists." };
        const ExternalToolDiscoveryResult result =
            RefreshOne(provider, settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::NotInstalled);
        EXPECT_TRUE(result.m_selectedPath.empty());
    }

    TEST(ExternalToolchainDiscoveryTests, HostDiscoveryCanBeDisabled)
    {
        FakeSettingsSource settings;
        settings.m_bools[
            "/O3DE/ExternalToolchain/Host/Discovery/Enabled"] = false;
        FakePathProbe paths;
        const ExternalToolDiscoveryResult result =
            RefreshOne(MakeProvider(), settings, paths);
        EXPECT_EQ(result.m_status, DiscoveryStatus::Disabled);
        EXPECT_EQ(paths.m_inspectionCount, 0);
    }

    TEST(ExternalToolchainDiscoveryTests, ProviderCountLimitFailsClosed)
    {
        FakeSettingsSource settings;
        settings.m_uint64[
            "/O3DE/ExternalToolchain/Host/Discovery/MaximumProviders"] = 1;
        FakePathProbe paths;
        ExternalToolchainConfigurationService configuration(settings);
        ExternalToolchainDiscoveryService discovery(paths, settings);

        const ProviderOperationResult operation = discovery.Refresh(
            { MakeProvider("fixture.alpha"), MakeProvider("fixture.zulu") },
            configuration,
            "windows");
        EXPECT_FALSE(operation.m_success);
        EXPECT_TRUE(discovery.GetResults().empty());
    }

    TEST(ExternalToolchainDiscoveryTests, ResultsAreOrderedByProviderIdentity)
    {
        FakeSettingsSource settings;
        FakePathProbe paths;
        ExternalToolchainConfigurationService configuration(settings);
        ExternalToolchainDiscoveryService discovery(paths, settings);

        ASSERT_TRUE(discovery.Refresh(
            { MakeProvider("fixture.zulu"), MakeProvider("fixture.alpha") },
            configuration,
            "windows").m_success);
        ASSERT_EQ(discovery.GetResults().size(), 2);
        EXPECT_EQ(discovery.GetResults()[0].m_providerId, "fixture.alpha");
        EXPECT_EQ(discovery.GetResults()[1].m_providerId, "fixture.zulu");
    }
} // namespace ExternalToolchain
