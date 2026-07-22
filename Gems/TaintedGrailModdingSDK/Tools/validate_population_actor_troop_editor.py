#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the implemented Actor/Troop Editor against actual repository bytes."""

from __future__ import annotations

import sys
from pathlib import Path

import validate_population_actor_troop_editor_legacy as _legacy

PopulationEditorContractError = _legacy.PopulationEditorContractError


def validate_population_actor_troop_editor(repo_root: Path) -> None:
    _legacy.validate_population_actor_troop_editor(repo_root)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_population_actor_troop_editor(repo_root)
    except (OSError, PopulationEditorContractError) as exc:
        print(
            f"Tainted Grail population editor validation failed: {exc}",
            file=sys.stderr,
        )
        return 1
    print(
        "Tainted Grail population editor validation passed against the actual "
        "unit-6 implementation, documentation, compiled-test sources, and lifecycle wiring."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
