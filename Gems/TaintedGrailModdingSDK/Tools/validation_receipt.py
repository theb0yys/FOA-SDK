#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Create and verify exact-head local validation receipts for TG SDK reviews."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
RECEIPT_NAME = "validation-receipt.json"
LOG_DIRECTORY = "logs"
SOURCE_COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
SAFE_TOKEN_RE = re.compile(r"^[A-Za-z0-9._-]{1,96}$")
GATE_NAMES = (
    "git-diff-check",
    "local-validation",
    "o3de-configure",
    "o3de-build",
    "compiled-tests",
    "windows-ui",
)
MANDATORY_PASS_GATES = (
    "git-diff-check",
    "local-validation",
)
RISK_ACCEPTABLE_GATES = (
    "o3de-configure",
    "o3de-build",
    "compiled-tests",
    "windows-ui",
)
VALID_STATUSES = ("passed", "failed", "not_run")


class ValidationReceiptError(RuntimeError):
    pass


def parse_utc(value: str, field: str) -> datetime:
    if not value.endswith("Z"):
        raise ValidationReceiptError(f"{field} must use UTC with a trailing Z.")
    try:
        parsed = datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as exc:
        raise ValidationReceiptError(f"{field} is not a valid UTC timestamp.") from exc
    if parsed.tzinfo != timezone.utc:
        raise ValidationReceiptError(f"{field} must resolve to UTC.")
    return parsed


def require_token(value: str, field: str) -> str:
    if not SAFE_TOKEN_RE.fullmatch(value):
        raise ValidationReceiptError(
            f"{field} must contain only letters, numbers, dot, underscore, or dash."
        )
    return value


def require_commit(value: str) -> str:
    if not SOURCE_COMMIT_RE.fullmatch(value):
        raise ValidationReceiptError(
            "source commit must be exactly 40 lowercase hexadecimal characters."
        )
    return value


def resolve_output(path: Path, *, create: bool) -> Path:
    output = path.expanduser().resolve()
    if create:
        output.mkdir(parents=True, exist_ok=True)
    if not output.is_dir():
        raise ValidationReceiptError(f"Output directory does not exist: {output}")
    if output.is_symlink():
        raise ValidationReceiptError("Output directory must not be a symlink.")
    return output


def receipt_path(output: Path) -> Path:
    return output / RECEIPT_NAME


def load_receipt(output: Path) -> dict[str, Any]:
    path = receipt_path(output)
    if not path.is_file() or path.is_symlink():
        raise ValidationReceiptError(f"Receipt is missing or unsafe: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValidationReceiptError(f"Unable to read receipt: {exc}") from exc
    if not isinstance(value, dict):
        raise ValidationReceiptError("Receipt root must be a JSON object.")
    return value


def write_receipt(output: Path, receipt: dict[str, Any]) -> None:
    path = receipt_path(output)
    temporary = path.with_suffix(".json.tmp")
    encoded = json.dumps(receipt, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    temporary.write_text(encoded, encoding="utf-8")
    temporary.replace(path)


def hash_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    total = 0
    with path.open("rb") as stream:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            total += len(chunk)
    return f"sha256:{digest.hexdigest()}", total


def safe_log_name(gate: str, stream_name: str) -> str:
    require_token(gate, "gate")
    return f"{gate}.{stream_name}.log"


def capture_log(
    output: Path,
    gate: str,
    stream_name: str,
    source: Path | None,
) -> dict[str, Any] | None:
    if source is None:
        return None
    source_path = source.expanduser().resolve()
    if not source_path.is_file() or source_path.is_symlink():
        raise ValidationReceiptError(
            f"{stream_name} log must be a regular non-symlink file: {source_path}"
        )
    logs = output / LOG_DIRECTORY
    logs.mkdir(parents=True, exist_ok=True)
    destination = logs / safe_log_name(gate, stream_name)
    shutil.copyfile(source_path, destination)
    digest, byte_count = hash_file(destination)
    return {
        "path": destination.relative_to(output).as_posix(),
        "sha256": digest,
        "bytes": byte_count,
    }


def find_gate(receipt: dict[str, Any], gate: str) -> dict[str, Any] | None:
    for entry in receipt.get("gates", []):
        if entry.get("name") == gate:
            return entry
    return None


def find_acceptance(receipt: dict[str, Any], gate: str) -> dict[str, Any] | None:
    for entry in receipt.get("risk_acceptances", []):
        if entry.get("gate") == gate:
            return entry
    return None


def validate_log_reference(output: Path, value: Any, field: str) -> None:
    if value is None:
        return
    if not isinstance(value, dict):
        raise ValidationReceiptError(f"{field} must be an object or null.")
    relative = value.get("path")
    digest = value.get("sha256")
    expected_bytes = value.get("bytes")
    if not isinstance(relative, str) or not relative:
        raise ValidationReceiptError(f"{field}.path is required.")
    candidate = (output / relative).resolve()
    try:
        candidate.relative_to(output)
    except ValueError as exc:
        raise ValidationReceiptError(f"{field}.path escapes the receipt directory.") from exc
    if not candidate.is_file() or candidate.is_symlink():
        raise ValidationReceiptError(f"{field}.path is missing or unsafe.")
    actual_digest, actual_bytes = hash_file(candidate)
    if digest != actual_digest or expected_bytes != actual_bytes:
        raise ValidationReceiptError(f"{field} hash or byte count does not match.")


def validate_receipt(
    output: Path,
    receipt: dict[str, Any],
    *,
    expected_commit: str | None,
    require_merge_ready: bool,
) -> None:
    if receipt.get("schema_version") != SCHEMA_VERSION:
        raise ValidationReceiptError(
            f"Unsupported receipt schema: {receipt.get('schema_version')!r}"
        )
    source_commit = require_commit(str(receipt.get("source_commit", "")))
    if expected_commit is not None and source_commit != require_commit(expected_commit):
        raise ValidationReceiptError(
            f"Receipt commit {source_commit} does not match {expected_commit}."
        )
    require_token(str(receipt.get("tester_alias", "")), "tester alias")
    if not isinstance(receipt.get("platform"), str) or not receipt["platform"].strip():
        raise ValidationReceiptError("platform is required.")
    if not isinstance(receipt.get("configuration"), str) or not receipt[
        "configuration"
    ].strip():
        raise ValidationReceiptError("configuration is required.")
    created_at = parse_utc(str(receipt.get("created_at_utc", "")), "created_at_utc")

    gates = receipt.get("gates")
    if not isinstance(gates, list):
        raise ValidationReceiptError("gates must be an array.")
    seen: set[str] = set()
    latest_time = created_at
    for entry in gates:
        if not isinstance(entry, dict):
            raise ValidationReceiptError("Every gate entry must be an object.")
        name = str(entry.get("name", ""))
        if name not in GATE_NAMES:
            raise ValidationReceiptError(f"Unknown validation gate: {name!r}")
        if name in seen:
            raise ValidationReceiptError(f"Duplicate validation gate: {name}")
        seen.add(name)
        status = entry.get("status")
        if status not in VALID_STATUSES:
            raise ValidationReceiptError(f"{name} has invalid status {status!r}.")
        command = entry.get("command")
        if not isinstance(command, str) or not command.strip():
            raise ValidationReceiptError(f"{name} must record the exact command.")
        notes = entry.get("notes")
        if not isinstance(notes, str):
            raise ValidationReceiptError(f"{name}.notes must be text.")
        exit_code = entry.get("exit_code")
        started = entry.get("started_at_utc")
        finished = entry.get("finished_at_utc")
        if status == "not_run":
            if exit_code is not None or started is not None or finished is not None:
                raise ValidationReceiptError(
                    f"{name} not_run state must not invent exit or timing data."
                )
            if not notes.strip():
                raise ValidationReceiptError(f"{name} not_run requires a reason.")
        else:
            if not isinstance(exit_code, int):
                raise ValidationReceiptError(f"{name} requires an integer exit code.")
            if status == "passed" and exit_code != 0:
                raise ValidationReceiptError(f"{name} passed but exit code is non-zero.")
            if status == "failed" and exit_code == 0:
                raise ValidationReceiptError(f"{name} failed but exit code is zero.")
            start_time = parse_utc(str(started or ""), f"{name}.started_at_utc")
            finish_time = parse_utc(str(finished or ""), f"{name}.finished_at_utc")
            if finish_time < start_time:
                raise ValidationReceiptError(f"{name} finished before it started.")
            latest_time = max(latest_time, finish_time)
        validate_log_reference(output, entry.get("stdout_log"), f"{name}.stdout_log")
        validate_log_reference(output, entry.get("stderr_log"), f"{name}.stderr_log")

    acceptances = receipt.get("risk_acceptances")
    if not isinstance(acceptances, list):
        raise ValidationReceiptError("risk_acceptances must be an array.")
    accepted_gates: set[str] = set()
    for entry in acceptances:
        if not isinstance(entry, dict):
            raise ValidationReceiptError("Every risk acceptance must be an object.")
        gate = entry.get("gate")
        if gate not in RISK_ACCEPTABLE_GATES:
            raise ValidationReceiptError(f"Risk acceptance cannot cover {gate!r}.")
        if gate in accepted_gates:
            raise ValidationReceiptError(f"Duplicate risk acceptance for {gate}.")
        accepted_gates.add(str(gate))
        require_token(str(entry.get("maintainer_alias", "")), "maintainer alias")
        accepted_at = parse_utc(
            str(entry.get("accepted_at_utc", "")),
            f"{gate}.accepted_at_utc",
        )
        latest_time = max(latest_time, accepted_at)
        rationale = entry.get("rationale")
        if not isinstance(rationale, str) or not rationale.strip():
            raise ValidationReceiptError(f"{gate} risk acceptance requires rationale.")

    finalized = receipt.get("finalized_at_utc")
    if finalized is not None:
        finalized_time = parse_utc(str(finalized), "finalized_at_utc")
        if finalized_time < latest_time:
            raise ValidationReceiptError("Receipt finalized before recorded evidence.")

    if not require_merge_ready:
        return
    if finalized is None:
        raise ValidationReceiptError("Merge-ready verification requires finalization.")
    missing = [gate for gate in GATE_NAMES if gate not in seen]
    if missing:
        raise ValidationReceiptError(
            "Merge-ready receipt is missing gates: " + ", ".join(missing)
        )
    failed = [
        gate
        for gate in GATE_NAMES
        if find_gate(receipt, gate) and find_gate(receipt, gate)["status"] == "failed"
    ]
    if failed:
        raise ValidationReceiptError("Failed gates remain: " + ", ".join(failed))
    for gate in MANDATORY_PASS_GATES:
        entry = find_gate(receipt, gate)
        if entry is None or entry.get("status") != "passed":
            raise ValidationReceiptError(f"{gate} must pass; it cannot be waived.")
    for gate in RISK_ACCEPTABLE_GATES:
        entry = find_gate(receipt, gate)
        if entry is None:
            raise ValidationReceiptError(f"{gate} must be recorded.")
        if entry.get("status") == "passed":
            continue
        if entry.get("status") != "not_run" or find_acceptance(receipt, gate) is None:
            raise ValidationReceiptError(
                f"{gate} must pass or have explicit maintainer risk acceptance."
            )


def initialize(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=True)
    path = receipt_path(output)
    if path.exists() and not args.force:
        raise ValidationReceiptError(
            f"Receipt already exists: {path}; use --force to replace it."
        )
    receipt = {
        "schema_version": SCHEMA_VERSION,
        "source_commit": require_commit(args.source_commit),
        "tester_alias": require_token(args.tester_alias, "tester alias"),
        "platform": args.platform.strip(),
        "configuration": args.configuration.strip(),
        "created_at_utc": args.created_at,
        "gates": [],
        "risk_acceptances": [],
        "finalized_at_utc": None,
    }
    parse_utc(args.created_at, "created_at_utc")
    if not receipt["platform"] or not receipt["configuration"]:
        raise ValidationReceiptError("platform and configuration are required.")
    write_receipt(output, receipt)
    print(path)
    return 0


def record_gate(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    if receipt.get("finalized_at_utc") is not None:
        raise ValidationReceiptError("Finalized receipts are immutable.")
    if args.name not in GATE_NAMES:
        raise ValidationReceiptError(f"Unknown gate: {args.name}")
    status = args.status.replace("-", "_")
    if status not in VALID_STATUSES:
        raise ValidationReceiptError(f"Unknown status: {args.status}")
    exit_code = args.exit_code
    if status == "passed" and exit_code != 0:
        raise ValidationReceiptError("passed requires --exit-code 0.")
    if status == "failed" and (exit_code is None or exit_code == 0):
        raise ValidationReceiptError("failed requires a non-zero --exit-code.")
    if status == "not_run" and exit_code is not None:
        raise ValidationReceiptError("not-run must not include --exit-code.")
    started = args.started_at
    finished = args.finished_at
    if status == "not_run":
        if started or finished:
            raise ValidationReceiptError("not-run must not include timestamps.")
        if not args.notes.strip():
            raise ValidationReceiptError("not-run requires --notes with the reason.")
    else:
        if not started or not finished:
            raise ValidationReceiptError("executed gates require start and finish times.")
        if parse_utc(finished, "finished_at_utc") < parse_utc(
            started, "started_at_utc"
        ):
            raise ValidationReceiptError("finish time precedes start time.")
    entry = {
        "name": args.name,
        "command": args.command,
        "status": status,
        "exit_code": exit_code,
        "started_at_utc": started,
        "finished_at_utc": finished,
        "notes": args.notes,
        "stdout_log": capture_log(output, args.name, "stdout", args.stdout_log),
        "stderr_log": capture_log(output, args.name, "stderr", args.stderr_log),
    }
    gates = [item for item in receipt.get("gates", []) if item.get("name") != args.name]
    gates.append(entry)
    gates.sort(key=lambda item: GATE_NAMES.index(item["name"]))
    receipt["gates"] = gates
    write_receipt(output, receipt)
    return 0


def accept_risk(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    if receipt.get("finalized_at_utc") is not None:
        raise ValidationReceiptError("Finalized receipts are immutable.")
    if args.gate not in RISK_ACCEPTABLE_GATES:
        raise ValidationReceiptError(f"Risk cannot be accepted for {args.gate}.")
    gate = find_gate(receipt, args.gate)
    if gate is None or gate.get("status") != "not_run":
        raise ValidationReceiptError(
            "Risk acceptance is allowed only after recording the gate as not_run."
        )
    parse_utc(args.accepted_at, "accepted_at_utc")
    acceptance = {
        "gate": args.gate,
        "maintainer_alias": require_token(args.maintainer_alias, "maintainer alias"),
        "accepted_at_utc": args.accepted_at,
        "rationale": args.rationale.strip(),
    }
    if not acceptance["rationale"]:
        raise ValidationReceiptError("Risk acceptance requires rationale.")
    values = [
        item
        for item in receipt.get("risk_acceptances", [])
        if item.get("gate") != args.gate
    ]
    values.append(acceptance)
    values.sort(key=lambda item: RISK_ACCEPTABLE_GATES.index(item["gate"]))
    receipt["risk_acceptances"] = values
    write_receipt(output, receipt)
    return 0


def finalize(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    receipt["finalized_at_utc"] = args.finalized_at
    validate_receipt(
        output,
        receipt,
        expected_commit=args.expected_commit,
        require_merge_ready=True,
    )
    write_receipt(output, receipt)
    print("Validation receipt finalized and merge-ready.")
    return 0


def verify(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    validate_receipt(
        output,
        receipt,
        expected_commit=args.expected_commit,
        require_merge_ready=args.require_merge_ready,
    )
    print("Validation receipt verified.")
    return 0


def summarize(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    validate_receipt(
        output,
        receipt,
        expected_commit=args.expected_commit,
        require_merge_ready=args.require_merge_ready,
    )
    print("## Exact-head validation receipt")
    print()
    print(f"- Commit: `{receipt['source_commit']}`")
    print(f"- Tester: `{receipt['tester_alias']}`")
    print(f"- Platform: {receipt['platform']}")
    print(f"- Configuration: {receipt['configuration']}")
    print(f"- Finalized: `{receipt.get('finalized_at_utc') or 'no'}`")
    print()
    print("| Gate | Status | Exit | Notes |")
    print("|---|---:|---:|---|")
    for gate_name in GATE_NAMES:
        gate = find_gate(receipt, gate_name)
        if gate is None:
            print(f"| `{gate_name}` | missing | — | — |")
            continue
        exit_value = "—" if gate["exit_code"] is None else str(gate["exit_code"])
        notes = gate["notes"].replace("|", "\\|").replace("\n", " ")
        print(f"| `{gate_name}` | {gate['status']} | {exit_value} | {notes} |")
    if receipt.get("risk_acceptances"):
        print()
        print("Maintainer risk acceptances:")
        for acceptance in receipt["risk_acceptances"]:
            print(
                f"- `{acceptance['gate']}` — `{acceptance['maintainer_alias']}` at "
                f"`{acceptance['accepted_at_utc']}`: {acceptance['rationale']}"
            )
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    init_parser = subparsers.add_parser("init", help="Create a pending receipt.")
    init_parser.add_argument("--output", type=Path, required=True)
    init_parser.add_argument("--source-commit", required=True)
    init_parser.add_argument("--tester-alias", required=True)
    init_parser.add_argument("--platform", required=True)
    init_parser.add_argument("--configuration", required=True)
    init_parser.add_argument("--created-at", required=True)
    init_parser.add_argument("--force", action="store_true")
    init_parser.set_defaults(handler=initialize)

    record_parser = subparsers.add_parser("record", help="Record one validation gate.")
    record_parser.add_argument("--output", type=Path, required=True)
    record_parser.add_argument("--name", required=True, choices=GATE_NAMES)
    record_parser.add_argument("--command", required=True)
    record_parser.add_argument(
        "--status", required=True, choices=("passed", "failed", "not-run")
    )
    record_parser.add_argument("--exit-code", type=int)
    record_parser.add_argument("--started-at")
    record_parser.add_argument("--finished-at")
    record_parser.add_argument("--notes", default="")
    record_parser.add_argument("--stdout-log", type=Path)
    record_parser.add_argument("--stderr-log", type=Path)
    record_parser.set_defaults(handler=record_gate)

    accept_parser = subparsers.add_parser(
        "accept-risk", help="Accept one explicitly recorded not-run gate."
    )
    accept_parser.add_argument("--output", type=Path, required=True)
    accept_parser.add_argument("--gate", required=True, choices=RISK_ACCEPTABLE_GATES)
    accept_parser.add_argument("--maintainer-alias", required=True)
    accept_parser.add_argument("--accepted-at", required=True)
    accept_parser.add_argument("--rationale", required=True)
    accept_parser.set_defaults(handler=accept_risk)

    finalize_parser = subparsers.add_parser(
        "finalize", help="Finalize only when the receipt is merge-ready."
    )
    finalize_parser.add_argument("--output", type=Path, required=True)
    finalize_parser.add_argument("--expected-commit", required=True)
    finalize_parser.add_argument("--finalized-at", required=True)
    finalize_parser.set_defaults(handler=finalize)

    verify_parser = subparsers.add_parser("verify", help="Verify receipt integrity.")
    verify_parser.add_argument("--output", type=Path, required=True)
    verify_parser.add_argument("--expected-commit")
    verify_parser.add_argument("--require-merge-ready", action="store_true")
    verify_parser.set_defaults(handler=verify)

    summary_parser = subparsers.add_parser(
        "summarize", help="Print a Markdown summary for a pull request."
    )
    summary_parser.add_argument("--output", type=Path, required=True)
    summary_parser.add_argument("--expected-commit")
    summary_parser.add_argument("--require-merge-ready", action="store_true")
    summary_parser.set_defaults(handler=summarize)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return int(args.handler(args))
    except (OSError, ValidationReceiptError) as exc:
        print(f"Validation receipt failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
