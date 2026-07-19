/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainConfigurationService.h"

#include <AzCore/Interface/Interface.h>
#include <AzCore/Settings/SettingsRegistry.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace ExternalToolchain
{
    namespace
    {
        ProviderOperationResult Success(AZStd::string message)
        {
            return ProviderOperationResult{ true, AZStd::move(message) };
        }

        ProviderOperationResult Failure(AZStd::string message)
        {
            return ProviderOperationResult{ false, AZStd::move(message) };
        }

        bool ValidateConfiguredValue(
            const ExternalToolConfigurationDescriptor& descriptor,
            const AZStd::string& value)
        {
            if (descriptor.m_required && value.empty())
            {
                return false;
            }
            if (value.size() > 4096
                || value.find('\0') != AZStd::string::npos)
            {
                return false;
            }
            return descriptor.m_kind != ConfigurationValueKind::SemanticVersion
                || value.empty()
                || IsValidSemanticVersion(value);
        }
    } // namespace

    bool SettingsRegistryExternalToolchainSettingsSource::GetString(
        const AZStd::string& path,
        AZStd::string& value) const
    {
        if (AZ::SettingsRegistryInterface* registry =
                AZ::Interface<AZ::SettingsRegistryInterface>::Get())
        {
            return registry->Get(value, path);
        }
        return false;
    }

    bool SettingsRegistryExternalToolchainSettingsSource::GetBool(
        const AZStd::string& path,
        bool& value) const
    {
        if (AZ::SettingsRegistryInterface* registry =
                AZ::Interface<AZ::SettingsRegistryInterface>::Get())
        {
            return registry->Get(value, path);
        }
        return false;
    }

    bool SettingsRegistryExternalToolchainSettingsSource::GetUInt64(
        const AZStd::string& path,
        AZ::u64& value) const
    {
        if (AZ::SettingsRegistryInterface* registry =
                AZ::Interface<AZ::SettingsRegistryInterface>::Get())
        {
            return registry->Get(value, path);
        }
        return false;
    }

    ExternalToolchainConfigurationService::ExternalToolchainConfigurationService(
        const ExternalToolchainSettingsSource& settingsSource)
        : m_settingsSource(settingsSource)
    {
    }

    ProviderOperationResult
    ExternalToolchainConfigurationService::SetSessionOverride(
        const ExternalToolProviderDescriptor& provider,
        const AZStd::string& key,
        const AZStd::string& value)
    {
        const ExternalToolConfigurationDescriptor* descriptor =
            FindDescriptor(provider, key);
        if (!descriptor)
        {
            return Failure("Configuration key is not declared by the provider.");
        }
        if (!ValidateConfiguredValue(*descriptor, value))
        {
            return Failure(
                "Session configuration value does not satisfy the declared type or limits.");
        }

        m_sessionOverrides[MakeSessionKey(provider.m_providerId, key)] = value;
        return Success("Session configuration override set.");
    }

    ProviderOperationResult
    ExternalToolchainConfigurationService::ClearSessionOverride(
        const ExternalToolProviderDescriptor& provider,
        const AZStd::string& key)
    {
        if (!FindDescriptor(provider, key))
        {
            return Failure("Configuration key is not declared by the provider.");
        }

        m_sessionOverrides.erase(MakeSessionKey(provider.m_providerId, key));
        return Success("Session configuration override cleared.");
    }

    ProviderOperationResult
    ExternalToolchainConfigurationService::SetSessionProviderEnabled(
        const ExternalToolProviderDescriptor& provider,
        bool enabled)
    {
        m_sessionEnabled[provider.m_providerId] = enabled;
        return Success("Session provider enabled state set.");
    }

    ProviderOperationResult
    ExternalToolchainConfigurationService::ClearSessionProviderEnabled(
        const ExternalToolProviderDescriptor& provider)
    {
        m_sessionEnabled.erase(provider.m_providerId);
        return Success("Session provider enabled state cleared.");
    }

    bool ExternalToolchainConfigurationService::ResolveValue(
        const ExternalToolProviderDescriptor& provider,
        const AZStd::string& key,
        ExternalToolResolvedConfigurationValue& value) const
    {
        const ExternalToolConfigurationDescriptor* descriptor =
            FindDescriptor(provider, key);
        if (!descriptor)
        {
            return false;
        }

        value = {};
        value.m_providerId = provider.m_providerId;
        value.m_key = descriptor->m_key;
        value.m_displayName = descriptor->m_displayName;
        value.m_kind = descriptor->m_kind;
        value.m_required = descriptor->m_required;
        value.m_sensitive = descriptor->m_sensitive;
        value.m_layer = ConfigurationLayer::ProviderDefault;
        value.m_value = descriptor->m_defaultValue;

        AZStd::string configuredValue;
        if (m_settingsSource.GetString(
                MakeRegistryPath(
                    ProjectConfigurationRoot,
                    provider.m_providerId,
                    key),
                configuredValue))
        {
            value.m_layer = ConfigurationLayer::Project;
            value.m_value = configuredValue;
        }

        if (m_settingsSource.GetString(
                MakeRegistryPath(
                    UserConfigurationRoot,
                    provider.m_providerId,
                    key),
                configuredValue))
        {
            value.m_layer = ConfigurationLayer::User;
            value.m_value = configuredValue;
        }

        const auto session = m_sessionOverrides.find(
            MakeSessionKey(provider.m_providerId, key));
        if (session != m_sessionOverrides.end())
        {
            value.m_layer = ConfigurationLayer::Session;
            value.m_value = session->second;
        }

        value.m_configured = !value.m_value.empty();
        value.m_valueValid = ValidateConfiguredValue(*descriptor, value.m_value);
        return true;
    }

    AZStd::vector<ExternalToolResolvedConfigurationValue>
    ExternalToolchainConfigurationService::ResolveAll(
        const ExternalToolProviderDescriptor& provider) const
    {
        AZStd::vector<ExternalToolResolvedConfigurationValue> values;
        values.reserve(provider.m_configuration.size());
        for (const ExternalToolConfigurationDescriptor& descriptor :
             provider.m_configuration)
        {
            ExternalToolResolvedConfigurationValue value;
            if (ResolveValue(provider, descriptor.m_key, value))
            {
                values.push_back(AZStd::move(value));
            }
        }

        AZStd::sort(
            values.begin(),
            values.end(),
            [](const ExternalToolResolvedConfigurationValue& left,
               const ExternalToolResolvedConfigurationValue& right)
            {
                return left.m_key < right.m_key;
            });
        return values;
    }

    void ExternalToolchainConfigurationService::ResolveProviderEnabled(
        const ExternalToolProviderDescriptor& provider,
        bool& enabled,
        ConfigurationLayer& layer) const
    {
        enabled = provider.m_enabledByDefault;
        layer = ConfigurationLayer::ProviderDefault;

        bool configuredEnabled = false;
        if (m_settingsSource.GetBool(
                MakeRegistryPath(
                    ProjectConfigurationRoot,
                    provider.m_providerId,
                    "Enabled"),
                configuredEnabled))
        {
            enabled = configuredEnabled;
            layer = ConfigurationLayer::Project;
        }

        if (m_settingsSource.GetBool(
                MakeRegistryPath(
                    UserConfigurationRoot,
                    provider.m_providerId,
                    "Enabled"),
                configuredEnabled))
        {
            enabled = configuredEnabled;
            layer = ConfigurationLayer::User;
        }

        const auto session = m_sessionEnabled.find(provider.m_providerId);
        if (session != m_sessionEnabled.end())
        {
            enabled = session->second;
            layer = ConfigurationLayer::Session;
        }
    }

    const ExternalToolchainSettingsSource&
    ExternalToolchainConfigurationService::GetSettingsSource() const
    {
        return m_settingsSource;
    }

    void ExternalToolchainConfigurationService::Clear()
    {
        m_sessionOverrides.clear();
        m_sessionEnabled.clear();
    }

    const ExternalToolConfigurationDescriptor*
    ExternalToolchainConfigurationService::FindDescriptor(
        const ExternalToolProviderDescriptor& provider,
        const AZStd::string& key)
    {
        for (const ExternalToolConfigurationDescriptor& descriptor :
             provider.m_configuration)
        {
            if (descriptor.m_key == key)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    AZStd::string ExternalToolchainConfigurationService::MakeSessionKey(
        const AZStd::string& providerId,
        const AZStd::string& key)
    {
        return providerId + '\n' + key;
    }

    AZStd::string ExternalToolchainConfigurationService::MakeRegistryPath(
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
} // namespace ExternalToolchain
