#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Launch the source-built O3DE Editor for Developer Preview 0.

The wrapper launches only the O3DE Editor executable. It does not launch FoA, inject
BepInEx or Harmony, invoke Avalon Core, deploy files, modify saves, or accept arbitrary
passthrough arguments.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shlex
import subprocess
import sys
import tempfile
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Sequence, TextIO

SCHEMA_VERSION = 1
PRIMARY_HOST = "Windows x64 Profile"
DEFAULT_CONFIGURATION = "profile"
EDITOR_FILENAME = "Editor.exe"
DEFAULT_RESULT_FILENAME = "tg-sdk-developer-preview-launch.json"
STDOUT_FILENAME = "editor-launch.stdout.log"
STDERR_FILENAME = "editor-launch.stderr.log"

ProcessExecutor = Callable[[Sequence[str], Path, TextIO | None, TextIO | None], int]


@dataclass(frozen=True)
class LaunchResult:
    schema_version: int
    preview: str
    status: str
    exit_code: int | None
    command: tuple[str, ...]
    working_directory: str
    editor: str
    project: str
    log_directory: str
    stdout_log: str
    stderr_log: str
    started_at_utc: str
    duration_seconds: float
    dry_run: bool


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def default_build_directory(repo_root: Path) -> Path:
    return repo_root / "build" / "tg-sdk-developer-preview-0-windows-profile"


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


def require_primary_host() -> None:
    system = platform.system()
    machine = platform.machine().lower()
    if system != "Windows" or machine not in {"amd64", "x86_64"}:
        raise RuntimeError(
            f"Developer Preview 0 launch is supported only on {PRIMARY_HOST}; "
            f"detected {system} {platform.machine()}."
        )


def editor_candidates(build_dir: Path) -> tuple[Path, ...]:
    return (
        build_dir / "bin" / DEFAULT_CONFIGURATION / EDITOR_FILENAME,
        build_dir / "bin" / "Profile" / EDITOR_FILENAME,
    )


def validate_build_directory(repo_root: Path, build_dir: Path) -> None:
    if build_dir == repo_root:
        raise RuntimeError("The build directory must not be the repository root.")
    if is_relative_to(repo_root, build_dir):
        raise RuntimeError("The build directory must not contain the repository checkout.")
    if is_relative_to(build_dir, repo_root / ".git"):
        raise RuntimeError("The build directory must not be inside .git.")


def validate_editor_executable(editor: Path) -> None:
    if not editor.is_file():
        raise RuntimeError(f"The O3DE Editor executable does not exist: {editor}")
    if editor.name.casefold() != EDITOR_FILENAME.casefold():
        raise RuntimeError(
            f"The launch wrapper accepts only {EDITOR_FILENAME}, not an arbitrary executable: {editor}"
        )


def resolve_editor_executable(
    repo_root: Path,
    *,
    explicit_editor: Path | None,
    build_dir: Path | None,
) -> tuple[Path, Path | None]:
    if explicit_editor is not None:
        editor = resolve_path(explicit_editor, repo_root)
        validate_editor_executable(editor)
        return editor, None

    resolved_build = resolve_path(build_dir or default_build_directory(repo_root), repo_root)
    validate_build_directory(repo_root, resolved_build)
    for candidate in editor_candidates(resolved_build):
        if candidate.is_file():
            validate_editor_executable(candidate)
            return candidate, resolved_build
    expected = ", ".join(str(candidate) for candidate in editor_candidates(resolved_build))
    raise RuntimeError(
        "The O3DE Editor has not been built for Developer Preview 0. "
        f"Expected one of: {expected}. Run the preview build command first."
    )


def validate_project_path(project: Path) -> None:
    if not project.is_dir():
        raise RuntimeError(f"The O3DE project directory does not exist: {project}")
    manifest = project / "project.json"
    if not manifest.is_file():
        raise RuntimeError(f"The O3DE project directory has no project.json: {project}")
    try:
        document = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"The O3DE project manifest is not valid UTF-8 JSON: {manifest}: {exc}") from exc
    if not isinstance(document, dict) or not isinstance(document.get("project_name"), str):
        raise RuntimeError(f"The O3DE project manifest has no project_name: {manifest}")


def validate_engine_path(engine: Path) -> None:
    if not engine.is_dir() or not (engine / "engine.json").is_file():
        raise RuntimeError(f"The O3DE engine directory is invalid: {engine}")


def validate_write_paths(
    project_cache: Path,
    project_user: Path,
    project_log: Path,
) -> None:
    for label, directory in (
        ("cache", project_cache),
        ("user", project_user),
        ("log", project_log),
    ):
        if directory.is_symlink() or not directory.is_dir():
            raise RuntimeError(f"The Developer Preview {label} directory is missing or unsafe: {directory}")
    resolved_user = project_user.resolve(strict=True)
    resolved_log = project_log.resolve(strict=True)
    if not is_relative_to(resolved_log, resolved_user):
        raise RuntimeError(
            f"The Developer Preview log directory must remain inside the user directory: {resolved_log}"
        )


def validate_startup_level(project: Path, startup_level: Path) -> None:
    if not startup_level.is_file():
        raise RuntimeError(f"The Editor startup level does not exist: {startup_level}")
    if startup_level.suffix.casefold() != ".prefab":
        raise RuntimeError(f"The Editor startup level must use the .prefab extension: {startup_level}")
    levels_root = (project / "Levels").resolve(strict=False)
    resolved_level = startup_level.resolve(strict=True)
    if not is_relative_to(resolved_level, levels_root):
        raise RuntimeError(
            f"The Editor startup level must remain inside the project Levels directory: {resolved_level}"
        )


def validate_log_directory(log_dir: Path) -> None:
    if log_dir.is_symlink():
        raise RuntimeError(f"The launch log directory must not be a symbolic link: {log_dir}")
    if log_dir.exists() and not log_dir.is_dir():
        raise RuntimeError(f"The launch log path is not a directory: {log_dir}")


def launch_command(
    editor: Path,
    project: Path | None,
    startup_level: Path | None,
    *,
    engine: Path | None = None,
    project_cache: Path | None = None,
    project_user: Path | None = None,
    project_log: Path | None = None,
) -> tuple[str, ...]:
    command = [str(editor)]
    if project is not None:
        command.extend(("--project-path", str(project)))
    write_paths = (project_cache, project_user, project_log)
    if engine is not None:
        if project is None:
            raise RuntimeError("An explicit O3DE engine path requires an explicit project.")
        command.extend(("--engine-path", str(engine)))
    if any(path is not None for path in write_paths):
        if project is None or any(path is None for path in write_paths):
            raise RuntimeError(
                "Project cache, user, and log paths must be supplied together with an explicit project."
            )
        command.extend(
            (
                "--project-cache-path",
                str(project_cache),
                "--project-user-path",
                str(project_user),
                "--project-log-path",
                str(project_log),
            )
        )
    if startup_level is not None:
        if project is None:
            raise RuntimeError("An Editor startup level requires an explicit O3DE project.")
        command.append(str(startup_level))
    return tuple(command)


def command_text(command: Sequence[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(list(command))
    return shlex.join(command)


def default_executor(
    command: Sequence[str],
    cwd: Path,
    stdout: TextIO | None,
    stderr: TextIO | None,
) -> int:
    completed = subprocess.run(
        list(command),
        cwd=str(cwd),
        check=False,
        stdout=stdout,
        stderr=stderr,
        env=os.environ.copy(),
    )
    return int(completed.returncode)


def atomic_write_json(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def pane_guidance() -> str:
    return (
        "After the Editor opens, use Tools -> Tainted Grail SDK. If the panes are absent, "
        "confirm engine.json registers Gems/TaintedGrailModdingSDK, rebuild the Editor target, "
        "and inspect Editor.log for TaintedGrailModdingSDK activation messages."
    )


def run_launch(
    *,
    editor: Path,
    project: Path | None,
    startup_level: Path | None,
    engine: Path | None = None,
    project_cache: Path | None = None,
    project_user: Path | None = None,
    project_log: Path | None = None,
    log_dir: Path | None,
    result_path: Path | None,
    dry_run: bool,
    executor: ProcessExecutor = default_executor,
) -> tuple[LaunchResult, int]:
    command = launch_command(
        editor,
        project,
        startup_level,
        engine=engine,
        project_cache=project_cache,
        project_user=project_user,
        project_log=project_log,
    )
    print(f"+ {command_text(command)}")
    print(pane_guidance())

    started = utc_now()
    if dry_run:
        result = LaunchResult(
            SCHEMA_VERSION,
            "Developer Preview 0",
            "planned",
            None,
            command,
            str(editor.parent),
            str(editor),
            str(project or ""),
            str(log_dir or ""),
            "",
            "",
            started,
            0.0,
            True,
        )
        return result, 0

    require_primary_host()
    stdout_path: Path | None = None
    stderr_path: Path | None = None
    stdout_stream: TextIO | None = None
    stderr_stream: TextIO | None = None
    if log_dir is not None:
        validate_log_directory(log_dir)
        log_dir.mkdir(parents=True, exist_ok=True)
        stdout_path = log_dir / STDOUT_FILENAME
        stderr_path = log_dir / STDERR_FILENAME
        stdout_stream = stdout_path.open("w", encoding="utf-8", newline="\n", errors="replace")
        stderr_stream = stderr_path.open("w", encoding="utf-8", newline="\n", errors="replace")

    began = time.monotonic()
    try:
        try:
            exit_code = executor(command, editor.parent, stdout_stream, stderr_stream)
        except OSError as exc:
            print(f"Unable to launch the O3DE Editor: {exc}", file=sys.stderr)
            exit_code = 127
    finally:
        if stdout_stream is not None:
            stdout_stream.close()
        if stderr_stream is not None:
            stderr_stream.close()
    duration = round(time.monotonic() - began, 3)
    status = "passed" if exit_code == 0 else "failed"
    result = LaunchResult(
        SCHEMA_VERSION,
        "Developer Preview 0",
        status,
        exit_code,
        command,
        str(editor.parent),
        str(editor),
        str(project or ""),
        str(log_dir or ""),
        str(stdout_path or ""),
        str(stderr_path or ""),
        started,
        duration,
        False,
    )
    output_path = result_path or (log_dir / DEFAULT_RESULT_FILENAME if log_dir is not None else None)
    if output_path is not None:
        atomic_write_json(output_path, asdict(result))
    return result, exit_code


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, help="Explicit O3DE repository root.")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--editor", type=Path, help=f"Explicit {EDITOR_FILENAME} path.")
    source.add_argument("--build-dir", type=Path, help="Configured Developer Preview build directory.")
    parser.add_argument("--project", type=Path, help="Optional O3DE project directory containing project.json.")
    parser.add_argument("--engine", type=Path, help="Optional O3DE engine directory containing engine.json.")
    parser.add_argument("--project-cache", type=Path, help="Optional writable project cache directory.")
    parser.add_argument("--project-user", type=Path, help="Optional writable project user directory.")
    parser.add_argument("--project-log", type=Path, help="Optional writable project log directory.")
    parser.add_argument(
        "--level",
        type=Path,
        help="Optional project-owned .prefab level to open in the Editor viewport.",
    )
    parser.add_argument(
        "--log-dir",
        type=Path,
        help="Optional wrapper-owned directory for stdout, stderr, and launch-result files.",
    )
    parser.add_argument("--result", type=Path, help="Optional explicit launch-result JSON path.")
    parser.add_argument("--dry-run", action="store_true", help="Print the resolved Editor command without launching.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = resolve_path(args.repo_root or repository_root_from_script(), Path.cwd())
    try:
        editor, _ = resolve_editor_executable(
            repo_root,
            explicit_editor=args.editor,
            build_dir=args.build_dir,
        )
        project = resolve_path(args.project, repo_root) if args.project else None
        if project is not None and not args.dry_run:
            validate_project_path(project)
        engine = resolve_path(args.engine, repo_root) if args.engine else None
        if engine is not None and not args.dry_run:
            validate_engine_path(engine)
        project_cache = resolve_path(args.project_cache, repo_root) if args.project_cache else None
        project_user = resolve_path(args.project_user, repo_root) if args.project_user else None
        project_log = resolve_path(args.project_log, repo_root) if args.project_log else None
        if any(path is not None for path in (project_cache, project_user, project_log)):
            if any(path is None for path in (project_cache, project_user, project_log)):
                raise RuntimeError(
                    "--project-cache, --project-user, and --project-log must be supplied together."
                )
            assert project_cache is not None and project_user is not None and project_log is not None
            if not args.dry_run:
                validate_write_paths(project_cache, project_user, project_log)
        startup_level = resolve_path(args.level, repo_root) if args.level else None
        if startup_level is not None:
            if project is None:
                raise RuntimeError("--level requires --project.")
            if not args.dry_run:
                validate_startup_level(project, startup_level)
        log_dir = resolve_path(args.log_dir, repo_root) if args.log_dir else None
        if log_dir is not None and not args.dry_run:
            validate_log_directory(log_dir)
        result_path = resolve_path(args.result, repo_root) if args.result else None
        _, exit_code = run_launch(
            editor=editor,
            project=project,
            startup_level=startup_level,
            engine=engine,
            project_cache=project_cache,
            project_user=project_user,
            project_log=project_log,
            log_dir=log_dir,
            result_path=result_path,
            dry_run=args.dry_run,
        )
        return exit_code
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"Developer Preview 0 launch failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
