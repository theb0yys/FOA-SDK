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


def validate_catalog_schema2(repo_root: Path) -> None:
    _legacy.validate_catalog_schema2(repo_root)


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
