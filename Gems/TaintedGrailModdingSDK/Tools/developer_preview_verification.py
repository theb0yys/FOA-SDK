#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Coordinate the repository's existing exact-head Developer Preview verification tools.

This is orchestration only. It delegates to validation_receipt.py, developer_preview.py,
run_local_validation.py, CTest, and developer_preview_ui_evidence.py. It does not replace
those checks, capture screenshots, automate Editor input, launch FoA, deploy files, sign
or verify data, upload evidence, or modify saves.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Callable, Sequence

SCHEMA_VERSION = 1
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
ALIAS_PATTERN = re.compile(r"^[A-Za-z0-9._-]{2,64}$")
DEFAULT_CONFIGURATION = "profile"
CATALOG_TEST_PATTERN = r"TaintedGrailModdingSDK\.Catalog\.Tests"
AUTOMATED_GATE_NAMES = (
    "git-diff-check",
    "local-validation",
    "o3de-configure",
    "o3de-build",
    "compiled-tests",
)
WINDOWS_UI_GATE = "windows-ui"
RECEIPT_DOCUMENT = "validation-receipt.json"
UI_EVIDENCE_DOCUMENT = "ui-evidence.json"


@dataclass(frozen=True)
class VerificationPaths:
    repo_root: Path
    build_dir: Path
    receipt_dir: Path
    ui_evidence_dir: Path
    state_path: Path


@dataclass(frozen=True)
class VerificationStep:
    name: str
    command: tuple[str, ...]
    cwd: Path


@dataclass(frozen=True)
class StepResult:
    name: str
    status: str
    exit_code: int | None
    command: tuple[str, ...]


class VerificationError(RuntimeError):
    pass


ProcessExecutor = Callable[[Sequence[str], Path], int]


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def resolve_path(value: Path, base: Path) -> Path:
    path = value.expanduser()
    return (path if path.is_absolute() else base / path).resolve(strict=False)


def command_text(command: Sequence[str]) -> str:
    return subprocess.list2cmdline(list(command)) if os.name == "nt" else shlex.join(command)


def capture_command(command: Sequence[str], cwd: Path) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            list(command),
            cwd=str(cwd),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        return 127, str(exc)
    return int(completed.returncode), (completed.stdout + completed.stderr).strip()


def default_executor(command: Sequence[str], cwd: Path) -> int:
    return int(subprocess.run(list(command), cwd=str(cwd), check=False).returncode)


def require_repository(repo_root: Path) -> None:
    required = (
        repo_root / ".git",
        repo_root / "engine.json",
        repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview.py",
        repo_root / "Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py",
        repo_root / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py",
        repo_root / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py",
    )
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise VerificationError("Repository is missing required verification files: " + ", ".join(missing))


def repository_state(repo_root: Path, *, require_clean: bool = True) -> str:
    require_repository(repo_root)
    code, top = capture_command(("git", "-C", str(repo_root), "rev-parse", "--show-toplevel"), repo_root)
    if code != 0 or Path(top).resolve(strict=False) != repo_root.resolve(strict=False):
        raise VerificationError(f"Repository root mismatch or Git failure: {top}")
    code, commit = capture_command(("git", "-C", str(repo_root), "rev-parse", "HEAD"), repo_root)
    commit = commit.strip().lower()
    if code != 0 or COMMIT_PATTERN.fullmatch(commit) is None:
        raise VerificationError("A full lowercase 40-character Git HEAD is required.")
    if require_clean:
        code, status = capture_command(
            ("git", "-C", str(repo_root), "status", "--porcelain=v1", "--untracked-files=all"),
            repo_root,
        )
        if code != 0:
            raise VerificationError(f"Unable to inspect working tree: {status}")
        if status.strip():
            raise VerificationError("Exact-head verification requires a clean working tree before every step.")
    return commit


def default_paths(repo_root: Path, commit: str) -> VerificationPaths:
    short = commit[:12]
    return VerificationPaths(
        repo_root=repo_root,
        build_dir=repo_root / "build/tg-sdk-developer-preview-0-windows-profile",
        receipt_dir=repo_root.parent / f"tg-sdk-exact-head-receipt-{short}",
        ui_evidence_dir=repo_root / f"build/tg-sdk-developer-preview-0-ui-evidence-{short}",
        state_path=repo_root / f"build/tg-sdk-developer-preview-0-verification-{short}.json",
    )


def validate_identity_inputs(tester_alias: str, windows_version: str, display_scale: int) -> None:
    if ALIAS_PATTERN.fullmatch(tester_alias.strip()) is None:
        raise VerificationError("Tester alias must use 2-64 letters, digits, dots, underscores, or hyphens.")
    if not windows_version.strip() or any(char in windows_version for char in ("\r", "\n", "\x00")):
        raise VerificationError("Windows version must be non-empty single-line text.")
    if not 100 <= display_scale <= 200:
        raise VerificationError("Display scale must be between 100 and 200 percent.")


def validate_paths(paths: VerificationPaths) -> None:
    repo = paths.repo_root.resolve(strict=False)
    build_root = repo / "build"
    if is_relative_to(paths.receipt_dir, repo):
        raise VerificationError(
            "Validation receipts and captured gate logs must be stored outside the repository."
        )
    if is_relative_to(paths.ui_evidence_dir, repo) and not is_relative_to(paths.ui_evidence_dir, build_root):
        raise VerificationError("UI evidence inside the repository must be stored beneath build/.")
    if is_relative_to(paths.state_path, repo) and not is_relative_to(paths.state_path, build_root):
        raise VerificationError("Verification state inside the repository must be stored beneath build/.")
    if paths.receipt_dir == paths.ui_evidence_dir:
        raise VerificationError("Receipt and UI evidence directories must be separate.")
    if paths.build_dir == repo:
        raise VerificationError("Build directory must not be the repository root.")


def validate_prepare_targets(paths: VerificationPaths) -> None:
    for path, label in (
        (paths.receipt_dir, "Validation receipt directory"),
        (paths.ui_evidence_dir, "UI evidence directory"),
    ):
        if not path.exists():
            continue
        if not path.is_dir() or any(path.iterdir()):
            raise VerificationError(f"{label} must be absent or empty for a new exact-head run: {path}")


def resolve_paths(args: argparse.Namespace, repo_root: Path, commit: str) -> VerificationPaths:
    defaults = default_paths(repo_root, commit)
    values = VerificationPaths(
        repo_root=repo_root,
        build_dir=resolve_path(args.build_dir, Path.cwd()) if args.build_dir else defaults.build_dir,
        receipt_dir=resolve_path(args.receipt_dir, Path.cwd()) if args.receipt_dir else defaults.receipt_dir,
        ui_evidence_dir=(
            resolve_path(args.ui_evidence_dir, Path.cwd())
            if args.ui_evidence_dir
            else defaults.ui_evidence_dir
        ),
        state_path=resolve_path(args.state_output, Path.cwd()) if args.state_output else defaults.state_path,
    )
    validate_paths(values)
    return values


def tools_root(paths: VerificationPaths) -> Path:
    return paths.repo_root / "Gems/TaintedGrailModdingSDK/Tools"


def python_tool(paths: VerificationPaths, filename: str, *args: str) -> tuple[str, ...]:
    return (sys.executable, str(tools_root(paths) / filename), *args)


def receipt_prefix(paths: VerificationPaths, command: str) -> tuple[str, ...]:
    return python_tool(
        paths,
        "validation_receipt.py",
        "--repo-root",
        str(paths.repo_root),
        command,
        "--output",
        str(paths.receipt_dir),
    )


def prerequisites_step(paths: VerificationPaths, name: str = "prerequisites") -> VerificationStep:
    command = python_tool(
        paths,
        "developer_preview.py",
        "prerequisites",
        "--repo-root",
        str(paths.repo_root),
        "--build-dir",
        str(paths.build_dir),
        "--json-output",
        str(paths.build_dir / "prerequisites.json"),
    )
    return VerificationStep(name, command, paths.repo_root)


def prepare_steps(
    paths: VerificationPaths,
    commit: str,
    *,
    tester_alias: str,
    windows_version: str,
    display_scale: int,
) -> tuple[VerificationStep, ...]:
    return (
        prerequisites_step(paths),
        VerificationStep(
            "initialize-receipt",
            (
                *receipt_prefix(paths, "init"),
                "--tester-alias",
                tester_alias,
                "--platform",
                f"{windows_version} x64",
                "--configuration",
                DEFAULT_CONFIGURATION,
            ),
            paths.repo_root,
        ),
        VerificationStep(
            "initialize-ui-evidence",
            python_tool(
                paths,
                "developer_preview_ui_evidence.py",
                "init",
                "--output",
                str(paths.ui_evidence_dir),
                "--repo-root",
                str(paths.repo_root),
                "--source-commit",
                commit,
                "--tester-alias",
                tester_alias,
                "--windows-version",
                windows_version,
                "--display-scale",
                str(display_scale),
            ),
            paths.repo_root,
        ),
    )


def automated_gate_commands(paths: VerificationPaths) -> dict[str, tuple[str, ...]]:
    return {
        "git-diff-check": ("git", "diff", "--check"),
        "local-validation": python_tool(paths, "run_local_validation.py", "--keep-going"),
        "o3de-configure": python_tool(
            paths,
            "developer_preview.py",
            "configure",
            "--repo-root",
            str(paths.repo_root),
            "--build-dir",
            str(paths.build_dir),
        ),
        "o3de-build": python_tool(
            paths,
            "developer_preview.py",
            "build",
            "--repo-root",
            str(paths.repo_root),
            "--build-dir",
            str(paths.build_dir),
        ),
        "compiled-tests": (
            "ctest",
            "--test-dir",
            str(paths.build_dir),
            "-C",
            DEFAULT_CONFIGURATION,
            "--output-on-failure",
            "-R",
            CATALOG_TEST_PATTERN,
        ),
    }


def record_step(paths: VerificationPaths, gate: str, command: Sequence[str], notes: str) -> VerificationStep:
    return VerificationStep(
        gate,
        (*receipt_prefix(paths, "record"), "--name", gate, "--notes", notes, "--", *command),
        paths.repo_root,
    )


def automated_steps(
    paths: VerificationPaths,
    *,
    recorded: dict[str, str] | None = None,
) -> tuple[VerificationStep, ...]:
    recorded = recorded or {}
    commands = automated_gate_commands(paths)
    notes = {
        "git-diff-check": "Repository whitespace check at the clean exact head.",
        "local-validation": "Authoritative repository-owned static and Python validation gate.",
        "o3de-configure": "Approved Windows x64 Developer Preview O3DE configure command.",
        "o3de-build": "Profile Editor and TG SDK compiled-test target build.",
        "compiled-tests": "Production-linked TaintedGrailModdingSDK.Catalog.Tests CTest run.",
    }
    steps = [prerequisites_step(paths, "prerequisites-recheck")]
    for gate in AUTOMATED_GATE_NAMES:
        status = recorded.get(gate)
        if status == "passed":
            continue
        if status is not None:
            raise VerificationError(
                f"Receipt already records {gate} as {status}; initialize a new receipt before rerunning."
            )
        steps.append(record_step(paths, gate, commands[gate], notes[gate]))
    return tuple(steps)


def finalize_steps(
    paths: VerificationPaths,
    commit: str,
    *,
    recorded: dict[str, str] | None = None,
) -> tuple[VerificationStep, ...]:
    recorded = recorded or {}
    missing = [gate for gate in AUTOMATED_GATE_NAMES if recorded.get(gate) != "passed"]
    if missing:
        raise VerificationError(
            "Automated receipt gates must pass before Windows UI finalization: " + ", ".join(missing)
        )
    windows_status = recorded.get(WINDOWS_UI_GATE)
    if windows_status not in (None, "passed"):
        raise VerificationError(
            f"Receipt already records windows-ui as {windows_status}; initialize a new receipt before rerunning."
        )
    steps: list[VerificationStep] = []
    if windows_status is None:
        ui_verify = python_tool(
            paths,
            "developer_preview_ui_evidence.py",
            "verify",
            "--output",
            str(paths.ui_evidence_dir),
            "--expected-commit",
            commit,
        )
        steps.append(
            record_step(
                paths,
                WINDOWS_UI_GATE,
                ui_verify,
                "Verified twenty-two-pane Windows x64 Profile UI evidence for the exact head.",
            )
        )
    for name, command, merge_ready in (
        ("finalize-receipt", "finalize", False),
        ("verify-receipt", "verify", True),
        ("summarize-receipt", "summarize", True),
    ):
        args = [*receipt_prefix(paths, command), "--expected-commit", commit]
        if merge_ready:
            args.append("--require-merge-ready")
        steps.append(VerificationStep(name, tuple(args), paths.repo_root))
    return tuple(steps)


def execute_steps(
    steps: Sequence[VerificationStep],
    *,
    dry_run: bool,
    executor: ProcessExecutor = default_executor,
) -> tuple[list[StepResult], int]:
    results: list[StepResult] = []
    for step in steps:
        print(f"\n=== {step.name} ===\n+ {command_text(step.command)}")
        if dry_run:
            results.append(StepResult(step.name, "planned", None, step.command))
            continue
        try:
            exit_code = executor(step.command, step.cwd)
        except OSError as exc:
            print(f"Unable to execute {step.name}: {exc}", file=sys.stderr)
            exit_code = 127
        status = "passed" if exit_code == 0 else "failed"
        results.append(StepResult(step.name, status, exit_code, step.command))
        if exit_code != 0:
            return results, exit_code
    return results, 0


def read_json(path: Path) -> dict:
    if not path.is_file():
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise VerificationError(f"Unable to read {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise VerificationError(f"Expected a JSON object: {path}")
    return value


def receipt_status(paths: VerificationPaths) -> tuple[dict, dict[str, str]]:
    receipt = read_json(paths.receipt_dir / RECEIPT_DOCUMENT)
    statuses: dict[str, str] = {}
    for gate in receipt.get("gates", []) if isinstance(receipt.get("gates", []), list) else []:
        if isinstance(gate, dict) and isinstance(gate.get("name"), str):
            statuses[gate["name"]] = str(gate.get("status", "unknown"))
    return receipt, statuses


def ui_status(paths: VerificationPaths) -> dict:
    document = read_json(paths.ui_evidence_dir / UI_EVIDENCE_DOCUMENT)
    counts = {"pass": 0, "fail": 0, "blocked": 0, "pending": 0, "other": 0}
    ids = {"fail": [], "blocked": [], "pending": []}
    checklist = document.get("checklist", []) if isinstance(document.get("checklist", []), list) else []
    for entry in checklist:
        status = entry.get("status") if isinstance(entry, dict) else None
        key = status if status in counts else "other"
        counts[key] += 1
        if key in ids and isinstance(entry, dict):
            ids[key].append(str(entry.get("id", "unknown")))
    screenshots = document.get("screenshots", [])
    return {
        "exists": bool(document),
        "source_commit": document.get("source_commit") if document else None,
        "status": document.get("status") if document else None,
        "checklist_counts": counts,
        "pending_checks": ids["pending"],
        "blocked_checks": ids["blocked"],
        "failed_checks": ids["fail"],
        "screenshots": len(screenshots) if isinstance(screenshots, list) else 0,
    }


def next_action(commit: str, receipt: dict, gates: dict[str, str], ui: dict) -> str:
    if not receipt:
        return "prepare"
    if receipt.get("source_commit") != commit:
        return "initialize a new receipt for the current exact head"
    if any(status == "failed" for status in gates.values()):
        return "fix the failed gate and initialize a new exact-head receipt"
    if any(gates.get(gate) != "passed" for gate in AUTOMATED_GATE_NAMES):
        return "automated"
    if gates.get(WINDOWS_UI_GATE) != "passed":
        if not ui["exists"] or ui["source_commit"] != commit:
            return "initialize Windows UI evidence for the current exact head"
        return "complete the manual Windows checklist, screenshots, and attestation; then finalize"
    return "complete" if receipt.get("finalized_at_utc") else "finalize"


def state_payload(
    paths: VerificationPaths,
    commit: str,
    *,
    results: Sequence[StepResult] = (),
) -> dict:
    receipt, gates = receipt_status(paths)
    ui = ui_status(paths)
    return {
        "schema_version": SCHEMA_VERSION,
        "source_commit": commit,
        "paths": {
            "repo_root": str(paths.repo_root),
            "build_dir": str(paths.build_dir),
            "receipt_dir": str(paths.receipt_dir),
            "ui_evidence_dir": str(paths.ui_evidence_dir),
        },
        "receipt": {
            "exists": bool(receipt),
            "source_commit": receipt.get("source_commit") if receipt else None,
            "finalized_at_utc": receipt.get("finalized_at_utc") if receipt else None,
            "gates": gates,
        },
        "ui_evidence": ui,
        "last_results": [asdict(result) for result in results],
        "next_action": next_action(commit, receipt, gates, ui),
    }


def atomic_write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="\n",
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
        delete=False,
    ) as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(path)


def print_status(payload: dict) -> None:
    print("\n## Developer Preview exact-head verification")
    print(f"- Commit: `{payload['source_commit']}`")
    print(f"- Receipt: {payload['paths']['receipt_dir']}")
    print(f"- UI evidence: {payload['paths']['ui_evidence_dir']}")
    print(f"- Next action: {payload['next_action']}")
    print("\n| Gate | Status |\n|---|---:|")
    gates = payload["receipt"]["gates"]
    for gate in (*AUTOMATED_GATE_NAMES, WINDOWS_UI_GATE):
        print(f"| `{gate}` | {gates.get(gate, 'missing')} |")
    ui = payload["ui_evidence"]
    if ui["exists"]:
        counts = ui["checklist_counts"]
        print(
            f"\nUI checklist: {counts['pass']} pass, {counts['pending']} pending, "
            f"{counts['blocked']} blocked, {counts['fail']} fail; {ui['screenshots']} screenshot(s)."
        )
        for label, key in (
            ("Pending", "pending_checks"),
            ("Blocked", "blocked_checks"),
            ("Failed", "failed_checks"),
        ):
            if ui[key]:
                print(f"{label} UI checks: " + ", ".join(ui[key]))


def add_common_paths(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--receipt-dir", type=Path)
    parser.add_argument("--ui-evidence-dir", type=Path)
    parser.add_argument("--state-output", type=Path)


def add_identity_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--tester-alias", required=True)
    parser.add_argument("--windows-version", required=True)
    parser.add_argument("--display-scale", type=int, default=100)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    plan = subparsers.add_parser("plan", help="Print the exact established verification commands.")
    add_common_paths(plan)
    add_identity_args(plan)
    prepare = subparsers.add_parser("prepare", help="Run prerequisites and initialize evidence directories.")
    add_common_paths(prepare)
    add_identity_args(prepare)
    prepare.add_argument("--dry-run", action="store_true")
    automated = subparsers.add_parser("automated", help="Record all missing mandatory automated gates.")
    add_common_paths(automated)
    automated.add_argument("--dry-run", action="store_true")
    finalize = subparsers.add_parser("finalize", help="Verify Windows UI evidence and finalize the receipt.")
    add_common_paths(finalize)
    finalize.add_argument("--dry-run", action="store_true")
    status = subparsers.add_parser("status", help="Report honest receipt and UI-evidence progress.")
    add_common_paths(status)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        repo_root = resolve_path(args.repo_root, Path.cwd()) if args.repo_root else repository_root_from_script()
        commit = repository_state(repo_root)
        paths = resolve_paths(args, repo_root, commit)

        if args.command in {"plan", "prepare"}:
            validate_identity_inputs(args.tester_alias, args.windows_version, args.display_scale)
        if args.command == "plan":
            print(f"Exact clean HEAD: {commit}")
            steps = (
                *prepare_steps(
                    paths,
                    commit,
                    tester_alias=args.tester_alias,
                    windows_version=args.windows_version,
                    display_scale=args.display_scale,
                ),
                *automated_steps(paths),
                *finalize_steps(
                    paths,
                    commit,
                    recorded={gate: "passed" for gate in AUTOMATED_GATE_NAMES},
                ),
            )
            execute_steps(steps, dry_run=True)
            payload = state_payload(paths, commit)
            atomic_write_json(paths.state_path, payload)
            print_status(payload)
            print(f"\nWrote verification plan state: {paths.state_path}")
            return 0
        if args.command == "prepare":
            if not args.dry_run:
                validate_prepare_targets(paths)
            steps = prepare_steps(
                paths,
                commit,
                tester_alias=args.tester_alias,
                windows_version=args.windows_version,
                display_scale=args.display_scale,
            )
            results, code = execute_steps(steps, dry_run=args.dry_run)
        else:
            receipt, recorded = receipt_status(paths)
            if not receipt:
                raise VerificationError(f"Validation receipt is missing: {paths.receipt_dir / RECEIPT_DOCUMENT}")
            if receipt.get("source_commit") != commit:
                raise VerificationError(
                    "Receipt source commit does not match the current exact HEAD; initialize a new receipt."
                )
            if args.command == "automated":
                if receipt.get("finalized_at_utc"):
                    raise VerificationError("Finalized receipts are immutable; this run is already closed.")
                results, code = execute_steps(
                    automated_steps(paths, recorded=recorded),
                    dry_run=args.dry_run,
                )
            elif args.command == "finalize":
                if receipt.get("finalized_at_utc"):
                    results, code = [], 0
                else:
                    results, code = execute_steps(
                        finalize_steps(paths, commit, recorded=recorded),
                        dry_run=args.dry_run,
                    )
            elif args.command == "status":
                results, code = [], 0
            else:
                raise VerificationError(f"Unsupported command: {args.command}")

        payload = state_payload(paths, commit, results=results)
        atomic_write_json(paths.state_path, payload)
        print_status(payload)
        print(f"\nWrote verification status: {paths.state_path}")
        return code
    except (OSError, VerificationError) as exc:
        print(f"Developer Preview exact-head verification failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
