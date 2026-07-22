#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Run the FOA-SDK validation pipeline against a separate pinned O3DE checkout."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

TOOLS_ROOT = Path(__file__).resolve().parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import developer_preview

PRODUCT_ROOT = Path(__file__).resolve().parents[3]
TESTS_ROOT = TOOLS_ROOT / "tests"
PRODUCT_GEM_PATHS = (
    ("ExternalToolchain", PRODUCT_ROOT / "Gems/ExternalToolchain"),
    ("AvalonAIAuthoring", PRODUCT_ROOT / "Plugins/Authoring/AvalonAI/Gem"),
    ("RoadAtlasAuthoring", PRODUCT_ROOT / "Plugins/Authoring/RoadAtlas/Gem"),
    ("TaintedGrailModdingSDK", PRODUCT_ROOT / "Gems/TaintedGrailModdingSDK"),
)

VALIDATORS = (
    "validate_repository_structure.py",
    "validate_plugin_packages.py",
    "validate_editor_lifecycle.py",
    "validate_ci_runner_policy.py",
    "validate_installer_workflow.py",
    "validate_core_framework_build_graph.py",
    "validate_downstream_compiled_tests.py",
    "validate_research_contract_hardening.py",
    "validate_tainted_system_ports.py",
    "validate_population_actor_troop_editor.py",
    "validate_population_contract_hardening.py",
    "validate_tainted_framework_knowledge.py",
    "validate_tainted_framework_editor_services.py",
    "validate_tainted_interface_ui_utilities.py",
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


class ValidationConfigurationError(RuntimeError):
    """Raised when a caller attempts to claim an incomplete validation mode."""


@dataclass(frozen=True)
class ValidationCommand:
    label: str
    argv: tuple[str, ...]
    cwd: Path = PRODUCT_ROOT


def python_command(*arguments: str) -> tuple[str, ...]:
    return (sys.executable, "-u", *arguments)


def resolve_engine_root(value: Path | None) -> Path:
    developer_preview.validate_product_root(PRODUCT_ROOT)
    lock = developer_preview.load_engine_lock(PRODUCT_ROOT)
    engine_root = (
        developer_preview.resolve_path(value, Path.cwd())
        if value is not None
        else developer_preview.default_engine_root(PRODUCT_ROOT, lock)
    )
    developer_preview.validate_engine_root(engine_root, lock)
    developer_preview.validate_engine_pin(engine_root, lock)
    if engine_root == PRODUCT_ROOT:
        raise RuntimeError("FOA-SDK and O3DE must be separate checkouts.")
    return engine_root


def build_static_commands(
    *,
    include_unit_tests: bool,
    source_policy_engine_root: Path | None,
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
    if source_policy_engine_root is not None:
        validator = (
            source_policy_engine_root
            / "scripts/commit_validation/validate_file_or_folder.py"
        )
        for gem_name, gem_path in PRODUCT_GEM_PATHS:
            commands.append(
                ValidationCommand(
                    f"O3DE source policy: {gem_name}",
                    python_command(str(validator), "--path", str(gem_path)),
                    cwd=source_policy_engine_root,
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
                str(PRODUCT_ROOT),
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


def validate_ctest_build_directory(build_directory: Path) -> Path:
    build_directory = build_directory.expanduser().resolve(strict=False)
    if not build_directory.is_dir():
        raise ValidationConfigurationError(
            f"Compiled validation requires an existing build directory: {build_directory}"
        )
    if not (build_directory / "CMakeCache.txt").is_file():
        raise ValidationConfigurationError(
            "Compiled validation requires a configured O3DE build containing CMakeCache.txt."
        )
    if not (build_directory / "CTestTestfile.cmake").is_file():
        raise ValidationConfigurationError(
            "Compiled validation requires CTestTestfile.cmake at the configured build root."
        )
    return build_directory


def build_ctest_command(build_directory: Path) -> ValidationCommand:
    build_directory = validate_ctest_build_directory(build_directory)
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
            "--no-tests=error",
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
        try:
            completed = subprocess.run(
                command.argv,
                cwd=command.cwd,
                env=environment,
                check=False,
            )
            return_code = int(completed.returncode)
            failure = f"{command.label} (exit {return_code})"
        except OSError as exc:
            return_code = 127
            failure = f"{command.label} (unable to execute: {exc})"
        if return_code == 0:
            continue
        failures.append(failure)
        if not keep_going:
            break
    return failures


def should_run_stage(failures: Sequence[str], *, keep_going: bool) -> bool:
    return keep_going or not failures


def validation_mode(arguments: argparse.Namespace) -> str:
    if arguments.static_only:
        return "static-only"
    if arguments.ctest_build_dir is None:
        raise ValidationConfigurationError(
            "A successful validation claim must include compiled CTest via "
            "--ctest-build-dir. Use --static-only only for the explicitly "
            "non-compiled GitHub-hosted validation layer."
        )
    if arguments.skip_source_policy:
        raise ValidationConfigurationError(
            "Full validation cannot skip the pinned O3DE source-policy checks."
        )
    return "full"


def run_validation_pipeline(
    arguments: argparse.Namespace,
    source_policy_engine_root: Path | None,
) -> list[str]:
    mode = validation_mode(arguments)
    failures = run_commands(
        build_static_commands(
            include_unit_tests=not arguments.skip_unit_tests,
            source_policy_engine_root=source_policy_engine_root,
        ),
        keep_going=arguments.keep_going,
    )

    if (
        not arguments.skip_fixtures
        and should_run_stage(failures, keep_going=arguments.keep_going)
    ):
        with tempfile.TemporaryDirectory(
            prefix="foa-sdk-validation-"
        ) as temporary:
            failures.extend(
                run_commands(
                    build_fixture_commands(Path(temporary)),
                    keep_going=arguments.keep_going,
                )
            )

    if (
        mode == "full"
        and should_run_stage(failures, keep_going=arguments.keep_going)
    ):
        failures.extend(
            run_commands(
                [build_ctest_command(arguments.ctest_build_dir)],
                keep_going=arguments.keep_going,
            )
        )
    return failures


def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run FOA-SDK Python tests, validators, fixtures, pinned O3DE source "
            "policy, and mandatory compiled Catalog CTest coverage."
        )
    )
    parser.add_argument("--list", action="store_true")
    parser.add_argument(
        "--keep-going",
        action="store_true",
        help=(
            "Continue through static checks, fixtures, and compiled CTest after "
            "failures so one run reports the complete failure inventory."
        ),
    )
    parser.add_argument("--skip-unit-tests", action="store_true")
    parser.add_argument("--skip-fixtures", action="store_true")
    parser.add_argument("--skip-source-policy", action="store_true")
    parser.add_argument(
        "--engine-root",
        type=Path,
        help="Pinned external O3DE checkout. Defaults to FOA_O3DE_ROOT or sibling o3de/.",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--ctest-build-dir",
        type=Path,
        help=(
            "Run the mandatory compiled TaintedGrailModdingSDK.Catalog.Tests from "
            "this configured O3DE build root."
        ),
    )
    mode.add_argument(
        "--static-only",
        action="store_true",
        help=(
            "Run the non-compiled GitHub-hosted layer only. This mode cannot be "
            "reported as full or exact-head validation."
        ),
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    if arguments.list:
        engine_root = None
        if not arguments.skip_source_policy:
            try:
                engine_root = resolve_engine_root(arguments.engine_root)
            except (OSError, RuntimeError) as exc:
                print(f"Pinned O3DE source policy: unavailable ({exc})")
        for command in build_static_commands(
            include_unit_tests=not arguments.skip_unit_tests,
            source_policy_engine_root=engine_root,
        ):
            print(f"{command.label}: {display_command(command)}")
        if arguments.ctest_build_dir:
            try:
                command = build_ctest_command(arguments.ctest_build_dir)
            except (OSError, RuntimeError, ValidationConfigurationError) as exc:
                print(f"Compiled CTest: unavailable ({exc})")
            else:
                print(f"{command.label}: {display_command(command)}")
        else:
            print(
                "Compiled CTest: required for full validation; pass --ctest-build-dir "
                "or explicitly select --static-only."
            )
        return 0

    try:
        mode = validation_mode(arguments)
        source_policy_engine_root = (
            None
            if arguments.skip_source_policy
            else resolve_engine_root(arguments.engine_root)
        )
        if mode == "static-only":
            suffix = (
                "Pinned O3DE source policy is intentionally not included."
                if source_policy_engine_root is None
                else "Pinned O3DE source policy is included."
            )
            print(
                "FOA-SDK static validation: Python tests, validators, and fixtures. "
                f"{suffix} No compiled or Windows result is claimed.",
                flush=True,
            )
        else:
            print(
                "FOA-SDK full validation: static checks, fixtures, pinned O3DE "
                "source policy, and mandatory compiled Catalog CTest coverage.",
                flush=True,
            )
        failures = run_validation_pipeline(arguments, source_policy_engine_root)
    except (OSError, RuntimeError, ValidationConfigurationError) as exc:
        print(f"FOA-SDK validation configuration failed: {exc}", file=sys.stderr)
        return 2

    if failures:
        print("\nFOA-SDK validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    if mode == "static-only":
        print(
            "\nFOA-SDK static validation passed. Pinned O3DE source policy, "
            "compiled tests, and Windows acceptance remain mandatory exact-head "
            "gates unless explicitly included above."
        )
    else:
        print("\nFOA-SDK full validation passed, including compiled Catalog CTest.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
