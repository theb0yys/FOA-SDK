/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainConfigurationService.h"

#include <AzCore/std/containers/unordered_map.h>
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

        AZStd::string SettingPath(
            const char* root,
            const AZStd::string& providerId,
            const AZStd::string& key)
        {
            return AZStd::string::format(
                "%s/%s/%s",
                root,
                providerId.c_str(),
                key.c_str());
        }

        ExternalToolProviderDescriptor MakeProvider()
        {
            ExternalToolConfigurationDescriptor path;
            path.m_key = "executable-path";
            path.m_displayName = "Executable path";
            path.m_kind = ConfigurationValueKind::Path;
            path.m_defaultValue = "C:/Provider/tool.exe";
            path.m_required = true;

            ExternalToolConfigurationDescriptor version;
            version.m_key = "tool-version";
            version.m_displayName = "Tool version";
            version.m_kind = ConfigurationValueKind::SemanticVersion;
            version.m_defaultValue = "3.1.0";

            ExternalToolProviderDescriptor provider;
            provider.m_providerId = "fixture.configuration";
            provider.m_configuration = { path, version };
            provider.m_enabledByDefault = true;
            return provider;
        }
    } // namespace

    TEST(ExternalToolchainConfigurationTests, ProviderDefaultIsResolved)
    {
        FakeSettingsSource settings;
        ExternalToolchainConfigurationService service(settings);
        ExternalToolResolvedConfigurationValue value;

        ASSERT_TRUE(service.ResolveValue(
            MakeProvider(),
            "executable-path",
            value));
        EXPECT_EQ(value.m_layer, ConfigurationLayer::ProviderDefault);
        EXPECT_EQ(value.m_value, "C:/Provider/tool.exe");
        EXPECT_TRUE(value.m_configured);
        EXPECT_TRUE(value.m_valueValid);
    }

    TEST(ExternalToolchainConfigurationTests, UserOverridesProjectAndDefault)
    {
        FakeSettingsSource settings;
        const ExternalToolProviderDescriptor provider = MakeProvider();
        settings.m_strings[SettingPath(
            ProjectConfigurationRoot,
            provider.m_providerId,
            "executable-path")] = "C:/Project/tool.exe";
        settings.m_strings[SettingPath(
            UserConfigurationRoot,
            provider.m_providerId,
            "executable-path")] = "C:/User/tool.exe";
        ExternalToolchainConfigurationService service(settings);
        ExternalToolResolvedConfigurationValue value;

        ASSERT_TRUE(service.ResolveValue(provider, "executable-path", value));
        EXPECT_EQ(value.m_layer, ConfigurationLayer::User);
        EXPECT_EQ(value.m_value, "C:/User/tool.exe");
    }

    TEST(ExternalToolchainConfigurationTests, SessionOverridesEveryPersistentLayer)
    {
        FakeSettingsSource settings;
        const ExternalToolProviderDescriptor provider = MakeProvider();
        settings.m_strings[SettingPath(
            UserConfigurationRoot,
            provider.m_providerId,
            "executable-path")] = "C:/User/tool.exe";
        ExternalToolchainConfigurationService service(settings);
        ASSERT_TRUE(service.SetSessionOverride(
            provider,
            "executable-path",
            "C:/Session/tool.exe").m_success);
        ExternalToolResolvedConfigurationValue value;

        ASSERT_TRUE(service.ResolveValue(provider, "executable-path", value));
        EXPECT_EQ(value.m_layer, ConfigurationLayer::Session);
        EXPECT_EQ(value.m_value, "C:/Session/tool.exe");
    }

    TEST(ExternalToolchainConfigurationTests, EmptyHigherLayerClearsConfiguredValue)
    {
        FakeSettingsSource settings;
        const ExternalToolProviderDescriptor provider = MakeProvider();
        settings.m_strings[SettingPath(
            UserConfigurationRoot,
            provider.m_providerId,
            "executable-path")] = "";
        ExternalToolchainConfigurationService service(settings);
        ExternalToolResolvedConfigurationValue value;

        ASSERT_TRUE(service.ResolveValue(provider, "executable-path", value));
        EXPECT_EQ(value.m_layer, ConfigurationLayer::User);
        EXPECT_FALSE(value.m_configured);
        EXPECT_FALSE(value.m_valueValid);
    }

    TEST(ExternalToolchainConfigurationTests, InvalidSemanticVersionIsRetainedAndMarkedInvalid)
    {
        FakeSettingsSource settings;
        const ExternalToolProviderDescriptor provider = MakeProvider();
        settings.m_strings[SettingPath(
            ProjectConfigurationRoot,
            provider.m_providerId,
            "tool-version")] = "not-semver";
        ExternalToolchainConfigurationService service(settings);
        ExternalToolResolvedConfigurationValue value;

        ASSERT_TRUE(service.ResolveValue(provider, "tool-version", value));
        EXPECT_TRUE(value.m_configured);
        EXPECT_FALSE(value.m_valueValid);
        EXPECT_EQ(value.m_layer, ConfigurationLayer::Project);
    }

    TEST(ExternalToolchainConfigurationTests, EnabledStateUsesSessionUserProjectDefaultPrecedence)
    {
        FakeSettingsSource settings;
        const ExternalToolProviderDescriptor provider = MakeProvider();
        settings.m_bools[SettingPath(
            ProjectConfigurationRoot,
            provider.m_providerId,
            "Enabled")] = false;
        settings.m_bools[SettingPath(
            UserConfigurationRoot,
            provider.m_providerId,
            "Enabled")] = true;
        ExternalToolchainConfigurationService service(settings);
        bool enabled = false;
        ConfigurationLayer layer = ConfigurationLayer::ProviderDefault;

        service.ResolveProviderEnabled(provider, enabled, layer);
        EXPECT_TRUE(enabled);
        EXPECT_EQ(layer, ConfigurationLayer::User);

        ASSERT_TRUE(service.SetSessionProviderEnabled(provider, false).m_success);
        service.ResolveProviderEnabled(provider, enabled, layer);
        EXPECT_FALSE(enabled);
        EXPECT_EQ(layer, ConfigurationLayer::Session);
    }

    TEST(ExternalToolchainConfigurationTests, UndeclaredSessionKeyIsRejected)
    {
        FakeSettingsSource settings;
        ExternalToolchainConfigurationService service(settings);
        EXPECT_FALSE(service.SetSessionOverride(
            MakeProvider(),
            "undeclared",
            "value").m_success);
    }
} // namespace ExternalToolchain
