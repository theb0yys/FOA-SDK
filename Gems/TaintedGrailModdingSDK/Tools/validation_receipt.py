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
import os
import re
import shlex
import stat
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, BinaryIO, Sequence


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
    "o3de-configure",
    "o3de-build",
    "compiled-tests",
)
RISK_ACCEPTABLE_GATES = ("windows-ui",)
VALID_STATUSES = ("passed", "failed", "not_run")
STREAM_CHUNK_BYTES = 64 * 1024
MAXIMUM_GATE_STREAM_BYTES = 256 * 1024 * 1024
LIMIT_FAILURE_EXIT_CODE = 125
PROCESS_TERMINATION_SECONDS = 5.0


class ValidationReceiptError(RuntimeError):
    pass


@dataclass(frozen=True)
class StreamedLog:
    path: Path
    sha256: str
    byte_count: int
    truncated: bool

    def receipt_value(self, output: Path) -> dict[str, Any]:
        return {
            "path": self.path.relative_to(output).as_posix(),
            "sha256": self.sha256,
            "bytes": self.byte_count,
            "truncated": self.truncated,
        }


@dataclass(frozen=True)
class StreamedProcessResult:
    exit_code: int
    stdout: StreamedLog
    stderr: StreamedLog
    limit_exceeded: bool


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


def lexical_absolute(path: Path) -> Path:
    return Path(os.path.abspath(path.expanduser()))


def reject_storage_indirection(path: Path, field: str) -> Path:
    declared = lexical_absolute(path)
    current = Path(declared.anchor)
    for component in declared.parts[1:]:
        current /= component
        if not current.exists():
            continue
        try:
            mode = current.lstat().st_mode
        except OSError as exc:
            raise ValidationReceiptError(f"Unable to inspect {field}: {exc}") from exc
        if stat.S_ISLNK(mode):
            raise ValidationReceiptError(f"{field} must not traverse a symlink: {current}")
    resolved = declared.resolve(strict=False)
    if os.path.normcase(str(declared)) != os.path.normcase(str(resolved)):
        raise ValidationReceiptError(
            f"{field} must not traverse a junction, reparse point, or redirected path."
        )
    return declared


def resolve_output(path: Path, *, create: bool) -> Path:
    output = reject_storage_indirection(path, "Output directory")
    if create:
        output.mkdir(parents=True, exist_ok=True)
        output = reject_storage_indirection(output, "Output directory")
    if not output.is_dir():
        raise ValidationReceiptError(f"Output directory does not exist: {output}")
    return output


def repository_state(
    repo_root: Path,
    *,
    expected_commit: str | None = None,
    require_clean: bool = True,
) -> str:
    repository = reject_storage_indirection(repo_root, "Repository root")
    if not repository.is_dir():
        raise ValidationReceiptError(f"Repository root does not exist: {repository}")
    try:
        commit_result = subprocess.run(
            ["git", "-C", str(repository), "rev-parse", "HEAD"],
            check=False,
            capture_output=True,
            text=True,
        )
        status_result = subprocess.run(
            [
                "git",
                "-C",
                str(repository),
                "status",
                "--porcelain=v1",
                "--untracked-files=all",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        raise ValidationReceiptError(f"Unable to inspect repository state: {exc}") from exc
    if commit_result.returncode != 0 or status_result.returncode != 0:
        raise ValidationReceiptError("Repository HEAD and working-tree state are required.")
    commit = require_commit(commit_result.stdout.strip())
    if expected_commit is not None and commit != require_commit(expected_commit):
        raise ValidationReceiptError(
            f"Repository HEAD {commit} does not match receipt commit {expected_commit}."
        )
    if require_clean and status_result.stdout.strip():
        raise ValidationReceiptError(
            "Validation receipts require a clean working tree at the exact tested HEAD."
        )
    return commit


def receipt_path(output: Path) -> Path:
    return output / RECEIPT_NAME


def load_receipt(output: Path) -> dict[str, Any]:
    path = receipt_path(output)
    reject_storage_indirection(path, "Receipt path")
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
    reject_storage_indirection(path, "Receipt path")
    reject_storage_indirection(temporary, "Temporary receipt path")
    if temporary.exists():
        raise ValidationReceiptError(
            f"Temporary receipt path already exists and will not be overwritten: {temporary}"
        )
    encoded = json.dumps(receipt, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    with temporary.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


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
    require_token(stream_name, "stream name")
    return f"{gate}.{stream_name}.log"


def prepare_log_destination(output: Path, gate: str, stream_name: str) -> Path:
    logs = output / LOG_DIRECTORY
    if not logs.exists():
        logs.mkdir(parents=False, exist_ok=False)
    reject_storage_indirection(logs, "Receipt log directory")
    destination = logs / safe_log_name(gate, stream_name)
    reject_storage_indirection(destination, f"{stream_name} log destination")
    if destination.exists():
        raise ValidationReceiptError(
            f"Log destination already exists and will not be overwritten: {destination}"
        )
    return destination


def open_exclusive_binary(path: Path) -> BinaryIO:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags, 0o600)
    return os.fdopen(descriptor, "wb")


def _terminate_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    try:
        process.terminate()
    except OSError:
        return


def _stream_pipe(
    pipe: BinaryIO,
    destination: BinaryIO,
    destination_path: Path,
    maximum_bytes: int,
    exceeded: threading.Event,
    process: subprocess.Popen[bytes],
    errors: list[BaseException],
) -> StreamedLog:
    digest = hashlib.sha256()
    total = 0
    truncated = False
    try:
        while True:
            chunk = pipe.read(STREAM_CHUNK_BYTES)
            if not chunk:
                break
            remaining = maximum_bytes - total
            if remaining > 0:
                accepted = chunk[:remaining]
                destination.write(accepted)
                digest.update(accepted)
                total += len(accepted)
            if len(chunk) > max(remaining, 0):
                truncated = True
                exceeded.set()
                _terminate_process(process)
        destination.flush()
        os.fsync(destination.fileno())
    except BaseException as exc:  # propagate thread failures to the recorder
        errors.append(exc)
        exceeded.set()
        _terminate_process(process)
    finally:
        try:
            pipe.close()
        except OSError:
            pass
        try:
            destination.close()
        except OSError:
            pass
    return StreamedLog(
        destination_path,
        f"sha256:{digest.hexdigest()}",
        total,
        truncated,
    )


def stream_gate_process(
    command: Sequence[str],
    cwd: Path,
    stdout_path: Path,
    stderr_path: Path,
    *,
    maximum_stream_bytes: int = MAXIMUM_GATE_STREAM_BYTES,
) -> StreamedProcessResult:
    if maximum_stream_bytes <= 0:
        raise ValidationReceiptError("maximum stream bytes must be positive.")

    stdout_file = open_exclusive_binary(stdout_path)
    try:
        stderr_file = open_exclusive_binary(stderr_path)
    except BaseException:
        stdout_file.close()
        stdout_path.unlink(missing_ok=True)
        raise

    try:
        process = subprocess.Popen(
            list(command),
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.DEVNULL,
        )
    except OSError as exc:
        stdout_file.close()
        stderr_file.close()
        stdout_path.unlink(missing_ok=True)
        stderr_path.unlink(missing_ok=True)
        raise ValidationReceiptError(f"Unable to execute validation gate: {exc}") from exc

    assert process.stdout is not None
    assert process.stderr is not None
    exceeded = threading.Event()
    errors: list[BaseException] = []
    results: dict[str, StreamedLog] = {}

    def capture(name: str, pipe: BinaryIO, destination: BinaryIO, path: Path) -> None:
        results[name] = _stream_pipe(
            pipe,
            destination,
            path,
            maximum_stream_bytes,
            exceeded,
            process,
            errors,
        )

    threads = (
        threading.Thread(
            target=capture,
            args=("stdout", process.stdout, stdout_file, stdout_path),
            daemon=True,
        ),
        threading.Thread(
            target=capture,
            args=("stderr", process.stderr, stderr_file, stderr_path),
            daemon=True,
        ),
    )
    for thread in threads:
        thread.start()

    termination_started: float | None = None
    while process.poll() is None:
        if exceeded.is_set():
            _terminate_process(process)
            if termination_started is None:
                termination_started = time.monotonic()
            elif time.monotonic() - termination_started >= PROCESS_TERMINATION_SECONDS:
                try:
                    process.kill()
                except OSError:
                    pass
        time.sleep(0.05)
    exit_code = int(process.wait())
    for thread in threads:
        thread.join()
    if errors:
        raise ValidationReceiptError(f"Unable to stream validation logs: {errors[0]}")
    if set(results) != {"stdout", "stderr"}:
        raise ValidationReceiptError("Validation log capture did not complete.")

    limit_exceeded = exceeded.is_set()
    if limit_exceeded and exit_code == 0:
        exit_code = LIMIT_FAILURE_EXIT_CODE
    return StreamedProcessResult(
        exit_code,
        results["stdout"],
        results["stderr"],
        limit_exceeded,
    )


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
    truncated = value.get("truncated", False)
    if not isinstance(relative, str) or not relative:
        raise ValidationReceiptError(f"{field}.path is required.")
    if not isinstance(truncated, bool):
        raise ValidationReceiptError(f"{field}.truncated must be boolean when present.")
    relative_path = Path(relative)
    if relative_path.is_absolute() or ".." in relative_path.parts:
        raise ValidationReceiptError(f"{field}.path escapes the receipt directory.")
    candidate = reject_storage_indirection(output / relative_path, f"{field}.path")
    try:
        candidate.relative_to(output)
    except ValueError as exc:
        raise ValidationReceiptError(f"{field}.path escapes the receipt directory.") from exc
    if not candidate.is_file() or candidate.is_symlink():
        raise ValidationReceiptError(f"{field}.path is missing or unsafe.")
    actual_digest, actual_bytes = hash_file(candidate)
    if digest != actual_digest or expected_bytes != actual_bytes:
        raise ValidationReceiptError(f"{field} hash or byte count does not match.")


def utc_now() -> str:
    return (
        datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )


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
            if entry.get("stdout_log") is not None or entry.get("stderr_log") is not None:
                raise ValidationReceiptError(f"{name} not_run must not carry execution logs.")
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
            if entry.get("stdout_log") is None or entry.get("stderr_log") is None:
                raise ValidationReceiptError(
                    f"{name} executed state requires tool-captured stdout and stderr logs."
                )
            if entry.get("log_limit_exceeded") not in (None, False, True):
                raise ValidationReceiptError(f"{name}.log_limit_exceeded must be boolean.")
            if entry.get("log_limit_exceeded") and status != "failed":
                raise ValidationReceiptError(f"{name} exceeded a log limit but is not failed.")
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
        "source_commit": repository_state(args.repo_root),
        "tester_alias": require_token(args.tester_alias, "tester alias"),
        "platform": args.platform.strip(),
        "configuration": args.configuration.strip(),
        "created_at_utc": utc_now(),
        "gates": [],
        "risk_acceptances": [],
        "finalized_at_utc": None,
    }
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
    if find_gate(receipt, args.name) is not None:
        raise ValidationReceiptError(
            f"{args.name} is already recorded; initialize a new exact-head receipt to rerun it."
        )
    repository = reject_storage_indirection(args.repo_root, "Repository root")
    repository_state(
        repository,
        expected_commit=str(receipt.get("source_commit", "")),
    )
    command = list(args.command)
    if command and command[0] == "--":
        command.pop(0)
    if not command:
        raise ValidationReceiptError(
            "record requires an executable and arguments after --."
        )

    stdout_path = prepare_log_destination(output, args.name, "stdout")
    stderr_path = prepare_log_destination(output, args.name, "stderr")
    started = utc_now()
    completed = stream_gate_process(
        command,
        repository,
        stdout_path,
        stderr_path,
    )
    finished = utc_now()
    repository_state(
        repository,
        expected_commit=str(receipt.get("source_commit", "")),
    )

    status = "passed" if completed.exit_code == 0 else "failed"
    notes = args.notes
    if completed.limit_exceeded:
        detail = (
            f"Gate output exceeded the fixed {MAXIMUM_GATE_STREAM_BYTES}-byte "
            "per-stream limit; the process was terminated and the captured prefix "
            "was retained with its incremental hash."
        )
        notes = f"{notes}\n{detail}".strip()
    entry = {
        "name": args.name,
        "command": shlex.join(command),
        "status": status,
        "exit_code": completed.exit_code,
        "started_at_utc": started,
        "finished_at_utc": finished,
        "notes": notes,
        "log_limit_exceeded": completed.limit_exceeded,
        "stdout_log": completed.stdout.receipt_value(output),
        "stderr_log": completed.stderr.receipt_value(output),
    }
    gates = list(receipt.get("gates", []))
    gates.append(entry)
    gates.sort(key=lambda item: GATE_NAMES.index(item["name"]))
    receipt["gates"] = gates
    write_receipt(output, receipt)
    print(f"{args.name}: {status} (exit {completed.exit_code})")
    return 0 if completed.exit_code == 0 else 1


def skip_gate(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    if receipt.get("finalized_at_utc") is not None:
        raise ValidationReceiptError("Finalized receipts are immutable.")
    if find_gate(receipt, args.name) is not None:
        raise ValidationReceiptError(f"{args.name} is already recorded.")
    repository_state(
        args.repo_root,
        expected_commit=str(receipt.get("source_commit", "")),
    )
    if not args.reason.strip():
        raise ValidationReceiptError("skip requires a concrete reason.")
    gates = list(receipt.get("gates", []))
    gates.append(
        {
            "name": args.name,
            "command": "not run",
            "status": "not_run",
            "exit_code": None,
            "started_at_utc": None,
            "finished_at_utc": None,
            "notes": args.reason.strip(),
            "stdout_log": None,
            "stderr_log": None,
        }
    )
    gates.sort(key=lambda item: GATE_NAMES.index(item["name"]))
    receipt["gates"] = gates
    write_receipt(output, receipt)
    return 0


def accept_risk(args: argparse.Namespace) -> int:
    output = resolve_output(args.output, create=False)
    receipt = load_receipt(output)
    if receipt.get("finalized_at_utc") is not None:
        raise ValidationReceiptError("Finalized receipts are immutable.")
    repository_state(
        args.repo_root,
        expected_commit=str(receipt.get("source_commit", "")),
    )
    if args.gate not in RISK_ACCEPTABLE_GATES:
        raise ValidationReceiptError(f"Risk cannot be accepted for {args.gate}.")
    gate = find_gate(receipt, args.gate)
    if gate is None or gate.get("status") != "not_run":
        raise ValidationReceiptError(
            "Risk acceptance is allowed only after recording the gate as not_run."
        )
    acceptance = {
        "gate": args.gate,
        "maintainer_alias": require_token(args.maintainer_alias, "maintainer alias"),
        "accepted_at_utc": utc_now(),
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
    repository_state(
        args.repo_root,
        expected_commit=str(receipt.get("source_commit", "")),
    )
    receipt["finalized_at_utc"] = utc_now()
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
    repository_state(
        args.repo_root,
        expected_commit=str(receipt.get("source_commit", "")),
    )
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
    repository_state(
        args.repo_root,
        expected_commit=str(receipt.get("source_commit", "")),
    )
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
            print(f"| `{gate_name}` | missing | \u2014 | \u2014 |")
            continue
        exit_value = "\u2014" if gate["exit_code"] is None else str(gate["exit_code"])
        notes = gate["notes"].replace("|", "\\|").replace("\n", " ")
        print(f"| `{gate_name}` | {gate['status']} | {exit_value} | {notes} |")
    if receipt.get("risk_acceptances"):
        print()
        print("Maintainer risk acceptances:")
        for acceptance in receipt["risk_acceptances"]:
            print(
                f"- `{acceptance['gate']}` \u2014 `{acceptance['maintainer_alias']}` at "
                f"`{acceptance['accepted_at_utc']}`: {acceptance['rationale']}"
            )
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
        help="Git repository whose clean exact HEAD is being validated.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    init_parser = subparsers.add_parser("init", help="Create a pending receipt.")
    init_parser.add_argument("--output", type=Path, required=True)
    init_parser.add_argument("--tester-alias", required=True)
    init_parser.add_argument("--platform", required=True)
    init_parser.add_argument("--configuration", required=True)
    init_parser.add_argument("--force", action="store_true")
    init_parser.set_defaults(handler=initialize)

    record_parser = subparsers.add_parser("record", help="Record one validation gate.")
    record_parser.add_argument("--output", type=Path, required=True)
    record_parser.add_argument("--name", required=True, choices=GATE_NAMES)
    record_parser.add_argument("--notes", default="")
    record_parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="Executable and arguments to run after --.",
    )
    record_parser.set_defaults(handler=record_gate)

    skip_parser = subparsers.add_parser(
        "skip", help="Record a gate as not run without inventing execution data."
    )
    skip_parser.add_argument("--output", type=Path, required=True)
    skip_parser.add_argument("--name", required=True, choices=GATE_NAMES)
    skip_parser.add_argument("--reason", required=True)
    skip_parser.set_defaults(handler=skip_gate)

    accept_parser = subparsers.add_parser(
        "accept-risk", help="Accept one explicitly recorded not-run gate."
    )
    accept_parser.add_argument("--output", type=Path, required=True)
    accept_parser.add_argument("--gate", required=True, choices=RISK_ACCEPTABLE_GATES)
    accept_parser.add_argument("--maintainer-alias", required=True)
    accept_parser.add_argument("--rationale", required=True)
    accept_parser.set_defaults(handler=accept_risk)

    finalize_parser = subparsers.add_parser(
        "finalize", help="Finalize only when the receipt is merge-ready."
    )
    finalize_parser.add_argument("--output", type=Path, required=True)
    finalize_parser.add_argument("--expected-commit", required=True)
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
