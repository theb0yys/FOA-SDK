#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate exact-head receipt tooling, tests, template, and merge policy wiring."""

from __future__ import annotations

import sys
from pathlib import Path


class ValidationReceiptContractError(RuntimeError):
    pass


def read(repo_root: Path, relative: str) -> str:
    path = repo_root / relative
    if not path.is_file():
        raise ValidationReceiptContractError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise ValidationReceiptContractError(
            f"{label} is missing required fragment {fragment!r}."
        )


def forbid(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise ValidationReceiptContractError(
            f"{label} retains prohibited fragment {fragment!r}."
        )


def require_all(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        require(text, fragment, label)


def function_slice(text: str, name: str, next_name: str) -> str:
    start = text.find(f"def {name}(")
    end = text.find(f"def {next_name}(", start + 1)
    if start < 0 or end < 0:
        raise ValidationReceiptContractError(
            f"Unable to isolate {name} for semantic receipt validation."
        )
    return text[start:end]


def validate(repo_root: Path) -> None:
    tool = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py",
    )
    require_all(
        tool,
        (
            'RECEIPT_NAME = "validation-receipt.json"',
            "SOURCE_COMMIT_RE",
            "MANDATORY_PASS_GATES",
            '"git-diff-check"',
            '"local-validation"',
            "RISK_ACCEPTABLE_GATES",
            '"o3de-configure"',
            '"o3de-build"',
            '"compiled-tests"',
            '"windows-ui"',
            "repository_state(",
            "subprocess.Popen(",
            "def stream_gate_process(",
            "MAXIMUM_GATE_STREAM_BYTES",
            "STREAM_CHUNK_BYTES",
            "hashlib.sha256()",
            "threading.Thread(",
            "os.O_EXCL",
            'if hasattr(os, "O_NOFOLLOW")',
            "reject_storage_indirection(",
            "nargs=argparse.REMAINDER",
            "def skip_gate(",
            '"created_at_utc": utc_now()',
            '"accepted_at_utc": utc_now()',
            'receipt["finalized_at_utc"] = utc_now()',
            "hash_file(",
            "validate_log_reference(",
            '"log_limit_exceeded"',
            '"truncated": self.truncated',
            "Receipt commit",
            "must pass; it cannot be waived",
            "explicit maintainer risk acceptance",
            "Finalized receipts are immutable",
            '"summarize", help="Print a Markdown summary for a pull request."',
        ),
        "Validation receipt tool",
    )
    forbid(tool, 'add_argument("--source-commit"', "Validation receipt tool")
    forbid(tool, 'add_argument("--status"', "Validation receipt tool")
    forbid(tool, 'add_argument("--exit-code"', "Validation receipt tool")
    forbid(tool, 'add_argument("--started-at"', "Validation receipt tool")
    forbid(tool, 'add_argument("--finished-at"', "Validation receipt tool")

    record_gate = function_slice(tool, "record_gate", "skip_gate")
    forbid(record_gate, "capture_output=True", "Validation receipt record gate")
    forbid(record_gate, "subprocess.run(", "Validation receipt record gate")
    require_all(
        record_gate,
        (
            "stream_gate_process(",
            "prepare_log_destination(",
            '"log_limit_exceeded": completed.limit_exceeded',
            "completed.stdout.receipt_value(output)",
            "completed.stderr.receipt_value(output)",
        ),
        "Validation receipt record gate",
    )

    tests = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/tests/test_validation_receipt.py",
    )
    require_all(
        tests,
        (
            "test_merge_ready_receipt_verifies_and_summarizes",
            "test_tampered_log_is_rejected",
            "test_wrong_expected_commit_is_rejected",
            "test_mandatory_local_validation_cannot_be_waived",
            "test_skipped_host_gate_requires_maintainer_acceptance",
            "test_finalized_receipt_is_immutable",
            "test_record_executes_command_and_derives_failure",
            "test_init_rejects_dirty_repository",
            "test_mandatory_compiled_gate_cannot_be_waived",
            "test_output_directory_symlink_is_rejected",
        ),
        "Validation receipt unit tests",
    )

    streaming_tests = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/tests/test_validation_receipt_streaming.py",
    )
    require_all(
        streaming_tests,
        (
            "test_process_streams_distinct_stdout_and_stderr_with_incremental_hashes",
            "test_stream_limit_terminates_gate_and_keeps_only_hashed_prefix",
            "test_existing_log_destination_is_never_overwritten",
            "stream_gate_process(",
            "maximum_stream_bytes=64",
            "result.limit_exceeded",
            "result.stdout.truncated",
            "hashlib.sha256",
        ),
        "Validation receipt streaming tests",
    )

    local_gate = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py",
    )
    require_all(
        local_gate,
        (
            '"validate_validation_receipt_contract.py"',
            "class ValidationConfigurationError",
            "def run_validation_pipeline(",
            "def should_run_stage(",
            "def validation_mode(",
            '"--static-only"',
            '"--ctest-build-dir"',
            '"--no-tests=error"',
            "Pinned O3DE source policy, ",
            "compiled tests, and Windows acceptance remain mandatory exact-head ",
        ),
        "Authoritative local validation gate",
    )

    preview_command = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/developer_preview.py",
    )
    require_all(
        preview_command,
        (
            '"compiled-catalog-tests"',
            '"--no-tests=error"',
            "CATALOG_TEST_PATTERN",
        ),
        "Developer Preview compiled validation path",
    )

    preview_command_tests = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/tests/test_developer_preview.py",
    )
    require_all(
        preview_command_tests,
        (
            'compiled = next(step for step in plan if step.name == "compiled-catalog-tests")',
            'self.assertIn("--no-tests=error", compiled.command)',
            "self.assertIn(preview.CATALOG_TEST_PATTERN, compiled.command)",
        ),
        "Developer Preview compiled validation tests",
    )

    coordinator = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/developer_preview_verification.py",
    )
    require_all(
        coordinator,
        (
            "import developer_preview",
            "developer_preview.validate_product_root(repo)",
            "developer_preview.default_build_directory(repo)",
            '"--product-root"',
            '"--engine-root"',
            '"local-validation": tool(',
            '"--static-only"',
            '"compiled-tests": (',
            '"--no-tests=error"',
            "def status_exit_code(",
            "return status_exit_code(report)",
        ),
        "Exact-head verification coordinator",
    )
    forbid(
        coordinator,
        'repo / "engine.json"',
        "Exact-head verification coordinator",
    )

    coordinator_tests = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/tests/test_developer_preview_verification.py",
    )
    require_all(
        coordinator_tests,
        (
            "test_external_engine_build_and_evidence_roots_are_separate",
            "test_local_and_compiled_gates_cannot_silently_skip_essential_layers",
            "test_incomplete_status_is_a_gating_failure",
            'self.assertIn("--static-only", local)',
            'self.assertIn("--no-tests=error", compiled)',
            "status_exit_code",
        ),
        "Exact-head verification coordinator tests",
    )

    runner_tests = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_ci_runner_policy.py",
    )
    require_all(
        runner_tests,
        (
            "test_full_validation_requires_ctest_or_explicit_static_only",
            "test_compiled_ctest_fails_when_regex_matches_no_tests",
            "test_keep_going_runs_static_fixtures_and_compiled_stages",
            "test_non_keep_going_stops_after_first_failed_stage",
            "test_automatic_static_workflow_must_use_explicit_static_only_mode",
        ),
        "Validation pipeline regression tests",
    )

    automatic_workflow = read(
        repo_root,
        ".github/workflows/tainted-grail-sdk-pr-validation.yml",
    )
    require_all(
        automatic_workflow,
        (
            "pull_request:",
            "push:",
            "workflow_dispatch:",
            "runs-on: ubuntu-latest",
            "run_local_validation.py --keep-going --static-only --skip-source-policy",
        ),
        "Automatic static validation workflow",
    )
    forbid(
        automatic_workflow,
        "self-hosted",
        "Automatic static validation workflow",
    )

    template = read(repo_root, ".github/PULL_REQUEST_TEMPLATE.md")
    require_all(
        template,
        (
            "## Exact-head validation receipt",
            "automatic PR static-validation workflow",
            "compiled `TaintedGrailModdingSDK.Catalog.Tests`",
            "validation_receipt.py verify",
            "--require-merge-ready",
            "validation_receipt.py summarize",
            "Do not commit the receipt or logs",
            "tester-supplied evidence",
        ),
        "Pull request template",
    )

    ci_policy = read(
        repo_root,
        "docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md",
    )
    require_all(
        ci_policy,
        (
            "automatic pull-request static validation",
            "--static-only",
            "--ctest-build-dir",
            "compiled Catalog CTest",
            "does not claim an O3DE build",
            "Pending is not passing",
            "## Exact-head validation receipt",
            "validation_receipt.py init",
            "validation_receipt.py record",
            "validation_receipt.py accept-risk",
            "validation_receipt.py finalize",
            "validation_receipt.py verify",
            "validation_receipt.py summarize",
            "derives the 40-character",
            "runs that command",
            "must all have an executed, zero-exit result",
            "waived. A Windows UI pass",
            "not a signature",
            "must not be committed",
        ),
        "CI and local validation policy",
    )

    review_policy = read(
        repo_root,
        "docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md",
    )
    require_all(
        review_policy,
        (
            "automatic PR static-validation check",
            "merge-ready exact-head validation receipt",
            "validation_receipt.py verify",
            "--require-merge-ready",
            "its receipt source",
            "commit matches the reviewed head",
            "compiled-test gates remain mandatory",
            "FOA-plug-in-development",
        ),
        "Review and merge policy",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, ValidationReceiptContractError) as exc:
        print(f"Validation-receipt contract failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Validation-receipt contract passed: automatic static checks, mandatory "
        "compiled gates, bounded logs, and exact-head status all fail closed."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
