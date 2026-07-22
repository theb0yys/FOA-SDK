#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Capability-gated, exact-identity process launcher for installer helpers."""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Mapping, Sequence

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
ENGINE_SOURCE = INSTALLER_ROOT / "Bootstrapper" / "PackageEngine" / "Source"
VIEW_MODEL_SOURCE = INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source"
for root in (ENGINE_SOURCE, VIEW_MODEL_SOURCE):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from package_engine import PackageEngineError, validate_engine_session  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS, canonical_json, sha256  # noqa: E402

LAUNCH_CAPABILITY = "package-engine.launch-process"
GRANT_SCOPE = "package-process-launch-grant"
RESULT_SCOPE = "package-process-launch-result"
MAX_GRANT_SECONDS = 900
MAX_TIMEOUT_SECONDS = 120
MAX_OUTPUT_BYTES = 1024 * 1024
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
ENV_NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]{0,63}$")
GRANT_STATEMENT = (
    "This grant authorizes one exact non-shell process launch for one exact package-engine "
    "session, executable identity, argument vector, environment, timeout, and output bound. "
    "It grants no elevation, product installation finalization, game launch, runtime adapter "
    "execution, deployment, save mutation, signing, publication, catalog mutation, or evidence promotion."
)
RESULT_STATEMENT = (
    "This result records one bounded non-shell process attempt. It grants no later authority "
    "and does not by itself finalize install, repair, upgrade, rollback, or uninstall."
)


class ProcessLaunchError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ProcessLaunchError(f"{label} must be an object.")
    return value


def _array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ProcessLaunchError(f"{label} must be an array.")
    return value


def _text(value: object, label: str, *, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ProcessLaunchError(f"{label} must be a non-empty trimmed string of at most {maximum} characters.")
    if any(ord(ch) == 0 or ord(ch) < 9 or 13 < ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise ProcessLaunchError(f"{label} contains a forbidden control character.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, maximum=128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise ProcessLaunchError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label, maximum=64)
    if SHA256_RE.fullmatch(result) is None:
        raise ProcessLaunchError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label, maximum=64)
    if not result.endswith("Z") or "T" not in result:
        raise ProcessLaunchError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise ProcessLaunchError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise ProcessLaunchError(f"{label} must use UTC.")
    return result


def _utc_dt(value: object, label: str) -> dt.datetime:
    return dt.datetime.fromisoformat(_utc(value, label)[:-1] + "+00:00")


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _validate_authority(value: object, label: str) -> None:
    authority = _object(value, label)
    if set(authority) != set(AUTHORITY_FIELDS) or any(authority.get(field) is not False for field in AUTHORITY_FIELDS):
        raise ProcessLaunchError(f"{label} must contain the exact all-false authority record.")


def _argv(value: object, label: str) -> list[str]:
    raw = _array(value, label)
    if len(raw) > 128:
        raise ProcessLaunchError(f"{label} must contain at most 128 arguments.")
    return [_text(item, f"{label}[{index}]", maximum=8192) for index, item in enumerate(raw)]


def _environment(value: object, label: str) -> dict[str, str]:
    raw = _object(value, label)
    if len(raw) > 64:
        raise ProcessLaunchError(f"{label} must contain at most 64 variables.")
    result: dict[str, str] = {}
    for name in sorted(raw):
        if not isinstance(name, str) or ENV_NAME_RE.fullmatch(name) is None:
            raise ProcessLaunchError(f"{label} contains an invalid variable name: {name!r}")
        result[name] = _text(raw[name], f"{label}.{name}", maximum=8192)
    return result


def _file_sha256(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    flags = os.O_RDONLY | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    descriptor = os.open(path, flags)
    try:
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            size += len(chunk)
    finally:
        os.close(descriptor)
    return digest.hexdigest(), size


def _reject_symlink_components(path: Path, label: str) -> None:
    current = path
    while True:
        if current.is_symlink():
            raise ProcessLaunchError(f"{label} contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def build_launch_grant(
    session: Mapping[str, object], *, executable_reference: str, executable_sha256: str,
    argv: Sequence[str], environment: Mapping[str, str], timeout_seconds: int,
    output_limit_bytes: int, issuer: str, issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    try:
        checked = validate_engine_session(session)
    except PackageEngineError as exc:
        raise ProcessLaunchError(f"Package-engine session verification failed: {exc}") from exc
    issued = _utc_dt(issued_at_utc, "issued_at_utc")
    expires = _utc_dt(expires_at_utc, "expires_at_utc")
    accepted = _utc_dt(checked["accepted_at_utc"], "session.accepted_at_utc")
    if issued < accepted:
        raise ProcessLaunchError("issued_at_utc must not precede session acceptance.")
    if expires <= issued or expires - issued > dt.timedelta(seconds=MAX_GRANT_SECONDS):
        raise ProcessLaunchError("Launch grant expiry must be after issuance and within 15 minutes.")
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= MAX_TIMEOUT_SECONDS:
        raise ProcessLaunchError("timeout_seconds must be between 1 and 120.")
    if type(output_limit_bytes) is not int or not 1 <= output_limit_bytes <= MAX_OUTPUT_BYTES:
        raise ProcessLaunchError("output_limit_bytes must be between 1 and 1048576.")
    checked_argv = _argv(list(argv), "argv")
    checked_environment = _environment(dict(environment), "environment")
    base = {
        "schema_version": 1,
        "grant_scope": GRANT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "capability": LAUNCH_CAPABILITY,
        "executable_reference": _reference(executable_reference, "executable_reference"),
        "executable_sha256": _hash(executable_sha256, "executable_sha256"),
        "argv": checked_argv,
        "argv_sha256": sha256(checked_argv),
        "environment": checked_environment,
        "environment_sha256": sha256(checked_environment),
        "timeout_seconds": timeout_seconds,
        "output_limit_bytes": output_limit_bytes,
        "issuer": _text(issuer, "issuer", maximum=160),
        "issued_at_utc": _utc(issued_at_utc, "issued_at_utc"),
        "expires_at_utc": _utc(expires_at_utc, "expires_at_utc"),
        "nonce": _reference(nonce, "nonce"),
        "session": checked,
        "statement": GRANT_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "grant_sha256": sha256(base)}


def validate_launch_grant(grant: Mapping[str, object]) -> dict[str, object]:
    document = dict(grant)
    if document.get("schema_version") != 1 or document.get("grant_scope") != GRANT_SCOPE:
        raise ProcessLaunchError("Launch grant schema or scope is invalid.")
    if document.get("capability") != LAUNCH_CAPABILITY or document.get("statement") != GRANT_STATEMENT:
        raise ProcessLaunchError("Launch grant capability or statement is invalid.")
    _validate_authority(document.get("authority"), "grant.authority")
    declared = _hash(document.get("grant_sha256"), "grant.grant_sha256")
    unsigned = {key: value for key, value in document.items() if key != "grant_sha256"}
    if sha256(unsigned) != declared:
        raise ProcessLaunchError("Launch grant fingerprint does not match its content.")
    expected = build_launch_grant(
        _object(document.get("session"), "grant.session"),
        executable_reference=_reference(document.get("executable_reference"), "grant.executable_reference"),
        executable_sha256=_hash(document.get("executable_sha256"), "grant.executable_sha256"),
        argv=_argv(document.get("argv"), "grant.argv"),
        environment=_environment(document.get("environment"), "grant.environment"),
        timeout_seconds=document.get("timeout_seconds"),
        output_limit_bytes=document.get("output_limit_bytes"),
        issuer=_text(document.get("issuer"), "grant.issuer", maximum=160),
        issued_at_utc=_utc(document.get("issued_at_utc"), "grant.issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "grant.expires_at_utc"),
        nonce=_reference(document.get("nonce"), "grant.nonce"),
    )
    if document != expected:
        raise ProcessLaunchError("Launch grant is stale, altered, or not canonically derived.")
    return document


def launch_process(
    grant: Mapping[str, object], executable_path: Path, working_directory: Path, *, launched_at_utc: str,
) -> dict[str, object]:
    checked = validate_launch_grant(grant)
    launched = _utc_dt(launched_at_utc, "launched_at_utc")
    if launched < _utc_dt(checked["issued_at_utc"], "grant.issued_at_utc"):
        raise ProcessLaunchError("launched_at_utc must not precede grant issuance.")
    if launched > _utc_dt(checked["expires_at_utc"], "grant.expires_at_utc"):
        raise ProcessLaunchError("Launch grant expired before process start.")
    executable = Path(executable_path)
    cwd = Path(working_directory)
    if not executable.is_absolute() or executable.is_symlink() or not executable.is_file():
        raise ProcessLaunchError("Executable must be an absolute regular non-symlink file.")
    if not cwd.is_absolute() or cwd.is_symlink() or not cwd.is_dir():
        raise ProcessLaunchError("Working directory must be an absolute existing non-symlink directory.")
    _reject_symlink_components(executable.parent, "Executable path")
    _reject_symlink_components(cwd, "Working directory")
    actual_hash, executable_size = _file_sha256(executable)
    if actual_hash != checked["executable_sha256"]:
        raise ProcessLaunchError("Executable SHA-256 does not match the launch grant.")
    command = [str(executable), *checked["argv"]]
    timed_out = False
    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd),
            env=dict(checked["environment"]),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False,
            check=False,
            timeout=checked["timeout_seconds"],
            close_fds=True,
        )
        return_code = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        return_code = None
        stdout = exc.stdout or b""
        stderr = exc.stderr or b""
    limit = checked["output_limit_bytes"]
    stdout_truncated = len(stdout) > limit
    stderr_truncated = len(stderr) > limit
    stdout = stdout[:limit]
    stderr = stderr[:limit]
    base = {
        "schema_version": 1,
        "result_scope": RESULT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "grant_sha256": checked["grant_sha256"],
        "executable_reference": checked["executable_reference"],
        "executable_sha256": actual_hash,
        "executable_size_bytes": executable_size,
        "argv_sha256": checked["argv_sha256"],
        "environment_sha256": checked["environment_sha256"],
        "launched_at_utc": _utc(launched_at_utc, "launched_at_utc"),
        "return_code": return_code,
        "timed_out": timed_out,
        "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
        "stdout_size_bytes": len(stdout),
        "stdout_truncated": stdout_truncated,
        "stderr_sha256": hashlib.sha256(stderr).hexdigest(),
        "stderr_size_bytes": len(stderr),
        "stderr_truncated": stderr_truncated,
        "process_launched": True,
        "elevation_requested": False,
        "shell_used": False,
        "statement": RESULT_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "result_sha256": sha256(base)}


def validate_launch_result(result: Mapping[str, object]) -> dict[str, object]:
    document = dict(result)
    if document.get("schema_version") != 1 or document.get("result_scope") != RESULT_SCOPE:
        raise ProcessLaunchError("Launch result schema or scope is invalid.")
    if document.get("statement") != RESULT_STATEMENT:
        raise ProcessLaunchError("Launch result statement was altered.")
    if document.get("process_launched") is not True or document.get("elevation_requested") is not False or document.get("shell_used") is not False:
        raise ProcessLaunchError("Launch result operational flags are invalid.")
    _validate_authority(document.get("authority"), "result.authority")
    declared = _hash(document.get("result_sha256"), "result.result_sha256")
    unsigned = {key: value for key, value in document.items() if key != "result_sha256"}
    if sha256(unsigned) != declared:
        raise ProcessLaunchError("Launch result fingerprint does not match its content.")
    return document


def canonical_grant_bytes(grant: Mapping[str, object]) -> bytes:
    return canonical_json(validate_launch_grant(grant))


def canonical_result_bytes(result: Mapping[str, object]) -> bytes:
    return canonical_json(validate_launch_result(result))
