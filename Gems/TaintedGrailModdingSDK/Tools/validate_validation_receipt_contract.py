#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate exact-head receipt tooling, tests, template, and merge policy wiring."""

from __future__ import annotations

import sys
from pathlib import Path


class ValidationReceiptContractError(RuntimeError):
    pass


def read(repo_root: Path, relative: str) -> str:
    path = repo_root / relative
    if not path.is_file():
        raise ValidationReceiptContractError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise ValidationReceiptContractError(
            f"{label} is missing required fragment {fragment!r}."
        )


def require_all(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        require(text, fragment, label)


def validate(repo_root: Path) -> None:
    tool = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py",
    )
    require_all(
        tool,
        (
            'RECEIPT_NAME = "validation-receipt.json"',
            "SOURCE_COMMIT_RE",
            "MANDATORY_PASS_GATES",
            '"git-diff-check"',
            '"local-validation"',
            "RISK_ACCEPTABLE_GATES",
            '"o3de-configure"',
            '"o3de-build"',
            '"compiled-tests"',
            '"windows-ui"',
            "hash_file(",
            "validate_log_reference(",
            "Receipt commit",
            "must pass; it cannot be waived",
            "explicit maintainer risk acceptance",
            "Finalized receipts are immutable",
            'subparsers.add_parser("summarize"',
        ),
        "Validation receipt tool",
    )

    tests = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/tests/test_validation_receipt.py",
    )
    require_all(
        tests,
        (
            "test_merge_ready_receipt_verifies_and_summarizes",
            "test_tampered_log_is_rejected",
            "test_wrong_expected_commit_is_rejected",
            "test_mandatory_local_validation_cannot_be_waived",
            "test_skipped_host_gate_requires_maintainer_acceptance",
            "test_finalized_receipt_is_immutable",
        ),
        "Validation receipt unit tests",
    )

    template = read(repo_root, ".github/PULL_REQUEST_TEMPLATE.md")
    require_all(
        template,
        (
            "## Exact-head validation receipt",
            "validation_receipt.py verify",
            "--require-merge-ready",
            "validation_receipt.py summarize",
            "Do not commit the receipt or logs",
        ),
        "Pull request template",
    )

    ci_policy = read(
        repo_root,
        "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md",
    )
    require_all(
        ci_policy,
        (
            "## Exact-head validation receipt",
            "validation_receipt.py init",
            "validation_receipt.py record",
            "validation_receipt.py accept-risk",
            "validation_receipt.py finalize",
            "validation_receipt.py verify",
            "validation_receipt.py summarize",
            "must not be committed",
        ),
        "CI and local validation policy",
    )

    review_policy = read(
        repo_root,
        "docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md",
    )
    require_all(
        review_policy,
        (
            "merge-ready exact-head validation receipt",
            "validation_receipt.py verify",
            "--require-merge-ready",
            "receipt source commit matches the reviewed head",
        ),
        "Review and merge policy",
    )

    gate = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py",
    )
    require(
        gate,
        '"validate_validation_receipt_contract.py"',
        "Authoritative local gate",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, ValidationReceiptContractError) as exc:
        print(f"Validation-receipt contract failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Validation-receipt contract passed: tooling, negative tests, PR template, "
        "and merge policy require exact-head machine-verifiable evidence."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
