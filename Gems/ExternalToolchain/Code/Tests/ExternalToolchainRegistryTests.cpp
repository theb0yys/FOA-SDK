/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainRegistry.h"

#include <AzCore/std/utility/move.h>
#include <AzTest/AzTest.h>

namespace ExternalToolchain
{
    namespace
    {
        ExternalToolProviderDescriptor MakeProvider(AZStd::string providerId)
        {
            ExternalToolCommandDescriptor command;
            command.m_commandId = "launch";
            command.m_displayName = "Launch";
            command.m_mode = CommandMode::Interactive;

            ExternalToolConfigurationDescriptor path;
            path.m_key = "executable-path";
            path.m_displayName = "Executable path";
            path.m_kind = ConfigurationValueKind::Path;
            path.m_required = true;

            ExternalToolConfigurationDescriptor version;
            version.m_key = "tool-version";
            version.m_displayName = "Tool version";
            version.m_kind = ConfigurationValueKind::SemanticVersion;

            ExternalToolDiscoveryProbeDescriptor probe;
            probe.m_probeId = "configured-executable";
            probe.m_kind = DiscoveryProbeKind::File;
            probe.m_pathConfigurationKey = path.m_key;
            probe.m_versionConfigurationKey = version.m_key;
            probe.m_platforms = { "windows" };

            ExternalToolProviderDescriptor provider;
            provider.m_providerId = AZStd::move(providerId);
            provider.m_displayName = "Fixture Provider";
            provider.m_providerVersion = "1.2.3";
            provider.m_minimumHostApiVersion = { 1, 0, 0 };
            provider.m_toolFamily = ToolFamily::Utility;
            provider.m_platforms = { "windows", "linux" };
            provider.m_capabilities.m_supportsInteractive = true;
            provider.m_commands = { command };
            provider.m_configuration = { version, path };
            provider.m_discoveryProbes = { probe };
            return provider;
        }
    } // namespace

    TEST(ExternalToolchainRegistryTests, ValidProviderRegisters)
    {
        ExternalToolchainRegistry registry;
        const ProviderOperationResult result =
            registry.RegisterProvider(MakeProvider("fixture.valid"));

        EXPECT_TRUE(result.m_success);
        ASSERT_EQ(registry.GetProviders().size(), 1);
        EXPECT_EQ(registry.GetProviders()[0].m_providerId, "fixture.valid");
    }

    TEST(ExternalToolchainRegistryTests, DuplicateProviderIsRejected)
    {
        ExternalToolchainRegistry registry;
        EXPECT_TRUE(registry.RegisterProvider(MakeProvider("fixture.duplicate")).m_success);
        EXPECT_FALSE(
            registry.RegisterProvider(MakeProvider("fixture.duplicate")).m_success);
    }

    TEST(ExternalToolchainRegistryTests, InvalidProviderIdentityIsRejected)
    {
        ExternalToolchainRegistry registry;
        EXPECT_FALSE(
            registry.RegisterProvider(MakeProvider("NotNamespaced")).m_success);
    }

    TEST(ExternalToolchainRegistryTests, InvalidSemanticVersionIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.version");
        provider.m_providerVersion = "01.0.0";
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, FutureHostApiIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.future");
        provider.m_minimumHostApiVersion = { 1, 2, 0 };
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, DuplicateCommandIdentityIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.commands");
        provider.m_commands.push_back(provider.m_commands.front());
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, CapabilityMismatchIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.capability");
        provider.m_capabilities.m_supportsInteractive = false;
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, DuplicateConfigurationKeyIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.configuration");
        provider.m_configuration.push_back(provider.m_configuration.front());
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, SensitiveProviderDefaultIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.sensitive");
        provider.m_configuration.front().m_sensitive = true;
        provider.m_configuration.front().m_defaultValue = "secret";
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, ProbeMustReferenceDeclaredPathConfiguration)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.probe-path");
        provider.m_discoveryProbes.front().m_pathConfigurationKey = "missing-path";
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, InvalidProbeVersionRangeIsRejected)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.probe-version");
        provider.m_discoveryProbes.front().m_minimumSupportedVersion = "4.0.0";
        provider.m_discoveryProbes.front().m_maximumSupportedVersion = "3.0.0";
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, VersionBoundsRequireVersionConfigurationKey)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.probe-version-key");
        provider.m_discoveryProbes.front().m_versionConfigurationKey.clear();
        provider.m_discoveryProbes.front().m_minimumSupportedVersion = "3.0.0";
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, ProbePlatformMustBeProviderPlatform)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor provider = MakeProvider("fixture.probe-platform");
        provider.m_discoveryProbes.front().m_platforms = { "mac" };
        EXPECT_FALSE(registry.RegisterProvider(provider).m_success);
    }

    TEST(ExternalToolchainRegistryTests, ProvidersAndNestedContractsAreCanonicalized)
    {
        ExternalToolchainRegistry registry;
        ExternalToolProviderDescriptor zulu = MakeProvider("fixture.zulu");
        zulu.m_platforms = { "windows", "linux" };
        zulu.m_configuration = {
            zulu.m_configuration[1],
            zulu.m_configuration[0] };
        EXPECT_TRUE(registry.RegisterProvider(zulu).m_success);
        EXPECT_TRUE(registry.RegisterProvider(MakeProvider("fixture.alpha")).m_success);

        ASSERT_EQ(registry.GetProviders().size(), 2);
        EXPECT_EQ(registry.GetProviders()[0].m_providerId, "fixture.alpha");
        EXPECT_EQ(registry.GetProviders()[1].m_providerId, "fixture.zulu");
        EXPECT_EQ(
            registry.GetProviders()[1].m_configuration[0].m_key,
            "executable-path");
        EXPECT_EQ(registry.GetProviders()[1].m_platforms[0], "linux");
    }

    TEST(ExternalToolchainRegistryTests, RegistrationClosesAfterFinalization)
    {
        ExternalToolchainRegistry registry;
        EXPECT_TRUE(registry.FinalizeRegistration().m_success);
        EXPECT_TRUE(registry.IsRegistrationFinalized());
        EXPECT_FALSE(
            registry.RegisterProvider(MakeProvider("fixture.too-late")).m_success);
        EXPECT_TRUE(registry.FinalizeRegistration().m_success);
    }
} // namespace ExternalToolchain
