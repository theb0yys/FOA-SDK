/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ExternalToolchain/ExternalToolchainTypes.h>

#include <AzCore/std/limits.h>
#include <AzCore/std/string/string_view.h>

namespace ExternalToolchain
{
    namespace
    {
        bool IsIdentifierCharacter(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'A' && value <= 'Z')
                || (value >= 'a' && value <= 'z')
                || value == '-';
        }

        bool IsValidIdentifierList(
            AZStd::string_view value,
            bool enforceNumericLeadingZero)
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

                bool numeric = true;
                for (size_t index = start; index < start + length; ++index)
                {
                    if (!IsIdentifierCharacter(value[index]))
                    {
                        return false;
                    }
                    numeric = numeric && value[index] >= '0' && value[index] <= '9';
                }
                if (enforceNumericLeadingZero
                    && numeric
                    && length > 1
                    && value[start] == '0')
                {
                    return false;
                }

                if (end == AZStd::string_view::npos)
                {
                    break;
                }
                start = end + 1;
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
            if (!IsValidIdentifierList(build, false))
            {
                return false;
            }
        }

        const size_t prereleasePosition = withoutBuild.find('-');
        const AZStd::string_view core = withoutBuild.substr(0, prereleasePosition);
        if (prereleasePosition != AZStd::string_view::npos)
        {
            const AZStd::string_view prerelease = withoutBuild.substr(prereleasePosition + 1);
            if (!IsValidIdentifierList(prerelease, true))
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

        AZ::u32 major = 0;
        AZ::u32 minor = 0;
        AZ::u32 patch = 0;
        return ParseNumber(core.substr(0, firstDot), major)
            && ParseNumber(core.substr(firstDot + 1, secondDot - firstDot - 1), minor)
            && ParseNumber(core.substr(secondDot + 1), patch);
    }
} // namespace ExternalToolchain
