/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include "CanonicalInterchangeCanonical.h"
#include "CanonicalInterchangeMigration.h"
#include "CanonicalInterchangeValidation.h"

#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <filesystem>
#include <fstream>
#include <iterator>

namespace TaintedGrailModdingSDK::Interchange::AcceptanceFixtures
{
    inline std::filesystem::path RepositoryRoot()
    {
        std::filesystem::path path(__FILE__);
        for (int index = 0; index < 5; ++index)
        {
            path = path.parent_path();
        }
        return path;
    }

    inline AZStd::string ReadRepositoryFile(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path path = RepositoryRoot() / relativePath;
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return {};
        }
        const std::string bytes(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
        return AZStd::string(bytes.data(), bytes.size());
    }

    inline AZStd::string DocumentsExampleBytes()
    {
        return ReadRepositoryFile(
            "docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-documents/manifest.tginterchange.json");
    }

    inline AZStd::string AssetExampleBytes()
    {
        return ReadRepositoryFile(
            "docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-asset/manifest.tginterchange.json");
    }

    inline bool ContainsIssue(
        const CanonicalInterchangeValidationResultV1& result,
        AZStd::string_view code)
    {
        for (const auto& issue : result.m_issues)
        {
            if (issue.m_code == code)
            {
                return true;
            }
        }
        return false;
    }
} // namespace TaintedGrailModdingSDK::Interchange::AcceptanceFixtures
