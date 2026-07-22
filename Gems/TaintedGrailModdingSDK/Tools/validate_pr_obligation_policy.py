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


def require(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise PullRequestObligationPolicyError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise PullRequestObligationPolicyError(
                f"{label} contains prohibited fragment {fragment!r}."
            )


def validate(root: Path = REPO_ROOT) -> None:
    workflow = read_text(root, WORKFLOW)
    template = read_text(root, TEMPLATE)
    runtime = read_text(root, RUNTIME)

    require(
        workflow,
        (
            "pull_request:",
            "pull_request_target:",
            "ready_for_review",
            "auto_merge_enabled",
            "Enforce ready-PR obligations",
            "github.event_name == 'pull_request_target'",
            "github.event.pull_request.base.sha",
            "pull-requests: write",
            "persist-credentials: false",
            "convertPullRequestToDraft",
            "PULL_REQUEST_NODE_ID",
            "Validate mandatory merge obligations",
            'validate_pr_obligations.py --event "$GITHUB_EVENT_PATH"',
            "github.event_name != 'pull_request_target'",
        ),
        "PR validation workflow",
    )
    reject(
        workflow,
        (
            "github.event.pull_request.head.sha }}\n          persist-credentials",
            "pull_request_target' && github.event.pull_request.draft == true",
        ),
        "PR validation workflow",
    )

    if "## Mandatory merge obligations" not in template:
        raise PullRequestObligationPolicyError(
            "Pull request template is missing the mandatory merge-obligation section."
        )
    if template.count("<!-- merge-head:") != 1:
        raise PullRequestObligationPolicyError(
            "Pull request template must contain exactly one merge-head placeholder."
        )
    if "REPLACE_WITH_CURRENT_40_CHARACTER_HEAD_SHA" not in template:
        raise PullRequestObligationPolicyError(
            "Pull request template must explain the exact-head replacement marker."
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

    require(
        runtime,
        (
            "if draft:",
            "HEAD_MARKER_RE",
            "GIT_SHA_RE",
            "head_sha",
            "missing its exact merge-head marker",
            "merge-head marker appears more than once",
            "malformed merge-head marker",
            "merge obligations are stale",
            "incomplete mandatory merge obligations",
            "appears more than once",
            "unsupported obligation markers",
        ),
        "Runtime obligation gate",
    )


def main() -> int:
    try:
        validate()
    except PullRequestObligationPolicyError as exc:
        print(f"Pull request obligation policy validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Pull request obligation policy validation passed: ready PRs are bound "
        "to the exact head and incomplete or stale PRs are returned to draft."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
