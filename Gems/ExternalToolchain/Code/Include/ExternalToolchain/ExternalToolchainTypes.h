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
    inline constexpr const char* ProjectConfigurationRoot =
        "/O3DE/ExternalToolchain/Configuration/Project/Providers";
    inline constexpr const char* UserConfigurationRoot =
        "/O3DE/ExternalToolchain/Configuration/User/Providers";

    struct ExternalToolchainApiVersion
    {
        AZ::u32 m_major = 1;
        AZ::u32 m_minor = 1;
        AZ::u32 m_patch = 0;
    };

    inline constexpr ExternalToolchainApiVersion HostApiVersion{ 1, 1, 0 };

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

    enum class ConfigurationValueKind : AZ::u8
    {
        String,
        Path,
        SemanticVersion,
    };

    enum class ConfigurationLayer : AZ::u8
    {
        ProviderDefault,
        Project,
        User,
        Session,
    };

    enum class DiscoveryProbeKind : AZ::u8
    {
        File,
        Directory,
    };

    enum class DiscoveryStatus : AZ::u8
    {
        NotRun,
        Disabled,
        UnsupportedPlatform,
        NotInstalled,
        Installed,
        UnsupportedVersion,
        Misconfigured,
        ProbeFailed,
        Ambiguous,
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

    struct ExternalToolConfigurationDescriptor
    {
        AZStd::string m_key;
        AZStd::string m_displayName;
        ConfigurationValueKind m_kind = ConfigurationValueKind::String;
        AZStd::string m_defaultValue;
        bool m_required = false;
        bool m_sensitive = false;
    };

    struct ExternalToolDiscoveryProbeDescriptor
    {
        AZStd::string m_probeId;
        DiscoveryProbeKind m_kind = DiscoveryProbeKind::File;
        AZStd::string m_pathConfigurationKey;
        AZStd::string m_versionConfigurationKey;
        AZStd::string m_minimumSupportedVersion;
        AZStd::string m_maximumSupportedVersion;
        AZStd::vector<AZStd::string> m_platforms;
        AZ::u32 m_timeoutMilliseconds = 100;
        bool m_required = true;
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
        AZStd::vector<ExternalToolConfigurationDescriptor> m_configuration;
        AZStd::vector<ExternalToolDiscoveryProbeDescriptor> m_discoveryProbes;
        bool m_enabledByDefault = true;
    };

    struct ExternalToolResolvedConfigurationValue
    {
        AZStd::string m_providerId;
        AZStd::string m_key;
        AZStd::string m_displayName;
        ConfigurationValueKind m_kind = ConfigurationValueKind::String;
        ConfigurationLayer m_layer = ConfigurationLayer::ProviderDefault;
        AZStd::string m_value;
        bool m_configured = false;
        bool m_valueValid = true;
        bool m_required = false;
        bool m_sensitive = false;
    };

    struct ExternalToolInstallationCandidate
    {
        AZStd::string m_providerId;
        AZStd::string m_probeId;
        AZStd::string m_path;
        AZStd::string m_version;
        ConfigurationLayer m_pathLayer = ConfigurationLayer::ProviderDefault;
        DiscoveryStatus m_status = DiscoveryStatus::NotRun;
        AZStd::string m_message;
        AZ::u64 m_elapsedMilliseconds = 0;
        bool m_pathExists = false;
        bool m_kindMatches = false;
    };

    struct ExternalToolDiscoveryResult
    {
        AZStd::string m_providerId;
        DiscoveryStatus m_status = DiscoveryStatus::NotRun;
        AZStd::string m_selectedPath;
        AZStd::string m_selectedVersion;
        AZ::u64 m_elapsedMilliseconds = 0;
        AZStd::vector<ExternalToolInstallationCandidate> m_candidates;
        AZStd::vector<AZStd::string> m_diagnostics;
    };

    struct ProviderOperationResult
    {
        bool m_success = false;
        AZStd::string m_message;
    };

    AZStd::string ToString(ToolFamily family);
    AZStd::string ToString(CommandMode mode);
    AZStd::string ToString(ConfigurationValueKind kind);
    AZStd::string ToString(ConfigurationLayer layer);
    AZStd::string ToString(DiscoveryProbeKind kind);
    AZStd::string ToString(DiscoveryStatus status);
    AZStd::string ToString(const ExternalToolchainApiVersion& version);
    bool IsHostApiCompatible(const ExternalToolchainApiVersion& minimumVersion);
    bool IsValidSemanticVersion(const AZStd::string& value);
    bool TryCompareSemanticVersions(
        const AZStd::string& left,
        const AZStd::string& right,
        int& comparison);
} // namespace ExternalToolchain
