#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the blocking visual game-content browser and preview pipeline gate."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
DOC_PATH = "docs/tainted-grail-sdk/VISUAL_GAME_CONTENT_BROWSER_AND_PREVIEW_PIPELINE.md"
INDEX_PATH = "docs/tainted-grail-sdk/README.md"


class VisualPreviewPipelineGateError(RuntimeError):
    """Raised when the visual preview gate is missing or weakened."""


def read_required(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file():
        raise VisualPreviewPipelineGateError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise VisualPreviewPipelineGateError(
            f"{label} is missing required fragment: {fragment}"
        )


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise VisualPreviewPipelineGateError(
            f"{label} contains forbidden fragment: {fragment}"
        )


def validate(root: Path = REPO_ROOT) -> None:
    document = read_required(root, DOC_PATH)
    index = read_required(root, INDEX_PATH)

    required_fragments = (
        "Status: blocking design gate.",
        "No item, recipe, actor, troop, placement, or visual-browser workflow may be described as function-complete",
        "FoA native asset reference",
        "version-bound discovery record",
        "local preview artefact",
        "generated O3DE preview product",
        "typed authoring binding",
        "Preview success does not grant runtime permission",
        "Generated outputs remain outside repository and engine source trees.",
        "No proprietary game payloads may be committed.",
        "Read-only asset discovery and indexing",
        "Native icon and thumbnail extraction",
        "Unity-to-neutral preview handoff",
        "Neutral-to-O3DE preview conversion",
        "Asset browser pane",
        "3D preview viewport",
        "Visual selectors in Item and Recipe Editor",
        "Actor equipment and appearance preview",
        "Troop composition preview",
        "Drag-and-drop world placement",
        "profile-bound, runtime-target-bound, fingerprint-bound, and tool-version-bound",
        "No runtime-assisted capture is approved for Alpha.",
        "runtime-assisted capture for Alpha",
        "automatic catalog mutation",
        "fidelity states: `exact`, `approximate`, `partial`, `placeholder`, `unsupported`, and `blocked`",
        "not function-complete",
        "read-only asset discovery and preview indexing",
    )
    for fragment in required_fragments:
        require(document, fragment, "Visual preview pipeline gate")

    forbidden_approval_fragments = (
        "Visual preview pipeline is complete",
        "visual preview pipeline is complete",
        "game-content browser is complete",
        "\nruntime-assisted capture is approved for Alpha",
        "Preview success grants runtime permission",
        "Generated preview products are runtime assets",
        "derived game content may be committed",
    )
    for fragment in forbidden_approval_fragments:
        reject(document, fragment, "Visual preview pipeline gate")

    require(
        index,
        "Visual Game-Content Browser and Preview Pipeline Gate",
        "Documentation index",
    )
    require(
        index,
        "blocks function-complete visual and item/recipe/actor/troop workflow claims",
        "Documentation index",
    )


def main() -> int:
    try:
        validate()
    except VisualPreviewPipelineGateError as exc:
        print(f"Visual preview pipeline gate validation failed: {exc}", file=sys.stderr)
        return 1
    print("Visual preview pipeline gate boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
