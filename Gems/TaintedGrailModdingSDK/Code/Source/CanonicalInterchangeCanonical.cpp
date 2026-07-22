/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "CanonicalInterchangeCanonical.h"

#include "CanonicalFingerprint.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>

namespace TaintedGrailModdingSDK::Interchange
{
    namespace
    {
        bool IsContinuation(unsigned char value)
        {
            return (value & 0xc0) == 0x80;
        }

        void AppendControlEscape(AZStd::string& output, unsigned char value)
        {
            constexpr char Hex[] = "0123456789abcdef";
            output += "\\u00";
            output.push_back(Hex[(value >> 4) & 0x0f]);
            output.push_back(Hex[value & 0x0f]);
        }
    }

    bool IsValidUtf8V1(AZStd::string_view value)
    {
        size_t index = 0;
        while (index < value.size())
        {
            const unsigned char lead = static_cast<unsigned char>(value[index]);
            if (lead <= 0x7f)
            {
                ++index;
                continue;
            }

            size_t continuationCount = 0;
            unsigned int codePoint = 0;
            if (lead >= 0xc2 && lead <= 0xdf)
            {
                continuationCount = 1;
                codePoint = lead & 0x1f;
            }
            else if (lead >= 0xe0 && lead <= 0xef)
            {
                continuationCount = 2;
                codePoint = lead & 0x0f;
            }
            else if (lead >= 0xf0 && lead <= 0xf4)
            {
                continuationCount = 3;
                codePoint = lead & 0x07;
            }
            else
            {
                return false;
            }

            if (index + continuationCount >= value.size())
            {
                return false;
            }
            for (size_t offset = 1; offset <= continuationCount; ++offset)
            {
                const unsigned char continuation =
                    static_cast<unsigned char>(value[index + offset]);
                if (!IsContinuation(continuation))
                {
                    return false;
                }
                codePoint = (codePoint << 6) | (continuation & 0x3f);
            }

            if ((continuationCount == 2 && codePoint < 0x800) ||
                (continuationCount == 3 && codePoint < 0x10000) ||
                codePoint > 0x10ffff ||
                (codePoint >= 0xd800 && codePoint <= 0xdfff))
            {
                return false;
            }
            index += continuationCount + 1;
        }
        return true;
    }

    bool AppendCanonicalPresentationStringV1(
        AZStd::string& output,
        AZStd::string_view value)
    {
        if (value.size() > MaxPresentationStringBytesV1 || !IsValidUtf8V1(value))
        {
            return false;
        }

        output.push_back('"');
        for (char character : value)
        {
            const unsigned char byte = static_cast<unsigned char>(character);
            switch (character)
            {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (byte < 0x20)
                {
                    AppendControlEscape(output, byte);
                }
                else
                {
                    output.push_back(character);
                }
                break;
            }
        }
        output.push_back('"');
        return true;
    }

    bool FormatCanonicalFiniteNumberV1(
        double value,
        AZStd::string& output)
    {
        if (!std::isfinite(value))
        {
            return false;
        }
        if (value == 0.0)
        {
            output = "0";
            return true;
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
            return false;
        }
        output.assign(buffer, static_cast<size_t>(converted.ptr - buffer));

        const size_t exponent = output.find('E');
        if (exponent != AZStd::string::npos)
        {
            output[exponent] = 'e';
        }
        return true;
    }

    Sha256DigestV1 CalculateCanonicalBytesDigestV1(AZStd::string_view bytes)
    {
        const AZStd::string prefixed =
            TaintedGrailModdingSDK::CalculateCanonicalSha256(AZStd::string(bytes));
        constexpr AZStd::string_view Prefix = "sha256:";
        Sha256DigestV1 result;
        if (prefixed.starts_with(Prefix))
        {
            result.m_hex = prefixed.substr(Prefix.size());
        }
        return result;
    }

    bool CanonicalBytesDigestMatchesV1(
        AZStd::string_view bytes,
        const Sha256DigestV1& digest)
    {
        return IsSha256HexV1(digest.m_hex) &&
            CalculateCanonicalBytesDigestV1(bytes) == digest;
    }
} // namespace TaintedGrailModdingSDK::Interchange
