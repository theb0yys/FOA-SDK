#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Developer Preview 0 prerequisite, configure, build, and validation commands.

FOA-SDK is the product checkout. O3DE is a separately resolved, exact-commit
engine dependency. This command does not launch FoA, deploy files, modify saves,
or invoke BepInEx, Harmony, Avalon Core runtime, or game APIs.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Callable, Sequence

PREVIEW_SCHEMA_VERSION = 2
ENGINE_LOCK_SCHEMA_VERSION = 1
PREVIEW_NAME = "Developer Preview 0"
PRIMARY_HOST = "Windows x64 Profile"
DEFAULT_CONFIGURE_PRESET = "windows-vs-unity"
DEFAULT_CONFIGURATION = "profile"
PREVIEW_PROJECT_DIRECTORY = "TaintedGrailModdingEditor"
ENGINE_LOCK_FILENAME = "o3de.lock.json"
ENGINE_ROOT_ENVIRONMENT_VARIABLE = "FOA_O3DE_ROOT"
BUILD_ROOT_ENVIRONMENT_VARIABLE = "FOA_BUILD_ROOT"
EDITOR_TARGET = "Editor"
ASSET_PROCESSOR_BATCH_TARGET = "AssetProcessorBatch"
CATALOG_TEST_TARGET = "TaintedGrailModdingSDK.Catalog.Tests"
CATALOG_TEST_PATTERN = r"TaintedGrailModdingSDK\.Catalog\.Tests"
MINIMUM_PYTHON = (3, 10, 0)
MINIMUM_CMAKE = (3, 23, 0)


@dataclass(frozen=True)
class EngineLock:
    repository: str
    commit: str
    engine_name: str
    engine_version: str
    checkout_directory: str


@dataclass(frozen=True)
class CheckResult:
    name: str
    status: str
    required: bool
    detail: str
    remediation: str = ""


@dataclass(frozen=True)
class CommandStep:
    name: str
    command: tuple[str, ...]
    cwd: str


@dataclass(frozen=True)
class StepResult:
    name: str
    command: tuple[str, ...]
    cwd: str
    status: str
    exit_code: int | None
    duration_seconds: float


ProcessExecutor = Callable[[Sequence[str], Path], int]


def product_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_path(value: Path, base: Path) -> Path:
    path = value.expanduser()
    if not path.is_absolute():
        path = base / path
    return path.resolve(strict=False)


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def validate_product_root(product_root: Path) -> None:
    required = (
        product_root / ENGINE_LOCK_FILENAME,
        product_root / PREVIEW_PROJECT_DIRECTORY / "project.json",
        product_root / "Gems" / "TaintedGrailModdingSDK" / "gem.json",
        product_root / "Gems" / "ExternalToolchain" / "gem.json",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError(
            "The FOA-SDK product root is invalid; missing required files: " + ", ".join(missing)
        )


def load_engine_lock(product_root: Path) -> EngineLock:
    lock_path = product_root / ENGINE_LOCK_FILENAME
    try:
        payload = json.loads(lock_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"Unable to read {lock_path}: {exc}") from exc

    required_fields = (
        "schema_version",
        "repository",
        "commit",
        "engine_name",
        "engine_version",
        "checkout_directory",
    )
    missing = [field for field in required_fields if field not in payload]
    if missing:
        raise RuntimeError(f"{lock_path} is missing required fields: {', '.join(missing)}")
    if payload["schema_version"] != ENGINE_LOCK_SCHEMA_VERSION:
        raise RuntimeError(
            f"Unsupported O3DE lock schema {payload['schema_version']}; "
            f"expected {ENGINE_LOCK_SCHEMA_VERSION}."
        )

    string_fields = required_fields[1:]
    invalid = [
        field
        for field in string_fields
        if not isinstance(payload[field], str) or not payload[field].strip()
    ]
    if invalid:
        raise RuntimeError(f"{lock_path} has invalid string fields: {', '.join(invalid)}")

    commit = payload["commit"].strip().lower()
    if re.fullmatch(r"[0-9a-f]{40}", commit) is None:
        raise RuntimeError(f"{lock_path} commit must be a full 40-character Git SHA.")
    checkout_directory = payload["checkout_directory"].strip()
    if Path(checkout_directory).name != checkout_directory or checkout_directory in {".", ".."}:
        raise RuntimeError(f"{lock_path} checkout_directory must be one directory name.")

    return EngineLock(
        repository=payload["repository"].strip(),
        commit=commit,
        engine_name=payload["engine_name"].strip(),
        engine_version=payload["engine_version"].strip(),
        checkout_directory=checkout_directory,
    )


def default_engine_root(product_root: Path, lock: EngineLock) -> Path:
    configured = os.environ.get(ENGINE_ROOT_ENVIRONMENT_VARIABLE)
    if configured:
        return resolve_path(Path(configured), Path.cwd())

    sibling = product_root.parent / lock.checkout_directory
    if sibling.is_dir():
        return sibling.resolve(strict=False)

    # Transitional compatibility while the product still lives in the inherited
    # engine tree. This fallback is removed by the history-extraction unit.
    if (product_root / "engine.json").is_file() and (product_root / "CMakePresets.json").is_file():
        return product_root.resolve(strict=False)
    return sibling.resolve(strict=False)


def default_build_directory(product_root: Path) -> Path:
    configured = os.environ.get(BUILD_ROOT_ENVIRONMENT_VARIABLE)
    if configured:
        build_root = resolve_path(Path(configured), Path.cwd())
    else:
        build_root = product_root.parent / "foa-build"
    return build_root / "tg-sdk-developer-preview-0-windows-profile"


def read_engine_descriptor(engine_root: Path) -> dict:
    descriptor_path = engine_root / "engine.json"
    try:
        payload = json.loads(descriptor_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"Unable to read O3DE engine descriptor {descriptor_path}: {exc}") from exc
    if not isinstance(payload, dict):
        raise RuntimeError(f"O3DE engine descriptor must be a JSON object: {descriptor_path}")
    return payload


def validate_engine_root(engine_root: Path, lock: EngineLock) -> None:
    required = (
        engine_root / "engine.json",
        engine_root / "CMakePresets.json",
        engine_root / "scripts" / "commit_validation" / "validate_file_or_folder.py",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError(
            "The O3DE engine root is invalid; missing required files: " + ", ".join(missing)
        )

    descriptor = read_engine_descriptor(engine_root)
    if descriptor.get("engine_name") != lock.engine_name:
        raise RuntimeError(
            f"O3DE engine name mismatch: expected {lock.engine_name!r}, "
            f"found {descriptor.get('engine_name')!r}."
        )
    if descriptor.get("version") != lock.engine_version:
        raise RuntimeError(
            f"O3DE engine version mismatch: expected {lock.engine_version!r}, "
            f"found {descriptor.get('version')!r}."
        )


def capture_command(command: Sequence[str], cwd: Path) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            list(command),
            cwd=str(cwd),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        return 127, str(exc)
    return int(completed.returncode), completed.stdout.strip()


def engine_checkout_commit(engine_root: Path) -> tuple[int, str]:
    return capture_command(("git", "-C", str(engine_root), "rev-parse", "HEAD"), engine_root)


def validate_engine_pin(engine_root: Path, lock: EngineLock) -> None:
    code, output = engine_checkout_commit(engine_root)
    if code != 0:
        raise RuntimeError(f"Unable to resolve the O3DE checkout commit: {output or code}")
    actual = output.splitlines()[-1].strip().lower()
    if actual != lock.commit:
        raise RuntimeError(
            f"O3DE checkout commit mismatch: expected {lock.commit}, found {actual}. "
            "Checkout the exact commit recorded in o3de.lock.json."
        )


def validate_build_directory(
    product_root: Path,
    engine_root: Path,
    build_dir: Path,
    *,
    require_configured: bool,
) -> None:
    if build_dir in {product_root, engine_root}:
        raise RuntimeError("The build directory must be separate from product_root and engine_root.")
    if is_relative_to(product_root, build_dir):
        raise RuntimeError("The build directory must not contain the FOA-SDK checkout.")
    if is_relative_to(build_dir, product_root):
        raise RuntimeError("The build directory must not be inside the FOA-SDK checkout.")
    if is_relative_to(engine_root, build_dir):
        raise RuntimeError("The build directory must not contain the O3DE checkout.")
    if is_relative_to(build_dir, engine_root):
        raise RuntimeError("The build directory must not be inside the O3DE checkout.")
    if is_relative_to(build_dir, product_root / ".git") or is_relative_to(
        build_dir, engine_root / ".git"
    ):
        raise RuntimeError("The build directory must not be inside a Git metadata directory.")

    cache_path = build_dir / "CMakeCache.txt"
    if require_configured and not cache_path.is_file():
        raise RuntimeError(
            f"The build directory is not configured: {build_dir}. Run the configure command first."
        )

    if cache_path.is_file():
        cache = cache_path.read_text(encoding="utf-8", errors="replace")
        match = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache, re.MULTILINE)
        if match:
            configured_source = Path(match.group(1).strip()).resolve(strict=False)
            if configured_source != engine_root:
                raise RuntimeError(
                    f"The build directory belongs to another engine source tree: {configured_source}"
                )
        if require_configured:
            projects_match = re.search(r"^LY_PROJECTS:STRING=(.*)$", cache, re.MULTILINE)
            configured_projects = {
                resolve_path(Path(value.strip()), engine_root)
                for value in (projects_match.group(1).split(";") if projects_match else ())
                if value.strip()
            }
            required_project = (product_root / PREVIEW_PROJECT_DIRECTORY).resolve(strict=False)
            if required_project not in configured_projects:
                raise RuntimeError(
                    "The build directory is not configured for the dedicated "
                    f"{PREVIEW_PROJECT_DIRECTORY} project. Run the configure command first."
                )
    elif build_dir.exists() and any(build_dir.iterdir()):
        raise RuntimeError(
            f"The build directory is non-empty but has no CMakeCache.txt: {build_dir}"
        )


def parse_version(text: str) -> tuple[int, int, int] | None:
    match = re.search(r"(?<!\d)(\d+)\.(\d+)(?:\.(\d+))?", text)
    if not match:
        return None
    return (int(match.group(1)), int(match.group(2)), int(match.group(3) or 0))


def version_text(version: tuple[int, int, int]) -> str:
    return ".".join(str(part) for part in version)


def command_text(command: Sequence[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(list(command))
    return shlex.join(command)


def default_executor(command: Sequence[str], cwd: Path) -> int:
    completed = subprocess.run(list(command), cwd=str(cwd), check=False)
    return int(completed.returncode)


def locate_vswhere() -> Path | None:
    from_path = shutil.which("vswhere")
    if from_path:
        return Path(from_path)
    program_files_x86 = os.environ.get("ProgramFiles(x86)")
    if program_files_x86:
        candidate = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if candidate.is_file():
            return candidate
    return None


def visual_studio_detail(product_root: Path) -> tuple[bool, str]:
    cl_path = shutil.which("cl")
    if cl_path:
        return True, f"MSVC compiler found on PATH: {cl_path}"
    vswhere = locate_vswhere()
    if not vswhere:
        return False, "Visual Studio Build Tools were not found on PATH and vswhere.exe was not found."
    code, output = capture_command(
        (
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ),
        product_root,
    )
    if code == 0 and output:
        return True, f"Visual Studio C++ tools found: {output.splitlines()[-1]}"
    return False, "Visual Studio 2022 with Desktop development with C++ was not found."


def executable_version_check(
    *,
    name: str,
    executable: str,
    arguments: tuple[str, ...],
    minimum: tuple[int, int, int] | None,
    product_root: Path,
    remediation: str,
) -> CheckResult:
    path = shutil.which(executable)
    if not path:
        return CheckResult(name, "failed", True, f"{executable} was not found on PATH.", remediation)
    code, output = capture_command((path, *arguments), product_root)
    if code != 0:
        return CheckResult(name, "failed", True, output or f"{executable} returned {code}.", remediation)
    if minimum is None:
        return CheckResult(name, "passed", True, output.splitlines()[0] if output else path)
    detected = parse_version(output)
    if detected is None:
        return CheckResult(name, "failed", True, f"Unable to parse version from: {output}", remediation)
    if detected < minimum:
        return CheckResult(
            name,
            "failed",
            True,
            f"Found {version_text(detected)}; requires {version_text(minimum)} or newer.",
            remediation,
        )
    return CheckResult(name, "passed", True, f"{version_text(detected)} at {path}")


def editor_candidates(build_dir: Path) -> tuple[Path, ...]:
    return (
        build_dir / "bin" / DEFAULT_CONFIGURATION / "Editor.exe",
        build_dir / "bin" / DEFAULT_CONFIGURATION / "Editor",
    )


def collect_prerequisite_checks(
    product_root: Path,
    engine_root: Path,
    build_dir: Path,
    lock: EngineLock,
) -> list[CheckResult]:
    checks: list[CheckResult] = []
    system = platform.system()
    machine = platform.machine().lower()
    windows_ok = system == "Windows"
    x64_ok = machine in {"amd64", "x86_64"}
    checks.append(
        CheckResult(
            "host",
            "passed" if windows_ok and x64_ok else "failed",
            True,
            f"Detected {system} {platform.machine()}; primary host is {PRIMARY_HOST}.",
            "Use a Windows x64 development machine for Developer Preview 0.",
        )
    )

    python_version = (sys.version_info.major, sys.version_info.minor, sys.version_info.micro)
    checks.append(
        CheckResult(
            "python",
            "passed" if python_version >= MINIMUM_PYTHON else "failed",
            True,
            f"Python {version_text(python_version)} at {sys.executable}",
            f"Install Python {version_text(MINIMUM_PYTHON)} or newer and place it on PATH.",
        )
    )

    try:
        validate_product_root(product_root)
        checks.append(CheckResult("product-root", "passed", True, f"FOA-SDK: {product_root}"))
    except RuntimeError as exc:
        checks.append(
            CheckResult(
                "product-root",
                "failed",
                True,
                str(exc),
                "Run from the FOA-SDK checkout or pass --product-root explicitly.",
            )
        )
        return checks

    try:
        validate_engine_root(engine_root, lock)
        checks.append(CheckResult("engine-root", "passed", True, f"O3DE: {engine_root}"))
    except RuntimeError as exc:
        checks.append(
            CheckResult(
                "engine-root",
                "failed",
                True,
                str(exc),
                f"Clone {lock.repository} beside FOA-SDK or pass --engine-root.",
            )
        )

    checks.append(
        executable_version_check(
            name="cmake",
            executable="cmake",
            arguments=("--version",),
            minimum=MINIMUM_CMAKE,
            product_root=product_root,
            remediation=f"Install CMake {version_text(MINIMUM_CMAKE)} or newer and place it on PATH.",
        )
    )
    git_check = executable_version_check(
        name="git",
        executable="git",
        arguments=("--version",),
        minimum=None,
        product_root=product_root,
        remediation="Install Git and place it on PATH.",
    )
    checks.append(git_check)
    checks.append(
        executable_version_check(
            name="git-lfs",
            executable="git",
            arguments=("lfs", "version"),
            minimum=None,
            product_root=product_root,
            remediation="Install Git LFS, then run 'git lfs install'.",
        )
    )

    if git_check.status == "passed":
        try:
            validate_engine_pin(engine_root, lock)
            checks.append(
                CheckResult(
                    "o3de-commit",
                    "passed",
                    True,
                    f"Pinned O3DE commit: {lock.commit}",
                )
            )
        except RuntimeError as exc:
            checks.append(
                CheckResult(
                    "o3de-commit",
                    "failed",
                    True,
                    str(exc),
                    f"Run 'git -C {engine_root} checkout {lock.commit}'.",
                )
            )

    vs_ok, vs_detail = visual_studio_detail(product_root)
    checks.append(
        CheckResult(
            "visual-studio-cpp",
            "passed" if vs_ok else "failed",
            True,
            vs_detail,
            "Install Visual Studio 2022 or Build Tools with Desktop development with C++.",
        )
    )

    third_party = os.environ.get("LY_3RDPARTY_PATH")
    if third_party:
        third_party_path = Path(third_party).expanduser().resolve(strict=False)
        checks.append(
            CheckResult(
                "o3de-third-party-path",
                "passed" if third_party_path.is_dir() else "failed",
                True,
                f"LY_3RDPARTY_PATH={third_party_path}",
                "Correct or unset LY_3RDPARTY_PATH before configuring O3DE.",
            )
        )
    else:
        checks.append(
            CheckResult(
                "o3de-third-party-path",
                "informational",
                False,
                "LY_3RDPARTY_PATH is not set; O3DE configure will resolve packages using engine defaults.",
            )
        )

    cache_path = build_dir / "CMakeCache.txt"
    if cache_path.is_file():
        try:
            validate_build_directory(
                product_root, engine_root, build_dir, require_configured=True
            )
            build_status = CheckResult("build-directory", "passed", False, f"Configured: {build_dir}")
        except RuntimeError as exc:
            build_status = CheckResult(
                "build-directory",
                "failed",
                False,
                str(exc),
                "Use a new build directory or remove the stale CMake configuration deliberately.",
            )
    elif build_dir.exists() and any(build_dir.iterdir()):
        build_status = CheckResult(
            "build-directory",
            "failed",
            False,
            f"Non-empty directory without CMakeCache.txt: {build_dir}",
            "Choose an empty directory for the preview build.",
        )
    else:
        build_status = CheckResult(
            "build-directory",
            "not-configured",
            False,
            f"Not configured yet: {build_dir}",
            "Run the configure command after required prerequisites pass.",
        )
    checks.append(build_status)

    editor = next((path for path in editor_candidates(build_dir) if path.is_file()), None)
    checks.append(
        CheckResult(
            "editor-executable",
            "passed" if editor else "not-built",
            False,
            f"Editor executable: {editor}" if editor else f"Editor has not been built under {build_dir}.",
            "Run the build command after configuration.",
        )
    )
    return checks


def required_checks_pass(checks: Sequence[CheckResult]) -> bool:
    return all(check.status == "passed" for check in checks if check.required)


def configure_command(
    product_root: Path,
    engine_root: Path,
    build_dir: Path,
    cmake: str = "cmake",
) -> tuple[str, ...]:
    return (
        cmake,
        "--preset",
        DEFAULT_CONFIGURE_PRESET,
        "-S",
        str(engine_root),
        "-B",
        str(build_dir),
        "-A",
        "x64",
        f"-DLY_PROJECTS={product_root / PREVIEW_PROJECT_DIRECTORY}",
    )


def build_command(build_dir: Path, cmake: str = "cmake") -> tuple[str, ...]:
    return (
        cmake,
        "--build",
        str(build_dir),
        "--config",
        DEFAULT_CONFIGURATION,
        "--target",
        EDITOR_TARGET,
        ASSET_PROCESSOR_BATCH_TARGET,
        CATALOG_TEST_TARGET,
    )


def validation_plan(
    product_root: Path,
    engine_root: Path,
    build_dir: Path,
    ctest: str = "ctest",
) -> list[CommandStep]:
    tools = product_root / "Gems" / "TaintedGrailModdingSDK" / "Tools"
    source_policy = engine_root / "scripts" / "commit_validation" / "validate_file_or_folder.py"
    return [
        CommandStep(
            "developer-preview-command-tests",
            (
                sys.executable,
                "-m",
                "unittest",
                "discover",
                "-s",
                str(tools / "tests"),
                "-p",
                "test_developer_preview.py",
            ),
            str(product_root),
        ),
        CommandStep(
            "developer-preview-contract",
            (sys.executable, str(tools / "validate_developer_preview.py")),
            str(product_root),
        ),
        CommandStep(
            "foundation",
            (sys.executable, str(tools / "validate_foundation.py")),
            str(product_root),
        ),
        CommandStep(
            "governance-hardening",
            (sys.executable, str(tools / "validate_governance_hardening.py")),
            str(product_root),
        ),
        CommandStep(
            "catalog-contract",
            (sys.executable, str(tools / "validate_catalog_tests.py")),
            str(product_root),
        ),
        CommandStep(
            "o3de-source-policy",
            (
                sys.executable,
                "-u",
                str(source_policy),
                "--path",
                str(product_root / "Gems" / "TaintedGrailModdingSDK"),
            ),
            str(product_root),
        ),
        CommandStep(
            "compiled-catalog-tests",
            (
                ctest,
                "--test-dir",
                str(build_dir),
                "-C",
                DEFAULT_CONFIGURATION,
                "--output-on-failure",
                "--no-tests=error",
                "-R",
                CATALOG_TEST_PATTERN,
            ),
            str(product_root),
        ),
    ]


def execute_plan(
    plan: Sequence[CommandStep],
    *,
    dry_run: bool,
    executor: ProcessExecutor = default_executor,
) -> tuple[list[StepResult], int]:
    results: list[StepResult] = []
    for step in plan:
        print(f"+ {command_text(step.command)}")
        if dry_run:
            results.append(StepResult(step.name, step.command, step.cwd, "planned", None, 0.0))
            continue

        started = time.monotonic()
        try:
            exit_code = executor(step.command, Path(step.cwd))
        except OSError as exc:
            print(f"Unable to execute {step.name}: {exc}", file=sys.stderr)
            exit_code = 127
        duration = round(time.monotonic() - started, 3)
        status = "passed" if exit_code == 0 else "failed"
        results.append(StepResult(step.name, step.command, step.cwd, status, exit_code, duration))
        if exit_code != 0:
            return results, exit_code
    return results, 0


def atomic_write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="\n",
        dir=path.parent,
        prefix=f".{path.name}.",
        suffix=".tmp",
        delete=False,
    ) as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(path)


def prerequisite_payload(
    product_root: Path,
    engine_root: Path,
    build_dir: Path,
    lock: EngineLock,
    checks: Sequence[CheckResult],
) -> dict:
    return {
        "schema_version": PREVIEW_SCHEMA_VERSION,
        "preview": PREVIEW_NAME,
        "command": "prerequisites",
        "primary_host": PRIMARY_HOST,
        "product_root": str(product_root),
        "engine_root": str(engine_root),
        "engine_commit": lock.commit,
        "build_root": str(build_dir),
        "status": "passed" if required_checks_pass(checks) else "failed",
        "checks": [asdict(check) for check in checks],
    }


def validation_payload(
    product_root: Path,
    engine_root: Path,
    build_dir: Path,
    lock: EngineLock,
    results: Sequence[StepResult],
    exit_code: int,
    *,
    dry_run: bool,
) -> dict:
    return {
        "schema_version": PREVIEW_SCHEMA_VERSION,
        "preview": PREVIEW_NAME,
        "command": "validate",
        "product_root": str(product_root),
        "engine_root": str(engine_root),
        "engine_commit": lock.commit,
        "build_root": str(build_dir),
        "configuration": DEFAULT_CONFIGURATION,
        "status": "planned" if dry_run else ("passed" if exit_code == 0 else "failed"),
        "exit_code": None if dry_run else exit_code,
        "steps": [asdict(result) for result in results],
    }


def add_common_paths(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--product-root",
        "--repo-root",
        dest="product_root",
        type=Path,
        help="FOA-SDK product checkout. --repo-root remains a temporary compatibility alias.",
    )
    parser.add_argument(
        "--engine-root",
        type=Path,
        help=f"Exact O3DE checkout. Defaults to ${ENGINE_ROOT_ENVIRONMENT_VARIABLE}, then a sibling checkout.",
    )
    parser.add_argument("--build-dir", type=Path, help="Explicit build root.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    prerequisites = subparsers.add_parser("prerequisites", help="Check host, product, engine, and build prerequisites.")
    add_common_paths(prerequisites)
    prerequisites.add_argument("--json-output", type=Path, help="Optional machine-readable result path.")

    configure = subparsers.add_parser("configure", help="Configure the approved Windows x64 O3DE build.")
    add_common_paths(configure)
    configure.add_argument("--cmake", default="cmake", help="CMake executable.")
    configure.add_argument("--dry-run", action="store_true", help="Print the command without executing it.")

    build = subparsers.add_parser(
        "build",
        help="Build the Editor, asset preflight, and TG SDK catalog tests.",
    )
    add_common_paths(build)
    build.add_argument("--cmake", default="cmake", help="CMake executable.")
    build.add_argument("--dry-run", action="store_true", help="Print the command without executing it.")

    validate = subparsers.add_parser("validate", help="Run all focused validators and compiled catalog tests.")
    add_common_paths(validate)
    validate.add_argument("--ctest", default="ctest", help="CTest executable.")
    validate.add_argument("--result", type=Path, help="Machine-readable validation result path.")
    validate.add_argument("--dry-run", action="store_true", help="Print the ordered validation plan.")
    return parser


def resolve_common_paths(args: argparse.Namespace) -> tuple[Path, Path, Path, EngineLock]:
    product_root = (
        resolve_path(args.product_root, Path.cwd())
        if args.product_root
        else product_root_from_script()
    )
    validate_product_root(product_root)
    lock = load_engine_lock(product_root)
    engine_root = (
        resolve_path(args.engine_root, Path.cwd())
        if args.engine_root
        else default_engine_root(product_root, lock)
    )
    build_dir = (
        resolve_path(args.build_dir, Path.cwd())
        if args.build_dir
        else default_build_directory(product_root)
    )
    return product_root, engine_root, build_dir, lock


def require_primary_host() -> None:
    if platform.system() != "Windows" or platform.machine().lower() not in {"amd64", "x86_64"}:
        raise RuntimeError(f"This command currently supports only {PRIMARY_HOST}.")


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        product_root, engine_root, build_dir, lock = resolve_common_paths(args)

        if args.command == "prerequisites":
            checks = collect_prerequisite_checks(product_root, engine_root, build_dir, lock)
            for check in checks:
                marker = {"passed": "PASS", "failed": "FAIL"}.get(check.status, "INFO")
                requirement = "required" if check.required else "optional"
                print(f"[{marker}] {check.name} ({requirement}): {check.detail}")
                if check.remediation and check.status != "passed":
                    print(f"       {check.remediation}")
            payload = prerequisite_payload(product_root, engine_root, build_dir, lock, checks)
            if args.json_output:
                output = resolve_path(args.json_output, Path.cwd())
                atomic_write_json(output, payload)
                print(f"Wrote prerequisite result: {output}")
            return 0 if required_checks_pass(checks) else 1

        validate_engine_root(engine_root, lock)
        validate_engine_pin(engine_root, lock)

        if args.command == "configure":
            require_primary_host()
            validate_build_directory(
                product_root, engine_root, build_dir, require_configured=False
            )
            step = CommandStep(
                "configure",
                configure_command(product_root, engine_root, build_dir, args.cmake),
                str(product_root),
            )
            _, exit_code = execute_plan((step,), dry_run=args.dry_run)
            return exit_code

        if args.command == "build":
            require_primary_host()
            validate_build_directory(
                product_root,
                engine_root,
                build_dir,
                require_configured=not args.dry_run,
            )
            step = CommandStep("build", build_command(build_dir, args.cmake), str(product_root))
            _, exit_code = execute_plan((step,), dry_run=args.dry_run)
            return exit_code

        if args.command == "validate":
            validate_build_directory(
                product_root,
                engine_root,
                build_dir,
                require_configured=not args.dry_run,
            )
            plan = validation_plan(product_root, engine_root, build_dir, args.ctest)
            results, exit_code = execute_plan(plan, dry_run=args.dry_run)
            result_path = (
                resolve_path(args.result, Path.cwd())
                if args.result
                else build_dir / "tg-sdk-developer-preview-validation.json"
            )
            atomic_write_json(
                result_path,
                validation_payload(
                    product_root,
                    engine_root,
                    build_dir,
                    lock,
                    results,
                    exit_code,
                    dry_run=args.dry_run,
                ),
            )
            print(f"Wrote validation result: {result_path}")
            return exit_code

        raise RuntimeError(f"Unsupported command: {args.command}")
    except (OSError, RuntimeError) as exc:
        print(f"Developer Preview 0 command failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
