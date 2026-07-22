#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Validate the repository-owned pull-request merge-obligation gate."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = Path(".github/workflows/tainted-grail-sdk-pr-validation.yml")
TEMPLATE = Path(".github/PULL_REQUEST_TEMPLATE.md")
RUNTIME = Path("Gems/TaintedGrailModdingSDK/Tools/validate_pr_obligations.py")
OBLIGATION_IDS = (
    "static",
    "reviewed-range",
    "host-build",
    "compiled-tests",
    "receipt",
    "ui-evidence",
    "review",
)


class PullRequestObligationPolicyError(RuntimeError):
    """Raised when the merge-obligation enforcement surface drifts."""


def read_text(root: Path, relative: Path) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeDecodeError) as exc:
        raise PullRequestObligationPolicyError(
            f"Unable to read {relative.as_posix()}."
        ) from exc


def validate(root: Path = REPO_ROOT) -> None:
    workflow = read_text(root, WORKFLOW)
    template = read_text(root, TEMPLATE)
    runtime = read_text(root, RUNTIME)

    for fragment in (
        "types:",
        "ready_for_review",
        "converted_to_draft",
        "edited",
        "Validate mandatory merge obligations",
        "validate_pr_obligations.py --event \"$GITHUB_EVENT_PATH\"",
    ):
        if fragment not in workflow:
            raise PullRequestObligationPolicyError(
                f"PR validation workflow is missing {fragment!r}."
            )

    if "## Mandatory merge obligations" not in template:
        raise PullRequestObligationPolicyError(
            "Pull request template is missing the mandatory merge-obligation section."
        )
    for identity in OBLIGATION_IDS:
        marker = f"<!-- merge-obligation:{identity} -->"
        if template.count(marker) != 1:
            raise PullRequestObligationPolicyError(
                f"Pull request template must contain exactly one {marker}."
            )
        if f'"{identity}"' not in runtime:
            raise PullRequestObligationPolicyError(
                f"Runtime gate does not govern obligation {identity!r}."
            )

    for fragment in (
        "if draft:",
        "incomplete mandatory merge obligations",
        "appears more than once",
        "unsupported obligation markers",
    ):
        if fragment not in runtime:
            raise PullRequestObligationPolicyError(
                f"Runtime obligation gate is missing fail-closed behavior {fragment!r}."
            )


def main() -> int:
    try:
        validate()
    except PullRequestObligationPolicyError as exc:
        print(f"Pull request obligation policy validation failed: {exc}", file=sys.stderr)
        return 1
    print("Pull request obligation policy validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
