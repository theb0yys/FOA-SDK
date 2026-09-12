/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "TerrainHeightmapDocument.h"
#include <QJsonObject>
#include <QString>
namespace TaintedGrailModdingSDK
{
    // Foundation prepares private editable copies; O3DE owns the level, paint tools and entities.
    QJsonObject PrepareNativeTerrain(const QString& workspace, const QString& locator,
        const TerrainHeightmap::ProfileBinding& profile, const TerrainHeightmap::ImportControl* control);
    bool LaunchNativeTerrain(const QString& requestPath);
}
