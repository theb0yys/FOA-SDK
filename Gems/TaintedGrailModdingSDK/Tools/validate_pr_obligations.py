#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate that a ready pull request contains a clear, proportional review record."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Mapping

REQUIRED_SECTIONS = (
    "Summary",
    "Change class",
    "Scope",
    "Out of scope",
    "Validation",
    "Risks and rollback",
)
CHANGE_CLASSES = ("routine", "significant", "critical")
STATUS_TOKENS = (
    "PASSED",
    "FAILED",
    "PARTIAL",
    "BLOCKED",
    "NOT_RUN",
    "NOT_APPLICABLE",
)
SECTION_RE = re.compile(r"^##\s+(?P<name>.+?)\s*$")
CLASS_RE = re.compile(
    r"^\s*-\s*\[(?P<state>[ xX])\].*?"
    r"<!--\s*change-class:(?P<identity>[a-z0-9-]+)\s*-->\s*$"
)
COMMENT_RE = re.compile(r"<!--.*?-->", re.DOTALL)
STATUS_RE = re.compile(
    r"\b(?:" + "|".join(re.escape(token) for token in STATUS_TOKENS) + r")\b"
)


class PullRequestDeclarationError(RuntimeError):
    """Raised when a ready pull request lacks a usable review declaration."""


def parse_sections(body: str) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for line in body.splitlines():
        match = SECTION_RE.match(line)
        if match:
            current = match.group("name").strip()
            if current in sections:
                raise PullRequestDeclarationError(
                    f"Ready pull request section {current!r} appears more than once."
                )
            sections[current] = []
            continue
        if current is not None:
            sections[current].append(line)
    return sections


def meaningful_text(lines: list[str]) -> str:
    text = "\n".join(lines)
    text = COMMENT_RE.sub("", text)
    return "\n".join(line.strip() for line in text.splitlines() if line.strip())


def validate_change_class(lines: list[str]) -> None:
    records: dict[str, bool] = {}
    for line in lines:
        match = CLASS_RE.match(line)
        if not match:
            continue
        identity = match.group("identity")
        if identity in records:
            raise PullRequestDeclarationError(
                f"Ready pull request change class {identity!r} appears more than once."
            )
        records[identity] = match.group("state").lower() == "x"

    missing = [identity for identity in CHANGE_CLASSES if identity not in records]
    if missing:
        raise PullRequestDeclarationError(
            "Ready pull request is missing change-class markers: " + ", ".join(missing)
        )

    unknown = sorted(set(records) - set(CHANGE_CLASSES))
    if unknown:
        raise PullRequestDeclarationError(
            "Ready pull request contains unsupported change classes: "
            + ", ".join(unknown)
        )

    selected = [identity for identity in CHANGE_CLASSES if records[identity]]
    if len(selected) != 1:
        raise PullRequestDeclarationError(
            "Ready pull request must select exactly one change class."
        )


def validate_body(body: str, *, draft: bool, head_sha: str = "") -> None:
    """Validate a pull-request body.

    `head_sha` is retained for caller compatibility. Hosted checks already bind the
    validation run to the event head, so the PR body no longer duplicates that SHA.
    """

    del head_sha
    if draft:
        return

    sections = parse_sections(body)
    missing = [name for name in REQUIRED_SECTIONS if name not in sections]
    if missing:
        raise PullRequestDeclarationError(
            "Ready pull request is missing required sections: " + ", ".join(missing)
        )

    validate_change_class(sections["Change class"])

    for name in ("Summary", "Scope", "Out of scope", "Validation", "Risks and rollback"):
        if not meaningful_text(sections[name]):
            raise PullRequestDeclarationError(
                f"Ready pull request section {name!r} has no substantive content."
            )

    validation = meaningful_text(sections["Validation"])
    if STATUS_RE.search(validation) is None:
        raise PullRequestDeclarationError(
            "Ready pull request Validation section must include an exact validation status."
        )


def validate_event(event: Mapping[str, object]) -> None:
    pull_request = event.get("pull_request")
    if pull_request is None:
        return
    if not isinstance(pull_request, dict):
        raise PullRequestDeclarationError("pull_request event payload is malformed.")

    body = pull_request.get("body")
    draft = pull_request.get("draft")
    if body is None:
        body = ""
    if not isinstance(body, str) or not isinstance(draft, bool):
        raise PullRequestDeclarationError(
            "pull_request body or draft state is malformed."
        )

    validate_body(body, draft=draft)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        event = json.loads(arguments.event.read_text(encoding="utf-8", errors="strict"))
        if not isinstance(event, dict):
            raise PullRequestDeclarationError("GitHub event payload must be an object.")
        validate_event(event)
    except (
        OSError,
        UnicodeDecodeError,
        json.JSONDecodeError,
        PullRequestDeclarationError,
    ) as exc:
        print(f"Pull request declaration validation failed: {exc}", file=sys.stderr)
        return 1

    print(
        "Pull request declaration is complete and proportionate, or the pull request remains draft."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
