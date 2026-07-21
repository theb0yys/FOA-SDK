#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the governed Tainted Interface UI utility and asset boundary."""

from __future__ import annotations

import hashlib
import re
import sys
from pathlib import Path


class TaintedInterfaceUiUtilitiesError(RuntimeError):
    pass


EXPECTED_IDS = (
    "icon.device.controller",
    "icon.device.keyboard",
    "icon.device.keyboard-mouse",
    "icon.ui.cancel",
    "icon.ui.close-page",
    "icon.ui.confirmed",
    "icon.ui.ready",
    "ui.checkbox.checked",
    "ui.checkbox.empty",
    "ui.decor.divider",
    "ui.dialog.popup",
    "ui.hud.map-container",
    "ui.hud.panel",
    "ui.progress.bar-background",
    "ui.progress.bar-fill",
    "ui.slot.item",
    "ui.slot.item-highlighted",
    "ui.tabs.bar",
    "ui.tabs.current",
)

EXPECTED_BINDINGS = {
    "mods/Tainted Interface/README.md": "8e61dc5fd3df443faf73716e181edf90c1ae15bf",
    "mods/Tainted Interface/docs/decisions/0001-curated-ui-texture-catalog.md":
        "efb0555020ec6074eaa27d5d501081e30cedfd17",
    "mods/Tainted Interface/docs/design.md":
        "186f73ecb59f2f961bbb57eb249bc1cb143f4cde",
    "mods/Tainted Interface/docs/research/ui-asset-register-curated-embed-plan-2026-06-21.md":
        "1a75afe8f60a8c89b6ff9bedd77ba6a3664c829d",
    "mods/Tainted Interface/src/Plugin.cs":
        "7b7e0e140062af8c8ad3acb234086f0a93cdff8f",
    "mods/Tainted Interface/src/TaintedInterface.csproj":
        "31a543226f23ad7c03611bbeccb643fed317831a",
    "mods/Tainted Interface/src/TaintedInterfaceApi.cs":
        "19d79d309a18c6e49cb1013107d81b6ad5a42f48",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise TaintedInterfaceUiUtilitiesError(message)


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="strict")
    except OSError as exc:
        raise TaintedInterfaceUiUtilitiesError(
            f"Unable to read {path}: {exc}"
        ) from exc


def parse_svg_constants(implementation: str) -> dict[str, str]:
    values = dict(
        re.findall(
            r'constexpr const char\*\s+(\w+)\s*=\s*'
            r'R"SVG\((.*?)\)SVG";',
            implementation,
            flags=re.DOTALL,
        )
    )
    require(
        set(values) == {
            "FrameFallbackSvg",
            "ActionFallbackSvg",
            "DeviceFallbackSvg",
        },
        "Expected exactly three project-owned SVG fallback classes",
    )
    return values


def parse_fallback_fingerprints(implementation: str) -> dict[str, str]:
    patterns = {
        "ActionFallbackSvg": (
            r'if \(descriptor\.m_category == "ui-action"\).*?'
            r'"sha256:([0-9a-f]{64})";.*?'
            r'asset\.m_utf8Bytes = ActionFallbackSvg;',
        ),
        "DeviceFallbackSvg": (
            r'else if \(descriptor\.m_category == "device"\).*?'
            r'"sha256:([0-9a-f]{64})";.*?'
            r'asset\.m_utf8Bytes = DeviceFallbackSvg;',
        ),
        "FrameFallbackSvg": (
            r'else\s*\{.*?"sha256:([0-9a-f]{64})";.*?'
            r'asset\.m_utf8Bytes = FrameFallbackSvg;',
        ),
    }
    values: dict[str, str] = {}
    for name, (pattern,) in patterns.items():
        match = re.search(pattern, implementation, flags=re.DOTALL)
        require(match is not None, f"Missing fingerprint binding for {name}")
        values[name] = match.group(1)
    return values


def validate(repo_root: Path) -> None:
    gem = repo_root / "Gems" / "TaintedGrailModdingSDK"
    source = gem / "Code" / "Source"
    tests = gem / "Code" / "Tests"
    tools = gem / "Tools"
    docs = repo_root / "docs" / "tainted-grail-sdk"

    header = read(source / "TaintedInterfaceUiUtilities.h")
    implementation = read(source / "TaintedInterfaceUiUtilities.cpp")
    foundation_header = read(source / "FoundationService.h")
    bridge = read(source / "FoundationTaintedInterfaceUiUtilities.cpp")
    framework_manifest = read(
        gem / "Code" / "taintedgrailmoddingsdk_framework_files.cmake"
    )
    test_manifest = read(
        gem / "Code" / "taintedgrailmoddingsdk_tainted_framework_tests_files.cmake"
    )
    compiled_tests = read(tests / "TaintedInterfaceUiUtilitiesTests.cpp")
    local_runner = read(tools / "run_local_validation.py")
    documentation_hub = read(docs / "README.md")

    for required in (
        "AssetResolutionStatus",
        "StyleToken",
        "MetricToken",
        "AssetDescriptor",
        "EmbeddedAsset",
        "GetAssetCatalog",
        "TryGetAssetDescriptor",
        "ResolveAsset",
        "TryGetEmbeddedFallback",
        "ComputeHudBadgeRect",
        "IsSemanticAssetId",
    ):
        require(required in header, f"Public UI utility contract missing {required}")

    for required in (
        '"theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods"',
        '"d7e740e7f167b73152b53409e483dab07d80d048"',
        '"main"',
        '"0.2.6"',
        '"kane.tgfoa.tainted-interface"',
        '"NOASSERTION"',
    ):
        require(required in implementation, f"Pinned upstream identity missing {required}")

    for path, fingerprint in EXPECTED_BINDINGS.items():
        require(
            f'{{ "{path}", "{fingerprint}" }}' in implementation,
            f"Pinned upstream source binding drifted: {path}",
        )

    observed_ids = tuple(sorted(set(re.findall(
        r'\{ "((?:ui|icon)\.[a-z0-9.-]+)",', implementation
    ))))
    require(observed_ids == EXPECTED_IDS, "Curated semantic asset catalog drifted")
    require(
        implementation.count('"NOASSERTION", false, false, true }') == 19,
        "Every curated upstream asset must remain unembedded, non-redistributable, and fallback-backed",
    )
    require(
        "m_upstreamPayloadEmbedded = true" not in implementation,
        "Upstream payload embedding must remain disabled",
    )
    require(
        "m_upstreamRedistributionAllowed = true" not in implementation,
        "Upstream redistribution must remain disabled",
    )

    svgs = parse_svg_constants(implementation)
    fingerprints = parse_fallback_fingerprints(implementation)
    for name, svg in svgs.items():
        require(svg.startswith("<svg "), f"{name} is not an SVG document")
        require(len(svg.encode("utf-8")) <= 4096, f"{name} exceeds the bounded fallback size")
        observed = hashlib.sha256(svg.encode("utf-8")).hexdigest()
        require(
            observed == fingerprints[name],
            f"Embedded SVG fingerprint mismatch for {name}",
        )
    require(
        implementation.count("project-owned generated semantic fallback") == 1,
        "Fallback provenance must be explicit and centralized",
    )
    require(
        "not upstream visual parity" in implementation,
        "Fallbacks must not claim upstream visual parity",
    )

    prohibited = (
        "BepInEx",
        "UnityEngine",
        "Harmony",
        "QFile",
        "QPixmap",
        "QIcon",
        "std::filesystem",
        "AZ::IO",
        "ifstream",
        "fopen",
        "CreateProcess",
        "QProcess",
        "CatalogDatabase&",
        "SourceEvidenceRegistry&",
        "WorkspaceModel&",
        "GameProfile&",
    )
    for fragment in prohibited:
        require(
            fragment not in header,
            f"Public UI utility exposes prohibited authority: {fragment}",
        )
        require(
            fragment not in implementation,
            f"UI utility imports prohibited runtime/file authority: {fragment}",
        )

    require(
        "TaintedInterfaceUi::Service m_taintedInterfaceUiUtilities" in foundation_header,
        "Foundation must own one persistent UI utility service",
    )
    require(
        "GetTaintedInterfaceUiUtilities" in foundation_header,
        "Foundation UI utility accessor is missing",
    )
    require(
        "return m_taintedInterfaceUiUtilities" in bridge,
        "Foundation accessor must return the persistent UI utility service",
    )

    for required in (
        "Source/TaintedInterfaceUiUtilities.cpp",
        "Source/TaintedInterfaceUiUtilities.h",
        "Source/FoundationTaintedInterfaceUiUtilities.cpp",
    ):
        require(required in framework_manifest, f"Framework build graph missing {required}")
    require(
        "Tests/TaintedInterfaceUiUtilitiesTests.cpp" in test_manifest,
        "Production-linked UI utility tests are not compiled",
    )

    for test_name in (
        "UpstreamIdentityAndBindingsArePinned",
        "CuratedCatalogIsSortedBoundedAndFailClosed",
        "EveryCuratedIdHasProjectOwnedSvgFallback",
        "ResolutionSeparatesFallbackFromBlockedUpstreamPayload",
        "SemanticIdsFailClosed",
        "StyleAndMetricTokensAreDeterministic",
        "HudBadgeLayoutIsDeterministicAndContained",
        "FoundationOwnsPersistentUtilityService",
    ):
        require(test_name in compiled_tests, f"Compiled UI utility coverage missing {test_name}")

    require(
        "validate_tainted_interface_ui_utilities.py" in local_runner,
        "Authoritative local validation does not include the UI utility gate",
    )
    require(
        "TAINTED_INTERFACE_UI_UTILITIES.md" in documentation_hub,
        "Documentation hub does not link the UI utility contract",
    )
    require(
        "TAINTED_INTERFACE_UI_UTILITIES_REVIEW.md" in documentation_hub,
        "Documentation hub does not link the UI utility review gate",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, TaintedInterfaceUiUtilitiesError) as exc:
        print(f"Tainted Interface UI utility validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Interface UI utility validation passed: pinned provenance, exact "
        "curated IDs, blocked upstream payloads, verified project-owned SVG "
        "fallbacks, deterministic layout, persistent Foundation ownership, and "
        "no runtime/file/catalog authority."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
