/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "TaintedInterfaceUiUtilities.h"

#include <AzCore/std/algorithm.h>

namespace TaintedGrailModdingSDK::TaintedInterfaceUi
{
    namespace
    {
        constexpr const char* FrameFallbackSvg =
            R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><rect width="128" height="128" rx="12" fill="#1f2425"/><rect x="10" y="18" width="108" height="92" rx="10" fill="#303638" stroke="#ebb35c" stroke-width="4"/><path d="M24 42h80M24 64h64M24 86h48" stroke="#b8b3a3" stroke-width="4" stroke-linecap="round"/></svg>)SVG";
        constexpr const char* ActionFallbackSvg =
            R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><rect width="128" height="128" rx="12" fill="#1f2425"/><circle cx="64" cy="64" r="44" fill="#301016" stroke="#ebb35c" stroke-width="5"/><path d="M42 65l14 15 31-35" fill="none" stroke="#fff1c9" stroke-width="8" stroke-linecap="round" stroke-linejoin="round"/></svg>)SVG";
        constexpr const char* DeviceFallbackSvg =
            R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><rect width="128" height="128" rx="12" fill="#1f2425"/><rect x="14" y="34" width="100" height="60" rx="8" fill="#303638" stroke="#ebb35c" stroke-width="4"/><path d="M28 52h72M28 68h72M28 82h18m8 0h30m8 0h8" stroke="#fff1c9" stroke-width="4" stroke-linecap="round"/></svg>)SVG";

        template<class T>
        void SortByTokenId(AZStd::vector<T>& values)
        {
            AZStd::sort(values.begin(), values.end(), [](const T& left, const T& right)
            {
                return left.m_tokenId < right.m_tokenId;
            });
        }

        AZStd::string LowerAscii(AZStd::string value)
        {
            for (char& character : value)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }
            return value;
        }

        float ClampFloat(float value, float minimum, float maximum)
        {
            return AZStd::min(AZStd::max(value, minimum), maximum);
        }

        const AZStd::vector<AssetDescriptor>& AssetCatalog()
        {
            static const AZStd::vector<AssetDescriptor> catalog = []()
            {
                AZStd::vector<AssetDescriptor> values = {
                { "ui.hud.panel", "hud", "HUD Panel",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/HUD/In-game/HUD - Panel.png",
                  "TaintedInterface.Assets.Ui.HudPanel.png", 1040, 138, "NOASSERTION", false, false, true },
                { "ui.hud.map-container", "hud", "HUD Map Container",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/HUD/In-game/map container.png",
                  "TaintedInterface.Assets.Ui.HudMapContainer.png", 244, 244, "NOASSERTION", false, false, true },
                { "ui.progress.bar-background", "progress", "Progress Bar Background",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/HUD/Progress/Progress bar BG.png",
                  "TaintedInterface.Assets.Ui.ProgressBarBackground.png", 1605, 21, "NOASSERTION", false, false, true },
                { "ui.progress.bar-fill", "progress", "Progress Bar Fill",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/HUD/Progress/Progress bar FG.png",
                  "TaintedInterface.Assets.Ui.ProgressBarFill.png", 1579, 15, "NOASSERTION", false, false, true },
                { "ui.slot.item", "slot", "Item Slot",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/panels & dialogs/Item-box.png",
                  "TaintedInterface.Assets.Ui.ItemSlot.png", 89, 89, "NOASSERTION", false, false, true },
                { "ui.slot.item-highlighted", "slot", "Highlighted Item Slot",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/panels & dialogs/Item-box (highlighted).png",
                  "TaintedInterface.Assets.Ui.ItemSlotHighlighted.png", 89, 89, "NOASSERTION", false, false, true },
                { "ui.dialog.popup", "dialog", "Dialog Popup",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/panels & dialogs/Dialog - popup.png",
                  "TaintedInterface.Assets.Ui.DialogPopup.png", 1063, 518, "NOASSERTION", false, false, true },
                { "ui.tabs.bar", "tab", "Tab Bar",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/panels & dialogs/tab bar.png",
                  "TaintedInterface.Assets.Ui.TabBar.png", 1920, 78, "NOASSERTION", false, false, true },
                { "ui.tabs.current", "tab", "Current Tab",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/panels & dialogs/tab (current).png",
                  "TaintedInterface.Assets.Ui.TabCurrent.png", 165, 78, "NOASSERTION", false, false, true },
                { "ui.checkbox.empty", "input", "Empty Checkbox",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/Inputs/input - checkbox.png",
                  "TaintedInterface.Assets.Ui.CheckboxEmpty.png", 34, 34, "NOASSERTION", false, false, true },
                { "ui.checkbox.checked", "input", "Checked Checkbox",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/Inputs/input - checkbox (checked).png",
                  "TaintedInterface.Assets.Ui.CheckboxChecked.png", 34, 34, "NOASSERTION", false, false, true },
                { "ui.decor.divider", "decor", "Decorative Divider",
                  "user interface structure kits/Dark Fantasy UI asset/PNG/1080p/decor/decor - div.png",
                  "TaintedInterface.Assets.Ui.DecorDivider.png", 351, 42, "NOASSERTION", false, false, true },
                { "icon.ui.close-page", "ui-action", "Close Page",
                  "icons/multiplayer icons/exit page/close-page.png",
                  "TaintedInterface.Assets.Icons.UiClosePage.png", 513, 513, "NOASSERTION", false, false, true },
                { "icon.ui.cancel", "ui-action", "Cancel",
                  "icons/multiplayer icons/exit page/cancel.png",
                  "TaintedInterface.Assets.Icons.UiCancel.png", 513, 513, "NOASSERTION", false, false, true },
                { "icon.ui.ready", "ui-action", "Ready",
                  "icons/multiplayer icons/confirm control type-ready/ready.png",
                  "TaintedInterface.Assets.Icons.UiReady.png", 513, 513, "NOASSERTION", false, false, true },
                { "icon.ui.confirmed", "ui-action", "Confirmed",
                  "icons/multiplayer icons/confirm control type-ready/confirmed.png",
                  "TaintedInterface.Assets.Icons.UiConfirmed.png", 513, 513, "NOASSERTION", false, false, true },
                { "icon.device.controller", "device", "Controller",
                  "icons/multiplayer icons/user control methods/controller.png",
                  "TaintedInterface.Assets.Icons.DeviceController.png", 513, 513, "NOASSERTION", false, false, true },
                { "icon.device.keyboard-mouse", "device", "Keyboard and Mouse",
                  "icons/multiplayer icons/user control methods/keyboard-mouse.png",
                  "TaintedInterface.Assets.Icons.DeviceKeyboardMouse.png", 512, 512, "NOASSERTION", false, false, true },
                { "icon.device.keyboard", "device", "Keyboard",
                  "icons/multiplayer icons/user control methods/keyboard.png",
                  "TaintedInterface.Assets.Icons.DeviceKeyboard.png", 513, 513, "NOASSERTION", false, false, true },
                };
                AZStd::sort(values.begin(), values.end(), [](const AssetDescriptor& left, const AssetDescriptor& right)
                {
                    return left.m_semanticId < right.m_semanticId;
                });
                return values;
            }();
            return catalog;
        }

        EmbeddedAsset BuildFallback(const AssetDescriptor& descriptor)
        {
            EmbeddedAsset asset;
            asset.m_semanticId = descriptor.m_semanticId;
            asset.m_mediaType = "image/svg+xml";
            asset.m_provenance =
                "FOA-SDK project-owned generated semantic fallback; not upstream visual parity.";
            asset.m_redistributionAllowed = true;

            if (descriptor.m_category == "ui-action")
            {
                asset.m_sha256 =
                    "sha256:963322ddbef505d6d513b96dedfd3f9f267f64da3c5177a56d4f0cb5844c2c93";
                asset.m_utf8Bytes = ActionFallbackSvg;
            }
            else if (descriptor.m_category == "device")
            {
                asset.m_sha256 =
                    "sha256:b28daf9c6a3a8cc0646d15f0eecf6b5d232e0a624e9e0cd2c025a06c41348899";
                asset.m_utf8Bytes = DeviceFallbackSvg;
            }
            else
            {
                asset.m_sha256 =
                    "sha256:28a7ce0f60ee0b8a2d679f5fa23778a003b7e2308d6d8c98ac5b1808a5f889e3";
                asset.m_utf8Bytes = FrameFallbackSvg;
            }
            return asset;
        }
    } // namespace

    UpstreamIdentity Service::GetUpstreamIdentity() const
    {
        return {
            "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods",
            "d7e740e7f167b73152b53409e483dab07d80d048",
            "main",
            "0.2.6",
            "kane.tgfoa.tainted-interface",
            "NOASSERTION",
            "2026-07-21T21:20:00Z",
        };
    }

    AZStd::vector<SourceBinding> Service::GetSourceBindings() const
    {
        return {
            { "mods/Tainted Interface/README.md", "8e61dc5fd3df443faf73716e181edf90c1ae15bf" },
            { "mods/Tainted Interface/docs/decisions/0001-curated-ui-texture-catalog.md", "efb0555020ec6074eaa27d5d501081e30cedfd17" },
            { "mods/Tainted Interface/docs/design.md", "186f73ecb59f2f961bbb57eb249bc1cb143f4cde" },
            { "mods/Tainted Interface/docs/research/ui-asset-register-curated-embed-plan-2026-06-21.md", "1a75afe8f60a8c89b6ff9bedd77ba6a3664c829d" },
            { "mods/Tainted Interface/src/Plugin.cs", "7b7e0e140062af8c8ad3acb234086f0a93cdff8f" },
            { "mods/Tainted Interface/src/TaintedInterface.csproj", "31a543226f23ad7c03611bbeccb643fed317831a" },
            { "mods/Tainted Interface/src/TaintedInterfaceApi.cs", "19d79d309a18c6e49cb1013107d81b6ad5a42f48" },
        };
    }

    AZStd::vector<StyleToken> Service::GetStyleTokens() const
    {
        AZStd::vector<StyleToken> values = {
            { "color.accent", { 235, 179, 92, 255 } },
            { "color.button", { 56, 27, 31, 245 } },
            { "color.card", { 38, 42, 43, 235 } },
            { "color.field", { 17, 19, 20, 245 } },
            { "color.header", { 48, 14, 18, 245 } },
            { "color.panel", { 31, 36, 37, 230 } },
            { "color.text", { 235, 230, 214, 255 } },
            { "color.text-muted", { 184, 179, 163, 255 } },
            { "color.window", { 27, 31, 32, 235 } },
        };
        SortByTokenId(values);
        return values;
    }

    AZStd::vector<MetricToken> Service::GetMetricTokens() const
    {
        AZStd::vector<MetricToken> values = {
            { "font.label", 14 },
            { "font.muted", 12 },
            { "font.title", 18 },
            { "hud.badge.maximum", 128 },
            { "hud.badge.minimum", 56 },
            { "layout.border", 6 },
            { "layout.padding", 12 },
        };
        SortByTokenId(values);
        return values;
    }

    AZStd::vector<AssetDescriptor> Service::GetAssetCatalog() const
    {
        return AssetCatalog();
    }

    bool Service::TryGetAssetDescriptor(
        const AZStd::string& semanticId,
        AssetDescriptor& descriptor) const
    {
        if (!IsSemanticAssetId(semanticId))
        {
            return false;
        }

        const auto& catalog = AssetCatalog();
        const auto found = AZStd::find_if(
            catalog.begin(), catalog.end(),
            [&semanticId](const AssetDescriptor& value)
            {
                return value.m_semanticId == semanticId;
            });
        if (found == catalog.end())
        {
            return false;
        }

        descriptor = *found;
        return true;
    }

    AssetResolution Service::ResolveAsset(const AZStd::string& semanticId) const
    {
        AssetResolution result;
        result.m_semanticId = semanticId;

        AssetDescriptor descriptor;
        if (!TryGetAssetDescriptor(semanticId, descriptor))
        {
            result.m_status = AssetResolutionStatus::Unknown;
            result.m_reason = "semantic_asset_id_unknown_or_invalid";
            return result;
        }

        result.m_status = descriptor.m_projectOwnedFallbackEmbedded
            ? AssetResolutionStatus::ProjectOwnedFallback
            : AssetResolutionStatus::UpstreamPayloadBlocked;
        result.m_reason = descriptor.m_projectOwnedFallbackEmbedded
            ? "project_owned_fallback_available;upstream_payload_blocked_noassertion"
            : "upstream_payload_blocked_noassertion";
        return result;
    }

    bool Service::TryGetEmbeddedFallback(
        const AZStd::string& semanticId,
        EmbeddedAsset& asset) const
    {
        AssetDescriptor descriptor;
        if (!TryGetAssetDescriptor(semanticId, descriptor)
            || !descriptor.m_projectOwnedFallbackEmbedded)
        {
            return false;
        }

        asset = BuildFallback(descriptor);
        return true;
    }

    LayoutRect Service::ComputeHudBadgeRect(
        int screenWidth,
        int screenHeight,
        const AZStd::string& corner,
        float requestedSize) const
    {
        LayoutRect result;
        if (screenWidth <= 24 || screenHeight <= 24)
        {
            return result;
        }

        const float maximumAvailable = AZStd::min(
            static_cast<float>(screenWidth - 24),
            static_cast<float>(screenHeight - 24));
        const float size = AZStd::min(
            ClampFloat(requestedSize, 56.0f, 128.0f),
            maximumAvailable);
        if (size <= 0.0f)
        {
            return result;
        }

        const AZStd::string normalizedCorner = LowerAscii(corner);
        const bool bottomRight =
            normalizedCorner == "bottomright"
            || normalizedCorner == "bottom right"
            || normalizedCorner == "bottom-right";

        const float maximumX = AZStd::max(
            12.0f, static_cast<float>(screenWidth) - size - 12.0f);
        const float maximumY = AZStd::max(
            12.0f, static_cast<float>(screenHeight) - size - 12.0f);
        const float preferredX = bottomRight
            ? static_cast<float>(screenWidth) - size - 40.0f
            : 24.0f;
        const float topLeftY = AZStd::max(12.0f, 136.0f - size - 12.0f);
        const float preferredY = bottomRight
            ? static_cast<float>(screenHeight) - size - 220.0f
            : topLeftY;

        result.m_x = ClampFloat(preferredX, 12.0f, maximumX);
        result.m_y = ClampFloat(preferredY, 12.0f, maximumY);
        result.m_width = size;
        result.m_height = size;
        return result;
    }

    bool Service::IsSemanticAssetId(const AZStd::string& semanticId)
    {
        if (semanticId.empty() || semanticId.size() > 96)
        {
            return false;
        }

        const auto isLowerLetter = [](char value)
        {
            return value >= 'a' && value <= 'z';
        };
        const auto isDigit = [](char value)
        {
            return value >= '0' && value <= '9';
        };

        if (!isLowerLetter(semanticId.front())
            || (!isLowerLetter(semanticId.back()) && !isDigit(semanticId.back())))
        {
            return false;
        }

        char previous = '\0';
        for (char character : semanticId)
        {
            const bool punctuation = character == '.' || character == '-';
            if (!isLowerLetter(character) && !isDigit(character) && !punctuation)
            {
                return false;
            }
            if (punctuation && (previous == '.' || previous == '-'))
            {
                return false;
            }
            previous = character;
        }
        return true;
    }
} // namespace TaintedGrailModdingSDK::TaintedInterfaceUi
