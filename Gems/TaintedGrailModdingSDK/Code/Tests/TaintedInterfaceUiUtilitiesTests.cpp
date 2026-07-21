/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <AzTest/AzTest.h>

#include "FoundationService.h"
#include "TaintedInterfaceUiUtilities.h"

namespace TaintedGrailModdingSDK
{
    using namespace TaintedInterfaceUi;

    TEST(TaintedInterfaceUiUtilitiesTests, UpstreamIdentityAndBindingsArePinned)
    {
        Service service;
        const auto identity = service.GetUpstreamIdentity();
        EXPECT_EQ(identity.m_repository, "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods");
        EXPECT_EQ(identity.m_commit, "d7e740e7f167b73152b53409e483dab07d80d048");
        EXPECT_EQ(identity.m_frameworkVersion, "0.2.6");
        EXPECT_EQ(identity.m_pluginGuid, "kane.tgfoa.tainted-interface");
        EXPECT_EQ(identity.m_licenseExpression, "NOASSERTION");

        const auto bindings = service.GetSourceBindings();
        ASSERT_EQ(bindings.size(), 7);
        for (size_t index = 1; index < bindings.size(); ++index)
        {
            EXPECT_LT(bindings[index - 1].m_path, bindings[index].m_path);
        }
    }

    TEST(TaintedInterfaceUiUtilitiesTests, CuratedCatalogIsSortedBoundedAndFailClosed)
    {
        Service service;
        const auto catalog = service.GetAssetCatalog();
        ASSERT_EQ(catalog.size(), 19);
        for (size_t index = 0; index < catalog.size(); ++index)
        {
            const auto& descriptor = catalog[index];
            EXPECT_TRUE(Service::IsSemanticAssetId(descriptor.m_semanticId));
            EXPECT_EQ(descriptor.m_upstreamLicenseExpression, "NOASSERTION");
            EXPECT_FALSE(descriptor.m_upstreamPayloadEmbedded);
            EXPECT_FALSE(descriptor.m_upstreamRedistributionAllowed);
            EXPECT_TRUE(descriptor.m_projectOwnedFallbackEmbedded);
            if (index > 0)
            {
                EXPECT_LT(catalog[index - 1].m_semanticId, descriptor.m_semanticId);
            }
        }
    }

    TEST(TaintedInterfaceUiUtilitiesTests, EveryCuratedIdHasProjectOwnedSvgFallback)
    {
        Service service;
        for (const auto& descriptor : service.GetAssetCatalog())
        {
            EmbeddedAsset asset;
            ASSERT_TRUE(service.TryGetEmbeddedFallback(descriptor.m_semanticId, asset));
            EXPECT_EQ(asset.m_semanticId, descriptor.m_semanticId);
            EXPECT_EQ(asset.m_mediaType, "image/svg+xml");
            EXPECT_TRUE(asset.m_redistributionAllowed);
            EXPECT_EQ(asset.m_sha256.size(), 71);
            EXPECT_EQ(asset.m_sha256.substr(0, 7), "sha256:");
            EXPECT_NE(asset.m_utf8Bytes.find("<svg"), AZStd::string::npos);
            EXPECT_NE(asset.m_provenance.find("project-owned"), AZStd::string::npos);
            EXPECT_NE(asset.m_provenance.find("not upstream visual parity"), AZStd::string::npos);
        }
    }

    TEST(TaintedInterfaceUiUtilitiesTests, ResolutionSeparatesFallbackFromBlockedUpstreamPayload)
    {
        Service service;
        const auto known = service.ResolveAsset("ui.checkbox.checked");
        EXPECT_EQ(known.m_status, AssetResolutionStatus::ProjectOwnedFallback);
        EXPECT_NE(
            known.m_reason.find("upstream_payload_blocked_noassertion"),
            AZStd::string::npos);

        const auto unknown = service.ResolveAsset("ui.unknown.asset");
        EXPECT_EQ(unknown.m_status, AssetResolutionStatus::Unknown);
        EXPECT_EQ(unknown.m_reason, "semantic_asset_id_unknown_or_invalid");
    }

    TEST(TaintedInterfaceUiUtilitiesTests, SemanticIdsFailClosed)
    {
        EXPECT_TRUE(Service::IsSemanticAssetId("ui.hud.panel"));
        EXPECT_TRUE(Service::IsSemanticAssetId("icon.device.keyboard-mouse"));
        EXPECT_FALSE(Service::IsSemanticAssetId(""));
        EXPECT_FALSE(Service::IsSemanticAssetId("Ui.Hud.Panel"));
        EXPECT_FALSE(Service::IsSemanticAssetId("ui..panel"));
        EXPECT_FALSE(Service::IsSemanticAssetId("../ui.panel"));
        EXPECT_FALSE(Service::IsSemanticAssetId("ui.panel/absolute"));
    }

    TEST(TaintedInterfaceUiUtilitiesTests, StyleAndMetricTokensAreDeterministic)
    {
        Service service;
        const auto styles = service.GetStyleTokens();
        const auto metrics = service.GetMetricTokens();
        ASSERT_EQ(styles.size(), 9);
        ASSERT_EQ(metrics.size(), 7);
        for (size_t index = 1; index < styles.size(); ++index)
        {
            EXPECT_LT(styles[index - 1].m_tokenId, styles[index].m_tokenId);
        }
        for (size_t index = 1; index < metrics.size(); ++index)
        {
            EXPECT_LT(metrics[index - 1].m_tokenId, metrics[index].m_tokenId);
        }
    }

    TEST(TaintedInterfaceUiUtilitiesTests, HudBadgeLayoutIsDeterministicAndContained)
    {
        Service service;
        const auto topLeft = service.ComputeHudBadgeRect(
            1920, 1080, "TopLeft", 72.0f);
        EXPECT_FLOAT_EQ(topLeft.m_x, 24.0f);
        EXPECT_FLOAT_EQ(topLeft.m_y, 52.0f);
        EXPECT_FLOAT_EQ(topLeft.m_width, 72.0f);
        EXPECT_FLOAT_EQ(topLeft.m_height, 72.0f);

        const auto bottomRight = service.ComputeHudBadgeRect(
            1920, 1080, "bottom-right", 200.0f);
        EXPECT_FLOAT_EQ(bottomRight.m_x, 1752.0f);
        EXPECT_FLOAT_EQ(bottomRight.m_y, 732.0f);
        EXPECT_FLOAT_EQ(bottomRight.m_width, 128.0f);
        EXPECT_FLOAT_EQ(bottomRight.m_height, 128.0f);

        const auto tooSmall = service.ComputeHudBadgeRect(
            20, 20, "TopLeft", 72.0f);
        EXPECT_FLOAT_EQ(tooSmall.m_width, 0.0f);
        EXPECT_FLOAT_EQ(tooSmall.m_height, 0.0f);
    }

    TEST(TaintedInterfaceUiUtilitiesTests, FoundationOwnsPersistentUtilityService)
    {
        FoundationService foundation(FoundationWorkspaceLoadDependencies{});
        auto& first = foundation.GetTaintedInterfaceUiUtilities();
        auto& second = foundation.GetTaintedInterfaceUiUtilities();
        EXPECT_EQ(&first, &second);
    }
} // namespace TaintedGrailModdingSDK
