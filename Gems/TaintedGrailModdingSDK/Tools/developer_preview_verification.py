#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Coordinate the existing exact-head Developer Preview verification tools.

The authoritative validators remain validation_receipt.py and
 developer_preview_ui_evidence.py. This command only orders their existing
 operations with developer_preview.py, run_local_validation.py, Git, and CTest.
 It does not capture screenshots, automate Editor input, launch FoA, deploy,
 sign, upload evidence, or modify saves.
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

SCHEMA_VERSION = 2
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
ALIAS_PATTERN = re.compile(r"^[A-Za-z0-9._-]{2,64}$")
DEFAULT_CONFIGURATION = "profile"
DEFAULT_BASE_REF = "origin/main"
DEFAULT_SYNC_REFS = ("origin/main", "origin/foa-development")
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
ProbeRunner = Callable[[Sequence[str], Path], tuple[int, str]]


def repository_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def resolve_path(value: Path, base: Path) -> Path:
    value = value.expanduser()
    return (value if value.is_absolute() else base / value).resolve(strict=False)


def command_text(command: Sequence[str]) -> str:
    return subprocess.list2cmdline(list(command)) if os.name == "nt" else shlex.join(command)


def capture_command(command: Sequence[str], cwd: Path) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            list(command), cwd=str(cwd), check=False, capture_output=True,
            text=True, encoding="utf-8", errors="replace"
        )
    except OSError as exc:
        return 127, str(exc)
    return int(completed.returncode), (completed.stdout + completed.stderr).strip()


def default_executor(command: Sequence[str], cwd: Path) -> int:
    return int(subprocess.run(list(command), cwd=str(cwd), check=False).returncode)


def require_commit(value: str, label: str) -> str:
    value = value.strip().lower()
    if COMMIT_PATTERN.fullmatch(value) is None:
        raise VerificationError(f"{label} must resolve to one full lowercase Git commit.")
    return value


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
        raise VerificationError("Repository is missing verification files: " + ", ".join(missing))


def repository_state(repo_root: Path) -> str:
    require_repository(repo_root)
    code, top = capture_command(("git", "-C", str(repo_root), "rev-parse", "--show-toplevel"), repo_root)
    if code or Path(top).resolve(strict=False) != repo_root.resolve(strict=False):
        raise VerificationError(f"Repository root mismatch or Git failure: {top}")
    code, head = capture_command(("git", "-C", str(repo_root), "rev-parse", "HEAD"), repo_root)
    if code:
        raise VerificationError(f"Unable to resolve repository HEAD: {head}")
    code, status = capture_command(
        ("git", "-C", str(repo_root), "status", "--porcelain=v1", "--untracked-files=all"),
        repo_root,
    )
    if code:
        raise VerificationError(f"Unable to inspect working tree: {status}")
    if status.strip():
        raise VerificationError("Exact-head verification requires a clean working tree before every step.")
    return require_commit(head, "Repository HEAD")


def resolve_ref_commit(repo_root: Path, ref: str) -> str:
    ref = ref.strip()
    if not ref or any(character in ref for character in ("\r", "\n", "\x00")):
        raise VerificationError("Git refs must be non-empty single-line values.")
    code, commit = capture_command(
        ("git", "-C", str(repo_root), "rev-parse", "--verify", f"{ref}^{{commit}}"), repo_root
    )
    if code:
        raise VerificationError(f"Unable to resolve required Git ref {ref!r}: {commit}")
    return require_commit(commit, f"Git ref {ref!r}")


def resolve_review_ancestry(
    repo_root: Path, head_commit: str, base_ref: str, sync_refs: Sequence[str]
) -> tuple[str, dict[str, str]]:
    resolved: dict[str, str] = {}
    for ref in dict.fromkeys((base_ref, *sync_refs)):
        commit = resolve_ref_commit(repo_root, ref)
        code, output = capture_command(
            ("git", "-C", str(repo_root), "merge-base", "--is-ancestor", commit, head_commit),
            repo_root,
        )
        if code:
            raise VerificationError(
                f"FOA-plug-in-development is not synchronized with {ref} ({commit}): "
                + (output or "the ref is not an ancestor of the exact HEAD")
            )
        resolved[ref] = commit
    return resolved[base_ref], resolved


def default_paths(repo_root: Path, commit: str) -> VerificationPaths:
    short = commit[:12]
    build_root = repo_root / "build"
    return VerificationPaths(
        repo_root,
        build_root / "tg-sdk-developer-preview-0-windows-profile",
        repo_root.parent / f"tg-sdk-exact-head-receipt-{short}",
        build_root / f"tg-sdk-developer-preview-0-ui-evidence-{short}",
        build_root / f"tg-sdk-developer-preview-0-verification-{short}.json",
    )


def prerequisites_path(paths: VerificationPaths) -> Path:
    return paths.state_path.with_name(paths.state_path.stem + "-prerequisites.json")


def validate_identity_inputs(alias: str, windows_version: str, display_scale: int) -> None:
    if ALIAS_PATTERN.fullmatch(alias.strip()) is None:
        raise VerificationError("Tester alias must use 2-64 letters, digits, dots, underscores, or hyphens.")
    if not windows_version.strip() or any(c in windows_version for c in ("\r", "\n", "\x00")):
        raise VerificationError("Windows version must be non-empty single-line text.")
    if not 100 <= display_scale <= 200:
        raise VerificationError("Display scale must be between 100 and 200 percent.")


def validate_paths(paths: VerificationPaths) -> None:
    repo = paths.repo_root.resolve(strict=False)
    build_root = repo / "build"
    build_dir = paths.build_dir.resolve(strict=False)
    receipt_dir = paths.receipt_dir.resolve(strict=False)
    ui_dir = paths.ui_evidence_dir.resolve(strict=False)
    state_path = paths.state_path.resolve(strict=False)
    prereq_path = prerequisites_path(paths).resolve(strict=False)
    if is_relative_to(receipt_dir, repo):
        raise VerificationError("Validation receipts and captured gate logs must be stored outside the repository.")
    for path, label in ((ui_dir, "UI evidence"), (state_path, "Verification state"), (prereq_path, "Prerequisite result")):
        if is_relative_to(path, repo) and not is_relative_to(path, build_root):
            raise VerificationError(f"{label} inside the repository must be beneath build/.")
    if receipt_dir == ui_dir:
        raise VerificationError("Receipt and UI evidence directories must be separate.")
    if build_dir == repo or is_relative_to(repo, build_dir):
        raise VerificationError("Build directory must not be or contain the repository.")
    if is_relative_to(build_dir, repo / ".git"):
        raise VerificationError("Build directory must not be inside .git.")
    if is_relative_to(prereq_path, build_dir):
        raise VerificationError(
            "Prerequisite output must not make an unconfigured O3DE build directory non-empty."
        )


def validate_prepare_targets(paths: VerificationPaths) -> None:
    for path, label in ((paths.receipt_dir, "Validation receipt directory"), (paths.ui_evidence_dir, "UI evidence directory")):
        if path.exists() and (not path.is_dir() or any(path.iterdir())):
            raise VerificationError(f"{label} must be absent or empty for a new exact-head run: {path}")


def resolve_paths(args: argparse.Namespace, repo_root: Path, commit: str) -> VerificationPaths:
    defaults = default_paths(repo_root, commit)
    paths = VerificationPaths(
        repo_root,
        resolve_path(args.build_dir, Path.cwd()) if args.build_dir else defaults.build_dir,
        resolve_path(args.receipt_dir, Path.cwd()) if args.receipt_dir else defaults.receipt_dir,
        resolve_path(args.ui_evidence_dir, Path.cwd()) if args.ui_evidence_dir else defaults.ui_evidence_dir,
        resolve_path(args.state_output, Path.cwd()) if args.state_output else defaults.state_path,
    )
    validate_paths(paths)
    return paths


def tools_root(paths: VerificationPaths) -> Path:
    return paths.repo_root / "Gems/TaintedGrailModdingSDK/Tools"


def python_tool(paths: VerificationPaths, filename: str, *args: str) -> tuple[str, ...]:
    return (sys.executable, str(tools_root(paths) / filename), *args)


def receipt_prefix(paths: VerificationPaths, command: str) -> tuple[str, ...]:
    return python_tool(
        paths, "validation_receipt.py", "--repo-root", str(paths.repo_root),
        command, "--output", str(paths.receipt_dir)
    )


def receipt_verify_command(paths: VerificationPaths, commit: str, *, merge_ready: bool) -> tuple[str, ...]:
    command = [*receipt_prefix(paths, "verify"), "--expected-commit", commit]
    if merge_ready:
        command.append("--require-merge-ready")
    return tuple(command)


def ui_verify_command(paths: VerificationPaths, commit: str) -> tuple[str, ...]:
    return python_tool(
        paths, "developer_preview_ui_evidence.py", "verify", "--output",
        str(paths.ui_evidence_dir), "--expected-commit", commit
    )


def prerequisites_step(paths: VerificationPaths, name: str = "prerequisites") -> VerificationStep:
    return VerificationStep(
        name,
        python_tool(
            paths, "developer_preview.py", "prerequisites", "--repo-root", str(paths.repo_root),
            "--build-dir", str(paths.build_dir), "--json-output", str(prerequisites_path(paths))
        ),
        paths.repo_root,
    )


def prepare_steps(
    paths: VerificationPaths, commit: str, *, tester_alias: str,
    windows_version: str, display_scale: int
) -> tuple[VerificationStep, ...]:
    return (
        prerequisites_step(paths),
        VerificationStep(
            "initialize-receipt",
            (*receipt_prefix(paths, "init"), "--tester-alias", tester_alias, "--platform",
             f"{windows_version} x64", "--configuration", DEFAULT_CONFIGURATION),
            paths.repo_root,
        ),
        VerificationStep(
            "initialize-ui-evidence",
            python_tool(
                paths, "developer_preview_ui_evidence.py", "init", "--output", str(paths.ui_evidence_dir),
                "--repo-root", str(paths.repo_root), "--source-commit", commit,
                "--tester-alias", tester_alias, "--windows-version", windows_version,
                "--display-scale", str(display_scale)
            ),
            paths.repo_root,
        ),
    )


def automated_gate_commands(paths: VerificationPaths, base_commit: str) -> dict[str, tuple[str, ...]]:
    base_commit = require_commit(base_commit, "Review base")
    return {
        "git-diff-check": ("git", "diff", "--check", base_commit, "HEAD"),
        "local-validation": python_tool(paths, "run_local_validation.py", "--keep-going"),
        "o3de-configure": python_tool(
            paths, "developer_preview.py", "configure", "--repo-root", str(paths.repo_root),
            "--build-dir", str(paths.build_dir)
        ),
        "o3de-build": python_tool(
            paths, "developer_preview.py", "build", "--repo-root", str(paths.repo_root),
            "--build-dir", str(paths.build_dir)
        ),
        "compiled-tests": (
            "ctest", "--test-dir", str(paths.build_dir), "-C", DEFAULT_CONFIGURATION,
            "--output-on-failure", "-R", CATALOG_TEST_PATTERN
        ),
    }


def record_step(paths: VerificationPaths, gate: str, command: Sequence[str], notes: str) -> VerificationStep:
    return VerificationStep(
        gate,
        (*receipt_prefix(paths, "record"), "--name", gate, "--notes", notes, "--", *command),
        paths.repo_root,
    )


def automated_steps(
    paths: VerificationPaths, base_commit: str, *, recorded: dict[str, str] | None = None
) -> tuple[VerificationStep, ...]:
    recorded = recorded or {}
    commands = automated_gate_commands(paths, base_commit)
    notes = {
        "git-diff-check": "Committed whitespace check for the synchronized review range.",
        "local-validation": "Authoritative repository-owned static and Python validation.",
        "o3de-configure": "Approved Windows x64 Developer Preview configure command.",
        "o3de-build": "Profile Editor and TG SDK compiled-test target build.",
        "compiled-tests": "Production-linked TG SDK Catalog.Tests CTest run.",
    }
    steps = [prerequisites_step(paths, "prerequisites-recheck")]
    for gate in AUTOMATED_GATE_NAMES:
        status = recorded.get(gate)
        if status == "passed":
            continue
        if status is not None:
            raise VerificationError(f"Receipt already records {gate} as {status}; initialize a new receipt.")
        steps.append(record_step(paths, gate, commands[gate], notes[gate]))
    return tuple(steps)


def finalize_steps(
    paths: VerificationPaths, commit: str, *, recorded: dict[str, str], receipt_finalized: bool
) -> tuple[VerificationStep, ...]:
    missing = [gate for gate in AUTOMATED_GATE_NAMES if recorded.get(gate) != "passed"]
    if missing:
        raise VerificationError("Automated receipt gates must pass before Windows UI finalization: " + ", ".join(missing))
    windows_status = recorded.get(WINDOWS_UI_GATE)
    if windows_status not in (None, "passed"):
        raise VerificationError(f"Receipt records windows-ui as {windows_status}; initialize a new receipt.")
    steps: list[VerificationStep] = []
    if windows_status is None:
        steps.append(record_step(paths, WINDOWS_UI_GATE, ui_verify_command(paths, commit),
                                 "Verified twenty-two-pane Windows x64 Profile UI evidence."))
    else:
        steps.append(VerificationStep("verify-ui-evidence", ui_verify_command(paths, commit), paths.repo_root))
    if not receipt_finalized:
        steps.append(VerificationStep(
            "finalize-receipt", (*receipt_prefix(paths, "finalize"), "--expected-commit", commit), paths.repo_root
        ))
    steps.extend((
        VerificationStep("verify-receipt", receipt_verify_command(paths, commit, merge_ready=True), paths.repo_root),
        VerificationStep(
            "summarize-receipt",
            (*receipt_prefix(paths, "summarize"), "--expected-commit", commit, "--require-merge-ready"),
            paths.repo_root,
        ),
    ))
    return tuple(steps)


def execute_steps(
    steps: Sequence[VerificationStep], *, dry_run: bool,
    executor: ProcessExecutor = default_executor
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
        results.append(StepResult(step.name, "passed" if exit_code == 0 else "failed", exit_code, step.command))
        if exit_code:
            return results, exit_code
    return results, 0


def read_receipt(paths: VerificationPaths) -> tuple[dict, dict[str, str]]:
    path = paths.receipt_dir / RECEIPT_DOCUMENT
    if not path.is_file():
        return {}, {}
    try:
        receipt = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise VerificationError(f"Unable to read {path}: {exc}") from exc
    if not isinstance(receipt, dict):
        raise VerificationError(f"Expected a JSON object: {path}")
    statuses: dict[str, str] = {}
    gates = receipt.get("gates", [])
    if isinstance(gates, list):
        for gate in gates:
            if isinstance(gate, dict) and isinstance(gate.get("name"), str):
                statuses[gate["name"]] = str(gate.get("status", "unknown"))
    return receipt, statuses


def probe(name: str, command: Sequence[str], cwd: Path, runner: ProbeRunner) -> dict:
    code, output = runner(command, cwd)
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    return {
        "name": name,
        "checked": True,
        "valid": code == 0,
        "exit_code": code,
        "summary": (lines[-1] if lines else "")[:512],
    }


def probe_receipt(
    paths: VerificationPaths, commit: str, *, merge_ready: bool, runner: ProbeRunner
) -> dict:
    if not (paths.receipt_dir / RECEIPT_DOCUMENT).is_file():
        return {"name": "receipt", "checked": False, "valid": False, "exit_code": None, "summary": "Receipt is missing."}
    return probe(
        "merge-ready-receipt" if merge_ready else "receipt-integrity",
        receipt_verify_command(paths, commit, merge_ready=merge_ready), paths.repo_root, runner
    )


def probe_ui_evidence(paths: VerificationPaths, commit: str, *, runner: ProbeRunner) -> dict:
    if not (paths.ui_evidence_dir / UI_EVIDENCE_DOCUMENT).is_file():
        return {"name": "ui-evidence", "checked": False, "valid": False, "exit_code": None, "summary": "UI evidence is missing."}
    return probe("ui-evidence", ui_verify_command(paths, commit), paths.repo_root, runner)


def next_action(
    commit: str, receipt: dict, gates: dict[str, str], receipt_probe: dict,
    merge_ready_probe: dict, ui_probe: dict
) -> str:
    if not receipt:
        return "prepare"
    if receipt.get("source_commit") != commit:
        return "initialize a new receipt for the current exact head"
    if not receipt_probe["valid"]:
        return "replace the invalid or tampered receipt with a new exact-head receipt"
    if any(status == "failed" for status in gates.values()):
        return "fix the failed gate and initialize a new exact-head receipt"
    if any(gates.get(gate) != "passed" for gate in AUTOMATED_GATE_NAMES):
        return "automated"
    windows_status = gates.get(WINDOWS_UI_GATE)
    if windows_status is None:
        return "finalize" if ui_probe["valid"] else "complete the manual Windows checklist, screenshots, and attestation"
    if windows_status != "passed":
        return "initialize a new receipt; the Windows UI gate is not an executed pass"
    if not ui_probe["valid"]:
        return "replace invalid Windows UI evidence and initialize a new receipt"
    if receipt.get("finalized_at_utc") is None:
        return "finalize"
    return "complete" if merge_ready_probe["valid"] else "replace the invalid or tampered finalized receipt"


def state_payload(
    paths: VerificationPaths, commit: str, base_ref: str, base_commit: str,
    synchronized_refs: dict[str, str], *, results: Sequence[StepResult] = (),
    runner: ProbeRunner = capture_command
) -> dict:
    receipt, gates = read_receipt(paths)
    receipt_probe = probe_receipt(paths, commit, merge_ready=False, runner=runner)
    finalized = bool(receipt and receipt.get("finalized_at_utc") is not None)
    merge_ready_probe = probe_receipt(paths, commit, merge_ready=True, runner=runner) if finalized else {
        "name": "merge-ready-receipt", "checked": False, "valid": False,
        "exit_code": None, "summary": "Receipt is not finalized."
    }
    ui_probe = probe_ui_evidence(paths, commit, runner=runner)
    return {
        "schema_version": SCHEMA_VERSION,
        "source_commit": commit,
        "review_base": {"ref": base_ref, "commit": base_commit},
        "synchronized_refs": synchronized_refs,
        "paths": {
            "repo_root": str(paths.repo_root), "build_dir": str(paths.build_dir),
            "receipt_dir": str(paths.receipt_dir), "ui_evidence_dir": str(paths.ui_evidence_dir),
            "prerequisites_result": str(prerequisites_path(paths)),
        },
        "receipt": {
            "exists": bool(receipt), "source_commit": receipt.get("source_commit") if receipt else None,
            "finalized_at_utc": receipt.get("finalized_at_utc") if receipt else None,
            "gates": gates, "integrity": receipt_probe,
            "merge_ready_verification": merge_ready_probe,
        },
        "ui_evidence": {
            "exists": (paths.ui_evidence_dir / UI_EVIDENCE_DOCUMENT).is_file(),
            "verification": ui_probe,
        },
        "last_results": [asdict(result) for result in results],
        "next_action": next_action(commit, receipt, gates, receipt_probe, merge_ready_probe, ui_probe),
    }


def atomic_write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", newline="\n", prefix=f".{path.name}.",
        suffix=".tmp", dir=path.parent, delete=False
    ) as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(path)


def print_status(payload: dict) -> None:
    print("\n## Developer Preview exact-head verification")
    print(f"- Commit: `{payload['source_commit']}`")
    print(f"- Review base: `{payload['review_base']['ref']}` at `{payload['review_base']['commit']}`")
    print(f"- Receipt: {payload['paths']['receipt_dir']}")
    print(f"- UI evidence: {payload['paths']['ui_evidence_dir']}")
    print(f"- Next action: {payload['next_action']}")
    receipt = payload["receipt"]
    print("- Receipt integrity: " + ("verified" if receipt["integrity"]["valid"] else "not verified"))
    print("- Merge-ready receipt: " + ("verified" if receipt["merge_ready_verification"]["valid"] else "not verified"))
    print("- UI evidence: " + ("verified" if payload["ui_evidence"]["verification"]["valid"] else "not verified"))
    print("\n| Gate | Status |\n|---|---:|")
    for gate in (*AUTOMATED_GATE_NAMES, WINDOWS_UI_GATE):
        print(f"| `{gate}` | {receipt['gates'].get(gate, 'missing')} |")


def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--receipt-dir", type=Path)
    parser.add_argument("--ui-evidence-dir", type=Path)
    parser.add_argument("--state-output", type=Path)
    parser.add_argument("--base-ref", default=DEFAULT_BASE_REF)
    parser.add_argument("--sync-ref", action="append")


def add_identity_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--tester-alias", required=True)
    parser.add_argument("--windows-version", required=True)
    parser.add_argument("--display-scale", type=int, default=100)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name, help_text, identity, dry_run in (
        ("plan", "Print the exact verification commands.", True, False),
        ("prepare", "Run prerequisites and initialize evidence.", True, True),
        ("automated", "Record missing mandatory automated gates.", False, True),
        ("finalize", "Verify UI evidence and finalize or reverify the receipt.", False, True),
        ("status", "Run authoritative integrity probes and report progress.", False, False),
    ):
        command = subparsers.add_parser(name, help=help_text)
        add_common_args(command)
        if identity:
            add_identity_args(command)
        if dry_run:
            command.add_argument("--dry-run", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        repo_root = resolve_path(args.repo_root, Path.cwd()) if args.repo_root else repository_root_from_script()
        commit = repository_state(repo_root)
        base_commit, synchronized_refs = resolve_review_ancestry(
            repo_root, commit, args.base_ref, tuple(args.sync_ref or DEFAULT_SYNC_REFS)
        )
        paths = resolve_paths(args, repo_root, commit)
        if args.command in {"plan", "prepare"}:
            validate_identity_inputs(args.tester_alias, args.windows_version, args.display_scale)

        if args.command == "plan":
            steps = (
                *prepare_steps(paths, commit, tester_alias=args.tester_alias,
                               windows_version=args.windows_version, display_scale=args.display_scale),
                *automated_steps(paths, base_commit),
                *finalize_steps(paths, commit,
                                recorded={gate: "passed" for gate in AUTOMATED_GATE_NAMES},
                                receipt_finalized=False),
            )
            results, code = execute_steps(steps, dry_run=True)
        elif args.command == "prepare":
            if not args.dry_run:
                validate_prepare_targets(paths)
            results, code = execute_steps(
                prepare_steps(paths, commit, tester_alias=args.tester_alias,
                              windows_version=args.windows_version, display_scale=args.display_scale),
                dry_run=args.dry_run,
            )
        elif args.command == "status":
            results, code = [], 0
        else:
            receipt, recorded = read_receipt(paths)
            if not receipt:
                raise VerificationError(f"Validation receipt is missing: {paths.receipt_dir / RECEIPT_DOCUMENT}")
            if receipt.get("source_commit") != commit:
                raise VerificationError("Receipt source commit does not match the current exact HEAD.")
            integrity = probe_receipt(paths, commit, merge_ready=False, runner=capture_command)
            if not integrity["valid"]:
                raise VerificationError("Authoritative receipt verification failed before execution: " + integrity["summary"])
            if args.command == "automated":
                if receipt.get("finalized_at_utc"):
                    raise VerificationError("Finalized receipts are immutable.")
                results, code = execute_steps(
                    automated_steps(paths, base_commit, recorded=recorded), dry_run=args.dry_run
                )
            elif args.command == "finalize":
                results, code = execute_steps(
                    finalize_steps(paths, commit, recorded=recorded,
                                   receipt_finalized=receipt.get("finalized_at_utc") is not None),
                    dry_run=args.dry_run,
                )
            else:
                raise VerificationError(f"Unsupported command: {args.command}")

        payload = state_payload(
            paths, commit, args.base_ref, base_commit, synchronized_refs, results=results
        )
        atomic_write_json(paths.state_path, payload)
        print_status(payload)
        print(f"\nWrote verification status: {paths.state_path}")
        return code
    except (OSError, VerificationError) as exc:
        print(f"Developer Preview exact-head verification failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
