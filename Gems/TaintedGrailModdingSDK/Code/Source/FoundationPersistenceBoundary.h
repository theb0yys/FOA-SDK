/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "PackPersistenceService.h"
#include "PathPolicyService.h"
#include "WorkspacePersistenceService.h"

#include <AzCore/std/functional.h>

namespace TaintedGrailModdingSDK
{
    //! Applies the canonical path policy before workspace persistence.
    class FoundationWorkspacePersistenceBoundary
    {
    public:
        explicit FoundationWorkspacePersistenceBoundary(const PathPolicyService& pathPolicy);

        AZ::Outcome<void, AZStd::string> Save(
            const WorkspaceModel& workspace,
            const AZStd::string& filePath) const;
        AZ::Outcome<WorkspaceModel, AZStd::string> Load(const AZStd::string& filePath) const;
        AZ::Outcome<WorkspaceModel, AZStd::string> LoadCandidate(
            const AZStd::string& filePath,
            AZStd::string& resolvedPath) const;
        void PublishResolvedPath(const AZStd::string& resolvedPath) const;
        const AZStd::string& GetLastResolvedPath() const;

    private:
        const PathPolicyService& m_pathPolicy;
        WorkspacePersistenceService m_persistence;
        mutable AZStd::string m_lastResolvedPath;
    };

    //! Applies workspace containment before pack persistence.
    class FoundationPackPersistenceBoundary
    {
    public:
        using WorkspaceProvider = AZStd::function<const WorkspaceModel&()>;
        using WorkspacePathProvider = AZStd::function<const AZStd::string&()>;

        FoundationPackPersistenceBoundary(
            const PathPolicyService& pathPolicy,
            WorkspaceProvider workspaceProvider,
            WorkspacePathProvider workspacePathProvider);

        AZ::Outcome<void, AZStd::string> Save(
            const PackManifest& pack,
            const AZStd::string& filePath) const;
        AZ::Outcome<PackManifest, AZStd::string> Load(const AZStd::string& filePath) const;

    private:
        const PathPolicyService& m_pathPolicy;
        WorkspaceProvider m_workspaceProvider;
        WorkspacePathProvider m_workspacePathProvider;
        PackPersistenceService m_persistence;
    };
} // namespace TaintedGrailModdingSDK
