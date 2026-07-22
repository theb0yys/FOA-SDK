#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Shared trust, reviewed-operation, one-shot, path, publication, and process safety primitives."""
from __future__ import annotations

import ctypes
import datetime as dt
import errno
import hashlib
import hmac
import json
import os
import re
import signal
import stat
import subprocess
import sys
import threading
import time
from pathlib import Path, PurePosixPath
from typing import Mapping, Sequence

REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
ENV_NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]{0,63}$")
OPERATIONS = ("install", "repair", "upgrade", "rollback", "uninstall")
COPY_OPERATIONS = frozenset(("install", "repair", "upgrade"))
PLAN_SCOPE = "reviewed-package-operation-plan"
AUTHORITY_SCOPE = "reviewed-package-operation-authority"
AUTHORITY_AUDIENCE = "foa-sdk.package-engine"
MAX_AUTHORITY_SECONDS = 900
MAX_HELPERS = 16
MAX_SUPPORT_FILES = 64
MAX_OUTPUT_BYTES = 1024 * 1024
MAX_JSON_BYTES = 8 * 1024 * 1024
MAX_JSON_DEPTH = 64
MAX_JSON_NODES = 250_000


class ExecutionSecurityError(RuntimeError):
    """Raised when trust, one-shot, path, publication, or execution safety fails."""


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256(value: object) -> str:
    payload = value if isinstance(value, bytes) else canonical_json(value)
    return hashlib.sha256(payload).hexdigest()


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ExecutionSecurityError(f"{label} must be an object.")
    return value


def _array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ExecutionSecurityError(f"{label} must be an array.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ExecutionSecurityError(f"{label} must be non-empty trimmed text of at most {maximum} characters.")
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise ExecutionSecurityError(f"{label} contains a forbidden control character.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise ExecutionSecurityError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise ExecutionSecurityError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if not result.endswith("Z") or "T" not in result:
        raise ExecutionSecurityError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise ExecutionSecurityError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise ExecutionSecurityError(f"{label} must use UTC.")
    return result


def utc_datetime(value: object, label: str) -> dt.datetime:
    return dt.datetime.fromisoformat(_utc(value, label)[:-1] + "+00:00")


def _relative_path(value: object, label: str) -> str:
    result = _text(value, label, 1024)
    path = PurePosixPath(result)
    if path.is_absolute() or "\\" in result or "//" in result or any(part in {"", ".", ".."} for part in path.parts):
        raise ExecutionSecurityError(f"{label} must be a safe relative POSIX path.")
    return result


def _argv(value: object, label: str) -> list[str]:
    raw = _array(value, label)
    if len(raw) > 128:
        raise ExecutionSecurityError(f"{label} must contain at most 128 entries.")
    return [_text(item, f"{label}[{index}]", 8192) for index, item in enumerate(raw)]


def _environment(value: object, label: str) -> dict[str, str]:
    raw = _object(value, label)
    if len(raw) > 64:
        raise ExecutionSecurityError(f"{label} must contain at most 64 variables.")
    result: dict[str, str] = {}
    for name in sorted(raw):
        if not isinstance(name, str) or ENV_NAME_RE.fullmatch(name) is None:
            raise ExecutionSecurityError(f"{label} contains an invalid variable name: {name!r}")
        result[name] = _text(raw[name], f"{label}.{name}", 8192)
    return result


def _support_files(value: object, label: str) -> list[dict[str, object]]:
    raw = _array(value, label)
    if len(raw) > MAX_SUPPORT_FILES:
        raise ExecutionSecurityError(f"{label} must contain at most {MAX_SUPPORT_FILES} entries.")
    result: list[dict[str, object]] = []
    identities: set[str] = set()
    for index, item in enumerate(raw):
        row = _object(item, f"{label}[{index}]")
        path = _relative_path(row.get("path"), f"{label}[{index}].path")
        identity = path.casefold()
        if identity in identities:
            raise ExecutionSecurityError(f"{label} contains a duplicate or case-colliding path: {path}")
        identities.add(identity)
        size = row.get("size_bytes")
        if type(size) is not int or size < 0:
            raise ExecutionSecurityError(f"{label}[{index}].size_bytes must be non-negative.")
        result.append({"path": path, "sha256": _hash(row.get("sha256"), f"{label}[{index}].sha256"), "size_bytes": size})
    result.sort(key=lambda row: (str(row["path"]).casefold(), str(row["path"])))
    return result


def required_capabilities(operation: str, *, requires_elevation: bool) -> list[str]:
    if operation not in OPERATIONS:
        raise ExecutionSecurityError("operation must be one of: " + ", ".join(OPERATIONS) + ".")
    capabilities = {
        f"package-engine.execute.{operation}",
        "package-engine.launch-process",
        "package-engine.publish-installation-state",
    }
    if operation in COPY_OPERATIONS:
        capabilities.add("package-engine.copy-payload")
    if requires_elevation:
        capabilities.add("package-engine.elevation")
    return sorted(capabilities)


def build_operation_plan(
    *,
    plan_reference: str,
    operation: str,
    receipt_plan_sha256: str,
    package_order: Sequence[str],
    helpers: Sequence[Mapping[str, object]],
    bootstrapper_reference: str | None = None,
) -> dict[str, object]:
    if operation not in OPERATIONS:
        raise ExecutionSecurityError("operation must be one of: " + ", ".join(OPERATIONS) + ".")
    checked_packages = [_reference(item, f"package_order[{index}]") for index, item in enumerate(package_order)]
    if not checked_packages or len(set(checked_packages)) != len(checked_packages):
        raise ExecutionSecurityError("package_order must be a non-empty unique resolver-owned package sequence.")
    if not helpers or len(helpers) > MAX_HELPERS:
        raise ExecutionSecurityError(f"helpers must contain between 1 and {MAX_HELPERS} reviewed helpers.")
    checked_helpers: list[dict[str, object]] = []
    references: set[str] = set()
    operation_helpers = 0
    elevation_required = False
    for index, raw in enumerate(helpers):
        helper = _object(dict(raw), f"helpers[{index}]")
        reference = _reference(helper.get("helper_reference"), f"helpers[{index}].helper_reference")
        if reference in references:
            raise ExecutionSecurityError(f"Duplicate helper_reference: {reference}")
        references.add(reference)
        role = _text(helper.get("role"), f"helpers[{index}].role", 32)
        if role not in {"operation-helper", "elevation-bootstrapper"}:
            raise ExecutionSecurityError("helper.role must be operation-helper or elevation-bootstrapper.")
        operations = sorted({_text(item, "helper.operations[]", 32) for item in _array(helper.get("operations"), "helper.operations")})
        if not operations or any(item not in OPERATIONS for item in operations):
            raise ExecutionSecurityError("helper.operations must contain known lifecycle operations.")
        requires_elevation = helper.get("requires_elevation")
        if type(requires_elevation) is not bool:
            raise ExecutionSecurityError("helper.requires_elevation must be a boolean.")
        timeout_seconds = helper.get("timeout_seconds")
        output_limit_bytes = helper.get("output_limit_bytes")
        if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= 120:
            raise ExecutionSecurityError("helper.timeout_seconds must be between 1 and 120.")
        if type(output_limit_bytes) is not int or not 1 <= output_limit_bytes <= MAX_OUTPUT_BYTES:
            raise ExecutionSecurityError("helper.output_limit_bytes must be between 1 and 1048576.")
        owner_package_id = _reference(helper.get("owner_package_id"), f"helpers[{index}].owner_package_id")
        if owner_package_id not in checked_packages:
            raise ExecutionSecurityError("Every reviewed helper must be owned by a package in the exact resolver order.")
        checked_argv = _argv(helper.get("argv"), "helper.argv")
        checked_environment = _environment(helper.get("environment"), "helper.environment")
        if role == "elevation-bootstrapper":
            if requires_elevation:
                raise ExecutionSecurityError("The elevation bootstrapper itself must not recursively require elevation.")
            if checked_environment:
                raise ExecutionSecurityError("The elevation bootstrapper must use an empty inherited environment contract; all configuration is exact reviewed argv.")
            required_options = {"--execution-root", "--authority-key", "--claim-root", "--completed-at-utc", "--output"}
            if not required_options.issubset(set(checked_argv)) or "--request" in checked_argv:
                raise ExecutionSecurityError("Elevation bootstrapper argv must contain exact broker configuration options and reserve --request for the one-shot request path.")
        checked: dict[str, object] = {
            "helper_reference": reference,
            "owner_package_id": owner_package_id,
            "role": role,
            "executable_path": _relative_path(helper.get("executable_path"), "helper.executable_path"),
            "executable_sha256": _hash(helper.get("executable_sha256"), "helper.executable_sha256"),
            "argv": checked_argv,
            "working_directory": _relative_path(helper.get("working_directory"), "helper.working_directory"),
            "environment": checked_environment,
            "operations": operations,
            "requires_elevation": requires_elevation,
            "timeout_seconds": timeout_seconds,
            "output_limit_bytes": output_limit_bytes,
        }
        if "support_files" in helper:
            support_files = _support_files(helper.get("support_files"), "helper.support_files")
            executable_identity = str(checked["executable_path"]).casefold()
            if any(str(row["path"]).casefold() == executable_identity for row in support_files):
                raise ExecutionSecurityError("helper.support_files must not repeat helper.executable_path.")
            checked["support_files"] = support_files
        if role == "operation-helper" and operation in operations:
            operation_helpers += 1
            elevation_required = elevation_required or requires_elevation
        checked_helpers.append(checked)
    checked_helpers.sort(key=lambda row: str(row["helper_reference"]))
    if operation_helpers != 1:
        raise ExecutionSecurityError("The reviewed plan must contain exactly one operation helper for its operation.")
    checked_bootstrapper: str | None = None
    if elevation_required:
        checked_bootstrapper = _reference(bootstrapper_reference, "bootstrapper_reference")
        matches = [row for row in checked_helpers if row["helper_reference"] == checked_bootstrapper]
        if len(matches) != 1 or matches[0]["role"] != "elevation-bootstrapper" or operation not in matches[0]["operations"]:
            raise ExecutionSecurityError("Elevation requires one matching reviewed elevation bootstrapper.")
    elif bootstrapper_reference is not None:
        raise ExecutionSecurityError("Non-elevated plans must not declare bootstrapper_reference.")
    capabilities = required_capabilities(operation, requires_elevation=elevation_required)
    base = {
        "schema_version": 1,
        "plan_scope": PLAN_SCOPE,
        "plan_reference": _reference(plan_reference, "plan_reference"),
        "operation": operation,
        "receipt_plan_sha256": _hash(receipt_plan_sha256, "receipt_plan_sha256"),
        "package_order": checked_packages,
        "capabilities": capabilities,
        "helpers": checked_helpers,
        "bootstrapper_reference": checked_bootstrapper,
        "statement": (
            "This reviewed operation plan is the complete executable, support-file, and capability allow-list "
            "for one package lifecycle operation. Callers may select only the exact bound helper records."
        ),
    }
    return {**base, "operation_plan_sha256": sha256(base)}


def validate_operation_plan(plan: Mapping[str, object]) -> dict[str, object]:
    document = dict(plan)
    declared = _hash(document.get("operation_plan_sha256"), "operation_plan_sha256")
    unsigned = {key: value for key, value in document.items() if key != "operation_plan_sha256"}
    if sha256(unsigned) != declared:
        raise ExecutionSecurityError("Operation-plan fingerprint does not match its content.")
    expected = build_operation_plan(
        plan_reference=_reference(document.get("plan_reference"), "plan_reference"),
        operation=_text(document.get("operation"), "operation", 32),
        receipt_plan_sha256=_hash(document.get("receipt_plan_sha256"), "receipt_plan_sha256"),
        package_order=[_reference(item, "package_order[]") for item in _array(document.get("package_order"), "package_order")],
        helpers=[_object(item, "helpers[]") for item in _array(document.get("helpers"), "helpers")],
        bootstrapper_reference=document.get("bootstrapper_reference"),
    )
    if document != expected:
        raise ExecutionSecurityError("Operation plan is stale, altered, or not canonically derived.")
    return document


def select_helper(plan: Mapping[str, object], helper_reference: str, *, role: str | None = None) -> dict[str, object]:
    checked = validate_operation_plan(plan)
    reference = _reference(helper_reference, "helper_reference")
    matches = [dict(row) for row in checked["helpers"] if row["helper_reference"] == reference]
    if len(matches) != 1:
        raise ExecutionSecurityError("Reviewed helper reference is absent from the operation plan.")
    helper = matches[0]
    if role is not None and helper["role"] != role:
        raise ExecutionSecurityError(f"Reviewed helper must have role {role}.")
    if checked["operation"] not in helper["operations"]:
        raise ExecutionSecurityError("Reviewed helper does not support the operation plan operation.")
    return helper


def _reject_symlink_components(path: Path, label: str) -> None:
    current = path
    while True:
        if current.is_symlink():
            raise ExecutionSecurityError(f"{label} contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def _read_authority_key(path: Path) -> tuple[bytes, str]:
    target = Path(path)
    if not target.is_absolute() or target.is_symlink() or not target.is_file():
        raise ExecutionSecurityError("Authority key must be an absolute regular non-symlink file.")
    _reject_symlink_components(target.parent, "Authority key path")
    if os.name != "nt" and stat.S_IMODE(target.stat().st_mode) & 0o077:
        raise ExecutionSecurityError("Authority key permissions must deny group and other access.")
    key = target.read_bytes()
    if not 32 <= len(key) <= 64:
        raise ExecutionSecurityError("Authority key must contain 32 to 64 raw bytes.")
    return key, hashlib.sha256(key).hexdigest()


def sign_authenticated_record(base: Mapping[str, object], *, authority_key_path: Path, signature_field: str = "authority_signature") -> dict[str, object]:
    key, key_id = _read_authority_key(authority_key_path)
    unsigned = {**dict(base), "authority_key_id": key_id}
    signature = hmac.new(key, canonical_json(unsigned), hashlib.sha256).hexdigest()
    return {**unsigned, signature_field: signature}


def verify_authenticated_record(document: Mapping[str, object], *, authority_key_path: Path, signature_field: str = "authority_signature") -> dict[str, object]:
    value = dict(document)
    key, key_id = _read_authority_key(authority_key_path)
    if value.get("authority_key_id") != key_id:
        raise ExecutionSecurityError("Authenticated record was not signed by the configured trust anchor.")
    signature = _hash(value.get(signature_field), signature_field)
    unsigned = {name: item for name, item in value.items() if name != signature_field}
    expected = hmac.new(key, canonical_json(unsigned), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(signature, expected):
        raise ExecutionSecurityError("Authenticated record signature verification failed.")
    return value


def seal_authenticated_record(
    base: Mapping[str, object], *, authority_key_path: Path, digest_field: str, signature_field: str = "authority_signature"
) -> dict[str, object]:
    signed = sign_authenticated_record(base, authority_key_path=authority_key_path, signature_field=signature_field)
    return {**signed, digest_field: sha256(signed)}


def verify_sealed_record(
    document: Mapping[str, object], *, authority_key_path: Path, digest_field: str, signature_field: str = "authority_signature"
) -> dict[str, object]:
    value = dict(document)
    declared = _hash(value.get(digest_field), digest_field)
    signed = {name: item for name, item in value.items() if name != digest_field}
    if sha256(signed) != declared:
        raise ExecutionSecurityError(f"{digest_field} does not match the authenticated record content.")
    verify_authenticated_record(signed, authority_key_path=authority_key_path, signature_field=signature_field)
    return value


def issue_authority_proof(
    handoff: Mapping[str, object], operation_plan: Mapping[str, object], *, authority_key_path: Path,
    issuer: str, issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    plan = validate_operation_plan(operation_plan)
    handoff_sha = _hash(handoff.get("handoff_sha256"), "handoff.handoff_sha256")
    if handoff.get("operation") != plan["operation"]:
        raise ExecutionSecurityError("Authority proof operation plan must match the handoff operation.")
    if handoff.get("plan_sha256") != plan["receipt_plan_sha256"]:
        raise ExecutionSecurityError("Authority proof operation plan must bind the exact resolver plan fingerprint.")
    if handoff.get("package_order") != plan["package_order"]:
        raise ExecutionSecurityError("Authority proof operation plan must bind the exact resolver package order.")
    issued = utc_datetime(issued_at_utc, "issued_at_utc")
    expires = utc_datetime(expires_at_utc, "expires_at_utc")
    requested = utc_datetime(handoff.get("requested_at_utc"), "handoff.requested_at_utc")
    if issued < requested or expires <= issued or expires - issued > dt.timedelta(seconds=MAX_AUTHORITY_SECONDS):
        raise ExecutionSecurityError("Authority proof must be issued after the handoff and expire within 15 minutes.")
    key, key_id = _read_authority_key(authority_key_path)
    base = {
        "schema_version": 1, "authority_scope": AUTHORITY_SCOPE, "audience": AUTHORITY_AUDIENCE,
        "key_id": key_id, "handoff_sha256": handoff_sha,
        "operation_plan_sha256": plan["operation_plan_sha256"], "capabilities": list(plan["capabilities"]),
        "issuer": _text(issuer, "issuer", 160), "issued_at_utc": _utc(issued_at_utc, "issued_at_utc"),
        "expires_at_utc": _utc(expires_at_utc, "expires_at_utc"), "nonce": _reference(nonce, "nonce"),
        "statement": "This proof is authenticated by the configured installer authority key and authorizes only the exact handoff, reviewed operation plan, and capability set named here.",
    }
    signature = hmac.new(key, canonical_json(base), hashlib.sha256).hexdigest()
    signed = {**base, "authority_signature": signature}
    return {**signed, "authority_proof_sha256": sha256(signed)}


def validate_authority_proof(
    proof: Mapping[str, object], handoff: Mapping[str, object], operation_plan: Mapping[str, object], *, authority_key_path: Path
) -> dict[str, object]:
    document = dict(proof)
    if document.get("schema_version") != 1 or document.get("authority_scope") != AUTHORITY_SCOPE or document.get("audience") != AUTHORITY_AUDIENCE:
        raise ExecutionSecurityError("Authority proof schema, scope, or audience is invalid.")
    key, key_id = _read_authority_key(authority_key_path)
    if document.get("key_id") != key_id:
        raise ExecutionSecurityError("Authority proof was not signed by the configured trust anchor.")
    declared = _hash(document.get("authority_proof_sha256"), "authority_proof_sha256")
    signed = {key_name: value for key_name, value in document.items() if key_name != "authority_proof_sha256"}
    if sha256(signed) != declared:
        raise ExecutionSecurityError("Authority-proof fingerprint does not match its content.")
    signature = _hash(document.get("authority_signature"), "authority_signature")
    base = {key_name: value for key_name, value in signed.items() if key_name != "authority_signature"}
    expected_signature = hmac.new(key, canonical_json(base), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(signature, expected_signature):
        raise ExecutionSecurityError("Authority signature verification failed.")
    expected = issue_authority_proof(
        handoff, operation_plan, authority_key_path=authority_key_path,
        issuer=_text(document.get("issuer"), "issuer", 160),
        issued_at_utc=_utc(document.get("issued_at_utc"), "issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "expires_at_utc"),
        nonce=_reference(document.get("nonce"), "nonce"),
    )
    if document != expected:
        raise ExecutionSecurityError("Authority proof is stale, altered, or not canonically derived.")
    return document


def claim_once(
    claim_root: Path, *, authority_key_path: Path, claim_kind: str,
    artifact_sha256: str, nonce: str, claimed_at_utc: str,
) -> dict[str, object]:
    root = Path(claim_root)
    if not root.is_absolute() or root.is_symlink() or not root.is_dir():
        raise ExecutionSecurityError("Claim root must be an absolute existing non-symlink directory.")
    _reject_symlink_components(root, "Claim root")
    kind = _reference(claim_kind, "claim_kind")
    artifact = _hash(artifact_sha256, "artifact_sha256")
    checked_nonce = _reference(nonce, "nonce")
    directory = root / kind
    directory.mkdir(mode=0o700, exist_ok=True)
    if directory.is_symlink() or not directory.is_dir():
        raise ExecutionSecurityError("Claim-kind path must be a regular directory.")
    base = {"schema_version": 1, "claim_kind": kind, "artifact_sha256": artifact, "nonce": checked_nonce, "claimed_at_utc": _utc(claimed_at_utc, "claimed_at_utc")}
    claim = seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="claim_sha256")
    payload = canonical_json(claim)
    target = directory / f"{artifact}.claim.json"
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    try:
        descriptor = os.open(target, flags, 0o600)
    except FileExistsError as exc:
        raise ExecutionSecurityError(f"Artifact has already been consumed: {artifact}") from exc
    try:
        _write_all(descriptor, payload, "Claim")
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
    if _read_regular_file(target, MAX_JSON_BYTES, "Claim") != payload:
        raise ExecutionSecurityError("One-shot claim bytes changed after publication.")
    return claim


def rename_no_replace(source: Path, destination: Path) -> None:
    src = Path(source)
    dst = Path(destination)
    if dst.exists() or dst.is_symlink():
        raise ExecutionSecurityError("No-replace destination already exists.")
    if os.name == "nt":
        move = ctypes.windll.kernel32.MoveFileExW
        move.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint]
        move.restype = ctypes.c_int
        if not move(str(src), str(dst), 0):
            code = ctypes.get_last_error()
            raise ExecutionSecurityError(f"Atomic no-replace publication failed with Windows error {code}.")
        return
    if sys.platform.startswith("linux"):
        libc = ctypes.CDLL(None, use_errno=True)
        renameat2 = getattr(libc, "renameat2", None)
        if renameat2 is None:
            raise ExecutionSecurityError("Atomic renameat2(RENAME_NOREPLACE) is unavailable.")
        result = renameat2(-100, os.fsencode(src), -100, os.fsencode(dst), 1)
        if result != 0:
            code = ctypes.get_errno()
            if code in {errno.EEXIST, errno.ENOTEMPTY}:
                raise ExecutionSecurityError("No-replace destination appeared during publication.")
            raise ExecutionSecurityError(f"Atomic no-replace publication failed: {os.strerror(code)}")
        return
    raise ExecutionSecurityError("This platform has no reviewed atomic no-replace publication backend.")


def _write_all(descriptor: int, payload: bytes, label: str) -> None:
    view = memoryview(payload)
    while view:
        written = os.write(descriptor, view)
        if written <= 0:
            raise ExecutionSecurityError(f"{label} write made no forward progress.")
        view = view[written:]


def _read_regular_file(path: Path, maximum_bytes: int, label: str) -> bytes:
    target = Path(path)
    if target.is_symlink() or not target.is_file():
        raise ExecutionSecurityError(f"{label} path must be a regular non-symlink file: {target}")
    _reject_symlink_components(target.parent, f"{label} path")
    size = target.stat().st_size
    if size > maximum_bytes:
        raise ExecutionSecurityError(f"{label} exceeds the {maximum_bytes}-byte limit.")
    flags = os.O_RDONLY | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    descriptor = os.open(target, flags)
    try:
        chunks: list[bytes] = []
        remaining = maximum_bytes + 1
        while remaining > 0:
            chunk = os.read(descriptor, min(1024 * 1024, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        payload = b"".join(chunks)
    finally:
        os.close(descriptor)
    if len(payload) > maximum_bytes:
        raise ExecutionSecurityError(f"{label} exceeds the {maximum_bytes}-byte limit.")
    return payload


def _validate_json_shape(value: object) -> None:
    stack: list[tuple[object, int]] = [(value, 1)]
    nodes = 0
    while stack:
        current, depth = stack.pop()
        nodes += 1
        if nodes > MAX_JSON_NODES:
            raise ExecutionSecurityError("JSON document contains too many values.")
        if depth > MAX_JSON_DEPTH:
            raise ExecutionSecurityError("JSON document exceeds the maximum nesting depth.")
        if isinstance(current, dict):
            stack.extend((item, depth + 1) for item in current.values())
        elif isinstance(current, list):
            stack.extend((item, depth + 1) for item in current)


def read_strict_json_file(path: Path, label: str, *, require_canonical: bool = False, maximum_bytes: int = MAX_JSON_BYTES) -> object:
    payload = _read_regular_file(Path(path), maximum_bytes, label)
    try:
        value = json.loads(payload.decode("utf-8", errors="strict"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ExecutionSecurityError(f"{label} is not strict UTF-8 JSON: {path}") from exc
    _validate_json_shape(value)
    if require_canonical and canonical_json(value) != payload:
        raise ExecutionSecurityError(f"{label} bytes are not canonical JSON.")
    return value


def publish_bytes_create_once(path: Path, payload: bytes, *, label: str, maximum_bytes: int = MAX_JSON_BYTES) -> Path:
    target = Path(path)
    if len(payload) > maximum_bytes:
        raise ExecutionSecurityError(f"{label} exceeds the {maximum_bytes}-byte publication limit.")
    if not target.parent.is_dir() or target.parent.is_symlink():
        raise ExecutionSecurityError(f"{label} parent must be an existing non-symlink directory.")
    _reject_symlink_components(target.parent, f"{label} destination")
    if target.exists() or target.is_symlink():
        raise ExecutionSecurityError(f"{label} destination already exists.")
    temporary = target.parent / f".{target.name}.{hashlib.sha256(payload).hexdigest()}.tmp"
    if temporary.exists() or temporary.is_symlink():
        raise ExecutionSecurityError(f"{label} temporary destination already exists.")
    descriptor: int | None = None
    try:
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
        descriptor = os.open(temporary, flags, 0o600)
        _write_all(descriptor, payload, label)
        os.fsync(descriptor)
        os.close(descriptor)
        descriptor = None
        if _read_regular_file(temporary, maximum_bytes, label) != payload:
            raise ExecutionSecurityError(f"{label} temporary bytes changed before publication.")
        rename_no_replace(temporary, target)
        if _read_regular_file(target, maximum_bytes, label) != payload:
            raise ExecutionSecurityError(f"Published {label} bytes do not match the requested bytes.")
        return target
    finally:
        if descriptor is not None:
            os.close(descriptor)
        if temporary.exists() and not temporary.is_symlink():
            try:
                temporary.unlink()
            except OSError:
                pass


def file_sha256(path: Path) -> tuple[str, int]:
    target = Path(path)
    if target.is_symlink() or not target.is_file():
        raise ExecutionSecurityError(f"File must be a regular non-symlink file: {target}")
    flags = os.O_RDONLY | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    descriptor = os.open(target, flags)
    digest = hashlib.sha256()
    size = 0
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


def resolve_reviewed_path(root: Path, relative: str, *, file_required: bool, label: str) -> Path:
    base = Path(root)
    if not base.is_absolute() or base.is_symlink() or not base.is_dir():
        raise ExecutionSecurityError(f"{label} root must be an absolute existing non-symlink directory.")
    _reject_symlink_components(base, f"{label} root")
    checked = _relative_path(relative, label)
    candidate = base.joinpath(*PurePosixPath(checked).parts)
    try:
        candidate.relative_to(base)
    except ValueError as exc:
        raise ExecutionSecurityError(f"{label} escapes its reviewed root.") from exc
    if candidate.is_symlink() or (file_required and not candidate.is_file()) or (not file_required and not candidate.is_dir()):
        kind = "file" if file_required else "directory"
        raise ExecutionSecurityError(f"{label} must resolve to an existing non-symlink {kind}.")
    _reject_symlink_components(candidate.parent if file_required else candidate, label)
    return candidate


def remove_tree(root: Path) -> None:
    target = Path(root)
    if not target.exists() or target.is_symlink():
        return
    for child in sorted(target.rglob("*"), key=lambda item: len(item.parts), reverse=True):
        if child.is_file() or child.is_symlink():
            child.unlink()
        elif child.is_dir():
            child.rmdir()
    target.rmdir()


def _copy_exact_file(source: Path, destination: Path, expected_hash: str, expected_size: int | None, *, executable: bool) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink() or destination.exists():
        raise ExecutionSecurityError(f"Private bundle destination already exists: {destination}")
    out_flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    in_flags = os.O_RDONLY | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    output = os.open(destination, out_flags, 0o700 if executable else 0o600)
    input_fd = os.open(source, in_flags)
    try:
        while True:
            chunk = os.read(input_fd, 1024 * 1024)
            if not chunk:
                break
            _write_all(output, chunk, "Private bundle")
        os.fsync(output)
    finally:
        os.close(input_fd)
        os.close(output)
    actual_hash, actual_size = file_sha256(destination)
    if actual_hash != expected_hash or (expected_size is not None and actual_size != expected_size):
        raise ExecutionSecurityError(f"Private bundle verification failed: {destination}")
    os.chmod(destination, 0o700 if executable else 0o600)


def copy_reviewed_bundle(execution_root: Path, helper: Mapping[str, object], private_root: Path, bundle_reference: str) -> tuple[Path, Path]:
    root = Path(private_root)
    if not root.is_absolute() or root.is_symlink() or not root.is_dir():
        raise ExecutionSecurityError("Private bundle root must be an absolute existing non-symlink directory.")
    _reject_symlink_components(root, "Private bundle root")
    reference = _hash(bundle_reference, "bundle_reference")
    directory = root / "private-executables" / reference
    directory.parent.mkdir(mode=0o700, exist_ok=True)
    if directory.parent.is_symlink():
        raise ExecutionSecurityError("Private executable root must not be a symbolic link.")
    try:
        directory.mkdir(mode=0o700)
    except FileExistsError as exc:
        raise ExecutionSecurityError("Private executable directory already exists for this authority.") from exc
    try:
        executable_relative = _relative_path(helper.get("executable_path"), "helper.executable_path")
        executable_source = resolve_reviewed_path(execution_root, executable_relative, file_required=True, label="Reviewed executable")
        expected_hash = _hash(helper.get("executable_sha256"), "helper.executable_sha256")
        if file_sha256(executable_source)[0] != expected_hash:
            raise ExecutionSecurityError("Reviewed executable SHA-256 does not match the signed operation plan.")
        executable_destination = directory.joinpath(*PurePosixPath(executable_relative).parts)
        _copy_exact_file(executable_source, executable_destination, expected_hash, None, executable=True)
        for index, row in enumerate(_support_files(helper.get("support_files", []), "helper.support_files")):
            relative = str(row["path"])
            source = resolve_reviewed_path(execution_root, relative, file_required=True, label=f"Reviewed support file {index}")
            if file_sha256(source) != (row["sha256"], row["size_bytes"]):
                raise ExecutionSecurityError(f"Reviewed support file hash or size mismatch: {relative}")
            destination = directory.joinpath(*PurePosixPath(relative).parts)
            _copy_exact_file(source, destination, str(row["sha256"]), int(row["size_bytes"]), executable=False)
        return executable_destination, directory
    except Exception:
        remove_tree(directory)
        raise


class _BoundedOutput:
    def __init__(self, limit: int) -> None:
        self.limit = limit
        self.data = {"stdout": bytearray(), "stderr": bytearray()}
        self.observed = {"stdout": 0, "stderr": 0}
        self.total_observed = 0
        self.overflow = threading.Event()
        self.lock = threading.Lock()

    def add(self, stream_name: str, chunk: bytes) -> None:
        with self.lock:
            self.observed[stream_name] += len(chunk)
            self.total_observed += len(chunk)
            retained = len(self.data["stdout"]) + len(self.data["stderr"])
            remaining = max(0, self.limit - retained)
            if remaining:
                self.data[stream_name].extend(chunk[:remaining])
            if self.total_observed > self.limit:
                self.overflow.set()


def _reader(stream: object, capture: _BoundedOutput, stream_name: str) -> None:
    try:
        while True:
            chunk = stream.read(65536)
            if not chunk:
                break
            capture.add(stream_name, chunk)
    finally:
        stream.close()


def _windows_system_executable(name: str) -> Path:
    buffer = ctypes.create_unicode_buffer(32768)
    length = ctypes.windll.kernel32.GetSystemDirectoryW(buffer, len(buffer))
    if length <= 0 or length >= len(buffer):
        raise ExecutionSecurityError("Windows system directory could not be resolved safely.")
    target = Path(buffer.value) / name
    if not target.is_file():
        raise ExecutionSecurityError(f"Required Windows system executable was not found: {target}")
    return target


def _terminate_process_tree(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        taskkill = _windows_system_executable("taskkill.exe")
        minimal_environment = {key: value for key, value in os.environ.items() if key.upper() in {"SYSTEMROOT", "WINDIR"}}
        subprocess.run(
            [str(taskkill), "/PID", str(process.pid), "/T", "/F"],
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            shell=False, check=False, env=minimal_environment,
        )
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=10)


def run_bounded_process(
    command: Sequence[str], *, cwd: Path, environment: Mapping[str, str], timeout_seconds: int, output_limit_bytes: int,
) -> dict[str, object]:
    if not command:
        raise ExecutionSecurityError("command must not be empty.")
    if not 1 <= output_limit_bytes <= MAX_OUTPUT_BYTES:
        raise ExecutionSecurityError(f"output_limit_bytes must be between 1 and {MAX_OUTPUT_BYTES}.")
    capture = _BoundedOutput(output_limit_bytes)
    creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    process = subprocess.Popen(
        list(command), cwd=str(cwd), env=dict(environment), stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False, close_fds=True,
        creationflags=creationflags, start_new_session=os.name != "nt",
    )
    assert process.stdout is not None and process.stderr is not None
    threads = [
        threading.Thread(target=_reader, args=(process.stdout, capture, "stdout"), daemon=True),
        threading.Thread(target=_reader, args=(process.stderr, capture, "stderr"), daemon=True),
    ]
    for thread in threads:
        thread.start()
    deadline = time.monotonic() + timeout_seconds
    timed_out = False
    output_exceeded = False
    while process.poll() is None:
        if capture.overflow.is_set():
            output_exceeded = True
            _terminate_process_tree(process)
            break
        if time.monotonic() >= deadline:
            timed_out = True
            _terminate_process_tree(process)
            break
        time.sleep(0.01)
    for thread in threads:
        thread.join(timeout=10)
    if any(thread.is_alive() for thread in threads):
        _terminate_process_tree(process)
        raise ExecutionSecurityError("Process output reader did not terminate cleanly.")
    if capture.overflow.is_set():
        output_exceeded = True
    return {
        "return_code": process.returncode,
        "timed_out": timed_out,
        "output_limit_exceeded": output_exceeded,
        "stdout": bytes(capture.data["stdout"]),
        "stderr": bytes(capture.data["stderr"]),
        "stdout_observed_bytes": capture.observed["stdout"],
        "stderr_observed_bytes": capture.observed["stderr"],
        "total_output_observed_bytes": capture.total_observed,
    }


__all__ = [
    "AUTHORITY_AUDIENCE", "ExecutionSecurityError", "MAX_JSON_BYTES", "PLAN_SCOPE",
    "build_operation_plan", "canonical_json", "claim_once", "copy_reviewed_bundle", "file_sha256",
    "issue_authority_proof", "publish_bytes_create_once", "read_strict_json_file", "remove_tree",
    "rename_no_replace", "required_capabilities", "resolve_reviewed_path", "run_bounded_process",
    "seal_authenticated_record", "select_helper", "sha256", "sign_authenticated_record", "utc_datetime",
    "validate_authority_proof", "validate_operation_plan", "verify_authenticated_record", "verify_sealed_record",
]
