#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate read-only GitHub automation and exact-head host validation policy."""

from __future__ import annotations

import sys
from pathlib import Path


class CiRunnerPolicyError(RuntimeError):
    """Raised when repository automation can imply unavailable or unauthorized coverage."""


REMOVED_AUTOMATIC_WORKFLOWS = (
    ".github/workflows/ar.yml",
    ".github/workflows/validation.yaml",
)

AUTOMATIC_STATIC_WORKFLOW = ".github/workflows/tainted-grail-sdk-pr-validation.yml"

MANUAL_WORKFLOWS = (
    ".github/workflows/tainted-grail-sdk-foundation.yml",
    ".github/workflows/tainted-grail-editor-entry.yml",
    ".github/workflows/tainted-grail-repository-hygiene.yml",
    ".github/workflows/tainted-grail-sdk-installer.yml",
)

AGENT_POLICY = "AGENTS.md"
CI_POLICY = "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
LOCAL_RUNNER = "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"


def read_text(path: Path) -> str:
    if not path.is_file():
        raise CiRunnerPolicyError(f"Required CI policy file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise CiRunnerPolicyError(f"{label} is missing required fragment {fragment!r}.")


def reject_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment in text:
            raise CiRunnerPolicyError(f"{label} contains prohibited fragment {fragment!r}.")


def validate_removed_workflows(repo_root: Path) -> None:
    for relative_path in REMOVED_AUTOMATIC_WORKFLOWS:
        if (repo_root / relative_path).exists():
            raise CiRunnerPolicyError(
                f"Unavailable inherited workflow must remain removed: {relative_path}"
            )


def validate_manual_workflows(repo_root: Path) -> None:
    for relative_path in MANUAL_WORKFLOWS:
        text = read_text(repo_root / relative_path)
        if "workflow_dispatch:" not in text:
            raise CiRunnerPolicyError(f"Manual workflow lacks workflow_dispatch: {relative_path}")
        for forbidden in (
            "pull_request:",
            "pull_request_target:",
            "push:",
            "self-hosted",
            "contents: write",
            "pull-requests: write",
            "issues: write",
            "actions: write",
            "gh workflow run",
            "gh pr ",
            "gh issue ",
            "git push",
        ):
            if forbidden in text:
                raise CiRunnerPolicyError(
                    f"Manual workflow contains forbidden trigger, permission, or mutation "
                    f"{forbidden!r}: {relative_path}"
                )
        require_fragments(
            text,
            ("permissions:", "contents: read"),
            f"Manual workflow {relative_path}",
        )


def validate_local_runner(repo_root: Path) -> None:
    local_runner = read_text(repo_root / LOCAL_RUNNER)
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


def validate_agent_mode(repo_root: Path, automatic: str) -> None:
    agent_policy = read_text(repo_root / AGENT_POLICY)
    require_fragments(
        agent_policy,
        (
            "Mandatory GitHub Agent Policy",
            "The designated branch is `main`",
            "commits directly to `main`",
            "create, rename, delete, or switch branches",
            "open, update, close, merge, or otherwise modify pull requests",
            "create, update, close, label, assign, lock, or comment on issues",
            "trigger, cancel, approve, or rerun workflows or jobs",
            "committing the user-requested file changes directly to the designated branch",
        ),
        "Agent policy",
    )

    require_fragments(
        automatic,
        (
            "pull_request:",
            "push:",
            "workflow_dispatch:",
            '".github/**"',
            '"docs/**"',
            '"scripts/**"',
            "permissions:",
            "contents: read",
            "static-validation:",
            "canonical-interchange-compiled:",
            "windows-prerequisites:",
            "runs-on: ubuntu-latest",
            "runs-on: windows-2022",
            "runs-on: windows-latest",
            "github.event.pull_request.head.sha || github.sha",
            "persist-credentials: false",
            "fetch-depth: 0",
            "github.event.pull_request.base.sha",
            "github.event.before",
            "git diff --check",
            "tg-sdk-reviewed-range.txt",
            "run_local_validation.py --keep-going --static-only --skip-source-policy",
            "--target \"$env:TEST_TARGET\" --parallel 2",
            "--no-tests=error",
            "developer_preview.py prerequisites",
        ),
        "Read-only TG SDK validation workflow",
    )
    reject_fragments(
        automatic,
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
            "--force",
            "self-hosted",
            "secrets.",
        ),
        "Read-only TG SDK validation workflow",
    )

    static_job_start = automatic.find("  static-validation:")
    compiled_job_start = automatic.find("  canonical-interchange-compiled:")
    windows_job_start = automatic.find("  windows-prerequisites:")
    if not (0 <= static_job_start < compiled_job_start < windows_job_start):
        raise CiRunnerPolicyError(
            "Read-only TG SDK workflow must keep static, compiled, and Windows "
            "prerequisite gates in separate ordered jobs."
        )

    static_job = automatic[static_job_start:compiled_job_start]
    reject_fragments(
        static_job,
        ("pull-requests: write", "contents: write", "self-hosted", "secrets."),
        "Read-only static validation job",
    )
    require_fragments(
        static_job,
        (
            "fetch-depth: 0",
            "persist-credentials: false",
            "git diff --check",
            "run_local_validation.py --keep-going --static-only --skip-source-policy",
        ),
        "Read-only static validation job",
    )

    compiled_job = automatic[compiled_job_start:windows_job_start]
    reject_fragments(
        compiled_job,
        ("contents: write", "pull-requests: write", "self-hosted", "secrets."),
        "Compiled validation job",
    )
    require_fragments(
        compiled_job,
        (
            "runs-on: windows-2022",
            "persist-credentials: false",
            "--parallel 2",
            "--no-tests=error",
        ),
        "Compiled validation job",
    )

    windows_job = automatic[windows_job_start:]
    reject_fragments(
        windows_job,
        (
            "contents: write",
            "pull-requests: write",
            "self-hosted",
            "secrets.",
            "cmake --build",
            "--ctest-build-dir",
        ),
        "Windows prerequisite job",
    )
    require_fragments(
        windows_job,
        (
            "runs-on: windows-latest",
            "persist-credentials: false",
            "O3DE_COMMIT:",
            "sparse-checkout",
            "developer_preview.py prerequisites",
        ),
        "Windows prerequisite job",
    )

    policy = read_text(repo_root / CI_POLICY)
    require_fragments(
        policy,
        (
            "Binding automation boundary",
            "normal file commits directly to",
            "Automated validation is read-only",
            "no `pull_request_target` trigger",
            "must not push commits, move refs, create branches, post comments",
            "--parallel 2",
            "--static-only",
            "--ctest-build-dir",
            "--no-tests=error",
            "Pending is not passing",
            "self-declared metadata are not proof",
            "registration token is a secret",
        ),
        "CI/local-validation policy",
    )


def validate_legacy_mode(repo_root: Path, automatic: str) -> None:
    """Retain compatibility for synthetic unit-test repositories without AGENTS.md."""

    require_fragments(
        automatic,
        (
            "pull_request:",
            "pull_request_target:",
            "push:",
            "workflow_dispatch:",
            '".github/**"',
            '"docs/**"',
            '"scripts/**"',
            "runs-on: ubuntu-latest",
            "runs-on: windows-latest",
            "contents: read",
            "run_local_validation.py --keep-going --static-only --skip-source-policy",
            "Enforce ready-PR obligations",
            "github.event_name == 'pull_request_target'",
            "pull-requests: write",
            "persist-credentials: false",
            "fetch-depth: 0",
            "git diff --check",
            "tg-sdk-reviewed-range.txt",
            "convertPullRequestToDraft",
            "Windows O3DE prerequisites",
            "developer_preview.py prerequisites",
        ),
        "Automatic TG SDK workflow",
    )
    reject_fragments(
        automatic,
        ("self-hosted", "--ctest-build-dir", "secrets."),
        "Automatic TG SDK workflow",
    )

    target_job_start = automatic.find("  enforce-obligations:")
    static_job_start = automatic.find("  static-validation:")
    windows_job_start = automatic.find("  windows-prerequisites:")
    if target_job_start < 0 or static_job_start <= target_job_start or windows_job_start <= static_job_start:
        raise CiRunnerPolicyError(
            "Automatic TG SDK workflow does not keep target enforcement, static validation, "
            "and Windows prerequisites in separate ordered jobs."
        )

    privileged_job = automatic[target_job_start:static_job_start]
    require_fragments(
        privileged_job,
        ("github.event.pull_request.base.sha", "validate_pr_obligations.py", "convertPullRequestToDraft"),
        "Privileged PR-target job",
    )
    reject_fragments(
        privileged_job,
        (
            "run_local_validation.py",
            "ctest",
            "cmake",
            "xvfb-run",
            "pip install",
            "npm ",
            "make ",
            "lfs: true",
            "windows-latest",
            "github.event.pull_request.head.sha",
        ),
        "Privileged PR-target job",
    )

    static_job = automatic[static_job_start:windows_job_start]
    require_fragments(
        static_job,
        (
            "github.event.pull_request.head.sha",
            "fetch-depth: 0",
            "github.event.pull_request.base.sha",
            "github.event.before",
            "git diff --check",
            "tg-sdk-reviewed-range.log",
            "tg-sdk-reviewed-range.txt",
        ),
        "Read-only static validation job",
    )
    reject_fragments(
        static_job,
        ("pull-requests: write", "self-hosted", "secrets."),
        "Read-only static validation job",
    )

    windows_job = automatic[windows_job_start:]
    require_fragments(
        windows_job,
        (
            "runs-on: windows-latest",
            "contents: read",
            "persist-credentials: false",
            "github.event.pull_request.head.sha",
            "O3DE_COMMIT:",
            "sparse-checkout",
            "developer_preview.py prerequisites",
            "tg-sdk-windows-prerequisites.log",
        ),
        "Windows prerequisite job",
    )
    reject_fragments(
        windows_job,
        ("pull-requests: write", "self-hosted", "secrets.", "cmake --build", "--ctest-build-dir"),
        "Windows prerequisite job",
    )

    policy = read_text(repo_root / CI_POLICY)
    require_fragments(
        policy,
        (
            "run_local_validation.py",
            "automatic pull-request static validation",
            "pull_request_target",
            "trusted base commit",
            "pull request to draft",
            "reviewed-range",
            "Windows prerequisite",
            "--static-only",
            "--ctest-build-dir",
            "compiled Catalog CTest",
            "self-hosted runner",
            "registration token",
            "does not claim an O3DE build",
            "Pending is not passing",
        ),
        "CI/local-validation policy",
    )


def validate_ci_runner_policy(repo_root: Path) -> None:
    validate_removed_workflows(repo_root)
    automatic = read_text(repo_root / AUTOMATIC_STATIC_WORKFLOW)
    if (repo_root / AGENT_POLICY).is_file():
        validate_agent_mode(repo_root, automatic)
    else:
        validate_legacy_mode(repo_root, automatic)
    validate_manual_workflows(repo_root)
    validate_local_runner(repo_root)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_ci_runner_policy(repo_root)
    except CiRunnerPolicyError as error:
        print(f"CI runner policy validation failed: {error}", file=sys.stderr)
        return 1
    print(
        "CI runner policy validation passed: GitHub automation is read-only, exact-head "
        "validation remains separated by evidence layer, and host-heavy workflows remain manual."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
