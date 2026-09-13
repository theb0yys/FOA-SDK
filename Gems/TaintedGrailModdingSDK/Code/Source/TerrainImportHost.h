/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "TerrainHeightmapDocument.h"
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <atomic>
#include <future>
#include <memory>

namespace TaintedGrailModdingSDK
{
    // Host-owned coordinator. The optional pane never receives a workspace path.
    class TerrainImportHost
    {
    public:
        ~TerrainImportHost();
        QJsonObject Dispatch(const QJsonObject& request, const QString& workspace,
            const QString& gameInstall, const TerrainHeightmap::ProfileBinding& profile, const QString& unityVersion = {});
    private:
        struct Operation
        {
            std::atomic_bool m_cancelled{false};
            std::atomic_int m_progress{0};
        };
        void CollectCompleted();
        QString m_nativeResult;
        std::shared_ptr<Operation> m_operation;
        std::future<QJsonObject> m_future;
        QJsonObject m_snapshot;
        QHash<QString, QString> m_revisionPaths;
        QString m_workspace;
        QString m_profileIdentity;
        bool m_discardResult = false;
    };
}
