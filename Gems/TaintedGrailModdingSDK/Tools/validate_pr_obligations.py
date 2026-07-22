#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
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


def validate_body(body: str, *, draft: bool) -> None:
    if draft:
        return

    records: dict[str, bool] = {}
    for line in extract_section(body):
        match = CHECKBOX_RE.match(line)
        if not match:
            continue
        identity = match.group("identity")
        if identity in records:
            raise PullRequestObligationError(
                f"Mandatory merge obligation {identity!r} appears more than once."
            )
        records[identity] = match.group("state").lower() == "x"

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
    if body is None:
        body = ""
    if not isinstance(body, str) or not isinstance(draft, bool):
        raise PullRequestObligationError("pull_request body or draft state is malformed.")
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
            raise PullRequestObligationError("GitHub event payload must be an object.")
        validate_event(event)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, PullRequestObligationError) as exc:
        print(f"Pull request obligation validation failed: {exc}", file=sys.stderr)
        return 1
    print("Pull request mandatory merge obligations are complete or the pull request remains draft.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
