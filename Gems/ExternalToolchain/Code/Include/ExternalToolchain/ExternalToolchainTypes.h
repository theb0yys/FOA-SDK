/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace ExternalToolchain
{
    struct ExternalToolchainApiVersion
    {
        AZ::u32 m_major = 1;
        AZ::u32 m_minor = 0;
        AZ::u32 m_patch = 0;
    };

    inline constexpr ExternalToolchainApiVersion HostApiVersion{ 1, 0, 0 };

    enum class ToolFamily : AZ::u8
    {
        Dcc,
        Generator,
        EngineBridge,
        Utility,
    };

    enum class CommandMode : AZ::u8
    {
        Interactive,
        Batch,
        Probe,
    };

    struct ProviderCapabilities
    {
        bool m_supportsInteractive = false;
        bool m_supportsBatch = false;
        bool m_supportsHeadless = false;
        bool m_producesAssetSources = false;
        bool m_supportsStructuredIpc = false;
    };

    struct ExternalToolCommandDescriptor
    {
        AZStd::string m_commandId;
        AZStd::string m_displayName;
        CommandMode m_mode = CommandMode::Interactive;
        AZStd::vector<AZStd::string> m_inputKinds;
        AZStd::vector<AZStd::string> m_outputKinds;
        bool m_supportsCancellation = false;
        bool m_requiresProject = true;
        bool m_requiresSelection = false;
    };

    struct ExternalToolProviderDescriptor
    {
        AZStd::string m_providerId;
        AZStd::string m_displayName;
        AZStd::string m_providerVersion;
        ExternalToolchainApiVersion m_minimumHostApiVersion;
        ToolFamily m_toolFamily = ToolFamily::Utility;
        AZStd::vector<AZStd::string> m_platforms;
        ProviderCapabilities m_capabilities;
        AZStd::vector<ExternalToolCommandDescriptor> m_commands;
    };

    struct ProviderOperationResult
    {
        bool m_success = false;
        AZStd::string m_message;
    };

    AZStd::string ToString(ToolFamily family);
    AZStd::string ToString(CommandMode mode);
    AZStd::string ToString(const ExternalToolchainApiVersion& version);
    bool IsHostApiCompatible(const ExternalToolchainApiVersion& minimumVersion);
    bool IsValidSemanticVersion(const AZStd::string& value);
} // namespace ExternalToolchain
