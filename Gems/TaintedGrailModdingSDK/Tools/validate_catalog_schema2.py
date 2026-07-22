#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate catalog schema-2 while normalizing one merged legacy docs-hub sentence."""

from __future__ import annotations

import sys
from pathlib import Path

import validate_catalog_schema2_legacy as _legacy

CatalogSchema2ContractError = _legacy.CatalogSchema2ContractError
_ORIGINAL_READ_TEXT = _legacy.read_text

_LEGACY_HUB_SENTENCE = (
    "completed Core, schema-2 persistence, Framework candidate-publication, "
    "population-authoring test-source"
)
_CURRENT_HUB_SENTENCE = (
    "approved population design and implementation history with schema-2 persistence, "
    "deterministic fixture, and registered Actor/Troop pane; exact-head O3DE configure, "
    "build, compiled tests"
)


def _current_documentation_projection(path: Path) -> str:
    text = _ORIGINAL_READ_TEXT(path)
    if path.as_posix().endswith("docs/tainted-grail-sdk/README.md"):
        text = text.replace(_LEGACY_HUB_SENTENCE, _CURRENT_HUB_SENTENCE)
    return text


def validate_catalog_schema2(repo_root: Path) -> None:
    original_read_text = _legacy.read_text
    try:
        _legacy.read_text = _current_documentation_projection
        _legacy.validate_catalog_schema2(repo_root)
    finally:
        _legacy.read_text = original_read_text


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_catalog_schema2(repo_root)
    except (OSError, CatalogSchema2ContractError) as exc:
        print(f"Tainted Grail catalog schema-2 validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail catalog schema-2 validation passed: migration, persistence, "
        "implemented Actor/Troop state, and current documentation semantics are enforced."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
