/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

namespace ExternalToolchain
{
    namespace
    {
        constexpr size_t MaximumConfigurationEntries = 64;
        constexpr size_t MaximumDiscoveryProbes = 32;

        ProviderOperationResult Success(AZStd::string message)
        {
            return ProviderOperationResult{ true, AZStd::move(message) };
        }

        ProviderOperationResult Failure(AZStd::string message)
        {
            return ProviderOperationResult{ false, AZStd::move(message) };
        }

        bool IsLowerAlphaNumeric(char value)
        {
            return (value >= 'a' && value <= 'z')
                || (value >= '0' && value <= '9');
        }

        bool IsLowerIdentifierCharacter(char value)
        {
            return IsLowerAlphaNumeric(value)
                || value == '-'
                || value == '_';
        }

        bool IsValidIdentifier(const AZStd::string& value, bool requireNamespace)
        {
            if (value.empty())
            {
                return false;
            }

            bool hasNamespaceSeparator = false;
            bool segmentHasCharacter = false;
            bool segmentStart = true;
            for (char character : value)
            {
                if (character == '.')
                {
                    if (!segmentHasCharacter)
                    {
                        return false;
                    }
                    hasNamespaceSeparator = true;
                    segmentHasCharacter = false;
                    segmentStart = true;
                    continue;
                }
                if ((segmentStart && !IsLowerAlphaNumeric(character))
                    || (!segmentStart && !IsLowerIdentifierCharacter(character)))
                {
                    return false;
                }
                segmentHasCharacter = true;
                segmentStart = false;
            }
            return segmentHasCharacter && (!requireNamespace || hasNamespaceSeparator);
        }

        bool ContainsString(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool ContainsCommand(
            const AZStd::vector<ExternalToolCommandDescriptor>& commands,
            const AZStd::string& commandId)
        {
            for (const ExternalToolCommandDescriptor& command : commands)
            {
                if (command.m_commandId == commandId)
                {
                    return true;
                }
            }
            return false;
        }

        bool ContainsConfiguration(
            const AZStd::vector<ExternalToolConfigurationDescriptor>& configuration,
            const AZStd::string& key)
        {
            for (const ExternalToolConfigurationDescriptor& entry : configuration)
            {
                if (entry.m_key == key)
                {
                    return true;
                }
            }
            return false;
        }

        bool ContainsProbe(
            const AZStd::vector<ExternalToolDiscoveryProbeDescriptor>& probes,
            const AZStd::string& probeId)
        {
            for (const ExternalToolDiscoveryProbeDescriptor& probe : probes)
            {
                if (probe.m_probeId == probeId)
                {
                    return true;
                }
            }
            return false;
        }

        const ExternalToolConfigurationDescriptor* FindConfiguration(
            const ExternalToolProviderDescriptor& provider,
            const AZStd::string& key)
        {
            for (const ExternalToolConfigurationDescriptor& entry :
                 provider.m_configuration)
            {
                if (entry.m_key == key)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        bool IsKnownConfigurationKind(ConfigurationValueKind kind)
        {
            return ToString(kind) != "unknown";
        }

        bool IsKnownProbeKind(DiscoveryProbeKind kind)
        {
            return ToString(kind) != "unknown";
        }

        void SortUniqueText(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        ExternalToolProviderDescriptor NormalizeDescriptor(
            ExternalToolProviderDescriptor descriptor)
        {
            SortUniqueText(descriptor.m_platforms);
            for (ExternalToolCommandDescriptor& command : descriptor.m_commands)
            {
                SortUniqueText(command.m_inputKinds);
                SortUniqueText(command.m_outputKinds);
            }
            AZStd::sort(
                descriptor.m_commands.begin(),
                descriptor.m_commands.end(),
                [](const ExternalToolCommandDescriptor& left,
                   const ExternalToolCommandDescriptor& right)
                {
                    return left.m_commandId < right.m_commandId;
                });
            AZStd::sort(
                descriptor.m_configuration.begin(),
                descriptor.m_configuration.end(),
                [](const ExternalToolConfigurationDescriptor& left,
                   const ExternalToolConfigurationDescriptor& right)
                {
                    return left.m_key < right.m_key;
                });
            for (ExternalToolDiscoveryProbeDescriptor& probe :
                 descriptor.m_discoveryProbes)
            {
                SortUniqueText(probe.m_platforms);
            }
            AZStd::sort(
                descriptor.m_discoveryProbes.begin(),
                descriptor.m_discoveryProbes.end(),
                [](const ExternalToolDiscoveryProbeDescriptor& left,
                   const ExternalToolDiscoveryProbeDescriptor& right)
                {
                    return left.m_probeId < right.m_probeId;
                });
            return descriptor;
        }
    } // namespace

    ProviderOperationResult ExternalToolchainRegistry::RegisterProvider(
        const ExternalToolProviderDescriptor& descriptor)
    {
        if (m_registrationFinalized)
        {
            return Failure("Provider registration is already finalized.");
        }

        if (const ProviderOperationResult validation = ValidateDescriptor(descriptor);
            !validation.m_success)
        {
            return validation;
        }

        if (FindProvider(descriptor.m_providerId))
        {
            return Failure("Provider ID is already registered.");
        }

        m_providers.push_back(NormalizeDescriptor(descriptor));
        AZStd::sort(
            m_providers.begin(),
            m_providers.end(),
            [](const ExternalToolProviderDescriptor& left,
               const ExternalToolProviderDescriptor& right)
            {
                return left.m_providerId < right.m_providerId;
            });

        return Success("Provider registered.");
    }

    ProviderOperationResult ExternalToolchainRegistry::FinalizeRegistration()
    {
        if (m_registrationFinalized)
        {
            return Success("Provider registration was already finalized.");
        }

        m_registrationFinalized = true;
        return Success("Provider registration finalized.");
    }

    void ExternalToolchainRegistry::Clear()
    {
        m_providers.clear();
        m_registrationFinalized = false;
    }

    bool ExternalToolchainRegistry::IsRegistrationFinalized() const
    {
        return m_registrationFinalized;
    }

    const AZStd::vector<ExternalToolProviderDescriptor>&
    ExternalToolchainRegistry::GetProviders() const
    {
        return m_providers;
    }

    const ExternalToolProviderDescriptor* ExternalToolchainRegistry::FindProvider(
        const AZStd::string& providerId) const
    {
        for (const ExternalToolProviderDescriptor& descriptor : m_providers)
        {
            if (descriptor.m_providerId == providerId)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    ProviderOperationResult ExternalToolchainRegistry::ValidateDescriptor(
        const ExternalToolProviderDescriptor& descriptor)
    {
        if (!IsValidIdentifier(descriptor.m_providerId, true))
        {
            return Failure(
                "Provider IDs must be lowercase namespaced identifiers.");
        }
        if (descriptor.m_displayName.empty())
        {
            return Failure("Provider display name is required.");
        }
        if (!IsValidSemanticVersion(descriptor.m_providerVersion))
        {
            return Failure("Provider version must be valid semantic versioning.");
        }
        if (!IsHostApiCompatible(descriptor.m_minimumHostApiVersion))
        {
            return Failure(
                "Provider minimum host API version is incompatible with this host.");
        }
        if (ToString(descriptor.m_toolFamily) == "unknown")
        {
            return Failure("Provider tool family is not supported.");
        }
        if (descriptor.m_platforms.empty())
        {
            return Failure("Providers must declare at least one platform.");
        }

        AZStd::vector<AZStd::string> platforms;
        for (const AZStd::string& platform : descriptor.m_platforms)
        {
            if (!IsValidIdentifier(platform, false))
            {
                return Failure("Provider platform IDs must be lowercase identifiers.");
            }
            if (ContainsString(platforms, platform))
            {
                return Failure("Provider platform IDs must be unique.");
            }
            platforms.push_back(platform);
        }

        if (descriptor.m_commands.empty())
        {
            return Failure("Providers must declare at least one command.");
        }

        AZStd::vector<ExternalToolCommandDescriptor> commands;
        for (const ExternalToolCommandDescriptor& command : descriptor.m_commands)
        {
            if (!IsValidIdentifier(command.m_commandId, false))
            {
                return Failure("Command IDs must be lowercase identifiers.");
            }
            if (command.m_displayName.empty())
            {
                return Failure("Command display names are required.");
            }
            if (ToString(command.m_mode) == "unknown")
            {
                return Failure("Command mode is not supported.");
            }
            if (ContainsCommand(commands, command.m_commandId))
            {
                return Failure("Command IDs must be unique within a provider.");
            }
            if (command.m_mode == CommandMode::Interactive
                && !descriptor.m_capabilities.m_supportsInteractive)
            {
                return Failure(
                    "Interactive commands require interactive provider capability.");
            }
            if (command.m_mode == CommandMode::Batch
                && !descriptor.m_capabilities.m_supportsBatch)
            {
                return Failure("Batch commands require batch provider capability.");
            }
            commands.push_back(command);
        }

        if (descriptor.m_configuration.size() > MaximumConfigurationEntries)
        {
            return Failure("Provider configuration entry count exceeds the host limit.");
        }

        AZStd::vector<ExternalToolConfigurationDescriptor> configuration;
        for (const ExternalToolConfigurationDescriptor& entry :
             descriptor.m_configuration)
        {
            if (!IsValidIdentifier(entry.m_key, false))
            {
                return Failure("Configuration keys must be lowercase identifiers.");
            }
            if (entry.m_displayName.empty())
            {
                return Failure("Configuration display names are required.");
            }
            if (!IsKnownConfigurationKind(entry.m_kind))
            {
                return Failure("Configuration value kind is not supported.");
            }
            if (ContainsConfiguration(configuration, entry.m_key))
            {
                return Failure(
                    "Configuration keys must be unique within a provider.");
            }
            if (entry.m_sensitive && !entry.m_defaultValue.empty())
            {
                return Failure(
                    "Sensitive configuration values must not have provider defaults.");
            }
            if (entry.m_defaultValue.size() > 4096
                || entry.m_defaultValue.find('\0') != AZStd::string::npos)
            {
                return Failure("Configuration default exceeds host limits.");
            }
            if (entry.m_kind == ConfigurationValueKind::SemanticVersion
                && !entry.m_defaultValue.empty()
                && !IsValidSemanticVersion(entry.m_defaultValue))
            {
                return Failure(
                    "Semantic-version configuration defaults must be valid SemVer.");
            }
            configuration.push_back(entry);
        }

        if (descriptor.m_discoveryProbes.size() > MaximumDiscoveryProbes)
        {
            return Failure("Provider discovery probe count exceeds the host limit.");
        }

        AZStd::vector<ExternalToolDiscoveryProbeDescriptor> probes;
        for (const ExternalToolDiscoveryProbeDescriptor& probe :
             descriptor.m_discoveryProbes)
        {
            if (!IsValidIdentifier(probe.m_probeId, false))
            {
                return Failure("Discovery probe IDs must be lowercase identifiers.");
            }
            if (ContainsProbe(probes, probe.m_probeId))
            {
                return Failure(
                    "Discovery probe IDs must be unique within a provider.");
            }
            if (!IsKnownProbeKind(probe.m_kind))
            {
                return Failure("Discovery probe kind is not supported.");
            }
            if (probe.m_timeoutMilliseconds < 1
                || probe.m_timeoutMilliseconds > 5000)
            {
                return Failure(
                    "Discovery probe timeout must be between 1 and 5000 milliseconds.");
            }

            const ExternalToolConfigurationDescriptor* pathConfiguration =
                FindConfiguration(descriptor, probe.m_pathConfigurationKey);
            if (!pathConfiguration
                || pathConfiguration->m_kind != ConfigurationValueKind::Path)
            {
                return Failure(
                    "Discovery probes must reference one declared path configuration key.");
            }

            if (!probe.m_versionConfigurationKey.empty())
            {
                const ExternalToolConfigurationDescriptor* versionConfiguration =
                    FindConfiguration(
                        descriptor,
                        probe.m_versionConfigurationKey);
                if (!versionConfiguration
                    || versionConfiguration->m_kind
                        != ConfigurationValueKind::SemanticVersion)
                {
                    return Failure(
                        "Discovery probe version keys must reference semantic-version configuration.");
                }
            }

            if ((!probe.m_minimumSupportedVersion.empty()
                    || !probe.m_maximumSupportedVersion.empty())
                && probe.m_versionConfigurationKey.empty())
            {
                return Failure(
                    "Discovery version bounds require a version configuration key.");
            }

            if ((!probe.m_minimumSupportedVersion.empty()
                    && !IsValidSemanticVersion(
                        probe.m_minimumSupportedVersion))
                || (!probe.m_maximumSupportedVersion.empty()
                    && !IsValidSemanticVersion(
                        probe.m_maximumSupportedVersion)))
            {
                return Failure(
                    "Discovery version bounds must use valid semantic versions.");
            }
            if (!probe.m_minimumSupportedVersion.empty()
                && !probe.m_maximumSupportedVersion.empty())
            {
                int comparison = 0;
                if (!TryCompareSemanticVersions(
                        probe.m_minimumSupportedVersion,
                        probe.m_maximumSupportedVersion,
                        comparison)
                    || comparison > 0)
                {
                    return Failure(
                        "Discovery minimum version must not exceed the maximum version.");
                }
            }

            AZStd::vector<AZStd::string> probePlatforms;
            for (const AZStd::string& platform : probe.m_platforms)
            {
                if (!IsValidIdentifier(platform, false)
                    || !ContainsString(descriptor.m_platforms, platform))
                {
                    return Failure(
                        "Discovery probe platforms must be declared provider platforms.");
                }
                if (ContainsString(probePlatforms, platform))
                {
                    return Failure(
                        "Discovery probe platform IDs must be unique.");
                }
                probePlatforms.push_back(platform);
            }
            probes.push_back(probe);
        }

        return Success("Provider descriptor is valid.");
    }
} // namespace ExternalToolchain
