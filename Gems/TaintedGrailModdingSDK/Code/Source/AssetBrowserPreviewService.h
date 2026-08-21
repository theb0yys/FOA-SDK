/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK
{
    struct AssetBrowserPreviewLoadRequest
    {
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_extractedDataPath;
        AZStd::string m_paneModelPath;
        AZStd::string m_thumbnailEvidencePath;
        AZStd::string m_thumbnailPreviewRootPath;
        AZStd::string m_viewportEvidencePath;
        size_t m_maximumEntries = 10000;
    };

    struct AssetBrowserPreviewEntry
    {
        AZStd::string m_entryId;
        AZStd::string m_displayName;
        AZStd::string m_category;
        AZStd::string m_entryKind;
        AZStd::string m_previewAvailability;
        AZStd::string m_fidelityState;
        AZStd::string m_thumbnailPath;
        AZStd::string m_thumbnailStatus;
        AZStd::string m_thumbnailFidelity;
        AZStd::string m_viewportRouteState;
        AZStd::string m_productAssetId;
        AZStd::string m_productCachePath;
        AZStd::string m_primarySourceAssetRecordId;
        AZStd::string m_blocker;
        AZStd::vector<AZStd::string> m_productAssetIds;
        AZStd::vector<AZStd::string> m_productCachePaths;
        AZStd::vector<AZStd::string> m_evidenceRefs;
        bool m_canRouteToViewport = false;
        bool m_canCreateTypedAuthoringBinding = false;
        bool m_requiresExplicitBindingStep = true;
    };

    struct AssetBrowserPreviewSnapshot
    {
        AZStd::vector<AssetBrowserPreviewEntry> m_entries;
        AZStd::vector<AZStd::string> m_categories;
        AZStd::vector<AZStd::string> m_issues;
    };

    struct AssetBrowserPreviewViewportRoute
    {
        AZStd::string m_routeId;
        AZStd::string m_profileId;
        AZStd::string m_gameVersion;
        AZStd::string m_branch;
        AZStd::string m_runtimeTarget;
        AZStd::string m_entryId;
        AZStd::string m_productAssetId;
        AZStd::string m_productCachePath;
        AZStd::string m_primarySourceAssetRecordId;
        AZStd::string m_fidelityState;
        AZStd::string m_viewportRouteState;
        bool m_o3deViewportMutationAllowed = false;
        bool m_typedAuthoringBindingCreated = false;
        bool m_catalogPromotionAllowed = false;
        bool m_runtimePermissionGranted = false;
    };

    class AssetBrowserPreviewService
    {
    public:
        AZ::Outcome<AssetBrowserPreviewSnapshot, AZStd::string> LoadPreview(
            const AssetBrowserPreviewLoadRequest& request) const;
        AZ::Outcome<AssetBrowserPreviewViewportRoute, AZStd::string> PrepareViewportRoute(
            const AssetBrowserPreviewLoadRequest& request,
            const AssetBrowserPreviewEntry& entry) const;

        static AZStd::string ClassifyCategory(const AssetBrowserPreviewEntry& entry);
        static AZStd::string DetermineFidelityState(
            const AssetBrowserPreviewEntry& entry,
            bool hasViewportEvidence);
    };

    class AssetBrowserPreviewRouteRegistry
    {
    public:
        static AssetBrowserPreviewRouteRegistry& Get();

        bool RegisterRoute(
            const AssetBrowserPreviewViewportRoute& route,
            AZStd::string* error = nullptr);
        void Clear();

        const AssetBrowserPreviewViewportRoute* FindByRouteId(
            const AZStd::string& routeId) const;
        const AssetBrowserPreviewViewportRoute* GetLatestRoute() const;
        const AZStd::vector<AssetBrowserPreviewViewportRoute>& GetRoutes() const;

    private:
        AZStd::vector<AssetBrowserPreviewViewportRoute> m_routes;
    };
} // namespace TaintedGrailModdingSDK
