/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "WorkspacePersistenceService.h"

#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    AZ::Outcome<void, AZStd::string> WorkspacePersistenceService::Save(
        const WorkspaceModel& workspace,
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Workspace file path is required."));
        }

        return AZ::JsonSerializationUtils::SaveObjectToFile(&workspace, filePath);
    }

    AZ::Outcome<WorkspaceModel, AZStd::string> WorkspacePersistenceService::Load(
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Workspace file path is required."));
        }

        WorkspaceModel workspace;
        AZ::Outcome<void, AZStd::string> loadResult =
            AZ::JsonSerializationUtils::LoadObjectFromFile(workspace, filePath);
        if (!loadResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(loadResult.GetError()));
        }

        return AZ::Success(AZStd::move(workspace));
    }
} // namespace TaintedGrailModdingSDK
