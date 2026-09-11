/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "FoundationModels.h"
#include <AzCore/Outcome/Outcome.h>
#include <QByteArray>
#include <QImage>
namespace TaintedGrailModdingSDK
{
    struct ProjectImage
    {
        ProjectAssetProfile m_metadata;
        QByteArray m_bytes;
        QImage m_image;
    };
    //! Reads and stores user-declared project images. No native asset or runtime conversion.
    class ProjectImageService final
    {
    public:
        static AZ::Outcome<ProjectImage, AZStd::string> ReadSource(
            const AZStd::string& file, const WorkspaceModel& workspace, const AZStd::string& root);
        static AZ::Outcome<ProjectImage, AZStd::string> ReadManaged(
            const ProjectAssetProfile& asset, const AZStd::string& owner, const WorkspaceModel& workspace, const AZStd::string& root);
        static AZ::Outcome<AZStd::string, AZStd::string> Store(
            const ProjectImage& image, const AZStd::string& owner, const WorkspaceModel& workspace, const AZStd::string& root);
    };
}
