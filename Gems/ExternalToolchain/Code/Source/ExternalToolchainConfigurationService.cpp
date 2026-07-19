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

        void ApplyStringLayer(
            const ExternalToolchainSettingsSource& source,
            const AZStd::string& path,
            ConfigurationLayer layer,
            const ExternalToolConfigurationDescriptor& descriptor,
            ExternalToolResolvedConfigurationValue& value)
        {
            const ExternalToolchainSettingValueType type = source.GetValueType(path);
            if (type == ExternalToolchainSettingValueType::Missing)
            {
                return;
            }

            value.m_layer = layer;
            value.m_configured = true;
            value.m_value.clear();
            if (type != ExternalToolchainSettingValueType::String
                || !source.GetString(path, value.m_value))
            {
                value.m_valueValid = false;
                return;
            }
            value.m_valueValid = ValidateConfiguredValue(descriptor, value.m_value);
        }

        bool ApplyBoolLayer(
            const ExternalToolchainSettingsSource& source,
            const AZStd::string& path,
            ConfigurationLayer candidateLayer,
            bool& enabled,
            ConfigurationLayer& layer,
            bool& valid)
        {
            const ExternalToolchainSettingValueType type = source.GetValueType(path);
            if (type == ExternalToolchainSettingValueType::Missing)
            {
                return false;
            }
            layer = candidateLayer;
            valid = type == ExternalToolchainSettingValueType::Boolean
                && source.GetBool(path, enabled);
            if (!valid)
            {
                enabled = false;
            }
            return true;
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

    ExternalToolchainSettingValueType
    SettingsRegistryExternalToolchainSettingsSource::GetValueType(
        const AZStd::string& path) const
    {
        const AZ::SettingsRegistryInterface* registry =
            AZ::Interface<AZ::SettingsRegistryInterface>::Get();
        if (!registry)
        {
            return ExternalToolchainSettingValueType::Missing;
        }
        const AZ::SettingsRegistryInterface::SettingsType type =
            registry->GetType(path);
        switch (type.m_type)
        {
        case AZ::SettingsRegistryInterface::Type::NoType:
            return ExternalToolchainSettingValueType::Missing;
        case AZ::SettingsRegistryInterface::Type::String:
            return ExternalToolchainSettingValueType::String;
        case AZ::SettingsRegistryInterface::Type::Boolean:
            return ExternalToolchainSettingValueType::Boolean;
        case AZ::SettingsRegistryInterface::Type::Integer:
            return type.m_signedness
                    == AZ::SettingsRegistryInterface::Signedness::Unsigned
                ? ExternalToolchainSettingValueType::UnsignedInteger
                : ExternalToolchainSettingValueType::Other;
        case AZ::SettingsRegistryInterface::Type::Null:
        case AZ::SettingsRegistryInterface::Type::FloatingPoint:
        case AZ::SettingsRegistryInterface::Type::Array:
        case AZ::SettingsRegistryInterface::Type::Object:
            return ExternalToolchainSettingValueType::Other;
        }
        return ExternalToolchainSettingValueType::Other;
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
        value.m_configured = !value.m_value.empty();
        value.m_valueValid = ValidateConfiguredValue(*descriptor, value.m_value);

        ApplyStringLayer(
            m_settingsSource,
            MakeRegistryPath(
                ProjectConfigurationRoot,
                provider.m_providerId,
                key),
            ConfigurationLayer::Project,
            *descriptor,
            value);
        ApplyStringLayer(
            m_settingsSource,
            MakeRegistryPath(
                UserConfigurationRoot,
                provider.m_providerId,
                key),
            ConfigurationLayer::User,
            *descriptor,
            value);

        const auto session = m_sessionOverrides.find(
            MakeSessionKey(provider.m_providerId, key));
        if (session != m_sessionOverrides.end())
        {
            value.m_layer = ConfigurationLayer::Session;
            value.m_value = session->second;
            value.m_configured = true;
            value.m_valueValid = ValidateConfiguredValue(*descriptor, value.m_value);
        }
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

    bool ExternalToolchainConfigurationService::ResolveProviderEnabled(
        const ExternalToolProviderDescriptor& provider,
        bool& enabled,
        ConfigurationLayer& layer) const
    {
        enabled = provider.m_enabledByDefault;
        layer = ConfigurationLayer::ProviderDefault;
        bool valid = true;

        ApplyBoolLayer(
            m_settingsSource,
            MakeRegistryPath(
                ProjectConfigurationRoot,
                provider.m_providerId,
                "Enabled"),
            ConfigurationLayer::Project,
            enabled,
            layer,
            valid);
        ApplyBoolLayer(
            m_settingsSource,
            MakeRegistryPath(
                UserConfigurationRoot,
                provider.m_providerId,
                "Enabled"),
            ConfigurationLayer::User,
            enabled,
            layer,
            valid);

        const auto session = m_sessionEnabled.find(provider.m_providerId);
        if (session != m_sessionEnabled.end())
        {
            enabled = session->second;
            layer = ConfigurationLayer::Session;
            valid = true;
        }
        return valid;
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
