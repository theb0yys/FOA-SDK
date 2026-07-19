/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ExternalToolchain/ExternalToolchainTypes.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/string/string_view.h>

namespace ExternalToolchain
{
    namespace
    {
        struct SemanticVersionParts
        {
            AZ::u32 m_major = 0;
            AZ::u32 m_minor = 0;
            AZ::u32 m_patch = 0;
            AZStd::vector<AZStd::string> m_prerelease;
        };

        bool IsIdentifierCharacter(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'A' && value <= 'Z')
                || (value >= 'a' && value <= 'z')
                || value == '-';
        }

        bool IsNumericIdentifier(AZStd::string_view value)
        {
            if (value.empty())
            {
                return false;
            }
            for (char character : value)
            {
                if (character < '0' || character > '9')
                {
                    return false;
                }
            }
            return true;
        }

        bool ParseNumber(AZStd::string_view value, AZ::u32& result)
        {
            if (value.empty() || (value.size() > 1 && value.front() == '0'))
            {
                return false;
            }

            result = 0;
            for (char character : value)
            {
                if (character < '0' || character > '9')
                {
                    return false;
                }
                const AZ::u32 digit = static_cast<AZ::u32>(character - '0');
                if (result > (AZStd::numeric_limits<AZ::u32>::max() - digit) / 10)
                {
                    return false;
                }
                result = result * 10 + digit;
            }
            return true;
        }

        bool ParseIdentifierList(
            AZStd::string_view value,
            bool enforceNumericLeadingZero,
            AZStd::vector<AZStd::string>* output)
        {
            if (value.empty())
            {
                return false;
            }

            size_t start = 0;
            while (start < value.size())
            {
                const size_t end = value.find('.', start);
                const size_t length = end == AZStd::string_view::npos
                    ? value.size() - start
                    : end - start;
                if (length == 0)
                {
                    return false;
                }

                const AZStd::string_view identifier = value.substr(start, length);
                for (char character : identifier)
                {
                    if (!IsIdentifierCharacter(character))
                    {
                        return false;
                    }
                }

                const bool numeric = IsNumericIdentifier(identifier);
                if (enforceNumericLeadingZero
                    && numeric
                    && identifier.size() > 1
                    && identifier.front() == '0')
                {
                    return false;
                }

                if (output)
                {
                    output->emplace_back(identifier.data(), identifier.size());
                }

                if (end == AZStd::string_view::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return true;
        }

        bool ParseSemanticVersion(
            const AZStd::string& value,
            SemanticVersionParts& result)
        {
            if (value.empty())
            {
                return false;
            }

            const AZStd::string_view version(value);
            const size_t buildPosition = version.find('+');
            const AZStd::string_view withoutBuild = version.substr(0, buildPosition);
            if (buildPosition != AZStd::string_view::npos)
            {
                const AZStd::string_view build = version.substr(buildPosition + 1);
                if (!ParseIdentifierList(build, false, nullptr))
                {
                    return false;
                }
            }

            const size_t prereleasePosition = withoutBuild.find('-');
            const AZStd::string_view core = withoutBuild.substr(0, prereleasePosition);
            result.m_prerelease.clear();
            if (prereleasePosition != AZStd::string_view::npos)
            {
                const AZStd::string_view prerelease =
                    withoutBuild.substr(prereleasePosition + 1);
                if (!ParseIdentifierList(prerelease, true, &result.m_prerelease))
                {
                    return false;
                }
            }

            const size_t firstDot = core.find('.');
            const size_t secondDot = firstDot == AZStd::string_view::npos
                ? AZStd::string_view::npos
                : core.find('.', firstDot + 1);
            if (firstDot == AZStd::string_view::npos
                || secondDot == AZStd::string_view::npos
                || core.find('.', secondDot + 1) != AZStd::string_view::npos)
            {
                return false;
            }

            return ParseNumber(core.substr(0, firstDot), result.m_major)
                && ParseNumber(
                    core.substr(firstDot + 1, secondDot - firstDot - 1),
                    result.m_minor)
                && ParseNumber(core.substr(secondDot + 1), result.m_patch);
        }

        int CompareUnsigned(AZ::u32 left, AZ::u32 right)
        {
            return left < right ? -1 : (left > right ? 1 : 0);
        }

        int CompareNumericIdentifier(
            const AZStd::string& left,
            const AZStd::string& right)
        {
            if (left.size() != right.size())
            {
                return left.size() < right.size() ? -1 : 1;
            }
            return left < right ? -1 : (left > right ? 1 : 0);
        }

        int ComparePrereleaseIdentifier(
            const AZStd::string& left,
            const AZStd::string& right)
        {
            const bool leftNumeric = IsNumericIdentifier(left);
            const bool rightNumeric = IsNumericIdentifier(right);
            if (leftNumeric && rightNumeric)
            {
                return CompareNumericIdentifier(left, right);
            }
            if (leftNumeric != rightNumeric)
            {
                return leftNumeric ? -1 : 1;
            }
            return left < right ? -1 : (left > right ? 1 : 0);
        }
    } // namespace

    AZStd::string ToString(ToolFamily family)
    {
        switch (family)
        {
        case ToolFamily::Dcc:
            return "dcc";
        case ToolFamily::Generator:
            return "generator";
        case ToolFamily::EngineBridge:
            return "engine_bridge";
        case ToolFamily::Utility:
            return "utility";
        }
        return "unknown";
    }

    AZStd::string ToString(CommandMode mode)
    {
        switch (mode)
        {
        case CommandMode::Interactive:
            return "interactive";
        case CommandMode::Batch:
            return "batch";
        case CommandMode::Probe:
            return "probe";
        }
        return "unknown";
    }

    AZStd::string ToString(ConfigurationValueKind kind)
    {
        switch (kind)
        {
        case ConfigurationValueKind::String:
            return "string";
        case ConfigurationValueKind::Path:
            return "path";
        case ConfigurationValueKind::SemanticVersion:
            return "semantic_version";
        }
        return "unknown";
    }

    AZStd::string ToString(ConfigurationLayer layer)
    {
        switch (layer)
        {
        case ConfigurationLayer::ProviderDefault:
            return "provider_default";
        case ConfigurationLayer::Project:
            return "project";
        case ConfigurationLayer::User:
            return "user";
        case ConfigurationLayer::Session:
            return "session";
        }
        return "unknown";
    }

    AZStd::string ToString(DiscoveryProbeKind kind)
    {
        switch (kind)
        {
        case DiscoveryProbeKind::File:
            return "file";
        case DiscoveryProbeKind::Directory:
            return "directory";
        }
        return "unknown";
    }

    AZStd::string ToString(DiscoveryStatus status)
    {
        switch (status)
        {
        case DiscoveryStatus::NotRun:
            return "not_run";
        case DiscoveryStatus::Disabled:
            return "disabled";
        case DiscoveryStatus::UnsupportedPlatform:
            return "unsupported_platform";
        case DiscoveryStatus::NotInstalled:
            return "not_installed";
        case DiscoveryStatus::Installed:
            return "installed";
        case DiscoveryStatus::UnsupportedVersion:
            return "unsupported_version";
        case DiscoveryStatus::Misconfigured:
            return "misconfigured";
        case DiscoveryStatus::ProbeFailed:
            return "probe_failed";
        case DiscoveryStatus::Ambiguous:
            return "ambiguous";
        }
        return "unknown";
    }

    AZStd::string ToString(const ExternalToolchainApiVersion& version)
    {
        return AZStd::string::format(
            "%u.%u.%u",
            version.m_major,
            version.m_minor,
            version.m_patch);
    }

    bool IsHostApiCompatible(const ExternalToolchainApiVersion& minimumVersion)
    {
        if (minimumVersion.m_major != HostApiVersion.m_major)
        {
            return false;
        }
        if (minimumVersion.m_minor != HostApiVersion.m_minor)
        {
            return minimumVersion.m_minor < HostApiVersion.m_minor;
        }
        return minimumVersion.m_patch <= HostApiVersion.m_patch;
    }

    bool IsValidSemanticVersion(const AZStd::string& value)
    {
        SemanticVersionParts parsed;
        return ParseSemanticVersion(value, parsed);
    }

    bool TryCompareSemanticVersions(
        const AZStd::string& left,
        const AZStd::string& right,
        int& comparison)
    {
        SemanticVersionParts leftParts;
        SemanticVersionParts rightParts;
        if (!ParseSemanticVersion(left, leftParts)
            || !ParseSemanticVersion(right, rightParts))
        {
            return false;
        }

        comparison = CompareUnsigned(leftParts.m_major, rightParts.m_major);
        if (comparison == 0)
        {
            comparison = CompareUnsigned(leftParts.m_minor, rightParts.m_minor);
        }
        if (comparison == 0)
        {
            comparison = CompareUnsigned(leftParts.m_patch, rightParts.m_patch);
        }
        if (comparison != 0)
        {
            return true;
        }

        if (leftParts.m_prerelease.empty() != rightParts.m_prerelease.empty())
        {
            comparison = leftParts.m_prerelease.empty() ? 1 : -1;
            return true;
        }

        const size_t sharedCount = AZStd::min(
            leftParts.m_prerelease.size(),
            rightParts.m_prerelease.size());
        for (size_t index = 0; index < sharedCount; ++index)
        {
            comparison = ComparePrereleaseIdentifier(
                leftParts.m_prerelease[index],
                rightParts.m_prerelease[index]);
            if (comparison != 0)
            {
                return true;
            }
        }

        comparison = leftParts.m_prerelease.size()
                < rightParts.m_prerelease.size()
            ? -1
            : (leftParts.m_prerelease.size()
                    > rightParts.m_prerelease.size()
                ? 1
                : 0);
        return true;
    }
} // namespace ExternalToolchain
