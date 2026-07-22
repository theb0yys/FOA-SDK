/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "FoundationModels.h"

#include <AzCore/Outcome/Outcome.h>

namespace TaintedGrailModdingSDK
{
    //! Canonical filesystem policy shared by workspace and pack persistence boundaries.
    class PathPolicyService
    {
    public:
        AZ::Outcome<AZStd::string, AZStd::string> ResolveWorkspaceDocumentPath(
            const AZStd::string& filePath,
            bool requireExisting) const;

        AZ::Outcome<AZStd::string, AZStd::string> ResolveWorkspaceRoot(
            const WorkspaceModel& workspace,
            const AZStd::string& workspaceFilePath,
            bool requireExisting) const;

        AZ::Outcome<void, AZStd::string> ValidateWorkspacePaths(
            const WorkspaceModel& workspace,
            const AZStd::string& canonicalWorkspaceRoot) const;

        AZ::Outcome<AZStd::string, AZStd::string> ResolvePackManifestPath(
            const WorkspaceModel& workspace,
            const AZStd::string& workspaceFilePath,
            const AZStd::string& filePath,
            bool requireExisting) const;

        AZ::Outcome<AZStd::string, AZStd::string> ResolveExtensionDocumentPath(
            const AZStd::string& canonicalWorkspaceRoot,
            const AZStd::string& extensionId,
            const AZStd::string& relativePath,
            bool requireExisting) const;

        static bool IsCanonicalPathContained(
            const AZStd::string& rootPath,
            const AZStd::string& candidatePath,
            bool caseInsensitive);
    };
} // namespace TaintedGrailModdingSDK
