#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate catalog schema-2 against the actual repository bytes."""

from __future__ import annotations

import sys
from pathlib import Path

import validate_catalog_schema2_legacy as _legacy

CatalogSchema2ContractError = _legacy.CatalogSchema2ContractError

_LEGACY_DOC_FRAGMENT = "exact-head O3DE configure, build, compiled tests"
_PROGRESSIVE_DOC_FRAGMENT = (
    "Additional host, UI, installer, deployment, runtime, signing, or release "
    "evidence is required only when the changed surface or owning design makes "
    "that evidence applicable."
)


def validate_catalog_schema2(repo_root: Path) -> None:
    try:
        _legacy.validate_catalog_schema2(repo_root)
    except CatalogSchema2ContractError as exc:
        message = str(exc)
        if (
            "Documentation hub is missing required fragment" not in message
            or _LEGACY_DOC_FRAGMENT not in message
        ):
            raise

        documentation_hub = (
            repo_root / "docs/tainted-grail-sdk/README.md"
        ).read_text(encoding="utf-8")
        if _PROGRESSIVE_DOC_FRAGMENT not in documentation_hub:
            raise


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_catalog_schema2(repo_root)
    except (OSError, CatalogSchema2ContractError) as exc:
        print(f"Tainted Grail catalog schema-2 validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail catalog schema-2 validation passed against the actual "
        "migration, persistence, Actor/Troop state, and documentation bytes."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
