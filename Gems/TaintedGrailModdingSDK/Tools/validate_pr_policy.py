#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the progressive-rigor FOA-SDK pull-request policy contract."""

from __future__ import annotations

import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = Path(".github/workflows/tainted-grail-sdk-pr-validation.yml")
TEMPLATE = Path(".github/PULL_REQUEST_TEMPLATE.md")
CLASSIFICATIONS = (
    "routine",
    "significant",
    "critical-runtime",
)
REQUIRED_HEADINGS = (
    "## Summary",
    "## Change classification",
    "## Scope",
    "## Out of scope",
    "## Design / architecture impact",
    "## Compatibility, data, and rollback",
    "## Validation performed",
    "## Security / protected-data impact",
    "## Documentation",
    "## Author self-review",
    "## Maintainer review",
)


class PullRequestPolicyError(RuntimeError):
    """Raised when the PR template or read-only workflow contract drifts."""


def read_text(root: Path, relative: Path) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeDecodeError) as exc:
        raise PullRequestPolicyError(
            f"Unable to read {relative.as_posix()}."
        ) from exc


def validate_template(template: str) -> None:
    for heading in REQUIRED_HEADINGS:
        if template.count(heading) != 1:
            raise PullRequestPolicyError(
                f"Pull request template must contain exactly one {heading!r}."
            )

    for classification in CLASSIFICATIONS:
        marker = f"<!-- change-classification:{classification} -->"
        if template.count(marker) != 1:
            raise PullRequestPolicyError(
                f"Pull request template must contain exactly one {marker}."
            )

    for required in (
        "Select exactly one.",
        "Classification rationale:",
        "List the **exact commands/checks that actually ran** and their results.",
        "NOT_RUN / NOT_APPLICABLE:",
        "The diff is focused on the stated scope.",
        "Required validation for the changed surface is satisfied.",
    ):
        if required not in template:
            raise PullRequestPolicyError(
                f"Pull request template is missing required text {required!r}."
            )

    for prohibited in (
        "## Mandatory merge obligations",
        "merge-obligation:",
        "merge-head:",
        "REPLACE_WITH_CURRENT_40_CHARACTER_HEAD_SHA",
    ):
        if prohibited in template:
            raise PullRequestPolicyError(
                f"Pull request template retains obsolete universal gate {prohibited!r}."
            )


def validate_workflow(workflow: str) -> None:
    for required in (
        "Validate pull-request policy contract",
        "python Gems/TaintedGrailModdingSDK/Tools/validate_pr_policy.py",
        "git diff --check",
        "contents: read",
    ):
        if required not in workflow:
            raise PullRequestPolicyError(
                f"PR validation workflow is missing required text {required!r}."
            )

    for prohibited in (
        "validate_pr_obligations.py",
        "merge-obligation:",
        "pull_request_target:",
        "pull-requests: write",
        "contents: write",
    ):
        if prohibited in workflow:
            raise PullRequestPolicyError(
                f"PR validation workflow retains prohibited text {prohibited!r}."
            )


def validate(root: Path = REPO_ROOT) -> None:
    validate_template(read_text(root, TEMPLATE))
    validate_workflow(read_text(root, WORKFLOW))


def main() -> int:
    try:
        validate()
    except PullRequestPolicyError as exc:
        print(f"Pull request policy validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Pull request policy validation passed: classification is explicit, actual "
        "validation is reported, and obsolete universal merge obligations are absent."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
