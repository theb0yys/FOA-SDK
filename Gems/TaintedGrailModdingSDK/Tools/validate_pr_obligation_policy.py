#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the read-only pull-request declaration policy."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = Path(".github/workflows/tainted-grail-sdk-pr-validation.yml")
TEMPLATE = Path(".github/PULL_REQUEST_TEMPLATE.md")
RUNTIME = Path("Gems/TaintedGrailModdingSDK/Tools/validate_pr_obligations.py")
REQUIRED_HEADINGS = (
    "## Summary",
    "## Change class",
    "## Scope",
    "## Out of scope",
    "## Validation",
    "## Risks and rollback",
)
CHANGE_CLASSES = ("routine", "significant", "critical")


class PullRequestObligationPolicyError(RuntimeError):
    """Raised when the pull-request declaration policy drifts."""


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
            "push:",
            "branches: [main]",
            "workflow_dispatch:",
            "permissions:",
            "contents: read",
            "static-validation:",
            "canonical-interchange-compiled:",
            "windows-prerequisites:",
            "persist-credentials: false",
            "github.event.pull_request.head.sha || github.sha",
            "github.event.pull_request.base.sha",
            "Validate pull-request declarations",
            'validate_pr_obligations.py --event "$GITHUB_EVENT_PATH"',
            "git diff --check",
            "Windows O3DE prerequisites",
        ),
        "PR validation workflow",
    )
    reject(
        workflow,
        (
            "pull_request_target:",
            "pull-requests: write",
            "issues: write",
            "contents: write",
            "actions: write",
            "convertPullRequestToDraft",
            "gh api",
            "gh pr ",
            "gh issue ",
            "gh workflow ",
            "git push",
            "git commit",
            "secrets.",
        ),
        "PR validation workflow",
    )

    static_job_start = workflow.find("  static-validation:")
    compiled_job_start = workflow.find("  canonical-interchange-compiled:")
    windows_job_start = workflow.find("  windows-prerequisites:")
    if not (0 <= static_job_start < compiled_job_start < windows_job_start):
        raise PullRequestObligationPolicyError(
            "PR validation workflow must keep static, compiled, and Windows prerequisite jobs separate."
        )

    for heading in REQUIRED_HEADINGS:
        if template.count(heading) != 1:
            raise PullRequestObligationPolicyError(
                f"Pull request template must contain exactly one {heading!r} heading."
            )

    for identity in CHANGE_CLASSES:
        marker = f"<!-- change-class:{identity} -->"
        if template.count(marker) != 1:
            raise PullRequestObligationPolicyError(
                f"Pull request template must contain exactly one {marker}."
            )

    require(
        template,
        (
            "PASSED",
            "FAILED",
            "PARTIAL",
            "BLOCKED",
            "NOT_RUN",
            "NOT_APPLICABLE",
            "Validation claims describe only commands and evidence that actually ran",
        ),
        "Pull request template",
    )
    reject(
        template,
        (
            "merge-head:",
            "merge-obligation:",
            "Mandatory merge obligations",
        ),
        "Pull request template",
    )

    require(
        runtime,
        (
            "REQUIRED_SECTIONS",
            "CHANGE_CLASSES",
            "STATUS_TOKENS",
            "if draft:",
            "missing required sections",
            "select exactly one change class",
            "has no substantive content",
            "must include an exact validation status",
            "Pull request declaration validation failed",
        ),
        "Runtime declaration validator",
    )
    reject(
        runtime,
        (
            "merge-head:",
            "merge-obligation:",
            "OBLIGATION_IDS",
        ),
        "Runtime declaration validator",
    )


def main() -> int:
    try:
        validate()
    except PullRequestObligationPolicyError as exc:
        print(f"Pull request declaration policy validation failed: {exc}", file=sys.stderr)
        return 1

    print(
        "Pull request declaration policy validation passed: automation remains read-only and ready reviews require clear scope, one change class, explicit validation status, and rollback information."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
