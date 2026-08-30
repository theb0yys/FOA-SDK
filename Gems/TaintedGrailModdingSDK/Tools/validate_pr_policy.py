#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the progressive-rigor pull-request policy and classification."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Mapping

REPO_ROOT = Path(__file__).resolve().parents[3]
TEMPLATE = Path(".github/PULL_REQUEST_TEMPLATE.md")
WORKFLOW = Path(".github/workflows/tainted-grail-sdk-pr-validation.yml")
SECTION_HEADING = "## Change classification"
CLASSIFICATION_IDS = ("routine", "significant", "critical-runtime")
CHECKBOX_RE = re.compile(
    r"^\s*-\s*\[(?P<state>[ xX])\].*?"
    r"<!--\s*change-classification:(?P<identity>[a-z0-9-]+)\s*-->\s*$"
)


class PullRequestPolicyError(RuntimeError):
    """Raised when the PR template, workflow, or selected classification drifts."""


def read_text(root: Path, relative: Path) -> str:
    try:
        return (root / relative).read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeDecodeError) as exc:
        raise PullRequestPolicyError(f"Unable to read {relative.as_posix()}.") from exc


def extract_section(body: str) -> list[str]:
    lines = body.splitlines()
    try:
        start = lines.index(SECTION_HEADING) + 1
    except ValueError as exc:
        raise PullRequestPolicyError(
            f"Pull request body is missing {SECTION_HEADING!r}."
        ) from exc

    section: list[str] = []
    for line in lines[start:]:
        if line.startswith("## "):
            break
        section.append(line)
    return section


def parse_classifications(body: str, *, label: str) -> dict[str, bool]:
    records: dict[str, bool] = {}
    for line in extract_section(body):
        match = CHECKBOX_RE.match(line)
        if not match:
            continue
        identity = match.group("identity")
        if identity in records:
            raise PullRequestPolicyError(
                f"{label} classification {identity!r} appears more than once."
            )
        records[identity] = match.group("state").lower() == "x"

    missing = [identity for identity in CLASSIFICATION_IDS if identity not in records]
    if missing:
        raise PullRequestPolicyError(
            f"{label} is missing classification markers: " + ", ".join(missing)
        )

    unknown = sorted(set(records) - set(CLASSIFICATION_IDS))
    if unknown:
        raise PullRequestPolicyError(
            f"{label} contains unsupported classification markers: "
            + ", ".join(unknown)
        )
    return records


def validate_template(root: Path = REPO_ROOT) -> None:
    template = read_text(root, TEMPLATE)
    workflow = read_text(root, WORKFLOW)

    records = parse_classifications(template, label="Pull request template")
    if any(records.values()):
        raise PullRequestPolicyError(
            "Pull request template classifications must be unchecked by default."
        )

    for fragment in (
        "## Summary",
        "## Scope",
        "## Out of scope",
        "## Design / architecture impact",
        "## Compatibility, data, and rollback",
        "## Validation performed",
        "## Security / protected-data impact",
        "## Documentation",
        "## Author self-review",
        "## Maintainer review",
        "PASSED:",
        "FAILED:",
        "NOT_RUN / NOT_APPLICABLE:",
    ):
        if fragment not in template:
            raise PullRequestPolicyError(
                f"Pull request template is missing required fragment {fragment!r}."
            )

    for prohibited in (
        "## Mandatory merge obligations",
        "merge-head:",
        "merge-obligation:",
    ):
        if prohibited in template:
            raise PullRequestPolicyError(
                f"Pull request template retains superseded fragment {prohibited!r}."
            )

    for required in (
        "Validate pull-request policy contract",
        'validate_pr_policy.py --event "$GITHUB_EVENT_PATH"',
    ):
        if required not in workflow:
            raise PullRequestPolicyError(
                f"PR validation workflow is missing required fragment {required!r}."
            )

    for prohibited in (
        "validate_pr_obligations.py",
        "merge-obligation:",
        "convertPullRequestToDraft",
    ):
        if prohibited in workflow:
            raise PullRequestPolicyError(
                f"PR validation workflow retains superseded fragment {prohibited!r}."
            )


def validate_body(body: str, *, draft: bool) -> None:
    if draft:
        return
    records = parse_classifications(body, label="Ready pull request")
    selected = [identity for identity, checked in records.items() if checked]
    if len(selected) != 1:
        raise PullRequestPolicyError(
            "Ready pull request must select exactly one change classification; "
            f"selected {len(selected)}."
        )


def validate_event(event: Mapping[str, object]) -> None:
    pull_request = event.get("pull_request")
    if pull_request is None:
        return
    if not isinstance(pull_request, dict):
        raise PullRequestPolicyError("pull_request event payload is malformed.")

    body = pull_request.get("body")
    draft = pull_request.get("draft")
    if body is None:
        body = ""
    if not isinstance(body, str) or not isinstance(draft, bool):
        raise PullRequestPolicyError(
            "pull_request body or draft state is malformed."
        )
    validate_body(body, draft=draft)


def parse_arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--event",
        type=Path,
        help="Optional GitHub event payload used to validate a non-draft PR body.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        validate_template()
        if arguments.event is not None:
            event = json.loads(
                arguments.event.read_text(encoding="utf-8", errors="strict")
            )
            if not isinstance(event, dict):
                raise PullRequestPolicyError("GitHub event payload must be an object.")
            validate_event(event)
    except (
        OSError,
        UnicodeDecodeError,
        json.JSONDecodeError,
        PullRequestPolicyError,
    ) as exc:
        print(f"Pull request policy validation failed: {exc}", file=sys.stderr)
        return 1

    print(
        "Pull request policy validation passed: the template uses one progressive-"
        "rigor classification and the workflow enforces it without universal merge "
        "obligations."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
