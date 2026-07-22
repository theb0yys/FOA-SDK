#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Coordinate exact-head Developer Preview verification without inventing evidence."""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Sequence

TOOLS_ROOT = Path(__file__).resolve().parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import developer_preview

COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
ALIAS_RE = re.compile(r"^[A-Za-z0-9._-]{2,64}$")
GATES = (
    "git-diff-check",
    "local-validation",
    "o3de-configure",
    "o3de-build",
    "compiled-tests",
)
UI_GATE = "windows-ui"
CONFIGURATION = "profile"
TEST_PATTERN = r"TaintedGrailModdingSDK\.Catalog\.Tests"
RECEIPT_NAME = "validation-receipt.json"


class VerificationError(RuntimeError):
    pass


def invoke(
    command: Sequence[str],
    cwd: Path,
    capture: bool = False,
) -> tuple[int, str]:
    try:
        result = subprocess.run(
            list(command),
            cwd=str(cwd),
            check=False,
            capture_output=capture,
            text=capture,
            encoding="utf-8" if capture else None,
            errors="replace" if capture else None,
        )
    except OSError as exc:
        return 127, str(exc)
    output = (result.stdout + result.stderr).strip() if capture else ""
    return int(result.returncode), output


def shown(command: Sequence[str]) -> str:
    return (
        subprocess.list2cmdline(list(command))
        if os.name == "nt"
        else shlex.join(command)
    )


def resolve(value: Path | None, default: Path) -> Path:
    if value is None:
        return default.resolve(strict=False)
    value = value.expanduser()
    return (
        value if value.is_absolute() else Path.cwd() / value
    ).resolve(strict=False)


def repository(args: argparse.Namespace) -> tuple[Path, str, str]:
    fallback = Path(__file__).resolve().parents[3]
    repo = resolve(args.repo_root, fallback)
    required = (
        repo / ".git",
        repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview.py",
        repo / "Gems/TaintedGrailModdingSDK/Tools/validation_receipt.py",
        repo
        / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_ui_evidence.py",
    )
    if any(not path.exists() for path in required):
        raise VerificationError(
            "Repository is missing exact-head verification files."
        )
    try:
        developer_preview.validate_product_root(repo)
    except RuntimeError as exc:
        raise VerificationError(str(exc)) from exc
    code, head = invoke(
        ("git", "-C", str(repo), "rev-parse", "HEAD"),
        repo,
        True,
    )
    head = head.lower()
    if code or not COMMIT_RE.fullmatch(head):
        raise VerificationError(
            "A full lowercase 40-character Git HEAD is required."
        )
    code, dirty = invoke(
        (
            "git",
            "-C",
            str(repo),
            "status",
            "--porcelain=v1",
            "--untracked-files=all",
        ),
        repo,
        True,
    )
    if code or dirty:
        raise VerificationError(
            "Exact-head verification requires a clean working tree."
        )
    base = args.review_base.strip().lower()
    if not COMMIT_RE.fullmatch(base):
        raise VerificationError(
            "--review-base must be a full lowercase commit SHA."
        )
    code, exact = invoke(
        (
            "git",
            "-C",
            str(repo),
            "rev-parse",
            "--verify",
            f"{base}^{{commit}}",
        ),
        repo,
        True,
    )
    if code or exact.lower() != base or base == head:
        raise VerificationError(
            "--review-base must identify a different exact repository commit."
        )
    code, _ = invoke(
        ("git", "-C", str(repo), "merge-base", "--is-ancestor", base, head),
        repo,
    )
    if code:
        raise VerificationError("--review-base must be an ancestor of HEAD.")
    return repo, head, base


def paths(
    args: argparse.Namespace,
    repo: Path,
    head: str,
) -> dict[str, Path]:
    short = head[:12]
    lock = developer_preview.load_engine_lock(repo)
    engine = (
        resolve(args.engine_root, Path.cwd())
        if args.engine_root is not None
        else developer_preview.default_engine_root(repo, lock)
    )
    build_default = developer_preview.default_build_directory(repo)
    evidence_root = repo.parent / "foa-validation" / short
    values = {
        "repo": repo,
        "engine": engine,
        "build": resolve(args.build_dir, build_default),
        "receipt": resolve(
            args.receipt_dir,
            evidence_root / "receipt",
        ),
        "ui": resolve(
            args.ui_evidence_dir,
            evidence_root / "ui",
        ),
        "state": resolve(
            args.verification_dir,
            evidence_root / "state",
        ),
    }
    try:
        developer_preview.validate_engine_root(engine, lock)
        developer_preview.validate_engine_pin(engine, lock)
    except RuntimeError as exc:
        raise VerificationError(str(exc)) from exc
    for key in ("receipt", "ui", "state"):
        try:
            values[key].relative_to(repo)
        except ValueError:
            pass
        else:
            raise VerificationError(
                "Receipt and verification evidence must be outside the FOA-SDK checkout."
            )
        try:
            values[key].relative_to(values["build"])
        except ValueError:
            continue
        raise VerificationError(
            "Receipt and verification evidence must be outside the O3DE build directory."
        )
    return values


def tool(
    p: dict[str, Path],
    name: str,
    *args: str,
) -> tuple[str, ...]:
    script = p["repo"] / "Gems/TaintedGrailModdingSDK/Tools" / name
    return (sys.executable, str(script), *args)


def receipt(
    p: dict[str, Path],
    action: str,
    *args: str,
) -> tuple[str, ...]:
    return tool(
        p,
        "validation_receipt.py",
        "--repo-root",
        str(p["repo"]),
        action,
        "--output",
        str(p["receipt"]),
        *args,
    )


def receipt_verify(
    p: dict[str, Path],
    head: str,
    ready: bool,
) -> tuple[str, ...]:
    command = list(receipt(p, "verify", "--expected-commit", head))
    if ready:
        command.append("--require-merge-ready")
    return tuple(command)


def ui_verify(p: dict[str, Path], head: str) -> tuple[str, ...]:
    return tool(
        p,
        "developer_preview_ui_evidence.py",
        "verify",
        "--output",
        str(p["ui"]),
        "--expected-commit",
        head,
    )


def prerequisites(p: dict[str, Path]) -> tuple[str, ...]:
    return tool(
        p,
        "developer_preview.py",
        "prerequisites",
        "--product-root",
        str(p["repo"]),
        "--engine-root",
        str(p["engine"]),
        "--build-dir",
        str(p["build"]),
        "--json-output",
        str(p["state"] / "prerequisites.json"),
    )


def gate_commands(
    p: dict[str, Path],
    base: str,
) -> dict[str, tuple[str, ...]]:
    return {
        "git-diff-check": ("git", "diff", "--check", base, "HEAD"),
        "local-validation": tool(
            p,
            "run_local_validation.py",
            "--keep-going",
            "--static-only",
            "--engine-root",
            str(p["engine"]),
        ),
        "o3de-configure": tool(
            p,
            "developer_preview.py",
            "configure",
            "--product-root",
            str(p["repo"]),
            "--engine-root",
            str(p["engine"]),
            "--build-dir",
            str(p["build"]),
        ),
        "o3de-build": tool(
            p,
            "developer_preview.py",
            "build",
            "--product-root",
            str(p["repo"]),
            "--engine-root",
            str(p["engine"]),
            "--build-dir",
            str(p["build"]),
        ),
        "compiled-tests": (
            "ctest",
            "--test-dir",
            str(p["build"]),
            "-C",
            CONFIGURATION,
            "--output-on-failure",
            "--no-tests=error",
            "-R",
            TEST_PATTERN,
        ),
    }


def recorded(
    p: dict[str, Path],
    name: str,
    command: Sequence[str],
    notes: str,
) -> tuple[str, ...]:
    return receipt(
        p,
        "record",
        "--name",
        name,
        "--notes",
        notes,
        "--",
        *command,
    )


def receipt_metadata(
    p: dict[str, Path],
) -> tuple[dict, dict[str, str]]:
    path = p["receipt"] / RECEIPT_NAME
    if not path.is_file():
        return {}, {}
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise VerificationError(
            f"Unable to read receipt metadata: {exc}"
        ) from exc
    if not isinstance(document, dict):
        raise VerificationError("Receipt metadata root must be an object.")
    states: dict[str, str] = {}
    gates = document.get("gates", [])
    for entry in gates if isinstance(gates, list) else []:
        if isinstance(entry, dict) and isinstance(entry.get("name"), str):
            states[entry["name"]] = str(entry.get("status", "unknown"))
    return document, states


def range_recorded(document: dict, base: str) -> bool:
    gates = (
        document.get("gates", [])
        if isinstance(document.get("gates"), list)
        else []
    )
    rows = [
        row
        for row in gates
        if isinstance(row, dict) and row.get("name") == "git-diff-check"
    ]
    expected = shlex.join(("git", "diff", "--check", base, "HEAD"))
    return len(rows) == 1 and rows[0].get("command") == expected


def execute(
    steps: Sequence[tuple[str, Sequence[str]]],
    repo: Path,
    dry_run: bool,
    keep_going: bool = False,
) -> int:
    failure = 0
    for name, command in steps:
        print(f"\n=== {name} ===\n+ {shown(command)}")
        if dry_run:
            continue
        code, _ = invoke(command, repo)
        if code:
            failure = failure or code
            if not keep_going:
                break
    return failure


def prepare_steps(
    p: dict[str, Path],
    head: str,
    args: argparse.Namespace,
) -> list[tuple[str, Sequence[str]]]:
    return [
        ("prerequisites", prerequisites(p)),
        (
            "initialize-receipt",
            receipt(
                p,
                "init",
                "--tester-alias",
                args.tester_alias,
                "--platform",
                f"{args.windows_version} x64",
                "--configuration",
                CONFIGURATION,
            ),
        ),
        (
            "initialize-ui-evidence",
            tool(
                p,
                "developer_preview_ui_evidence.py",
                "init",
                "--output",
                str(p["ui"]),
                "--repo-root",
                str(p["repo"]),
                "--source-commit",
                head,
                "--tester-alias",
                args.tester_alias,
                "--windows-version",
                args.windows_version,
                "--display-scale",
                str(args.display_scale),
            ),
        ),
    ]


def automated_steps(
    p: dict[str, Path],
    base: str,
    states: dict[str, str],
) -> list[tuple[str, Sequence[str]]]:
    if states:
        raise VerificationError(
            "Automated recording requires a new receipt with no recorded gates."
        )
    commands = gate_commands(p, base)
    steps: list[tuple[str, Sequence[str]]] = [
        ("prerequisites-recheck", prerequisites(p))
    ]
    for gate in GATES:
        notes = (
            f"Reviewed range {base}..HEAD."
            if gate == "git-diff-check"
            else gate
        )
        steps.append((gate, recorded(p, gate, commands[gate], notes)))
    return steps


def final_steps(
    p: dict[str, Path],
    head: str,
    states: dict[str, str],
    finalized: bool,
) -> list[tuple[str, Sequence[str]]]:
    verify = [
        ("verify-receipt", receipt_verify(p, head, True)),
        ("verify-ui-evidence", ui_verify(p, head)),
        (
            "summarize-receipt",
            receipt(
                p,
                "summarize",
                "--expected-commit",
                head,
                "--require-merge-ready",
            ),
        ),
    ]
    if finalized:
        return verify
    missing = [gate for gate in GATES if states.get(gate) != "passed"]
    if missing:
        raise VerificationError(
            "Automated gates must pass before finalization: "
            + ", ".join(missing)
        )
    steps: list[tuple[str, Sequence[str]]] = []
    if states.get(UI_GATE) is None:
        steps.append(
            (
                UI_GATE,
                recorded(
                    p,
                    UI_GATE,
                    ui_verify(p, head),
                    "Verified Windows UI evidence.",
                ),
            )
        )
    elif states.get(UI_GATE) != "passed":
        raise VerificationError(
            "windows-ui is not passed; initialize a new receipt."
        )
    steps.append(
        (
            "finalize-receipt",
            receipt(p, "finalize", "--expected-commit", head),
        )
    )
    return steps + verify


def status(
    p: dict[str, Path],
    head: str,
    base: str,
) -> dict[str, object]:
    document, states = receipt_metadata(p)
    receipt_code, receipt_error = (
        invoke(receipt_verify(p, head, True), p["repo"], True)
        if document
        else (1, "missing")
    )
    ui_path = p["ui"] / "ui-evidence.json"
    ui_code, ui_error = (
        invoke(ui_verify(p, head), p["repo"], True)
        if ui_path.is_file()
        else (1, "missing")
    )
    range_ok = receipt_code == 0 and range_recorded(document, base)
    return {
        "head": head,
        "review_base": base,
        "receipt_metadata_unverified": states,
        "receipt_merge_ready_verified": receipt_code == 0,
        "ui_evidence_verified": ui_code == 0,
        "review_range_verified": range_ok,
        "complete_verified": receipt_code == 0 and ui_code == 0 and range_ok,
        "receipt_error": "" if receipt_code == 0 else receipt_error[-4096:],
        "ui_error": "" if ui_code == 0 else ui_error[-4096:],
    }


def status_exit_code(report: dict[str, object]) -> int:
    return 0 if report.get("complete_verified") is True else 1


def build_parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)
    for name in ("plan", "prepare", "automated", "finalize", "status"):
        command = commands.add_parser(name)
        command.add_argument("--repo-root", type=Path)
        command.add_argument("--engine-root", type=Path)
        command.add_argument("--build-dir", type=Path)
        command.add_argument("--receipt-dir", type=Path)
        command.add_argument("--ui-evidence-dir", type=Path)
        command.add_argument("--verification-dir", type=Path)
        command.add_argument("--review-base", required=True)
        if name in {"plan", "prepare"}:
            command.add_argument("--tester-alias", required=True)
            command.add_argument("--windows-version", required=True)
            command.add_argument("--display-scale", type=int, default=100)
        if name in {"prepare", "automated", "finalize"}:
            command.add_argument("--dry-run", action="store_true")
        if name == "status":
            command.add_argument("--json", action="store_true")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        repo, head, base = repository(args)
        p = paths(args, repo, head)
        if args.command in {"plan", "prepare"}:
            if not ALIAS_RE.fullmatch(args.tester_alias.strip()):
                raise VerificationError("Tester alias has an invalid format.")
            if (
                not args.windows_version.strip()
                or not 100 <= args.display_scale <= 200
            ):
                raise VerificationError(
                    "Windows identity metadata is invalid."
                )
        if args.command == "plan":
            fake = {gate: "passed" for gate in GATES}
            steps = (
                prepare_steps(p, head, args)
                + automated_steps(p, base, {})
                + final_steps(p, head, fake, False)
            )
            return execute(steps, repo, True)
        if args.command == "prepare":
            if not args.dry_run:
                for path in (p["receipt"], p["ui"], p["state"]):
                    if path.exists() and (
                        not path.is_dir() or any(path.iterdir())
                    ):
                        raise VerificationError(
                            f"New-run directory must be absent or empty: {path}"
                        )
            return execute(prepare_steps(p, head, args), repo, args.dry_run)
        document, states = receipt_metadata(p)
        if args.command == "status":
            report = status(p, head, base)
            print(
                json.dumps(report, indent=2, sort_keys=True)
                if args.json
                else report
            )
            return status_exit_code(report)
        if not document:
            raise VerificationError(
                f"Receipt is missing: {p['receipt'] / RECEIPT_NAME}"
            )
        code, detail = invoke(receipt_verify(p, head, False), repo, True)
        if code:
            raise VerificationError(
                "Receipt failed authoritative verification: " + detail[-4096:]
            )
        if (
            states.get("git-diff-check") == "passed"
            and not range_recorded(document, base)
        ):
            raise VerificationError(
                "Receipt does not record the requested reviewed range."
            )
        if args.command == "automated":
            if document.get("finalized_at_utc"):
                raise VerificationError("Finalized receipts are immutable.")
            return execute(
                automated_steps(p, base, states),
                repo,
                args.dry_run,
            )
        finalized = bool(document.get("finalized_at_utc"))
        return execute(
            final_steps(p, head, states, finalized),
            repo,
            args.dry_run,
            finalized,
        )
    except (OSError, VerificationError) as exc:
        print(
            f"Developer Preview exact-head verification failed: {exc}",
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
