/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ToolExecutionRedactor.h"
#include <algorithm>

namespace ExternalToolchain
{
    namespace
    {
        char Fold(char c)
        {
            if (c >= 'A' && c <= 'Z')
            {
                return c - 'A' + 'a';
            }
            return c == '\\' ? '/' : c;
        }
        bool Matches(const AZStd::string& text, size_t pos, const AZStd::string& marker)
        {
            if (marker.size() > text.size() - pos)
            {
                return false;
            }
            for (size_t i = 0; i < marker.size(); ++i)
            {
                if (Fold(text[pos + i]) != Fold(marker[i]))
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace
    ToolExecutionRedactor::ToolExecutionRedactor(AZStd::vector<AZStd::string> markers)
    {
        for (auto& marker : markers)
        {
            if (!marker.empty() && marker.size() <= 4096 && m_markers.size() < 64)
            {
                m_overlap = std::max(m_overlap, marker.size());
                m_markers.push_back(AZStd::move(marker));
            }
        }
        m_overlap = std::max(m_overlap, size_t(4));
        std::sort(
            m_markers.begin(),
            m_markers.end(),
            [](const auto& a, const auto& b)
            {
                return a.size() > b.size();
            });
    }
    AZStd::string ToolExecutionRedactor::Push(const char* bytes, size_t length, bool finish)
    {
        // The backend feeds bounded pipe chunks. Oversized input is refused, never copied.
        if (length > 16384)
        {
            return "[oversized log chunk]";
        }
        m_pending.append(bytes, length);
        AZStd::string result;
        size_t i = 0;
        while (i < m_pending.size() && (finish || m_pending.size() - i > m_overlap))
        {
            bool replaced = false;
            for (const auto& marker : m_markers)
            {
                if (Matches(m_pending, i, marker))
                {
                    result += "[redacted]";
                    i += marker.size();
                    replaced = true;
                    break;
                }
            }
            if (replaced)
            {
                continue;
            }
            unsigned char c = static_cast<unsigned char>(m_pending[i++]);
            if (m_escape)
            {
                if (c == '[')
                {
                    m_csi = true;
                    continue;
                }
                if (!m_csi || (c >= 0x40 && c <= 0x7e))
                {
                    m_escape = false;
                    m_csi = false;
                }
                continue;
            }
            if (c == 0x1b)
            {
                m_escape = true;
                continue;
            }
            if (c < 32 && c != '\n' && c != '\r' && c != '\t')
            {
                continue;
            }
            if (c == 127)
            {
                continue;
            }
            if (c < 128)
            {
                result.push_back(static_cast<char>(c));
                continue;
            }
            size_t count = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
            bool valid = count && i - 1 + count <= m_pending.size();
            for (size_t j = 1; valid && j < count; ++j)
            {
                auto next = static_cast<unsigned char>(m_pending[i - 1 + j]);
                valid = next >= 0x80 && next <= 0xbf;
            }
            if (valid && count >= 3)
            {
                auto next = static_cast<unsigned char>(m_pending[i]);
                valid = !(c == 0xe0 && next < 0xa0) && !(c == 0xed && next >= 0xa0) && !(c == 0xf0 && next < 0x90) &&
                    !(c == 0xf4 && next >= 0x90);
            }
            if (valid)
            {
                result.append(m_pending.data() + i - 1, count);
                i += count - 1;
            }
            else
            {
                result.push_back('?');
            }
        }
        m_pending.erase(0, i);
        return result;
    }
} // namespace ExternalToolchain
