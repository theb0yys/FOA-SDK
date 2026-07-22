#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Fail a ready pull request when mandatory merge obligations remain incomplete."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Mapping

SECTION_HEADING = "## Mandatory merge obligations"
OBLIGATION_IDS = (
    "static",
    "reviewed-range",
    "host-build",
    "compiled-tests",
    "receipt",
    "ui-evidence",
    "review",
)
CHECKBOX_RE = re.compile(
    r"^\s*-\s*\[(?P<state>[ xX])\].*?"
    r"<!--\s*merge-obligation:(?P<identity>[a-z0-9-]+)\s*-->\s*$"
)
HEAD_MARKER_RE = re.compile(
    r"^\s*<!--\s*merge-head:(?P<sha>[0-9a-f]{40})\s*-->\s*$"
)
GIT_SHA_RE = re.compile(r"^[0-9a-f]{40}$")


class PullRequestObligationError(RuntimeError):
    """Raised when a ready pull request is not merge-complete."""


def extract_section(body: str) -> list[str]:
    lines = body.splitlines()
    try:
        start = lines.index(SECTION_HEADING) + 1
    except ValueError as exc:
        raise PullRequestObligationError(
            f"Ready pull request body is missing {SECTION_HEADING!r}."
        ) from exc

    section: list[str] = []
    for line in lines[start:]:
        if line.startswith("## "):
            break
        section.append(line)
    return section


def validate_body(body: str, *, draft: bool, head_sha: str = "") -> None:
    if draft:
        return
    if GIT_SHA_RE.fullmatch(head_sha) is None:
        raise PullRequestObligationError(
            "Ready pull request event is missing a valid 40-character head SHA."
        )

    records: dict[str, bool] = {}
    recorded_heads: list[str] = []
    malformed_head_marker = False
    for line in extract_section(body):
        head_match = HEAD_MARKER_RE.match(line)
        if head_match:
            recorded_heads.append(head_match.group("sha"))
            continue
        if "merge-head:" in line:
            malformed_head_marker = True

        match = CHECKBOX_RE.match(line)
        if not match:
            continue
        identity = match.group("identity")
        if identity in records:
            raise PullRequestObligationError(
                f"Mandatory merge obligation {identity!r} appears more than once."
            )
        records[identity] = match.group("state").lower() == "x"

    if malformed_head_marker:
        raise PullRequestObligationError(
            "Ready pull request body contains a malformed merge-head marker."
        )
    if not recorded_heads:
        raise PullRequestObligationError(
            "Ready pull request body is missing its exact merge-head marker."
        )
    if len(recorded_heads) != 1:
        raise PullRequestObligationError(
            "Ready pull request merge-head marker appears more than once."
        )
    if recorded_heads[0] != head_sha:
        raise PullRequestObligationError(
            "Ready pull request merge obligations are stale: recorded head "
            f"{recorded_heads[0]} does not match current head {head_sha}."
        )

    missing = [identity for identity in OBLIGATION_IDS if identity not in records]
    if missing:
        raise PullRequestObligationError(
            "Ready pull request body is missing mandatory obligation markers: "
            + ", ".join(missing)
        )

    unknown = sorted(set(records) - set(OBLIGATION_IDS))
    if unknown:
        raise PullRequestObligationError(
            "Ready pull request body contains unsupported obligation markers: "
            + ", ".join(unknown)
        )

    incomplete = [identity for identity in OBLIGATION_IDS if not records[identity]]
    if incomplete:
        raise PullRequestObligationError(
            "Ready pull request has incomplete mandatory merge obligations: "
            + ", ".join(incomplete)
        )


def validate_event(event: Mapping[str, object]) -> None:
    pull_request = event.get("pull_request")
    if pull_request is None:
        return
    if not isinstance(pull_request, dict):
        raise PullRequestObligationError("pull_request event payload is malformed.")
    body = pull_request.get("body")
    draft = pull_request.get("draft")
    head = pull_request.get("head")
    if body is None:
        body = ""
    if (
        not isinstance(body, str)
        or not isinstance(draft, bool)
        or not isinstance(head, dict)
        or not isinstance(head.get("sha"), str)
    ):
        raise PullRequestObligationError(
            "pull_request body, draft state, or head identity is malformed."
        )
    validate_body(body, draft=draft, head_sha=head["sha"])


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        event = json.loads(arguments.event.read_text(encoding="utf-8", errors="strict"))
        if not isinstance(event, dict):
            raise PullRequestObligationError("GitHub event payload must be an object.")
        validate_event(event)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, PullRequestObligationError) as exc:
        print(f"Pull request obligation validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Pull request mandatory merge obligations are complete for the exact head "
        "or the pull request remains draft."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
