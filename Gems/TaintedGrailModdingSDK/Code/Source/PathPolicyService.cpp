/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PathPolicyService.h"

#include <AzCore/PlatformDef.h>
#if AZ_TRAIT_USE_WINDOWS_FILE_API
#   include <AzCore/PlatformIncl.h>
#endif
#include <AzCore/std/utility/move.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>

namespace TaintedGrailModdingSDK
{
    namespace
    {
        namespace Filesystem = std::filesystem;

#if AZ_TRAIT_USE_WINDOWS_FILE_API
        constexpr bool PlatformPathsAreCaseInsensitive = true;
#else
        constexpr bool PlatformPathsAreCaseInsensitive = false;
#endif

        Filesystem::path FromUtf8(const AZStd::string& value)
        {
#if defined(__cpp_lib_char8_t)
            return Filesystem::path(std::u8string(
                reinterpret_cast<const char8_t*>(value.data()),
                value.size()));
#else
            return Filesystem::u8path(value.c_str());
#endif
        }

        AZStd::string ToUtf8(const Filesystem::path& value)
        {
#if defined(__cpp_lib_char8_t)
            const std::u8string text = value.generic_u8string();
            return AZStd::string(reinterpret_cast<const char*>(text.data()), text.size());
#else
            const std::string text = value.generic_u8string();
            return AZStd::string(text.data(), text.size());
#endif
        }

        AZStd::string FoldAscii(AZStd::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return value;
        }

        bool ComponentsEqual(
            const Filesystem::path& left,
            const Filesystem::path& right,
            bool caseInsensitive)
        {
#if AZ_TRAIT_USE_WINDOWS_FILE_API
            if (caseInsensitive)
            {
                const std::wstring& leftNative = left.native();
                const std::wstring& rightNative = right.native();
                return CompareStringOrdinal(
                    leftNative.c_str(),
                    static_cast<int>(leftNative.size()),
                    rightNative.c_str(),
                    static_cast<int>(rightNative.size()),
                    TRUE) == CSTR_EQUAL;
            }
#endif

            AZStd::string leftText = ToUtf8(left);
            AZStd::string rightText = ToUtf8(right);
            if (caseInsensitive)
            {
                leftText = FoldAscii(AZStd::move(leftText));
                rightText = FoldAscii(AZStd::move(rightText));
            }
            return leftText == rightText;
        }

        bool HasSuffix(
            const Filesystem::path& path,
            AZStd::string suffix,
            bool caseInsensitive)
        {
            AZStd::string filename = ToUtf8(path.filename());
            if (caseInsensitive)
            {
                filename = FoldAscii(AZStd::move(filename));
                suffix = FoldAscii(AZStd::move(suffix));
            }
            return filename.size() >= suffix.size()
                && filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        AZ::Outcome<Filesystem::path, AZStd::string> ResolveFilesystemPath(
            const AZStd::string& value,
            const Filesystem::path& basePath,
            bool requireExisting,
            bool requireDirectory)
        {
            if (value.empty())
            {
                return AZ::Failure(AZStd::string("A filesystem path is required."));
            }

            Filesystem::path path = FromUtf8(value);
            if (path.is_relative())
            {
                if (basePath.empty())
                {
                    std::error_code currentError;
                    path = Filesystem::current_path(currentError) / path;
                    if (currentError)
                    {
                        return AZ::Failure(
                            AZStd::string("Unable to resolve the current directory: ")
                            + currentError.message().c_str());
                    }
                }
                else
                {
                    path = basePath / path;
                }
            }

            std::error_code error;
            path = Filesystem::absolute(path, error);
            if (error)
            {
                return AZ::Failure(
                    AZStd::string("Unable to make the path absolute: ") + value + ": "
                    + error.message().c_str());
            }

            Filesystem::path canonicalPath = requireExisting
                ? Filesystem::canonical(path, error)
                : Filesystem::weakly_canonical(path, error);
            if (error)
            {
                return AZ::Failure(
                    AZStd::string("Unable to resolve the canonical path: ") + value + ": "
                    + error.message().c_str());
            }

            const bool exists = Filesystem::exists(canonicalPath, error);
            if (error)
            {
                return AZ::Failure(
                    AZStd::string("Unable to inspect the canonical path: ")
                    + ToUtf8(canonicalPath) + ": " + error.message().c_str());
            }
            if (requireExisting && !exists)
            {
                return AZ::Failure(
                    AZStd::string("The required path does not exist: ") + ToUtf8(canonicalPath));
            }
            if (exists)
            {
                const bool correctKind = requireDirectory
                    ? Filesystem::is_directory(canonicalPath, error)
                    : Filesystem::is_regular_file(canonicalPath, error);
                if (error || !correctKind)
                {
                    const char* kindError = requireDirectory
                        ? "The path is not a directory: "
                        : "The path is not a regular file: ";
                    return AZ::Failure(AZStd::string(kindError) + ToUtf8(canonicalPath));
                }
            }

            return AZ::Success(AZStd::move(canonicalPath));
        }
    } // namespace

    AZ::Outcome<AZStd::string, AZStd::string> PathPolicyService::ResolveWorkspaceDocumentPath(
        const AZStd::string& filePath,
        bool requireExisting) const
    {
        AZ::Outcome<Filesystem::path, AZStd::string> resolved = ResolveFilesystemPath(
            filePath,
            {},
            requireExisting,
            false);
        if (!resolved.IsSuccess())
        {
            return AZ::Failure(AZStd::string(resolved.GetError()));
        }
        if (!HasSuffix(resolved.GetValue(), ".tgworkspace.json", PlatformPathsAreCaseInsensitive))
        {
            return AZ::Failure(
                AZStd::string("Workspace documents must use the .tgworkspace.json suffix."));
        }
        return AZ::Success(ToUtf8(resolved.GetValue()));
    }

    AZ::Outcome<AZStd::string, AZStd::string> PathPolicyService::ResolveWorkspaceRoot(
        const WorkspaceModel& workspace,
        const AZStd::string& workspaceFilePath,
        bool requireExisting) const
    {
        if (workspace.m_rootPath.empty())
        {
            return AZ::Failure(AZStd::string("The active workspace root is required."));
        }

        Filesystem::path basePath;
        if (FromUtf8(workspace.m_rootPath).is_relative())
        {
            if (workspaceFilePath.empty())
            {
                return AZ::Failure(AZStd::string(
                    "Save or open the workspace document before using a relative workspace root."));
            }
            AZ::Outcome<AZStd::string, AZStd::string> documentPath =
                ResolveWorkspaceDocumentPath(workspaceFilePath, false);
            if (!documentPath.IsSuccess())
            {
                return AZ::Failure(AZStd::string(documentPath.GetError()));
            }
            basePath = FromUtf8(documentPath.GetValue()).parent_path();
        }

        AZ::Outcome<Filesystem::path, AZStd::string> resolved = ResolveFilesystemPath(
            workspace.m_rootPath,
            basePath,
            requireExisting,
            true);
        if (!resolved.IsSuccess())
        {
            return AZ::Failure(AZStd::string(resolved.GetError()));
        }
        return AZ::Success(ToUtf8(resolved.GetValue()));
    }

    AZ::Outcome<AZStd::string, AZStd::string> PathPolicyService::ResolvePackManifestPath(
        const WorkspaceModel& workspace,
        const AZStd::string& workspaceFilePath,
        const AZStd::string& filePath,
        bool requireExisting) const
    {
        AZ::Outcome<AZStd::string, AZStd::string> workspaceRoot = ResolveWorkspaceRoot(
            workspace,
            workspaceFilePath,
            true);
        if (!workspaceRoot.IsSuccess())
        {
            return AZ::Failure(AZStd::string(workspaceRoot.GetError()));
        }

        AZ::Outcome<Filesystem::path, AZStd::string> candidate = ResolveFilesystemPath(
            filePath,
            FromUtf8(workspaceRoot.GetValue()),
            requireExisting,
            false);
        if (!candidate.IsSuccess())
        {
            return AZ::Failure(AZStd::string(candidate.GetError()));
        }
        if (!HasSuffix(candidate.GetValue(), ".tgpack.json", PlatformPathsAreCaseInsensitive))
        {
            return AZ::Failure(AZStd::string("Pack manifests must use the .tgpack.json suffix."));
        }

        const AZStd::string canonicalCandidate = ToUtf8(candidate.GetValue());
        if (!IsCanonicalPathContained(
                workspaceRoot.GetValue(),
                canonicalCandidate,
                PlatformPathsAreCaseInsensitive))
        {
            return AZ::Failure(
                AZStd::string("Pack manifests must remain inside the canonical workspace root: ")
                + workspaceRoot.GetValue());
        }
        return AZ::Success(canonicalCandidate);
    }

    AZ::Outcome<AZStd::string, AZStd::string> PathPolicyService::ResolveExtensionDocumentPath(
        const AZStd::string& canonicalWorkspaceRoot,
        const AZStd::string& extensionId,
        const AZStd::string& relativePath,
        bool requireExisting) const
    {
        if (canonicalWorkspaceRoot.empty() || extensionId.empty() || relativePath.empty())
        {
            return AZ::Failure(AZStd::string(
                "Extension document resolution requires a workspace root, extension identity, and relative path."));
        }
        const Filesystem::path extensionRoot =
            FromUtf8(canonicalWorkspaceRoot) / "Extensions" / FromUtf8(extensionId);
        AZ::Outcome<Filesystem::path, AZStd::string> candidate = ResolveFilesystemPath(
            relativePath,
            extensionRoot,
            requireExisting,
            false);
        if (!candidate.IsSuccess())
        {
            return AZ::Failure(AZStd::string(candidate.GetError()));
        }
        if (!HasSuffix(candidate.GetValue(), ".json", PlatformPathsAreCaseInsensitive))
        {
            return AZ::Failure(AZStd::string(
                "Extension documents must use the .json suffix."));
        }

        std::error_code rootError;
        const Filesystem::path canonicalExtensionRoot =
            Filesystem::weakly_canonical(extensionRoot, rootError);
        if (rootError)
        {
            return AZ::Failure(
                AZStd::string("Unable to resolve the extension document root: ")
                + rootError.message().c_str());
        }
        const AZStd::string rootText = ToUtf8(canonicalExtensionRoot);
        const AZStd::string candidateText = ToUtf8(candidate.GetValue());
        if (!IsCanonicalPathContained(
                canonicalWorkspaceRoot, rootText, PlatformPathsAreCaseInsensitive)
            || !IsCanonicalPathContained(
                rootText, candidateText, PlatformPathsAreCaseInsensitive))
        {
            return AZ::Failure(AZStd::string(
                "Extension documents must remain inside their canonical extension root."));
        }
        return AZ::Success(candidateText);
    }

    bool PathPolicyService::IsCanonicalPathContained(
        const AZStd::string& rootPath,
        const AZStd::string& candidatePath,
        bool caseInsensitive)
    {
        const Filesystem::path root = FromUtf8(rootPath).lexically_normal();
        const Filesystem::path candidate = FromUtf8(candidatePath).lexically_normal();
        auto rootIterator = root.begin();
        auto candidateIterator = candidate.begin();
        for (; rootIterator != root.end(); ++rootIterator, ++candidateIterator)
        {
            if (candidateIterator == candidate.end()
                || !ComponentsEqual(*rootIterator, *candidateIterator, caseInsensitive))
            {
                return false;
            }
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK
