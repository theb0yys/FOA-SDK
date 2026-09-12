/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include "TerrainHeightmapDocument.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace TaintedGrailModdingSDK
{
    struct TerrainCampaignProvider
    {
        QString m_python;
        QString m_script;
    };
    TerrainCampaignProvider ResolveTerrainCampaignProvider();
    QJsonArray AvailableTerrainCampaigns(const TerrainCampaignProvider& provider, const QString& gameRoot, const QString& unityVersion);
    QJsonObject ExportTerrainCampaign(const TerrainCampaignProvider& provider, const QString& workspace,
        const QString& gameRoot, const QString& unityVersion, const TerrainHeightmap::ProfileBinding& profile,
        const QString& campaign, const QString& operation, const QString& createdAt,
        const TerrainHeightmap::ImportControl* control, int timeoutMs = 300000);
}
