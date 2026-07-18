/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "PackPersistenceService.h"

#include "PersistenceJsonUtils.h"

#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/utility/move.h>

namespace TaintedGrailModdingSDK
{
    AZ::Outcome<void, AZStd::string> PackPersistenceService::Save(
        const PackManifest& pack,
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Pack manifest file path is required."));
        }
        if (pack.m_schemaVersion != 1)
        {
            return AZ::Failure(AZStd::string("Unsupported TG pack manifest schema version."));
        }
        if (!pack.HasStableIdentity())
        {
            return AZ::Failure(AZStd::string("Pack ID, owner ID, and semantic version must be valid before saving."));
        }
        if (pack.m_runtimeActionsEnabled)
        {
            return AZ::Failure(AZStd::string("Runtime actions cannot be enabled in an editor-owned pack manifest."));
        }

        return AZ::JsonSerializationUtils::SaveObjectToFile(&pack, filePath);
    }

    AZ::Outcome<PackManifest, AZStd::string> PackPersistenceService::Load(
        const AZStd::string& filePath) const
    {
        if (filePath.empty())
        {
            return AZ::Failure(AZStd::string("Pack manifest file path is required."));
        }

        PackManifest pack;
        const AZ::Outcome<void, AZStd::string> loadResult =
            PersistenceJsonUtils::LoadObjectFromFile(pack, filePath);
        if (!loadResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(loadResult.GetError()));
        }
        if (pack.m_schemaVersion != 1)
        {
            return AZ::Failure(AZStd::string("Unsupported TG pack manifest schema version."));
        }
        if (!pack.HasStableIdentity())
        {
            return AZ::Failure(AZStd::string("The pack manifest does not contain a valid namespaced ID, owner, and semantic version."));
        }
        if (pack.m_runtimeActionsEnabled)
        {
            return AZ::Failure(AZStd::string("The pack manifest requests runtime actions, which are disabled in the editor foundation."));
        }

        return AZ::Success(AZStd::move(pack));
    }
} // namespace TaintedGrailModdingSDK
