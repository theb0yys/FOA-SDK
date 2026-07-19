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

            ExternalToolProviderDescriptor provider;
            provider.m_providerId = AZStd::move(providerId);
            provider.m_displayName = "Fixture Provider";
            provider.m_providerVersion = "1.2.3";
            provider.m_minimumHostApiVersion = HostApiVersion;
            provider.m_toolFamily = ToolFamily::Utility;
            provider.m_platforms = { "windows", "linux" };
            provider.m_capabilities.m_supportsInteractive = true;
            provider.m_commands = { command };
            return provider;
        }
    }

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

        const ProviderOperationResult duplicate =
            registry.RegisterProvider(MakeProvider("fixture.duplicate"));
        EXPECT_FALSE(duplicate.m_success);
    }

    TEST(ExternalToolchainRegistryTests, InvalidProviderIdentityIsRejected)
    {
        ExternalToolchainRegistry registry;
        const ProviderOperationResult result =
            registry.RegisterProvider(MakeProvider("NotNamespaced"));

        EXPECT_FALSE(result.m_success);
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
        provider.m_minimumHostApiVersion = { 1, 1, 0 };

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

    TEST(ExternalToolchainRegistryTests, ProvidersAreEnumeratedDeterministically)
    {
        ExternalToolchainRegistry registry;
        EXPECT_TRUE(registry.RegisterProvider(MakeProvider("fixture.zulu")).m_success);
        EXPECT_TRUE(registry.RegisterProvider(MakeProvider("fixture.alpha")).m_success);

        ASSERT_EQ(registry.GetProviders().size(), 2);
        EXPECT_EQ(registry.GetProviders()[0].m_providerId, "fixture.alpha");
        EXPECT_EQ(registry.GetProviders()[1].m_providerId, "fixture.zulu");
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
