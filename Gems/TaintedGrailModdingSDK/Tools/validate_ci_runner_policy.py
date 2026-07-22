#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the split GitHub-hosted static and exact-head host validation policy."""

from __future__ import annotations

import sys
from pathlib import Path


class CiRunnerPolicyError(RuntimeError):
    """Raised when repository automation can imply unavailable test coverage."""


REMOVED_AUTOMATIC_WORKFLOWS = (
    ".github/workflows/ar.yml",
    ".github/workflows/validation.yaml",
)

AUTOMATIC_STATIC_WORKFLOW = (
    ".github/workflows/tainted-grail-sdk-pr-validation.yml"
)

MANUAL_WORKFLOWS = (
    ".github/workflows/tainted-grail-sdk-foundation.yml",
    ".github/workflows/tainted-grail-editor-entry.yml",
    ".github/workflows/tainted-grail-repository-hygiene.yml",
    ".github/workflows/tainted-grail-sdk-installer.yml",
)


def read_text(path: Path) -> str:
    if not path.is_file():
        raise CiRunnerPolicyError(f"Required CI policy file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise CiRunnerPolicyError(
                f"{label} is missing required fragment {fragment!r}."
            )


def reject_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise CiRunnerPolicyError(
                f"{label} contains prohibited fragment {fragment!r}."
            )


def validate_ci_runner_policy(repo_root: Path) -> None:
    for relative_path in REMOVED_AUTOMATIC_WORKFLOWS:
        path = repo_root / relative_path
        if path.exists():
            raise CiRunnerPolicyError(
                f"Unavailable inherited workflow must remain removed: {relative_path}"
            )

    automatic = read_text(repo_root / AUTOMATIC_STATIC_WORKFLOW)
    require_fragments(
        automatic,
        (
            "pull_request:",
            "push:",
            "workflow_dispatch:",
            "runs-on: ubuntu-latest",
            "permissions:",
            "contents: read",
            "timeout-minutes:",
            "run_local_validation.py --keep-going --static-only --skip-source-policy",
        ),
        "Automatic TG SDK static workflow",
    )
    reject_fragments(
        automatic,
        (
            "self-hosted",
            "--ctest-build-dir",
            "windows-latest",
            "secrets.",
        ),
        "Automatic TG SDK static workflow",
    )

    for relative_path in MANUAL_WORKFLOWS:
        text = read_text(repo_root / relative_path)
        if "workflow_dispatch:" not in text:
            raise CiRunnerPolicyError(
                f"Manual workflow lacks workflow_dispatch: {relative_path}"
            )
        for forbidden in ("pull_request:", "push:", "self-hosted"):
            if forbidden in text:
                raise CiRunnerPolicyError(
                    f"Manual workflow contains forbidden trigger/runner {forbidden!r}: "
                    f"{relative_path}"
                )
        if "Automatic triggers are intentionally suspended" not in text:
            raise CiRunnerPolicyError(
                f"Manual workflow does not explain the host-trigger suspension: "
                f"{relative_path}"
            )

    local_runner = read_text(
        repo_root
        / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
    )
    require_fragments(
        local_runner,
        (
            '"--static-only"',
            '"--ctest-build-dir"',
            '"--no-tests=error"',
            "def run_validation_pipeline(",
            "def should_run_stage(",
            "Pinned O3DE source policy, ",
            "compiled tests, and Windows acceptance remain mandatory exact-head ",
        ),
        "Local validation entry point",
    )

    policy = read_text(
        repo_root / "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
    )
    required_policy_fragments = (
        "run_local_validation.py",
        "automatic pull-request static validation",
        "--static-only",
        "--ctest-build-dir",
        "compiled Catalog CTest",
        "self-hosted runner",
        "registration token",
        "does not claim an O3DE build",
        "Pending is not passing",
    )
    for fragment in required_policy_fragments:
        if fragment not in policy:
            raise CiRunnerPolicyError(
                f"CI/local-validation policy is missing {fragment!r}."
            )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_ci_runner_policy(repo_root)
    except CiRunnerPolicyError as error:
        print(f"CI runner policy validation failed: {error}", file=sys.stderr)
        return 1
    print(
        "CI runner policy validation passed: automatic static coverage and "
        "mandatory exact-head host coverage remain separate and explicit."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
