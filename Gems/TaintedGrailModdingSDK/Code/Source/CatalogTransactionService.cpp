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
    AZ::Outcome<CatalogCommitResult, AZStd::string> CatalogTransactionService::Commit(
        const CatalogDatabase& candidate,
        const WorkspaceModel& workspace,
        const GameProfile& profile,
        const SaveCallback& save) const
    {
        if (!save)
        {
            return AZ::Failure(AZStd::string("A catalog save callback is required."));
        }
        if (workspace.m_workspaceId.empty() || workspace.m_rootPath.empty() || !profile.IsConfigured())
        {
            return AZ::Failure(AZStd::string(
                "A configured workspace and exact game profile are required before committing a catalog candidate."));
        }

        const CatalogDocument document = candidate.BuildDocument(workspace, profile);
        AZ::Outcome<AZStd::string, AZStd::string> saveResult = save(document, workspace.m_rootPath);
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
