/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <ExternalToolchain/ExternalToolchainTypes.h>

#include <AzCore/std/containers/unordered_map.h>

namespace ExternalToolchain
{
    enum class ExternalToolchainSettingValueType : AZ::u8
    {
        Missing,
        String,
        Boolean,
        UnsignedInteger,
        Other,
    };

    class ExternalToolchainSettingsSource
    {
    public:
        virtual ~ExternalToolchainSettingsSource() = default;

        virtual bool GetString(
            const AZStd::string& path,
            AZStd::string& value) const = 0;
        virtual bool GetBool(
            const AZStd::string& path,
            bool& value) const = 0;
        virtual bool GetUInt64(
            const AZStd::string& path,
            AZ::u64& value) const = 0;
        virtual ExternalToolchainSettingValueType GetValueType(
            const AZStd::string& path) const
        {
            AZStd::string stringValue;
            if (GetString(path, stringValue))
            {
                return ExternalToolchainSettingValueType::String;
            }
            bool boolValue = false;
            if (GetBool(path, boolValue))
            {
                return ExternalToolchainSettingValueType::Boolean;
            }
            AZ::u64 unsignedValue = 0;
            if (GetUInt64(path, unsignedValue))
            {
                return ExternalToolchainSettingValueType::UnsignedInteger;
            }
            return ExternalToolchainSettingValueType::Missing;
        }
    };

    class SettingsRegistryExternalToolchainSettingsSource final
        : public ExternalToolchainSettingsSource
    {
    public:
        bool GetString(
            const AZStd::string& path,
            AZStd::string& value) const override;
        bool GetBool(
            const AZStd::string& path,
            bool& value) const override;
        bool GetUInt64(
            const AZStd::string& path,
            AZ::u64& value) const override;
        ExternalToolchainSettingValueType GetValueType(
            const AZStd::string& path) const override;
    };

    class ExternalToolchainConfigurationService
    {
    public:
        explicit ExternalToolchainConfigurationService(
            const ExternalToolchainSettingsSource& settingsSource);

        ProviderOperationResult SetSessionOverride(
            const ExternalToolProviderDescriptor& provider,
            const AZStd::string& key,
            const AZStd::string& value);
        ProviderOperationResult ClearSessionOverride(
            const ExternalToolProviderDescriptor& provider,
            const AZStd::string& key);
        ProviderOperationResult SetSessionProviderEnabled(
            const ExternalToolProviderDescriptor& provider,
            bool enabled);
        ProviderOperationResult ClearSessionProviderEnabled(
            const ExternalToolProviderDescriptor& provider);

        bool ResolveValue(
            const ExternalToolProviderDescriptor& provider,
            const AZStd::string& key,
            ExternalToolResolvedConfigurationValue& value) const;
        AZStd::vector<ExternalToolResolvedConfigurationValue> ResolveAll(
            const ExternalToolProviderDescriptor& provider) const;
        bool ResolveProviderEnabled(
            const ExternalToolProviderDescriptor& provider,
            bool& enabled,
            ConfigurationLayer& layer) const;

        const ExternalToolchainSettingsSource& GetSettingsSource() const;
        void Clear();

    private:
        static const ExternalToolConfigurationDescriptor* FindDescriptor(
            const ExternalToolProviderDescriptor& provider,
            const AZStd::string& key);
        static AZStd::string MakeSessionKey(
            const AZStd::string& providerId,
            const AZStd::string& key);
        static AZStd::string MakeRegistryPath(
            const char* root,
            const AZStd::string& providerId,
            const AZStd::string& key);

        const ExternalToolchainSettingsSource& m_settingsSource;
        AZStd::unordered_map<AZStd::string, AZStd::string> m_sessionOverrides;
        AZStd::unordered_map<AZStd::string, bool> m_sessionEnabled;
    };
} // namespace ExternalToolchain
