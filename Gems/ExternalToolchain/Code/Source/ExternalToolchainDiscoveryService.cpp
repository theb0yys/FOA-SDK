/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ExternalToolchainDiscoveryService.h"

#include <AzCore/IO/SystemFile.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/std/utility/move.h>

namespace ExternalToolchain
{
    namespace
    {
        constexpr const char* DiscoveryEnabledPath =
            "/O3DE/ExternalToolchain/Host/Discovery/Enabled";
        constexpr const char* MaximumProvidersPath =
            "/O3DE/ExternalToolchain/Host/Discovery/MaximumProviders";
        constexpr const char* MaximumProbesPerProviderPath =
            "/O3DE/ExternalToolchain/Host/Discovery/MaximumProbesPerProvider";
        constexpr const char* ProviderBudgetMillisecondsPath =
            "/O3DE/ExternalToolchain/Host/Discovery/ProviderBudgetMilliseconds";

        ProviderOperationResult Success(AZStd::string message)
        {
            return ProviderOperationResult{ true, AZStd::move(message) };
        }

        ProviderOperationResult Failure(AZStd::string message)
        {
            return ProviderOperationResult{ false, AZStd::move(message) };
        }

        bool ContainsString(
            const AZStd::vector<AZStd::string>& values,
            const AZStd::string& value)
        {
            return AZStd::find(values.begin(), values.end(), value) != values.end();
        }

        bool StartsWith(const AZStd::string& value, const char* prefix)
        {
            const AZStd::string_view prefixView(prefix);
            return value.size() >= prefixView.size()
                && AZStd::string_view(value.data(), prefixView.size()) == prefixView;
        }

        bool EndsWith(const AZStd::string& value, const char* suffix)
        {
            const AZStd::string_view suffixView(suffix);
            return value.size() >= suffixView.size()
                && AZStd::string_view(
                       value.data() + value.size() - suffixView.size(),
                       suffixView.size()) == suffixView;
        }

        bool IsAsciiAlpha(char value)
        {
            return (value >= 'A' && value <= 'Z')
                || (value >= 'a' && value <= 'z');
        }

        bool IsNetworkOrUriPath(const AZStd::string& value)
        {
            return StartsWith(value, "//")
                || StartsWith(value, "\\\\")
                || value.find("://") != AZStd::string::npos;
        }

        bool IsAbsoluteLocalPath(const AZStd::string& value)
        {
            if (StartsWith(value, "/"))
            {
                return true;
            }
            return value.size() >= 3
                && IsAsciiAlpha(value[0])
                && value[1] == ':'
                && (value[2] == '/' || value[2] == '\\');
        }

        bool HasTraversalSegment(const AZStd::string& value)
        {
            AZStd::string normalized = value;
            AZStd::replace(normalized.begin(), normalized.end(), '\\', '/');

            size_t start = 0;
            while (start <= normalized.size())
            {
                const size_t end = normalized.find('/', start);
                const size_t length = end == AZStd::string::npos
                    ? normalized.size() - start
                    : end - start;
                if (normalized.substr(start, length) == "..")
                {
                    return true;
                }
                if (end == AZStd::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return false;
        }

        AZStd::string NormalizeLocalPath(AZStd::string value)
        {
            AZStd::replace(value.begin(), value.end(), '\\', '/');

            AZStd::string normalized;
            normalized.reserve(value.size());
            bool previousSlash = false;
            for (char character : value)
            {
                const bool slash = character == '/';
                if (slash && previousSlash)
                {
                    continue;
                }
                normalized.push_back(character);
                previousSlash = slash;
            }

            for (;;)
            {
                const size_t position = normalized.find("/./");
                if (position == AZStd::string::npos)
                {
                    break;
                }
                normalized.erase(position, 2);
            }
            if (EndsWith(normalized, "/."))
            {
                normalized.erase(normalized.size() - 2);
            }

            const bool driveRoot = normalized.size() == 3
                && IsAsciiAlpha(normalized[0])
                && normalized[1] == ':'
                && normalized[2] == '/';
            if (normalized.size() > 1 && EndsWith(normalized, "/") && !driveRoot)
            {
                normalized.pop_back();
            }
            return normalized;
        }

        AZStd::string MakePathIdentity(
            AZStd::string normalizedPath,
            const AZStd::string& platformId)
        {
            if (platformId == "windows")
            {
                for (char& character : normalizedPath)
                {
                    if (character >= 'A' && character <= 'Z')
                    {
                        character = static_cast<char>(character - 'A' + 'a');
                    }
                }
            }
            return normalizedPath;
        }

        const ExternalToolResolvedConfigurationValue* FindResolvedValue(
            const AZStd::vector<ExternalToolResolvedConfigurationValue>& values,
            const AZStd::string& key)
        {
            for (const ExternalToolResolvedConfigurationValue& value : values)
            {
                if (value.m_key == key)
                {
                    return &value;
                }
            }
            return nullptr;
        }

        int StatusPriority(DiscoveryStatus status)
        {
            switch (status)
            {
            case DiscoveryStatus::Misconfigured:
                return 7;
            case DiscoveryStatus::ProbeFailed:
                return 6;
            case DiscoveryStatus::UnsupportedVersion:
                return 5;
            case DiscoveryStatus::NotInstalled:
                return 4;
            case DiscoveryStatus::NotRun:
                return 3;
            case DiscoveryStatus::Disabled:
                return 2;
            case DiscoveryStatus::UnsupportedPlatform:
                return 1;
            case DiscoveryStatus::Installed:
            case DiscoveryStatus::Ambiguous:
                return 8;
            }
            return 0;
        }

        AZ::u64 BoundValue(
            AZ::u64 value,
            AZ::u64 minimum,
            AZ::u64 maximum)
        {
            return value < minimum ? minimum : (value > maximum ? maximum : value);
        }

        AZ::u64 ReadBoundedUInt64(
            const ExternalToolchainSettingsSource& settings,
            const char* path,
            AZ::u64 fallback,
            AZ::u64 minimum,
            AZ::u64 maximum)
        {
            AZ::u64 value = fallback;
            settings.GetUInt64(path, value);
            return BoundValue(value, minimum, maximum);
        }

        AZ::u64 ElapsedMilliseconds(
            const AZStd::chrono::steady_clock::time_point& started)
        {
            const auto elapsed =
                AZStd::chrono::duration_cast<AZStd::chrono::milliseconds>(
                    AZStd::chrono::steady_clock::now() - started);
            return elapsed.count() <= 0
                ? 0
                : static_cast<AZ::u64>(elapsed.count());
        }

        bool ProbeAppliesToPlatform(
            const ExternalToolDiscoveryProbeDescriptor& probe,
            const AZStd::string& platformId)
        {
            return probe.m_platforms.empty()
                || ContainsString(probe.m_platforms, platformId);
        }

        bool VersionWithinBounds(
            const AZStd::string& version,
            const ExternalToolDiscoveryProbeDescriptor& probe,
            AZStd::string& message)
        {
            if (version.empty())
            {
                if (!probe.m_minimumSupportedVersion.empty()
                    || !probe.m_maximumSupportedVersion.empty())
                {
                    message =
                        "A configured semantic tool version is required for compatibility checks.";
                    return false;
                }
                return true;
            }

            if (!IsValidSemanticVersion(version))
            {
                message = "Configured tool version is not valid semantic versioning.";
                return false;
            }

            int comparison = 0;
            if (!probe.m_minimumSupportedVersion.empty())
            {
                if (!TryCompareSemanticVersions(
                        version,
                        probe.m_minimumSupportedVersion,
                        comparison))
                {
                    message = "Minimum supported version comparison failed.";
                    return false;
                }
                if (comparison < 0)
                {
                    message = "Configured tool version is below the supported minimum.";
                    return false;
                }
            }

            if (!probe.m_maximumSupportedVersion.empty())
            {
                if (!TryCompareSemanticVersions(
                        version,
                        probe.m_maximumSupportedVersion,
                        comparison))
                {
                    message = "Maximum supported version comparison failed.";
                    return false;
                }
                if (comparison > 0)
                {
                    message = "Configured tool version is above the supported maximum.";
                    return false;
                }
            }
            return true;
        }
    } // namespace

    ExternalToolPathObservation SystemFileExternalToolPathProbe::Inspect(
        const AZStd::string& path) const
    {
        ExternalToolPathObservation observation;
        observation.m_exists = AZ::IO::SystemFile::Exists(path.c_str());
        if (observation.m_exists)
        {
            observation.m_isDirectory = AZ::IO::SystemFile::IsDirectory(path.c_str());
            observation.m_message = observation.m_isDirectory
                ? "Directory exists."
                : "File exists.";
        }
        else
        {
            observation.m_message = "Path does not exist.";
        }
        return observation;
    }

    ExternalToolchainDiscoveryService::ExternalToolchainDiscoveryService(
        const ExternalToolPathProbe& pathProbe,
        const ExternalToolchainSettingsSource& settingsSource)
        : m_pathProbe(pathProbe)
        , m_settingsSource(settingsSource)
    {
    }

    ProviderOperationResult ExternalToolchainDiscoveryService::Refresh(
        const AZStd::vector<ExternalToolProviderDescriptor>& providers,
        const ExternalToolchainConfigurationService& configurationService,
        const AZStd::string& platformId)
    {
        bool discoveryEnabled = true;
        m_settingsSource.GetBool(DiscoveryEnabledPath, discoveryEnabled);

        const AZ::u64 maximumProviders = ReadBoundedUInt64(
            m_settingsSource,
            MaximumProvidersPath,
            64,
            1,
            1024);
        const AZ::u64 maximumProbes = ReadBoundedUInt64(
            m_settingsSource,
            MaximumProbesPerProviderPath,
            16,
            1,
            128);
        const AZ::u64 providerBudgetMilliseconds = ReadBoundedUInt64(
            m_settingsSource,
            ProviderBudgetMillisecondsPath,
            250,
            1,
            5000);

        if (providers.size() > maximumProviders)
        {
            m_results.clear();
            return Failure(
                "Registered provider count exceeds the configured discovery limit.");
        }

        AZStd::vector<ExternalToolProviderDescriptor> orderedProviders = providers;
        AZStd::sort(
            orderedProviders.begin(),
            orderedProviders.end(),
            [](const ExternalToolProviderDescriptor& left,
               const ExternalToolProviderDescriptor& right)
            {
                return left.m_providerId < right.m_providerId;
            });

        m_results.clear();
        m_results.reserve(orderedProviders.size());
        for (const ExternalToolProviderDescriptor& provider : orderedProviders)
        {
            if (!discoveryEnabled)
            {
                ExternalToolDiscoveryResult result;
                result.m_providerId = provider.m_providerId;
                result.m_status = DiscoveryStatus::Disabled;
                result.m_diagnostics.push_back(
                    "Host discovery is disabled by Settings Registry configuration.");
                m_results.push_back(AZStd::move(result));
                continue;
            }

            m_results.push_back(
                DiscoverProvider(
                    provider,
                    configurationService,
                    platformId,
                    maximumProbes,
                    providerBudgetMilliseconds));
        }

        return Success("Provider discovery refreshed.");
    }

    const AZStd::vector<ExternalToolDiscoveryResult>&
    ExternalToolchainDiscoveryService::GetResults() const
    {
        return m_results;
    }

    const ExternalToolDiscoveryResult*
    ExternalToolchainDiscoveryService::FindResult(
        const AZStd::string& providerId) const
    {
        for (const ExternalToolDiscoveryResult& result : m_results)
        {
            if (result.m_providerId == providerId)
            {
                return &result;
            }
        }
        return nullptr;
    }

    void ExternalToolchainDiscoveryService::Clear()
    {
        m_results.clear();
    }

    ExternalToolDiscoveryResult
    ExternalToolchainDiscoveryService::DiscoverProvider(
        const ExternalToolProviderDescriptor& provider,
        const ExternalToolchainConfigurationService& configurationService,
        const AZStd::string& platformId,
        AZ::u64 maximumProbes,
        AZ::u64 providerBudgetMilliseconds) const
    {
        const auto providerStarted = AZStd::chrono::steady_clock::now();
        ExternalToolDiscoveryResult result;
        result.m_providerId = provider.m_providerId;

        if (!ContainsString(provider.m_platforms, platformId))
        {
            result.m_status = DiscoveryStatus::UnsupportedPlatform;
            result.m_diagnostics.push_back(
                "Provider does not declare support for the current host platform.");
            return result;
        }

        bool providerEnabled = true;
        ConfigurationLayer enabledLayer = ConfigurationLayer::ProviderDefault;
        configurationService.ResolveProviderEnabled(
            provider,
            providerEnabled,
            enabledLayer);
        if (!providerEnabled)
        {
            result.m_status = DiscoveryStatus::Disabled;
            result.m_diagnostics.push_back(
                AZStd::string::format(
                    "Provider is disabled by the %s configuration layer.",
                    ToString(enabledLayer).c_str()));
            return result;
        }

        if (provider.m_discoveryProbes.empty())
        {
            result.m_status = DiscoveryStatus::NotRun;
            result.m_diagnostics.push_back(
                "Provider declares no discovery probes.");
            return result;
        }

        if (provider.m_discoveryProbes.size() > maximumProbes)
        {
            result.m_status = DiscoveryStatus::ProbeFailed;
            result.m_diagnostics.push_back(
                "Provider probe count exceeds the configured per-provider limit.");
            return result;
        }

        const AZStd::vector<ExternalToolResolvedConfigurationValue>
            resolvedConfiguration = configurationService.ResolveAll(provider);
        AZStd::vector<ExternalToolDiscoveryProbeDescriptor> probes =
            provider.m_discoveryProbes;
        AZStd::sort(
            probes.begin(),
            probes.end(),
            [](const ExternalToolDiscoveryProbeDescriptor& left,
               const ExternalToolDiscoveryProbeDescriptor& right)
            {
                return left.m_probeId < right.m_probeId;
            });

        AZStd::unordered_set<AZStd::string> seenPaths;
        size_t applicableProbeCount = 0;
        for (const ExternalToolDiscoveryProbeDescriptor& probe : probes)
        {
            if (!ProbeAppliesToPlatform(probe, platformId))
            {
                continue;
            }
            ++applicableProbeCount;

            ExternalToolInstallationCandidate candidate;
            candidate.m_providerId = provider.m_providerId;
            candidate.m_probeId = probe.m_probeId;

            const AZ::u64 providerElapsedMilliseconds =
                ElapsedMilliseconds(providerStarted);
            if (providerElapsedMilliseconds >= providerBudgetMilliseconds)
            {
                candidate.m_status = DiscoveryStatus::ProbeFailed;
                candidate.m_message =
                    "Provider discovery budget was exhausted before this probe.";
                result.m_candidates.push_back(AZStd::move(candidate));
                break;
            }

            const ExternalToolResolvedConfigurationValue* pathValue =
                FindResolvedValue(
                    resolvedConfiguration,
                    probe.m_pathConfigurationKey);
            if (!pathValue || !pathValue->m_configured)
            {
                candidate.m_status = probe.m_required
                    ? DiscoveryStatus::Misconfigured
                    : DiscoveryStatus::NotInstalled;
                candidate.m_message =
                    "The probe path configuration is missing.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }
            if (!pathValue->m_valueValid)
            {
                candidate.m_status = DiscoveryStatus::Misconfigured;
                candidate.m_message =
                    "The probe path configuration violates declared limits.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }

            candidate.m_pathLayer = pathValue->m_layer;
            candidate.m_path = pathValue->m_value;
            if (IsNetworkOrUriPath(candidate.m_path))
            {
                candidate.m_status = DiscoveryStatus::Misconfigured;
                candidate.m_message =
                    "Network and URI paths are prohibited by bounded local discovery.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }
            if (!IsAbsoluteLocalPath(candidate.m_path))
            {
                candidate.m_status = DiscoveryStatus::Misconfigured;
                candidate.m_message =
                    "Discovery paths must be absolute local paths.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }
            if (HasTraversalSegment(candidate.m_path))
            {
                candidate.m_status = DiscoveryStatus::Misconfigured;
                candidate.m_message =
                    "Discovery paths must not contain parent traversal segments.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }

            candidate.m_path = NormalizeLocalPath(candidate.m_path);
            const AZStd::string identity =
                MakePathIdentity(candidate.m_path, platformId);
            if (!seenPaths.insert(identity).second)
            {
                result.m_diagnostics.push_back(
                    AZStd::string::format(
                        "Duplicate candidate path from probe '%s' was ignored.",
                        probe.m_probeId.c_str()));
                continue;
            }

            const auto probeStarted = AZStd::chrono::steady_clock::now();
            const ExternalToolPathObservation observation =
                m_pathProbe.Inspect(candidate.m_path);
            candidate.m_elapsedMilliseconds = ElapsedMilliseconds(probeStarted);
            if (candidate.m_elapsedMilliseconds > probe.m_timeoutMilliseconds)
            {
                candidate.m_status = DiscoveryStatus::ProbeFailed;
                candidate.m_message =
                    "Path inspection exceeded the best-effort probe timeout.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }

            if (ElapsedMilliseconds(providerStarted) > providerBudgetMilliseconds)
            {
                candidate.m_status = DiscoveryStatus::ProbeFailed;
                candidate.m_message =
                    "Provider discovery budget was exceeded during path inspection.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }

            candidate.m_pathExists = observation.m_exists;
            candidate.m_kindMatches = observation.m_exists
                && ((probe.m_kind == DiscoveryProbeKind::Directory
                        && observation.m_isDirectory)
                    || (probe.m_kind == DiscoveryProbeKind::File
                        && !observation.m_isDirectory));
            if (!observation.m_exists)
            {
                candidate.m_status = DiscoveryStatus::NotInstalled;
                candidate.m_message = observation.m_message;
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }
            if (!candidate.m_kindMatches)
            {
                candidate.m_status = DiscoveryStatus::Misconfigured;
                candidate.m_message =
                    "Discovered path does not match the declared file/directory kind.";
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }

            if (!probe.m_versionConfigurationKey.empty())
            {
                const ExternalToolResolvedConfigurationValue* versionValue =
                    FindResolvedValue(
                        resolvedConfiguration,
                        probe.m_versionConfigurationKey);
                if (versionValue && versionValue->m_configured)
                {
                    if (!versionValue->m_valueValid)
                    {
                        candidate.m_status = DiscoveryStatus::Misconfigured;
                        candidate.m_message =
                            "The configured tool version is invalid.";
                        result.m_candidates.push_back(AZStd::move(candidate));
                        continue;
                    }
                    candidate.m_version = versionValue->m_value;
                }
            }

            AZStd::string versionMessage;
            if (!VersionWithinBounds(candidate.m_version, probe, versionMessage))
            {
                candidate.m_status = candidate.m_version.empty()
                    || !IsValidSemanticVersion(candidate.m_version)
                    ? DiscoveryStatus::Misconfigured
                    : DiscoveryStatus::UnsupportedVersion;
                candidate.m_message = AZStd::move(versionMessage);
                result.m_candidates.push_back(AZStd::move(candidate));
                continue;
            }

            candidate.m_status = DiscoveryStatus::Installed;
            candidate.m_message =
                "Configured local path exists and satisfies the declared probe.";
            result.m_candidates.push_back(AZStd::move(candidate));
        }

        if (applicableProbeCount == 0)
        {
            result.m_status = DiscoveryStatus::NotRun;
            result.m_diagnostics.push_back(
                "Provider declares no discovery probes for the current platform.");
            return result;
        }

        result.m_elapsedMilliseconds = ElapsedMilliseconds(providerStarted);

        bool requiredProbeFailed = false;
        DiscoveryStatus requiredFailureStatus = DiscoveryStatus::NotInstalled;
        int requiredFailurePriority = StatusPriority(requiredFailureStatus);
        AZStd::vector<const ExternalToolInstallationCandidate*> installed;
        for (const ExternalToolInstallationCandidate& candidate :
             result.m_candidates)
        {
            if (candidate.m_status == DiscoveryStatus::Installed)
            {
                installed.push_back(&candidate);
            }

            const auto probe = AZStd::find_if(
                probes.begin(),
                probes.end(),
                [&candidate](const ExternalToolDiscoveryProbeDescriptor& value)
                {
                    return value.m_probeId == candidate.m_probeId;
                });
            if (probe != probes.end()
                && probe->m_required
                && candidate.m_status != DiscoveryStatus::Installed)
            {
                requiredProbeFailed = true;
                const int candidatePriority = StatusPriority(candidate.m_status);
                if (candidatePriority > requiredFailurePriority)
                {
                    requiredFailurePriority = candidatePriority;
                    requiredFailureStatus = candidate.m_status;
                }
            }
        }

        if (requiredProbeFailed)
        {
            result.m_status = requiredFailureStatus;
            result.m_diagnostics.push_back(
                "One or more required discovery probes did not succeed.");
            return result;
        }

        if (installed.size() == 1)
        {
            result.m_status = DiscoveryStatus::Installed;
            result.m_selectedPath = installed.front()->m_path;
            result.m_selectedVersion = installed.front()->m_version;
            return result;
        }
        if (installed.size() > 1)
        {
            result.m_status = DiscoveryStatus::Ambiguous;
            result.m_diagnostics.push_back(
                "Multiple distinct compatible installations were discovered; "
                "configuration must select one exact path.");
            return result;
        }

        result.m_status = DiscoveryStatus::NotInstalled;
        int priority = StatusPriority(result.m_status);
        for (const ExternalToolInstallationCandidate& candidate :
             result.m_candidates)
        {
            const int candidatePriority = StatusPriority(candidate.m_status);
            if (candidatePriority > priority)
            {
                priority = candidatePriority;
                result.m_status = candidate.m_status;
            }
        }
        return result;
    }

    AZStd::string GetCurrentHostPlatformId()
    {
#if defined(_WIN32)
        return "windows";
#elif defined(__APPLE__)
        return "mac";
#elif defined(__linux__)
        return "linux";
#else
        return "unknown";
#endif
    }
} // namespace ExternalToolchain
