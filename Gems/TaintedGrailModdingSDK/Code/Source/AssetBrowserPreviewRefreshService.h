/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct AssetBrowserPreviewRefreshResult
    {
        AZStd::string m_importProofPath;
        AZStd::string m_modelPath;
    };

    //! Regenerates the shared Asset Browser pane model for the active exact FoA profile.
    //!
    //! The service only orchestrates the existing Python model generator through O3DE's
    //! embedded Python runner. The Python generator remains the single owner of pane-model
    //! schema, normalization, validation, output naming, and authority semantics.
    class AssetBrowserPreviewRefreshService
    {
    public:
        AZ::Outcome<AssetBrowserPreviewRefreshResult, AZStd::string> RefreshActiveProfileModel() const;
    };
} // namespace TaintedGrailModdingSDK
