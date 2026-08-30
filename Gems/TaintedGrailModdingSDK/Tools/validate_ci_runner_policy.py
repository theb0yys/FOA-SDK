#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate read-only GitHub automation and progressive FOA-SDK CI policy."""

from __future__ import annotations

import sys
from pathlib import Path


class CiRunnerPolicyError(RuntimeError):
    """Raised when repository automation or its policy drifts."""


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
ENGINEERING_PROCESS = "docs/tainted-grail-sdk/ENGINEERING_PROCESS.md"
CI_POLICY = "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md"
LOCAL_RUNNER = "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
PR_POLICY_VALIDATOR = "Gems/TaintedGrailModdingSDK/Tools/validate_pr_policy.py"


def read_text(path: Path) -> str:
    if not path.is_file():
        raise CiRunnerPolicyError(f"Required CI policy file is missing: {path}")
    return path.read_text(encoding="utf-8", errors="strict")


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


def uses_progressive_process(repo_root: Path) -> bool:
    return (repo_root / ENGINEERING_PROCESS).is_file()


def validate_agent_policy(repo_root: Path, *, progressive: bool) -> None:
    agent_policy = read_text(repo_root / AGENT_POLICY)
    if progressive:
        require_fragments(
            agent_policy,
            (
                "# FOA-SDK Agent Execution Policy",
                "## Authority order",
                "## Research escalation",
                "Research is a tool, not a universal precondition.",
                "Routine implementation inside accepted architecture does not require",
                "focused non-`main` working branch",
                "pull request for maintainer audit",
                "commit directly to `main`",
                "leave approval and merge to the maintainer",
                "Use exact states:",
            ),
            "Agent execution policy",
        )
    else:
        # Compatibility path for the historical isolated unit-test fixture. The real
        # repository takes the progressive path because ENGINEERING_PROCESS.md exists.
        require_fragments(
            agent_policy,
            (
                "Mandatory GitHub Agent Policy",
                "binding for every automated agent, assistant, bot, workflow",
                "The reviewed integration branch is `main`",
                "non-`main` working branch",
                "pull request for maintainer audit",
                "commit directly to `main`",
                "bypass a required pull-request audit",
                "modify tests, validators, workflows, process documents, governance documents",
                "claim validation, review, approval, authorization, provenance, signing, or completion",
                "leave merge, approval, and final acceptance to the maintainer",
                "direct-to-main defaults",
            ),
            "Legacy isolated agent-policy fixture",
        )


def validate_removed_workflows(repo_root: Path) -> None:
    for relative_path in REMOVED_AUTOMATIC_WORKFLOWS:
        if (repo_root / relative_path).exists():
            raise CiRunnerPolicyError(
                f"Unavailable inherited workflow must remain removed: {relative_path}"
            )


def validate_manual_workflows(
    repo_root: Path,
    *,
    require_explicit_read_only: bool,
) -> None:
    for relative_path in MANUAL_WORKFLOWS:
        text = read_text(repo_root / relative_path)
        if "workflow_dispatch:" not in text:
            raise CiRunnerPolicyError(
                f"Manual workflow lacks workflow_dispatch: {relative_path}"
            )
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
        if require_explicit_read_only:
            require_fragments(
                text,
                ("permissions:", "contents: read"),
                f"Manual workflow {relative_path}",
            )


def validate_local_runner(repo_root: Path, *, progressive: bool) -> None:
    local_runner = read_text(repo_root / LOCAL_RUNNER)
    common = (
        '"--static-only"',
        '"--ctest-build-dir"',
        '"--no-tests=error"',
        "def run_validation_pipeline(",
    )
    require_fragments(local_runner, common, "Local validation entry point")
    if progressive:
        require_fragments(
            local_runner,
            ("def build_ctest_command(",),
            "Local validation entry point",
        )
    else:
        require_fragments(
            local_runner,
            (
                "def should_run_stage(",
                "Pinned O3DE source policy, ",
                "compiled tests, and Windows acceptance remain mandatory exact-head ",
            ),
            "Legacy isolated local-runner fixture",
        )


def validate_common_read_only_workflow(automatic: str) -> None:
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
            "runs-on: windows-latest",
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


def validate_progressive_read_only_mode(repo_root: Path, automatic: str) -> None:
    require_fragments(
        automatic,
        (
            '".codex/**"',
            "Validate pull-request policy contract",
            "validate_pr_policy.py",
            "host_required",
            "windows_required",
            "needs: static-validation",
            "needs.static-validation.outputs.host_required == 'true'",
            "needs.static-validation.outputs.windows_required == 'true'",
        ),
        "Progressive read-only validation workflow",
    )
    reject_fragments(
        automatic,
        (
            "validate_pr_obligations.py",
            "merge-obligation:",
        ),
        "Progressive read-only validation workflow",
    )

    static_job_start = automatic.find("  static-validation:")
    compiled_job_start = automatic.find("  canonical-interchange-compiled:")
    windows_job_start = automatic.find("  windows-prerequisites:")
    static_job = automatic[static_job_start:compiled_job_start]
    compiled_job = automatic[compiled_job_start:windows_job_start]
    windows_job = automatic[windows_job_start:]

    require_fragments(
        static_job,
        (
            "outputs:",
            "host_required:",
            "windows_required:",
            "id: classify",
            "git diff --name-only",
            "validate_pr_policy.py",
        ),
        "Progressive static validation job",
    )
    require_fragments(
        compiled_job,
        (
            "needs: static-validation",
            "needs.static-validation.outputs.host_required == 'true'",
            "runs-on: windows-2022",
            "persist-credentials: false",
            "--parallel 2",
            "--no-tests=error",
        ),
        "Conditional compiled validation job",
    )
    require_fragments(
        windows_job,
        (
            "needs: static-validation",
            "needs.static-validation.outputs.windows_required == 'true'",
            "runs-on: windows-2022",
            "persist-credentials: false",
            "O3DE_COMMIT:",
            "sparse-checkout",
            "developer_preview.py prerequisites",
        ),
        "Conditional Windows prerequisite job",
    )
    reject_fragments(
        windows_job,
        ("cmake --build", "--ctest-build-dir"),
        "Conditional Windows prerequisite job",
    )

    policy = " ".join(read_text(repo_root / CI_POLICY).split())
    require_fragments(
        policy,
        (
            "single validation matrix",
            "Validation requirements are selected by changed surface and risk",
            "L0 — Repository and static validation",
            "L1 — Focused unit and contract tests",
            "L2 — Configure, build, and compiled host tests",
            "L3 — Editor/UI/manual host interaction",
            "L4 — Operational/runtime evidence",
            "Automated validation is read-only",
            "no `pull_request_target` trigger",
            "host/build jobs are conditional",
            "Routine changes do not require a receipt",
            "Pending is not passing",
            "runner registration token is a secret",
        ),
        "Progressive CI/local-validation policy",
    )
    read_text(repo_root / PR_POLICY_VALIDATOR)


def validate_legacy_fixture_mode(repo_root: Path, automatic: str) -> None:
    static_job_start = automatic.find("  static-validation:")
    compiled_job_start = automatic.find("  canonical-interchange-compiled:")
    windows_job_start = automatic.find("  windows-prerequisites:")
    static_job = automatic[static_job_start:compiled_job_start]
    compiled_job = automatic[compiled_job_start:windows_job_start]
    windows_job = automatic[windows_job_start:]

    require_fragments(
        static_job,
        (
            "fetch-depth: 0",
            "persist-credentials: false",
            "git diff --check",
            "run_local_validation.py --keep-going --static-only --skip-source-policy",
        ),
        "Legacy isolated static job fixture",
    )
    require_fragments(
        compiled_job,
        (
            "runs-on: windows-2022",
            "persist-credentials: false",
            "--parallel 2",
            "--no-tests=error",
        ),
        "Legacy isolated compiled job fixture",
    )
    require_fragments(
        windows_job,
        (
            "runs-on: windows-2022",
            "persist-credentials: false",
            "O3DE_COMMIT:",
            "sparse-checkout",
            "developer_preview.py prerequisites",
        ),
        "Legacy isolated Windows job fixture",
    )

    policy = " ".join(read_text(repo_root / CI_POLICY).split())
    require_fragments(
        policy,
        (
            "Binding automation boundary",
            "Agent-authored repository changes must happen on a non-main branch",
            "pull request for maintainer audit",
            "must not commit directly to main",
            "Automated validation is read-only",
            "no `pull_request_target` trigger",
            "pinned `windows-2022`",
            "must not push commits, move refs, post comments, merge pull requests",
            "--parallel 2",
            "--static-only",
            "--ctest-build-dir",
            "--no-tests=error",
            "Pending is not passing",
            "self-declared metadata are not proof",
            "repository owner authorized an action",
            "registration token is a secret",
        ),
        "Legacy isolated CI-policy fixture",
    )


def validate_read_only_mode(repo_root: Path, automatic: str) -> None:
    progressive = uses_progressive_process(repo_root)
    validate_common_read_only_workflow(automatic)
    if progressive:
        validate_progressive_read_only_mode(repo_root, automatic)
    else:
        validate_legacy_fixture_mode(repo_root, automatic)


def validate_ci_runner_policy(repo_root: Path) -> None:
    progressive = uses_progressive_process(repo_root)
    validate_agent_policy(repo_root, progressive=progressive)
    validate_removed_workflows(repo_root)
    automatic = read_text(repo_root / AUTOMATIC_STATIC_WORKFLOW)
    validate_read_only_mode(repo_root, automatic)
    validate_manual_workflows(repo_root, require_explicit_read_only=True)
    validate_local_runner(repo_root, progressive=progressive)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_ci_runner_policy(repo_root)
    except (OSError, UnicodeDecodeError, CiRunnerPolicyError) as error:
        print(f"CI runner policy validation failed: {error}", file=sys.stderr)
        return 1
    print(
        "CI runner policy validation passed: automation is read-only, static checks "
        "run for the reviewed range, and host jobs are selected by affected surface."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
