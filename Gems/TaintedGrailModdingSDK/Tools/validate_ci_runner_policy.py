#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the temporary manual-only CI and runner policy."""

from __future__ import annotations

import sys
from pathlib import Path


class CiRunnerPolicyError(RuntimeError):
    """Raised when repository automation can imply unavailable test coverage."""


REMOVED_AUTOMATIC_WORKFLOWS = (
    ".github/workflows/ar.yml",
    ".github/workflows/validation.yaml",
)

MANUAL_WORKFLOWS = (
    ".github/workflows/tainted-grail-sdk-foundation.yml",
    ".github/workflows/tainted-grail-editor-entry.yml",
    ".github/workflows/tainted-grail-repository-hygiene.yml",
)


def read_text(path: Path) -> str:
    if not path.is_file():
        raise CiRunnerPolicyError(f"Required CI policy file is missing: {path}")
    return path.read_text(encoding="utf-8")


def validate_ci_runner_policy(repo_root: Path) -> None:
    for relative_path in REMOVED_AUTOMATIC_WORKFLOWS:
        path = repo_root / relative_path
        if path.exists():
            raise CiRunnerPolicyError(
                f"Unavailable automatic workflow must remain removed: {relative_path}"
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
                f"Manual workflow does not explain the suspension: {relative_path}"
            )

    local_runner = repo_root / (
        "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
    )
    if not local_runner.is_file():
        raise CiRunnerPolicyError("The local validation entry point is missing.")

    policy = read_text(
        repo_root / "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
    )
    required_policy_fragments = (
        "run_local_validation.py",
        "manual-only",
        "self-hosted runner",
        "registration token",
        "No automated per-commit test result is claimed",
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
    print("CI runner policy validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
