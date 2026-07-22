/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/string.h>

#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>

namespace TaintedGrailModdingSDK::DeterministicContractJson
{
    inline AZStd::string UnsignedString(AZ::u64 value)
    {
        char buffer[32];
        size_t position = sizeof(buffer);
        do
        {
            buffer[--position] = static_cast<char>('0' + (value % 10));
            value /= 10;
        } while (value != 0);
        return AZStd::string(buffer + position, sizeof(buffer) - position);
    }

    inline AZStd::string SignedString(AZ::s64 value)
    {
        if (value >= 0)
        {
            return UnsignedString(static_cast<AZ::u64>(value));
        }
        const AZ::u64 magnitude = static_cast<AZ::u64>(-(value + 1)) + 1;
        return "-" + UnsignedString(magnitude);
    }

    inline AZStd::string DoubleString(double value)
    {
        if (!std::isfinite(value))
        {
            // Canonical serializers must always emit syntactically valid JSON. Domain
            // validators still reject non-finite coordinates before fingerprints are used.
            return "null";
        }

        char buffer[128] = {};
        const auto converted = std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            value,
            std::chars_format::general,
            std::numeric_limits<double>::max_digits10);
        if (converted.ec != std::errc{})
        {
            return "null";
        }
        return AZStd::string(buffer, static_cast<size_t>(converted.ptr - buffer));
    }

    inline void AppendEscaped(AZStd::string& output, const AZStd::string& value)
    {
        constexpr char HexDigits[] = "0123456789abcdef";
        output.push_back('"');
        for (char character : value)
        {
            const unsigned char byte = static_cast<unsigned char>(character);
            switch (character)
            {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                // This serializer is byte-oriented. Escaping non-ASCII bytes keeps the
                // canonical output valid UTF-8 JSON while preserving a one-to-one byte map.
                if (byte < 0x20 || byte >= 0x7f)
                {
                    output += "\\u00";
                    output.push_back(HexDigits[(byte >> 4) & 0x0f]);
                    output.push_back(HexDigits[byte & 0x0f]);
                }
                else
                {
                    output.push_back(character);
                }
                break;
            }
        }
        output.push_back('"');
    }

    inline void AppendName(AZStd::string& output, const char* name)
    {
        AppendEscaped(output, name);
        output.push_back(':');
    }

    inline void AppendString(
        AZStd::string& output,
        const char* name,
        const AZStd::string& value,
        bool comma = true)
    {
        AppendName(output, name);
        AppendEscaped(output, value);
        if (comma)
        {
            output.push_back(',');
        }
    }

    inline void AppendUnsigned(
        AZStd::string& output,
        const char* name,
        AZ::u64 value,
        bool comma = true)
    {
        AppendName(output, name);
        output += UnsignedString(value);
        if (comma)
        {
            output.push_back(',');
        }
    }

    inline void AppendSigned(
        AZStd::string& output,
        const char* name,
        AZ::s64 value,
        bool comma = true)
    {
        AppendName(output, name);
        output += SignedString(value);
        if (comma)
        {
            output.push_back(',');
        }
    }

    inline void AppendDouble(
        AZStd::string& output,
        const char* name,
        double value,
        bool comma = true)
    {
        AppendName(output, name);
        output += DoubleString(value);
        if (comma)
        {
            output.push_back(',');
        }
    }

    inline void AppendBool(
        AZStd::string& output,
        const char* name,
        bool value,
        bool comma = true)
    {
        AppendName(output, name);
        output += value ? "true" : "false";
        if (comma)
        {
            output.push_back(',');
        }
    }

    inline void AppendStringArray(
        AZStd::string& output,
        const char* name,
        const AZStd::vector<AZStd::string>& values,
        bool comma = true)
    {
        AppendName(output, name);
        output.push_back('[');
        for (size_t index = 0; index < values.size(); ++index)
        {
            if (index != 0)
            {
                output.push_back(',');
            }
            AppendEscaped(output, values[index]);
        }
        output.push_back(']');
        if (comma)
        {
            output.push_back(',');
        }
    }

    inline void AppendSortedStringArray(
        AZStd::string& output,
        const char* name,
        AZStd::vector<AZStd::string> values,
        bool comma = true)
    {
        AZStd::sort(values.begin(), values.end());
        AppendStringArray(output, name, values, comma);
    }

    inline void TrimTrailingComma(AZStd::string& output)
    {
        if (!output.empty() && output.back() == ',')
        {
            output.pop_back();
        }
    }
} // namespace TaintedGrailModdingSDK::DeterministicContractJson
