#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Authenticated reviewed-helper launcher with one-shot, verified bundles, and bounded execution."""
from __future__ import annotations

import datetime as dt
import hashlib
import re
import sys
from pathlib import Path
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
    copy_reviewed_bundle,
    file_sha256,
    remove_tree,
    resolve_reviewed_path,
    run_bounded_process,
    seal_authenticated_record,
    select_helper,
    sha256,
    utc_datetime,
    verify_sealed_record,
)
from package_engine import PackageEngineError, validate_engine_session  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS  # noqa: E402

LAUNCH_CAPABILITY = "package-engine.launch-process"
GRANT_SCOPE = "package-process-launch-grant"
RESULT_SCOPE = "package-process-launch-result"
MAX_GRANT_SECONDS = 900
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
GRANT_STATEMENT = (
    "This authenticated grant selects one exact helper and its reviewed support files from the signed "
    "operation plan. Executable, argv, working directory, environment, timeout, and output bound are not caller-authored."
)
RESULT_STATEMENT = (
    "This result records one one-shot bounded non-shell launch of a verified private copy of the reviewed helper bundle."
)


class ProcessLaunchError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ProcessLaunchError(f"{label} must be an object.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ProcessLaunchError(f"{label} must be non-empty trimmed text of at most {maximum} characters.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise ProcessLaunchError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise ProcessLaunchError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    try:
        utc_datetime(value, label)
    except ExecutionSecurityError as exc:
        raise ProcessLaunchError(str(exc)) from exc
    return str(value)


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def build_launch_grant(
    session: Mapping[str, object], *, authority_key_path: Path, helper_reference: str,
    issuer: str, issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    try:
        checked = validate_engine_session(session, authority_key_path=authority_key_path)
        helper = select_helper(checked["operation_plan"], helper_reference, role="operation-helper")
    except (PackageEngineError, ExecutionSecurityError) as exc:
        raise ProcessLaunchError(f"Reviewed launch intake failed: {exc}") from exc
    capabilities = checked.get("authorized_capabilities")
    if not isinstance(capabilities, list) or LAUNCH_CAPABILITY not in capabilities:
        raise ProcessLaunchError("The authenticated session does not grant package-engine.launch-process.")
    issued = utc_datetime(issued_at_utc, "issued_at_utc")
    expires = utc_datetime(expires_at_utc, "expires_at_utc")
    accepted = utc_datetime(checked["accepted_at_utc"], "session.accepted_at_utc")
    if issued < accepted or expires <= issued or expires - issued > dt.timedelta(seconds=MAX_GRANT_SECONDS):
        raise ProcessLaunchError("Launch grant must follow session acceptance and expire within 15 minutes.")
    base = {
        "schema_version": 2,
        "grant_scope": GRANT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "operation_plan_sha256": checked["operation_plan_sha256"],
        "capability": LAUNCH_CAPABILITY,
        "helper_reference": helper["helper_reference"],
        "helper": helper,
        "executable_sha256": helper["executable_sha256"],
        "support_files_sha256": sha256(helper.get("support_files", [])),
        "argv_sha256": sha256(helper["argv"]),
        "environment_sha256": sha256(helper["environment"]),
        "working_directory": helper["working_directory"],
        "timeout_seconds": helper["timeout_seconds"],
        "output_limit_bytes": helper["output_limit_bytes"],
        "requires_elevation": helper["requires_elevation"],
        "issuer": _text(issuer, "issuer", 160),
        "issued_at_utc": _utc(issued_at_utc, "issued_at_utc"),
        "expires_at_utc": _utc(expires_at_utc, "expires_at_utc"),
        "nonce": _reference(nonce, "nonce"),
        "session": checked,
        "statement": GRANT_STATEMENT,
        "authority": _authority(),
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="grant_sha256")
    except ExecutionSecurityError as exc:
        raise ProcessLaunchError(f"Launch grant authentication failed: {exc}") from exc


def validate_launch_grant(grant: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(grant)
    if document.get("schema_version") != 2 or document.get("grant_scope") != GRANT_SCOPE or document.get("capability") != LAUNCH_CAPABILITY or document.get("statement") != GRANT_STATEMENT:
        raise ProcessLaunchError("Launch grant contract is invalid.")
    if document.get("authority") != _authority():
        raise ProcessLaunchError("Launch grant authority must remain all false.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="grant_sha256")
    except ExecutionSecurityError as exc:
        raise ProcessLaunchError(f"Launch grant authentication failed: {exc}") from exc
    expected = build_launch_grant(
        _object(document.get("session"), "grant.session"), authority_key_path=authority_key_path,
        helper_reference=_reference(document.get("helper_reference"), "helper_reference"),
        issuer=_text(document.get("issuer"), "issuer", 160),
        issued_at_utc=_utc(document.get("issued_at_utc"), "issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "expires_at_utc"),
        nonce=_reference(document.get("nonce"), "nonce"),
    )
    if document != expected:
        raise ProcessLaunchError("Launch grant is stale, altered, or not canonically derived.")
    return document


def launch_process(
    grant: Mapping[str, object], execution_root: Path, *, authority_key_path: Path,
    claim_root: Path, launched_at_utc: str,
) -> dict[str, object]:
    checked = validate_launch_grant(grant, authority_key_path=authority_key_path)
    launched = utc_datetime(launched_at_utc, "launched_at_utc")
    if launched < utc_datetime(checked["issued_at_utc"], "issued_at_utc") or launched > utc_datetime(checked["expires_at_utc"], "expires_at_utc"):
        raise ProcessLaunchError("Launch grant is not valid at process start.")
    if checked.get("requires_elevation") is True:
        raise ProcessLaunchError("Reviewed helper requires the controlled elevation bootstrapper and cannot launch directly.")
    helper = _object(checked["helper"], "grant.helper")
    private_directory: Path | None = None
    try:
        working_directory = resolve_reviewed_path(
            execution_root, str(helper["working_directory"]), file_required=False, label="Reviewed working directory"
        )
        private_executable, private_directory = copy_reviewed_bundle(
            Path(execution_root), helper, Path(claim_root), str(checked["grant_sha256"])
        )
        actual_hash, executable_size = file_sha256(private_executable)
        if actual_hash != checked["executable_sha256"]:
            raise ProcessLaunchError("Verified private executable does not match the authenticated launch grant.")
    except (OSError, ExecutionSecurityError) as exc:
        if private_directory is not None:
            remove_tree(private_directory)
        raise ProcessLaunchError(f"Reviewed launch preflight failed: {exc}") from exc

    try:
        try:
            claim = claim_once(
                claim_root, authority_key_path=authority_key_path,
                claim_kind="claim.package-launch-grant", artifact_sha256=str(checked["grant_sha256"]),
                nonce=str(checked["nonce"]), claimed_at_utc=launched_at_utc,
            )
        except ExecutionSecurityError as exc:
            raise ProcessLaunchError(f"Launch grant consumption failed: {exc}") from exc
        try:
            outcome = run_bounded_process(
                [str(private_executable), *list(helper["argv"])], cwd=working_directory,
                environment=dict(helper["environment"]), timeout_seconds=int(helper["timeout_seconds"]),
                output_limit_bytes=int(helper["output_limit_bytes"]),
            )
        except (OSError, ExecutionSecurityError) as exc:
            raise ProcessLaunchError(f"Reviewed helper launch failed: {exc}") from exc
    finally:
        if private_directory is not None:
            remove_tree(private_directory)

    stdout = bytes(outcome.pop("stdout"))
    stderr = bytes(outcome.pop("stderr"))
    base = {
        "schema_version": 2,
        "result_scope": RESULT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "grant_sha256": checked["grant_sha256"],
        "claim_sha256": claim["claim_sha256"],
        "helper_reference": checked["helper_reference"],
        "executable_sha256": actual_hash,
        "executable_size_bytes": executable_size,
        "support_files_sha256": checked["support_files_sha256"],
        "argv_sha256": checked["argv_sha256"],
        "environment_sha256": checked["environment_sha256"],
        "working_directory": checked["working_directory"],
        "launched_at_utc": _utc(launched_at_utc, "launched_at_utc"),
        **outcome,
        "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
        "stdout_size_bytes": len(stdout),
        "stderr_sha256": hashlib.sha256(stderr).hexdigest(),
        "stderr_size_bytes": len(stderr),
        "process_launched": True,
        "elevation_requested": False,
        "shell_used": False,
        "immutable_private_copy_used": True,
        "private_bundle_removed": True,
        "statement": RESULT_STATEMENT,
        "authority": _authority(),
        "launch_grant": checked,
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="result_sha256")
    except ExecutionSecurityError as exc:
        raise ProcessLaunchError(f"Launch result authentication failed: {exc}") from exc


def validate_launch_result(result: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(result)
    if document.get("schema_version") != 2 or document.get("result_scope") != RESULT_SCOPE or document.get("statement") != RESULT_STATEMENT:
        raise ProcessLaunchError("Launch result contract is invalid.")
    checked_grant = validate_launch_grant(_object(document.get("launch_grant"), "result.launch_grant"), authority_key_path=authority_key_path)
    if document.get("grant_sha256") != checked_grant["grant_sha256"] or document.get("session_sha256") != checked_grant["session_sha256"]:
        raise ProcessLaunchError("Launch result is not bound to its authenticated grant.")
    for field, expected in (
        ("process_launched", True), ("elevation_requested", False), ("shell_used", False),
        ("immutable_private_copy_used", True), ("private_bundle_removed", True),
    ):
        if document.get(field) is not expected:
            raise ProcessLaunchError(f"Launch result flag {field} is invalid.")
    if document.get("support_files_sha256") != checked_grant["support_files_sha256"]:
        raise ProcessLaunchError("Launch result support-file binding is invalid.")
    if document.get("authority") != _authority():
        raise ProcessLaunchError("Launch result authority must remain all false.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="result_sha256")
    except ExecutionSecurityError as exc:
        raise ProcessLaunchError(f"Launch result authentication failed: {exc}") from exc
    return document


__all__ = ["LAUNCH_CAPABILITY", "ProcessLaunchError", "build_launch_grant", "launch_process", "validate_launch_grant", "validate_launch_result"]
