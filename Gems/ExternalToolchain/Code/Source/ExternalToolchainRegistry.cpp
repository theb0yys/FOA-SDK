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

        m_providers.push_back(descriptor);
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

        return Success("Provider descriptor is valid.");
    }
} // namespace ExternalToolchain
