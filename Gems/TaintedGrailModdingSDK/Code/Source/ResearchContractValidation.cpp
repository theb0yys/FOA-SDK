/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ResearchContractValidation.h"

#include <cmath>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        bool IsAsciiDigit(char value)
        {
            return value >= '0' && value <= '9';
        }

        bool IsLowerAscii(char value)
        {
            return value >= 'a' && value <= 'z';
        }

        bool IsIdentifierEdge(char value)
        {
            return IsLowerAscii(value) || IsAsciiDigit(value);
        }

        bool IsSemVerIdentifierCharacter(char value)
        {
            return (value >= 'A' && value <= 'Z')
                || IsLowerAscii(value)
                || IsAsciiDigit(value)
                || value == '-';
        }

        bool ParseUnsignedComponent(
            const AZStd::string& value,
            size_t& offset,
            bool rejectLeadingZero,
            int& result)
        {
            const size_t start = offset;
            result = 0;
            while (offset < value.size() && IsAsciiDigit(value[offset]))
            {
                result = result * 10 + (value[offset] - '0');
                ++offset;
            }
            if (offset == start)
            {
                return false;
            }
            return !rejectLeadingZero
                || offset - start == 1
                || value[start] != '0';
        }

        bool ValidateSemVerIdentifiers(
            const AZStd::string& value,
            size_t begin,
            size_t end,
            bool rejectNumericLeadingZero)
        {
            if (begin >= end)
            {
                return false;
            }
            size_t componentStart = begin;
            for (size_t index = begin; index <= end; ++index)
            {
                if (index != end && value[index] != '.')
                {
                    if (!IsSemVerIdentifierCharacter(value[index]))
                    {
                        return false;
                    }
                    continue;
                }
                if (index == componentStart)
                {
                    return false;
                }
                bool numeric = true;
                for (size_t character = componentStart; character < index; ++character)
                {
                    numeric = numeric && IsAsciiDigit(value[character]);
                }
                if (rejectNumericLeadingZero
                    && numeric
                    && index - componentStart > 1
                    && value[componentStart] == '0')
                {
                    return false;
                }
                componentStart = index + 1;
            }
            return true;
        }

        int DaysInMonth(int year, int month)
        {
            constexpr int Days[] = {
                0, 31, 28, 31, 30, 31, 30,
                31, 31, 30, 31, 30, 31,
            };
            if (month != 2)
            {
                return Days[month];
            }
            const bool leap = (year % 4 == 0)
                && ((year % 100 != 0) || (year % 400 == 0));
            return leap ? 29 : 28;
        }

        int ParseTwoDigits(const AZStd::string& value, size_t offset)
        {
            return (value[offset] - '0') * 10 + (value[offset + 1] - '0');
        }

        int ParseFourDigits(const AZStd::string& value, size_t offset)
        {
            return (value[offset] - '0') * 1000
                + (value[offset + 1] - '0') * 100
                + (value[offset + 2] - '0') * 10
                + (value[offset + 3] - '0');
        }
    } // namespace

    bool IsStableContractId(const AZStd::string& value, size_t maximumLength)
    {
        if (value.size() < 3 || value.size() > maximumLength)
        {
            return false;
        }
        if (!IsIdentifierEdge(value.front()) || !IsIdentifierEdge(value.back()))
        {
            return false;
        }
        bool hasNamespaceSeparator = false;
        char previous = '\0';
        for (char character : value)
        {
            const bool allowed = IsIdentifierEdge(character)
                || character == '.'
                || character == '-'
                || character == '_'
                || character == ':';
            if (!allowed)
            {
                return false;
            }
            if ((character == '.' || character == ':')
                && (previous == '.' || previous == ':' || previous == '-'
                    || previous == '_'))
            {
                return false;
            }
            hasNamespaceSeparator = hasNamespaceSeparator
                || character == '.'
                || character == ':';
            previous = character;
        }
        return hasNamespaceSeparator;
    }

    bool IsSafePersistenceId(const AZStd::string& value, size_t maximumLength)
    {
        if (!IsStableContractId(value, maximumLength)
            || value.find(':') != AZStd::string::npos)
        {
            return false;
        }
        return value.find('/') == AZStd::string::npos
            && value.find('\\') == AZStd::string::npos
            && value.find("..") == AZStd::string::npos;
    }

    bool IsSha256Fingerprint(const AZStd::string& value)
    {
        constexpr size_t PrefixLength = 7;
        constexpr size_t DigestLength = 64;
        if (value.size() != PrefixLength + DigestLength
            || value.substr(0, PrefixLength) != "sha256:")
        {
            return false;
        }
        for (size_t index = PrefixLength; index < value.size(); ++index)
        {
            const char character = value[index];
            if (!IsAsciiDigit(character)
                && !(character >= 'a' && character <= 'f'))
            {
                return false;
            }
        }
        return true;
    }

    bool IsStrictSemanticVersion(const AZStd::string& value)
    {
        size_t offset = 0;
        int ignored = 0;
        if (!ParseUnsignedComponent(value, offset, true, ignored)
            || offset >= value.size() || value[offset++] != '.'
            || !ParseUnsignedComponent(value, offset, true, ignored)
            || offset >= value.size() || value[offset++] != '.'
            || !ParseUnsignedComponent(value, offset, true, ignored))
        {
            return false;
        }

        if (offset < value.size() && value[offset] == '-')
        {
            const size_t begin = ++offset;
            while (offset < value.size() && value[offset] != '+')
            {
                ++offset;
            }
            if (!ValidateSemVerIdentifiers(value, begin, offset, true))
            {
                return false;
            }
        }
        if (offset < value.size() && value[offset] == '+')
        {
            const size_t begin = ++offset;
            offset = value.size();
            if (!ValidateSemVerIdentifiers(value, begin, offset, false))
            {
                return false;
            }
        }
        return offset == value.size();
    }

    bool IsStrictUtcTimestamp(const AZStd::string& value)
    {
        const bool hasMilliseconds = value.size() == 24
            && value[19] == '.'
            && value[23] == 'Z';
        const bool secondsOnly = value.size() == 20 && value[19] == 'Z';
        if ((!hasMilliseconds && !secondsOnly)
            || value[4] != '-'
            || value[7] != '-'
            || value[10] != 'T'
            || value[13] != ':'
            || value[16] != ':')
        {
            return false;
        }
        constexpr size_t RequiredDigitPositions[] = {
            0, 1, 2, 3, 5, 6, 8, 9,
            11, 12, 14, 15, 17, 18,
        };
        for (size_t position : RequiredDigitPositions)
        {
            if (!IsAsciiDigit(value[position]))
            {
                return false;
            }
        }
        if (hasMilliseconds
            && (!IsAsciiDigit(value[20])
                || !IsAsciiDigit(value[21])
                || !IsAsciiDigit(value[22])))
        {
            return false;
        }
        const int year = ParseFourDigits(value, 0);
        const int month = ParseTwoDigits(value, 5);
        const int day = ParseTwoDigits(value, 8);
        const int hour = ParseTwoDigits(value, 11);
        const int minute = ParseTwoDigits(value, 14);
        const int second = ParseTwoDigits(value, 17);
        return year >= 1
            && month >= 1 && month <= 12
            && day >= 1 && day <= DaysInMonth(year, month)
            && hour <= 23
            && minute <= 59
            && second <= 59;
    }

    bool IsSupportedRuntimeTarget(const AZStd::string& value)
    {
        return value == "Mono" || value == "IL2CPP";
    }

    bool IsUsableImportStatus(const AZStd::string& value)
    {
        return value == "imported"
            || value == "ok"
            || value == "warning";
    }

    bool IsFiniteNonNegative(double value)
    {
        return std::isfinite(value) && value >= 0.0;
    }

    bool IsFiniteProbability(double value, bool allowZero)
    {
        return std::isfinite(value)
            && value <= 1.0
            && (allowZero ? value >= 0.0 : value > 0.0);
    }
} // namespace TaintedGrailModdingSDK
