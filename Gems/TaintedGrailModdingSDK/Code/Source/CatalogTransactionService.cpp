/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CatalogTransactionService.h"

#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    AZ::Outcome<CatalogCommitResult, AZStd::string>
    CatalogTransactionService::Commit(
        const CatalogDatabase& candidate,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SourceEvidenceRegistry& sourceRegistry,
        const SaveCallback& save) const
    {
        if (!save)
        {
            return AZ::Failure(AZStd::string(
                "A catalog save callback is required."));
        }
        const GameProfile* activeProfile = workspace.FindActiveGameProfile();
        if (workspace.m_workspaceId.empty()
            || workspace.m_rootPath.empty()
            || !profile.IsConfigured()
            || !activeProfile
            || activeProfile->m_profileId != profile.m_profileId
            || activeProfile->m_gameVersion != profile.m_gameVersion
            || activeProfile->m_branch != profile.m_branch
            || activeProfile->m_runtimeTarget != profile.m_runtimeTarget)
        {
            return AZ::Failure(AZStd::string(
                "The exact configured active workspace profile is required before committing a catalog candidate."));
        }

        AZStd::string integrityError;
        if (!candidate.ValidateIntegrity(
                workspace,
                profile,
                sourceRegistry,
                &integrityError))
        {
            return AZ::Failure(
                AZStd::string("Catalog commit-time integrity validation failed: ")
                + integrityError);
        }

        const CatalogDocument document = candidate.BuildDocument(workspace, profile);
        AZ::Outcome<AZStd::string, AZStd::string> saveResult =
            save(document, workspace.m_rootPath);
        if (!saveResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(saveResult.GetError()));
        }

        CatalogCommitResult result;
        result.m_catalog = candidate;
        result.m_filePath = saveResult.TakeValue();
        return AZ::Success(AZStd::move(result));
    }
} // namespace TaintedGrailModdingSDK
