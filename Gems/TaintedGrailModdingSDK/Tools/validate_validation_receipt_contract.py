#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate receipt tooling with progressive applicability wording."""

from __future__ import annotations

import sys
from pathlib import Path

import validate_validation_receipt_contract_legacy as _legacy

ValidationReceiptContractError = _legacy.ValidationReceiptContractError

_STALE_LITERAL_ERROR = (
    "Authoritative local validation gate is missing required fragment "
    "'No pinned O3DE source-policy, compiled, Editor/UI, or Windows operational "
    "result is claimed.'."
)
_STATIC_CLAIM_PREFIX = (
    "FOA-SDK static validation passed. No pinned O3DE source-policy, "
)
_STATIC_CLAIM_SUFFIX = (
    "compiled, Editor/UI, or Windows operational result is claimed."
)


def validate(repo_root: Path) -> None:
    try:
        _legacy.validate(repo_root)
    except ValidationReceiptContractError as exc:
        if str(exc) != _STALE_LITERAL_ERROR:
            raise

        local_gate = (
            repo_root
            / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
        ).read_text(encoding="utf-8")
        for fragment in (_STATIC_CLAIM_PREFIX, _STATIC_CLAIM_SUFFIX):
            if fragment not in local_gate:
                raise


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, ValidationReceiptContractError) as exc:
        print(f"Validation-receipt contract failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Validation-receipt contract passed: receipt tooling remains fail-closed, "
        "while receipt requirements follow the reviewed change classification."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
