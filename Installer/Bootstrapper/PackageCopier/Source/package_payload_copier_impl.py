#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Authenticated, one-shot, hash-verified package payload staging."""
from __future__ import annotations

import datetime as dt
import os
import re
import sys
from pathlib import Path, PurePosixPath
from typing import Mapping

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for root in (
    INSTALLER_ROOT / "Bootstrapper" / "PackageEngine" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "Security" / "Source",
    INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from execution_security import (  # noqa: E402
    ExecutionSecurityError,
    claim_once,
    file_sha256,
    remove_tree,
    rename_no_replace,
    seal_authenticated_record,
    sha256,
    utc_datetime,
    verify_sealed_record,
)
from package_engine import PackageEngineError, validate_engine_session  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS  # noqa: E402

COPY_CAPABILITY = "package-engine.copy-payload"
GRANT_SCOPE = "package-payload-copy-grant"
RECEIPT_SCOPE = "package-payload-copy-receipt"
MAX_GRANT_SECONDS = 1800
REF_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
HASH_RE = re.compile(r"^[0-9a-f]{64}$")
GRANT_STATEMENT = "This grant authorizes one one-shot hash-verified payload staging operation for one authenticated package-engine session."
RECEIPT_STATEMENT = "This receipt records atomic no-replace publication of one complete hash-verified staging tree."


class PackageCopyError(RuntimeError):
    pass


def _obj(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise PackageCopyError(f"{label} must be an object.")
    return value


def _arr(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise PackageCopyError(f"{label} must be an array.")
    return value


def _text(value: object, label: str) -> str:
    if not isinstance(value, str) or not value or value != value.strip():
        raise PackageCopyError(f"{label} must be non-empty trimmed text.")
    return value


def _ref(value: object, label: str) -> str:
    result = _text(value, label)
    if len(result) > 128 or REF_RE.fullmatch(result) is None:
        raise PackageCopyError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label)
    if HASH_RE.fullmatch(result) is None:
        raise PackageCopyError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    try:
        utc_datetime(value, label)
    except ExecutionSecurityError as exc:
        raise PackageCopyError(str(exc)) from exc
    return str(value)


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _relative(value: object, label: str) -> str:
    result = _text(value, label)
    path = PurePosixPath(result)
    if path.is_absolute() or "\\" in result or "//" in result or any(part in {"", ".", ".."} for part in path.parts) or len(result) > 1024:
        raise PackageCopyError(f"{label} must be a safe relative POSIX path.")
    return result


def _no_links(path: Path, label: str) -> None:
    current = path
    while True:
        if current.is_symlink():
            raise PackageCopyError(f"{label} contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def _plan(session: Mapping[str, object]) -> dict[str, object]:
    return _obj(_obj(_obj(session.get("handoff"), "session.handoff").get("receipt"), "session.handoff.receipt").get("plan"), "session.handoff.receipt.plan")


def _inventory(session: Mapping[str, object]) -> list[dict[str, object]]:
    plan = _plan(session)
    rows: list[dict[str, object]] = []
    destinations: set[str] = set()
    for package_index, raw_package in enumerate(_arr(plan.get("packages"), "plan.packages")):
        package = _obj(raw_package, f"plan.packages[{package_index}]")
        package_id = _ref(package.get("package_id"), "package_id")
        package_root = _relative(_obj(package.get("source"), f"{package_id}.source").get("path"), f"{package_id}.source.path")
        for file_index, raw_item in enumerate(_arr(package.get("payload"), f"{package_id}.payload")):
            item = _obj(raw_item, f"{package_id}.payload[{file_index}]")
            destination = _relative(item.get("destination"), "destination")
            key = destination.casefold()
            if key in destinations:
                raise PackageCopyError(f"Duplicate or case-colliding destination: {destination}")
            destinations.add(key)
            size = item.get("size_bytes")
            if type(size) is not int or size < 0:
                raise PackageCopyError("size_bytes must be non-negative.")
            source = (PurePosixPath(package_root) / PurePosixPath(_relative(item.get("source"), "source"))).as_posix()
            rows.append({
                "package_id": package_id,
                "source": source,
                "destination": destination,
                "sha256": _hash(item.get("sha256"), "sha256"),
                "size_bytes": size,
                "redistribution": _text(item.get("redistribution"), "redistribution"),
            })
    rows.sort(key=lambda row: (str(row["destination"]).casefold(), str(row["destination"])))
    summary = _obj(plan.get("summary"), "plan.summary")
    if len(rows) != summary.get("payload_file_count") or sum(int(row["size_bytes"]) for row in rows) != summary.get("payload_size_bytes"):
        raise PackageCopyError("Payload inventory does not match resolver summary.")
    return rows


def build_copy_grant(
    session: Mapping[str, object], *, authority_key_path: Path, issuer: str,
    issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    try:
        checked = validate_engine_session(session, authority_key_path=authority_key_path)
    except PackageEngineError as exc:
        raise PackageCopyError(f"Package-engine session verification failed: {exc}") from exc
    capabilities = checked.get("authorized_capabilities")
    if not isinstance(capabilities, list) or COPY_CAPABILITY not in capabilities:
        raise PackageCopyError("The authenticated session does not grant package-engine.copy-payload.")
    issued = utc_datetime(issued_at_utc, "issued_at_utc")
    expires = utc_datetime(expires_at_utc, "expires_at_utc")
    accepted = utc_datetime(checked["accepted_at_utc"], "session.accepted_at_utc")
    if issued < accepted or expires <= issued or expires - issued > dt.timedelta(seconds=MAX_GRANT_SECONDS):
        raise PackageCopyError("Copy grant must follow session acceptance and expire within 30 minutes.")
    inventory = _inventory(checked)
    base = {
        "schema_version": 2,
        "grant_scope": GRANT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "capability": COPY_CAPABILITY,
        "issuer": _text(issuer, "issuer"),
        "issued_at_utc": _utc(issued_at_utc, "issued_at_utc"),
        "expires_at_utc": _utc(expires_at_utc, "expires_at_utc"),
        "nonce": _ref(nonce, "nonce"),
        "inventory_sha256": sha256(inventory),
        "session": checked,
        "statement": GRANT_STATEMENT,
        "authority": _authority(),
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="grant_sha256")
    except ExecutionSecurityError as exc:
        raise PackageCopyError(f"Copy grant authentication failed: {exc}") from exc


def validate_copy_grant(grant: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(grant)
    if document.get("schema_version") != 2 or document.get("grant_scope") != GRANT_SCOPE or document.get("capability") != COPY_CAPABILITY or document.get("statement") != GRANT_STATEMENT:
        raise PackageCopyError("Copy grant contract is invalid.")
    if document.get("authority") != _authority():
        raise PackageCopyError("Copy grant authority must remain all false.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="grant_sha256")
    except ExecutionSecurityError as exc:
        raise PackageCopyError(f"Copy grant authentication failed: {exc}") from exc
    expected = build_copy_grant(
        _obj(document.get("session"), "grant.session"), authority_key_path=authority_key_path,
        issuer=_text(document.get("issuer"), "issuer"),
        issued_at_utc=_utc(document.get("issued_at_utc"), "issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "expires_at_utc"),
        nonce=_ref(document.get("nonce"), "nonce"),
    )
    if document != expected:
        raise PackageCopyError("Copy grant is stale, altered, or not canonically derived.")
    return document


def _copy_file(source: Path, destination: Path, expected_hash: str, expected_size: int) -> None:
    if source.is_symlink() or not source.is_file():
        raise PackageCopyError(f"Payload source is not a regular non-symlink file: {source}")
    _no_links(source.parent, "Payload source")
    if file_sha256(source) != (expected_hash, expected_size):
        raise PackageCopyError(f"Payload source hash or size mismatch: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0)
    output = os.open(destination, flags, 0o600)
    input_fd = os.open(source, os.O_RDONLY | (os.O_NOFOLLOW if hasattr(os, "O_NOFOLLOW") else 0))
    try:
        while True:
            chunk = os.read(input_fd, 1024 * 1024)
            if not chunk:
                break
            view = memoryview(chunk)
            while view:
                written = os.write(output, view)
                if written <= 0:
                    raise PackageCopyError("Payload copy made no forward progress.")
                view = view[written:]
        os.fsync(output)
    finally:
        os.close(input_fd)
        os.close(output)
    if file_sha256(destination) != (expected_hash, expected_size):
        raise PackageCopyError(f"Staged payload verification failed: {destination}")


def stage_payload(
    grant: Mapping[str, object], source_root: Path, staging_root: Path, *, authority_key_path: Path,
    claim_root: Path, copied_at_utc: str,
) -> dict[str, object]:
    checked = validate_copy_grant(grant, authority_key_path=authority_key_path)
    copied = utc_datetime(copied_at_utc, "copied_at_utc")
    if copied < utc_datetime(checked["issued_at_utc"], "issued_at_utc") or copied > utc_datetime(checked["expires_at_utc"], "expires_at_utc"):
        raise PackageCopyError("Copy grant is not valid at staging time.")
    source = Path(source_root)
    target = Path(staging_root)
    if not source.is_absolute() or not source.is_dir() or source.is_symlink():
        raise PackageCopyError("Source root must be an absolute existing non-symlink directory.")
    _no_links(source, "Source root")
    if not target.is_absolute() or target.exists() or target.is_symlink():
        raise PackageCopyError("Staging root must be an absent absolute path.")
    if not target.parent.is_dir() or target.parent.is_symlink():
        raise PackageCopyError("Staging parent must be an existing non-symlink directory.")
    _no_links(target.parent, "Staging parent")
    inventory = _inventory(_obj(checked.get("session"), "grant.session"))
    temporary = target.parent / f".{target.name}.{checked['grant_sha256']}.tmp"
    if temporary.exists() or temporary.is_symlink():
        raise PackageCopyError("Deterministic temporary staging directory already exists.")
    temporary.mkdir(mode=0o700)
    copied_rows: list[dict[str, object]] = []
    try:
        for row in inventory:
            source_file = source.joinpath(*PurePosixPath(str(row["source"])).parts)
            destination = temporary.joinpath(*PurePosixPath(str(row["destination"])).parts)
            _copy_file(source_file, destination, str(row["sha256"]), int(row["size_bytes"]))
            copied_rows.append(dict(row))
        # Authority is consumed only after all caller-controlled paths and payload bytes have passed preflight.
        try:
            claim = claim_once(
                claim_root, authority_key_path=authority_key_path,
                claim_kind="claim.package-copy-grant", artifact_sha256=str(checked["grant_sha256"]),
                nonce=str(checked["nonce"]), claimed_at_utc=copied_at_utc,
            )
        except ExecutionSecurityError as exc:
            raise PackageCopyError(f"Copy grant consumption failed: {exc}") from exc
        rename_no_replace(temporary, target)
    except (OSError, ExecutionSecurityError, PackageCopyError):
        remove_tree(temporary)
        raise
    base = {
        "schema_version": 2,
        "receipt_scope": RECEIPT_SCOPE,
        "grant_sha256": checked["grant_sha256"],
        "session_sha256": checked["session_sha256"],
        "claim_sha256": claim["claim_sha256"],
        "inventory_sha256": checked["inventory_sha256"],
        "copied_at_utc": _utc(copied_at_utc, "copied_at_utc"),
        "staging_reference": _ref("staging.foa-sdk.payload", "staging_reference"),
        "file_count": len(copied_rows),
        "size_bytes": sum(int(row["size_bytes"]) for row in copied_rows),
        "files": copied_rows,
        "copy_performed": True,
        "process_launched": False,
        "elevation_requested": False,
        "installation_finalized": False,
        "statement": RECEIPT_STATEMENT,
        "authority": _authority(),
        "copy_grant": checked,
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="copy_receipt_sha256")
    except ExecutionSecurityError as exc:
        raise PackageCopyError(f"Copy receipt authentication failed: {exc}") from exc


def validate_copy_receipt(receipt: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(receipt)
    if document.get("schema_version") != 2 or document.get("receipt_scope") != RECEIPT_SCOPE or document.get("statement") != RECEIPT_STATEMENT:
        raise PackageCopyError("Copy receipt contract is invalid.")
    checked_grant = validate_copy_grant(_obj(document.get("copy_grant"), "receipt.copy_grant"), authority_key_path=authority_key_path)
    if document.get("grant_sha256") != checked_grant["grant_sha256"] or document.get("session_sha256") != checked_grant["session_sha256"]:
        raise PackageCopyError("Copy receipt is not bound to its authenticated grant.")
    if any(document.get(field) is not expected for field, expected in (("copy_performed", True), ("process_launched", False), ("elevation_requested", False), ("installation_finalized", False))):
        raise PackageCopyError("Copy receipt operational flags are invalid.")
    if document.get("authority") != _authority():
        raise PackageCopyError("Copy receipt authority must remain all false.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="copy_receipt_sha256")
    except ExecutionSecurityError as exc:
        raise PackageCopyError(f"Copy receipt authentication failed: {exc}") from exc
    return document


__all__ = ["COPY_CAPABILITY", "PackageCopyError", "build_copy_grant", "stage_payload", "validate_copy_grant", "validate_copy_receipt"]
