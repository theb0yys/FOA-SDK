/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once
#include <AzCore/Outcome/Outcome.h>
#include <QImage>
#include <QString>

namespace TaintedGrailModdingSDK
{
    //! Bounded authoring-image presentation. No O3DE product or runtime appearance claims.
    class PopulationPortraitService final
    {
    public:
        static AZ::Outcome<QString, QString> ReferenceForFile(const QString& workspaceRoot, const QString& file);
        static AZ::Outcome<QImage, QString> Read(const QString& workspaceRoot, const QString& reference);
    };
}
