#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Run the repository-owned TG SDK validation gate without GitHub Actions."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_ROOT = Path(__file__).resolve().parent
TESTS_ROOT = TOOLS_ROOT / "tests"

VALIDATORS = (
    "validate_ci_runner_policy.py",
    "validate_installer_workflow.py",
    "validate_core_framework_build_graph.py",
    "validate_downstream_compiled_tests.py",
    "validate_research_contract_hardening.py",
    "validate_population_actor_troop_editor.py",
    "validate_population_contract_hardening.py",
    "validate_tainted_framework_knowledge.py",
    "validate_tainted_framework_editor_services.py",
    "validate_catalog_schema2.py",
    "validate_economy_coverage_dashboard.py",
    "validate_economy_duplicate_detection.py",
    "validate_adapter_contracts.py",
    "validate_external_tool_interchange_contracts.py",
    "validate_adapter_work_order_plans.py",
    "validate_adapter_runtime_results.py",
    "validate_adapter_build_manifests.py",
    "validate_adapter_package_assembly_preview.py",
    "validate_adapter_staging_deployment_preview.py",
    "validate_adapter_deployment_work_orders.py",
    "validate_adapter_deployment_execution_results.py",
    "validate_release_artifact_contract.py",
    "validate_release_assembly_results.py",
    "validate_release_signing_results.py",
    "validate_developer_preview.py",
    "validate_developer_preview_project.py",
    "validate_path_policy.py",
    "validate_workspace_atomicity_schema.py",
    "validate_developer_preview_fixture.py",
    "validate_developer_preview_smoke.py",
    "validate_developer_preview_launch_diagnostics.py",
    "validate_developer_preview_ui_evidence.py",
    "validate_foundation.py",
    "validate_governance_hardening.py",
    "validate_catalog_tests.py",
    "validate_validation_receipt_contract.py",
    "validate_tracked_path_collisions.py",
)


@dataclass(frozen=True)
class ValidationCommand:
    label: str
    argv: tuple[str, ...]


def python_command(*arguments: str) -> tuple[str, ...]:
    return (sys.executable, "-u", *arguments)


def build_static_commands(
    *,
    include_unit_tests: bool,
    include_source_policy: bool,
) -> list[ValidationCommand]:
    commands: list[ValidationCommand] = []
    if include_unit_tests:
        commands.append(
            ValidationCommand(
                "Python unit tests",
                python_command(
                    "-m",
                    "unittest",
                    "discover",
                    "-s",
                    str(TESTS_ROOT),
                    "-p",
                    "test_*.py",
                    "-v",
                ),
            )
        )
    for validator in VALIDATORS:
        commands.append(
            ValidationCommand(
                f"Validator: {validator}",
                python_command(str(TOOLS_ROOT / validator)),
            )
        )
    if include_source_policy:
        commands.append(
            ValidationCommand(
                "O3DE source policy",
                python_command(
                    str(
                        REPO_ROOT
                        / "scripts/commit_validation/validate_file_or_folder.py"
                    ),
                    "--path",
                    "Gems/TaintedGrailModdingSDK",
                ),
            )
        )
    return commands


def build_fixture_commands(temporary_root: Path) -> list[ValidationCommand]:
    fixture = temporary_root / "developer-preview-fixture"
    diagnostics = temporary_root / "developer-preview-diagnostics"
    tainted_framework = temporary_root / "tainted-framework-knowledge"
    return [
        ValidationCommand(
            "Generate Tainted Framework knowledge",
            python_command(
                str(TOOLS_ROOT / "tainted_framework_knowledge.py"),
                "generate",
                "--output",
                str(tainted_framework),
            ),
        ),
        ValidationCommand(
            "Verify Tainted Framework knowledge",
            python_command(
                str(TOOLS_ROOT / "tainted_framework_knowledge.py"),
                "verify",
                "--output",
                str(tainted_framework),
            ),
        ),
        ValidationCommand(
            "Generate Developer Preview fixture",
            python_command(
                str(TOOLS_ROOT / "developer_preview_fixture.py"),
                "generate",
                "--output",
                str(fixture),
            ),
        ),
        ValidationCommand(
            "Verify Developer Preview fixture",
            python_command(
                str(TOOLS_ROOT / "developer_preview_fixture.py"),
                "verify",
                "--output",
                str(fixture),
            ),
        ),
        ValidationCommand(
            "Collect redacted diagnostics fixture",
            python_command(
                str(TOOLS_ROOT / "developer_preview_diagnostics.py"),
                "collect",
                "--repo-root",
                str(REPO_ROOT),
                "--output",
                str(diagnostics),
            ),
        ),
        ValidationCommand(
            "Verify redacted diagnostics fixture",
            python_command(
                str(TOOLS_ROOT / "developer_preview_diagnostics.py"),
                "verify",
                "--output",
                str(diagnostics),
            ),
        ),
    ]


def find_ctest(build_directory: Path) -> str:
    discovered = shutil.which("ctest")
    if discovered:
        return discovered
    cache = build_directory / "CMakeCache.txt"
    if cache.is_file():
        prefix = "CMAKE_COMMAND:INTERNAL="
        for line in cache.read_text(
            encoding="utf-8",
            errors="strict",
        ).splitlines():
            if not line.startswith(prefix):
                continue
            cmake = Path(line[len(prefix) :])
            candidate = cmake.with_name(
                "ctest.exe" if os.name == "nt" else "ctest"
            )
            if candidate.is_file():
                return str(candidate)
            break
    raise RuntimeError(
        "Unable to locate ctest from PATH or the configured build's CMakeCache.txt."
    )


def build_ctest_command(build_directory: Path) -> ValidationCommand:
    return ValidationCommand(
        "Compiled TG SDK catalog tests",
        (
            find_ctest(build_directory),
            "--test-dir",
            str(build_directory),
            "-C",
            "profile",
            "-R",
            "TaintedGrailModdingSDK.Catalog.Tests",
            "--output-on-failure",
        ),
    )


def display_command(command: ValidationCommand) -> str:
    return " ".join(command.argv)


def run_commands(
    commands: Iterable[ValidationCommand],
    *,
    keep_going: bool,
) -> list[str]:
    failures: list[str] = []
    environment = os.environ.copy()
    environment.setdefault("PYTHONUTF8", "1")
    for command in commands:
        print(f"\n=== {command.label} ===", flush=True)
        print(display_command(command), flush=True)
        completed = subprocess.run(
            command.argv,
            cwd=REPO_ROOT,
            env=environment,
            check=False,
        )
        if completed.returncode == 0:
            continue
        failures.append(f"{command.label} (exit {completed.returncode})")
        if not keep_going:
            break
    return failures


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run TG SDK unit tests, contract validators, fixture checks, path "
            "hygiene, and source policy locally."
        )
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Print the static validation commands without running them.",
    )
    parser.add_argument(
        "--keep-going",
        action="store_true",
        help="Continue after failures and report all failed commands.",
    )
    parser.add_argument(
        "--skip-unit-tests",
        action="store_true",
        help="Skip Python unit-test discovery.",
    )
    parser.add_argument(
        "--skip-fixtures",
        action="store_true",
        help="Skip temporary fixture generation and diagnostics verification.",
    )
    parser.add_argument(
        "--skip-source-policy",
        action="store_true",
        help="Skip the O3DE source-policy validator.",
    )
    parser.add_argument(
        "--ctest-build-dir",
        type=Path,
        help=(
            "Also run compiled TaintedGrailModdingSDK.Catalog.Tests from this "
            "configured O3DE build directory."
        ),
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    static_commands = build_static_commands(
        include_unit_tests=not arguments.skip_unit_tests,
        include_source_policy=not arguments.skip_source_policy,
    )
    if arguments.list:
        for command in static_commands:
            print(f"{command.label}: {display_command(command)}")
        return 0
    print(
        "TG SDK local validation is authoritative while GitHub Actions remains "
        "manual-only. No automated per-commit test result is claimed.",
        flush=True,
    )
    failures = run_commands(
        static_commands,
        keep_going=arguments.keep_going,
    )
    if not failures and not arguments.skip_fixtures:
        with tempfile.TemporaryDirectory(
            prefix="tg-sdk-validation-"
        ) as temporary:
            failures.extend(
                run_commands(
                    build_fixture_commands(Path(temporary)),
                    keep_going=arguments.keep_going,
                )
            )
    if not failures and arguments.ctest_build_dir:
        failures.extend(
            run_commands(
                [build_ctest_command(arguments.ctest_build_dir.resolve())],
                keep_going=arguments.keep_going,
            )
        )
    if failures:
        print("\nTG SDK local validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("\nTG SDK local validation passed.")
    if not arguments.ctest_build_dir:
        print(
            "Compiled O3DE tests were not run; pass --ctest-build-dir to add them."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
