/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::TaintedInterfaceUi
{
    enum class AssetResolutionStatus
    {
        ProjectOwnedFallback,
        UpstreamPayloadBlocked,
        Unknown,
    };

    struct Rgba8
    {
        unsigned char m_red = 0;
        unsigned char m_green = 0;
        unsigned char m_blue = 0;
        unsigned char m_alpha = 255;
    };

    struct StyleToken
    {
        AZStd::string m_tokenId;
        Rgba8 m_value;
    };

    struct MetricToken
    {
        AZStd::string m_tokenId;
        int m_value = 0;
    };

    struct UpstreamIdentity
    {
        AZStd::string m_repository;
        AZStd::string m_commit;
        AZStd::string m_branch;
        AZStd::string m_frameworkVersion;
        AZStd::string m_pluginGuid;
        AZStd::string m_licenseExpression;
        AZStd::string m_capturedAtUtc;
    };

    struct SourceBinding
    {
        AZStd::string m_path;
        AZStd::string m_gitBlobSha1;
    };

    struct AssetDescriptor
    {
        AZStd::string m_semanticId;
        AZStd::string m_category;
        AZStd::string m_displayName;
        AZStd::string m_upstreamSourcePath;
        AZStd::string m_upstreamResourceName;
        int m_upstreamWidth = 0;
        int m_upstreamHeight = 0;
        AZStd::string m_upstreamLicenseExpression;
        bool m_upstreamPayloadEmbedded = false;
        bool m_upstreamRedistributionAllowed = false;
        bool m_projectOwnedFallbackEmbedded = false;
    };

    struct EmbeddedAsset
    {
        AZStd::string m_semanticId;
        AZStd::string m_mediaType;
        AZStd::string m_sha256;
        AZStd::string m_utf8Bytes;
        AZStd::string m_provenance;
        bool m_redistributionAllowed = false;
    };

    struct AssetResolution
    {
        AssetResolutionStatus m_status = AssetResolutionStatus::Unknown;
        AZStd::string m_semanticId;
        AZStd::string m_reason;
    };

    struct LayoutRect
    {
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_width = 0.0f;
        float m_height = 0.0f;
    };

    class Service final
    {
    public:
        UpstreamIdentity GetUpstreamIdentity() const;
        AZStd::vector<SourceBinding> GetSourceBindings() const;
        AZStd::vector<StyleToken> GetStyleTokens() const;
        AZStd::vector<MetricToken> GetMetricTokens() const;
        AZStd::vector<AssetDescriptor> GetAssetCatalog() const;

        bool TryGetAssetDescriptor(
            const AZStd::string& semanticId,
            AssetDescriptor& descriptor) const;

        AssetResolution ResolveAsset(const AZStd::string& semanticId) const;

        bool TryGetEmbeddedFallback(
            const AZStd::string& semanticId,
            EmbeddedAsset& asset) const;

        LayoutRect ComputeHudBadgeRect(
            int screenWidth,
            int screenHeight,
            const AZStd::string& corner,
            float requestedSize) const;

        static bool IsSemanticAssetId(const AZStd::string& semanticId);
    };
} // namespace TaintedGrailModdingSDK::TaintedInterfaceUi
