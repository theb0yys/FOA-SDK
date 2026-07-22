#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Authenticated Windows elevation request for one controlled private bootstrapper bundle."""
from __future__ import annotations

import ctypes
import datetime as dt
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Callable, Mapping, Sequence

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for root in (
    INSTALLER_ROOT / "Bootstrapper" / "PackageEngine" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "ProcessLauncher" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "Security" / "Source",
    INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from capability_process_launcher import ProcessLaunchError, validate_launch_grant  # noqa: E402
from execution_security import (  # noqa: E402
    ExecutionSecurityError,
    canonical_json,
    claim_once,
    copy_reviewed_bundle,
    file_sha256,
    publish_bytes_create_once,
    read_strict_json_file,
    remove_tree,
    resolve_reviewed_path,
    select_helper,
    sha256,
    seal_authenticated_record,
    sign_authenticated_record,
    utc_datetime,
    verify_authenticated_record,
    verify_sealed_record,
)
from package_engine import PackageEngineError, validate_engine_session  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS  # noqa: E402

ELEVATION_CAPABILITY = "package-engine.elevation"
CONSENT_SCOPE = "package-elevation-consent"
GRANT_SCOPE = "package-elevation-grant"
REQUEST_SCOPE = "package-elevation-bootstrap-request"
RESULT_SCOPE = "package-elevation-result"
MAX_GRANT_SECONDS = 300
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
CONSENT_STATEMENT = "The trusted installer authority explicitly approved one exact reviewed elevated helper launch."
GRANT_STATEMENT = "This grant authorizes one one-shot OS elevation prompt for the exact controlled bootstrap request and no arbitrary executable."
REQUEST_STATEMENT = "This request is the complete authenticated input for the controlled elevation bootstrapper, including exact environment and reviewed helper policy."
RESULT_STATEMENT = "This result records OS acceptance of one verified private-bootstrapper elevation request; completion is proven only by a separate bootstrapper receipt."


class ElevationError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ElevationError(f"{label} must be an object.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ElevationError(f"{label} must be non-empty trimmed text of at most {maximum} characters.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise ElevationError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise ElevationError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    try:
        utc_datetime(value, label)
    except ExecutionSecurityError as exc:
        raise ElevationError(str(exc)) from exc
    return str(value)


def _authority() -> dict[str, bool]:
    return {field: field == "elevation" for field in AUTHORITY_FIELDS}


def build_consent(
    *, authority_key_path: Path, consent_reference: str, approved_by: str, approved_at_utc: str,
    helper_reference: str, launch_grant_sha256: str, rationale: str,
) -> dict[str, object]:
    base = {
        "schema_version": 2,
        "consent_scope": CONSENT_SCOPE,
        "consent_reference": _reference(consent_reference, "consent_reference"),
        "approved": True,
        "approved_by": _text(approved_by, "approved_by", 160),
        "approved_at_utc": _utc(approved_at_utc, "approved_at_utc"),
        "helper_reference": _reference(helper_reference, "helper_reference"),
        "launch_grant_sha256": _hash(launch_grant_sha256, "launch_grant_sha256"),
        "rationale": _text(rationale, "rationale", 512),
        "statement": CONSENT_STATEMENT,
    }
    try:
        signed = sign_authenticated_record(base, authority_key_path=authority_key_path, signature_field="consent_signature")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Consent signing failed: {exc}") from exc
    return {**signed, "consent_sha256": sha256(signed)}


def validate_consent(consent: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(consent)
    if document.get("schema_version") != 2 or document.get("consent_scope") != CONSENT_SCOPE or document.get("approved") is not True or document.get("statement") != CONSENT_STATEMENT:
        raise ElevationError("Elevation consent contract is invalid.")
    declared = _hash(document.get("consent_sha256"), "consent_sha256")
    signed = {key: value for key, value in document.items() if key != "consent_sha256"}
    if sha256(signed) != declared:
        raise ElevationError("Elevation consent fingerprint does not match its content.")
    try:
        verify_authenticated_record(signed, authority_key_path=authority_key_path, signature_field="consent_signature")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Elevation consent authentication failed: {exc}") from exc
    expected = build_consent(
        authority_key_path=authority_key_path,
        consent_reference=_reference(document.get("consent_reference"), "consent_reference"),
        approved_by=_text(document.get("approved_by"), "approved_by", 160),
        approved_at_utc=_utc(document.get("approved_at_utc"), "approved_at_utc"),
        helper_reference=_reference(document.get("helper_reference"), "helper_reference"),
        launch_grant_sha256=_hash(document.get("launch_grant_sha256"), "launch_grant_sha256"),
        rationale=_text(document.get("rationale"), "rationale", 512),
    )
    if document != expected:
        raise ElevationError("Elevation consent is stale, altered, or not canonically derived.")
    return document


def build_elevation_grant(
    session: Mapping[str, object], launch_grant: Mapping[str, object], consent: Mapping[str, object], *,
    authority_key_path: Path, issuer: str, issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    try:
        checked_session = validate_engine_session(session, authority_key_path=authority_key_path)
        checked_launch = validate_launch_grant(launch_grant, authority_key_path=authority_key_path)
        checked_consent = validate_consent(consent, authority_key_path=authority_key_path)
        bootstrapper = select_helper(
            checked_session["operation_plan"], str(checked_session["operation_plan"]["bootstrapper_reference"]),
            role="elevation-bootstrapper",
        )
    except (PackageEngineError, ProcessLaunchError, ExecutionSecurityError, ElevationError) as exc:
        raise ElevationError(f"Elevation grant intake failed: {exc}") from exc
    capabilities = checked_session.get("authorized_capabilities")
    if not isinstance(capabilities, list) or ELEVATION_CAPABILITY not in capabilities:
        raise ElevationError("The authenticated session does not grant package-engine.elevation.")
    if checked_launch["session_sha256"] != checked_session["session_sha256"] or checked_launch.get("requires_elevation") is not True:
        raise ElevationError("Elevation requires an exact elevation-marked launch grant for this session.")
    if checked_consent["launch_grant_sha256"] != checked_launch["grant_sha256"] or checked_consent["helper_reference"] != checked_launch["helper_reference"]:
        raise ElevationError("Consent is not bound to the exact launch grant and reviewed helper.")
    issued = utc_datetime(issued_at_utc, "issued_at_utc")
    expires = utc_datetime(expires_at_utc, "expires_at_utc")
    approved = utc_datetime(checked_consent["approved_at_utc"], "consent.approved_at_utc")
    if issued < approved or expires <= issued or expires - issued > dt.timedelta(seconds=MAX_GRANT_SECONDS):
        raise ElevationError("Elevation grant must follow authenticated consent and expire within five minutes.")
    base = {
        "schema_version": 2,
        "grant_scope": GRANT_SCOPE,
        "capability": ELEVATION_CAPABILITY,
        "session_sha256": checked_session["session_sha256"],
        "launch_grant_sha256": checked_launch["grant_sha256"],
        "consent_sha256": checked_consent["consent_sha256"],
        "helper_reference": checked_launch["helper_reference"],
        "bootstrapper_reference": bootstrapper["helper_reference"],
        "bootstrapper": bootstrapper,
        "bootstrapper_support_files_sha256": sha256(bootstrapper.get("support_files", [])),
        "executable_sha256": checked_launch["executable_sha256"],
        "argv_sha256": checked_launch["argv_sha256"],
        "environment_sha256": checked_launch["environment_sha256"],
        "issuer": _text(issuer, "issuer", 160),
        "issued_at_utc": _utc(issued_at_utc, "issued_at_utc"),
        "expires_at_utc": _utc(expires_at_utc, "expires_at_utc"),
        "nonce": _reference(nonce, "nonce"),
        "session": checked_session,
        "launch_grant": checked_launch,
        "consent": checked_consent,
        "statement": GRANT_STATEMENT,
        "authority": _authority(),
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="grant_sha256")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Elevation grant authentication failed: {exc}") from exc


def validate_elevation_grant(grant: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(grant)
    if document.get("schema_version") != 2 or document.get("grant_scope") != GRANT_SCOPE or document.get("capability") != ELEVATION_CAPABILITY or document.get("statement") != GRANT_STATEMENT:
        raise ElevationError("Elevation grant contract is invalid.")
    if document.get("authority") != _authority():
        raise ElevationError("Elevation grant must grant elevation only.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="grant_sha256")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Elevation grant authentication failed: {exc}") from exc
    expected = build_elevation_grant(
        _object(document.get("session"), "grant.session"),
        _object(document.get("launch_grant"), "grant.launch_grant"),
        _object(document.get("consent"), "grant.consent"),
        authority_key_path=authority_key_path,
        issuer=_text(document.get("issuer"), "issuer", 160),
        issued_at_utc=_utc(document.get("issued_at_utc"), "issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "expires_at_utc"),
        nonce=_reference(document.get("nonce"), "nonce"),
    )
    if document != expected:
        raise ElevationError("Elevation grant is stale, altered, or not canonically derived.")
    return document


def build_bootstrap_request(
    grant: Mapping[str, object], *, authority_key_path: Path, request_reference: str, requested_at_utc: str
) -> dict[str, object]:
    checked = validate_elevation_grant(grant, authority_key_path=authority_key_path)
    requested = utc_datetime(requested_at_utc, "requested_at_utc")
    if requested < utc_datetime(checked["issued_at_utc"], "issued_at_utc") or requested > utc_datetime(checked["expires_at_utc"], "expires_at_utc"):
        raise ElevationError("Elevation grant is not valid at request creation.")
    launch = _object(checked["launch_grant"], "grant.launch_grant")
    base = {
        "schema_version": 2,
        "request_scope": REQUEST_SCOPE,
        "request_reference": _reference(request_reference, "request_reference"),
        "session_sha256": checked["session_sha256"],
        "elevation_grant_sha256": checked["grant_sha256"],
        "launch_grant_sha256": checked["launch_grant_sha256"],
        "helper": dict(launch["helper"]),
        "bootstrapper": dict(checked["bootstrapper"]),
        "requested_at_utc": _utc(requested_at_utc, "requested_at_utc"),
        "elevation_grant": checked,
        "statement": REQUEST_STATEMENT,
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="request_sha256")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Bootstrap request authentication failed: {exc}") from exc


def validate_bootstrap_request(request: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(request)
    if document.get("schema_version") != 2 or document.get("request_scope") != REQUEST_SCOPE or document.get("statement") != REQUEST_STATEMENT:
        raise ElevationError("Bootstrap request contract is invalid.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="request_sha256")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Bootstrap request authentication failed: {exc}") from exc
    expected = build_bootstrap_request(
        _object(document.get("elevation_grant"), "request.elevation_grant"), authority_key_path=authority_key_path,
        request_reference=_reference(document.get("request_reference"), "request_reference"),
        requested_at_utc=_utc(document.get("requested_at_utc"), "requested_at_utc"),
    )
    if document != expected:
        raise ElevationError("Bootstrap request is stale, altered, or not canonically derived.")
    return document


def _claim_path(claim_root: Path, grant_sha256: str) -> Path:
    return Path(claim_root) / "claim.package-elevation-grant" / f"{grant_sha256}.claim.json"


def _reject_consumed_grant(claim_root: Path, grant_sha256: str) -> None:
    target = _claim_path(claim_root, grant_sha256)
    if target.is_symlink():
        raise ElevationError(f"Elevation claim path must not be a symbolic link: {target}")
    if target.exists():
        if not target.is_file():
            raise ElevationError(f"Elevation claim path is not a regular file: {target}")
        raise ElevationError(f"Artifact has already been consumed: {grant_sha256}")


def _publish_request(request: Mapping[str, object], root: Path) -> Path:
    directory = Path(root)
    if not directory.is_absolute() or directory.is_symlink() or not directory.is_dir():
        raise ElevationError("Bootstrap request root must be an absolute existing non-symlink directory.")
    target = directory / f"{request['request_sha256']}.foa-elevation-request.json"
    payload = canonical_json(request)
    if target.exists() and not target.is_symlink():
        try:
            existing = read_strict_json_file(target, "Bootstrap request", require_canonical=True)
        except ExecutionSecurityError as exc:
            raise ElevationError(str(exc)) from exc
        if existing == dict(request):
            return target
        raise ElevationError("Bootstrap request destination already exists with different canonical content.")
    try:
        return publish_bytes_create_once(target, payload, label="Bootstrap request")
    except ExecutionSecurityError as exc:
        raise ElevationError(str(exc)) from exc


def _windows_runas(bootstrapper: Path, parameters: Sequence[str], working_directory: Path) -> int:
    if os.name != "nt":
        raise ElevationError("The production elevation backend is available only on Windows.")
    command_line = subprocess.list2cmdline(list(parameters))
    result = ctypes.windll.shell32.ShellExecuteW(None, "runas", str(bootstrapper), command_line, str(working_directory), 1)
    code = int(result)
    if code <= 32:
        raise ElevationError(f"Windows rejected the controlled elevation request with ShellExecute code {code}.")
    return code


def request_elevation(
    grant: Mapping[str, object], execution_root: Path, request_root: Path, *, authority_key_path: Path,
    claim_root: Path, request_reference: str, requested_at_utc: str,
    backend: Callable[[Path, Sequence[str], Path], int] | None = None,
) -> dict[str, object]:
    checked = validate_elevation_grant(grant, authority_key_path=authority_key_path)
    requested = utc_datetime(requested_at_utc, "requested_at_utc")
    if requested < utc_datetime(checked["issued_at_utc"], "issued_at_utc") or requested > utc_datetime(checked["expires_at_utc"], "expires_at_utc"):
        raise ElevationError("Elevation grant is not valid at request time.")
    grant_sha = str(checked["grant_sha256"])
    _reject_consumed_grant(Path(claim_root), grant_sha)
    bootstrapper = _object(checked["bootstrapper"], "grant.bootstrapper")
    private_directory = Path(claim_root) / "private-executables" / grant_sha
    request_path: Path | None = None
    if private_directory.exists() and not private_directory.is_symlink():
        try:
            remove_tree(private_directory)
        except OSError as exc:
            raise ElevationError(f"Stale private bootstrapper bundle could not be removed: {exc}") from exc
    try:
        cwd = resolve_reviewed_path(
            execution_root, str(bootstrapper["working_directory"]), file_required=False,
            label="Elevation bootstrapper working directory",
        )
        private_bootstrapper, private_directory = copy_reviewed_bundle(
            Path(execution_root), bootstrapper, Path(claim_root), grant_sha
        )
        actual_hash, executable_size = file_sha256(private_bootstrapper)
        if actual_hash != bootstrapper["executable_sha256"]:
            raise ElevationError("Private elevation bootstrapper hash does not match the signed operation plan.")
        request = build_bootstrap_request(
            checked, authority_key_path=authority_key_path,
            request_reference=request_reference, requested_at_utc=requested_at_utc,
        )
        request_path = _publish_request(request, request_root)
        try:
            claim = claim_once(
                claim_root, authority_key_path=authority_key_path,
                claim_kind="claim.package-elevation-grant", artifact_sha256=grant_sha,
                nonce=str(checked["nonce"]), claimed_at_utc=requested_at_utc,
            )
        except ExecutionSecurityError as exc:
            raise ElevationError(f"Elevation grant consumption failed: {exc}") from exc
        parameters = [*list(bootstrapper["argv"]), "--request", str(request_path)]
        backend_code = (backend or _windows_runas)(private_bootstrapper, parameters, cwd)
    except (OSError, ExecutionSecurityError, ElevationError) as exc:
        if request_path is not None and request_path.exists() and not request_path.is_symlink() and not _claim_path(Path(claim_root), grant_sha).exists():
            try:
                request_path.unlink()
            except OSError:
                pass
        if private_directory.exists() and not _claim_path(Path(claim_root), grant_sha).exists():
            try:
                remove_tree(private_directory)
            except OSError:
                pass
        if isinstance(exc, ElevationError):
            raise
        raise ElevationError(f"Elevation request preflight failed: {exc}") from exc
    base = {
        "schema_version": 2,
        "result_scope": RESULT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "launch_grant_sha256": checked["launch_grant_sha256"],
        "elevation_grant_sha256": checked["grant_sha256"],
        "consent_sha256": checked["consent_sha256"],
        "claim_sha256": claim["claim_sha256"],
        "request_sha256": request["request_sha256"],
        "bootstrapper_reference": checked["bootstrapper_reference"],
        "bootstrapper_sha256": actual_hash,
        "bootstrapper_size_bytes": executable_size,
        "bootstrapper_support_files_sha256": checked["bootstrapper_support_files_sha256"],
        "bootstrapper_argv_sha256": sha256(bootstrapper["argv"]),
        "bootstrapper_environment_sha256": sha256(bootstrapper["environment"]),
        "request_parameters_sha256": sha256(parameters),
        "requested_at_utc": _utc(requested_at_utc, "requested_at_utc"),
        "backend_code": backend_code,
        "elevation_requested": True,
        "controlled_bootstrapper_only": True,
        "private_bootstrapper_bundle_used": True,
        "consent_ui_suppressed": False,
        "credentials_collected": False,
        "process_completion_observed": False,
        "statement": RESULT_STATEMENT,
        "authority": _authority(),
        "elevation_grant": checked,
        "bootstrap_request": request,
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="result_sha256")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Elevation result authentication failed: {exc}") from exc


def validate_elevation_result(result: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(result)
    if document.get("schema_version") != 2 or document.get("result_scope") != RESULT_SCOPE or document.get("statement") != RESULT_STATEMENT:
        raise ElevationError("Elevation result contract is invalid.")
    checked_grant = validate_elevation_grant(_object(document.get("elevation_grant"), "result.elevation_grant"), authority_key_path=authority_key_path)
    checked_request = validate_bootstrap_request(_object(document.get("bootstrap_request"), "result.bootstrap_request"), authority_key_path=authority_key_path)
    if document.get("elevation_grant_sha256") != checked_grant["grant_sha256"] or document.get("request_sha256") != checked_request["request_sha256"]:
        raise ElevationError("Elevation result is not bound to the authenticated grant and bootstrap request.")
    for field, expected in (
        ("elevation_requested", True), ("controlled_bootstrapper_only", True),
        ("private_bootstrapper_bundle_used", True), ("consent_ui_suppressed", False),
        ("credentials_collected", False), ("process_completion_observed", False),
    ):
        if document.get(field) is not expected:
            raise ElevationError(f"Elevation result flag {field} is invalid.")
    if document.get("bootstrapper_support_files_sha256") != checked_grant["bootstrapper_support_files_sha256"]:
        raise ElevationError("Elevation result support-file binding is invalid.")
    if document.get("authority") != _authority():
        raise ElevationError("Elevation result must grant elevation only.")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="result_sha256")
    except ExecutionSecurityError as exc:
        raise ElevationError(f"Elevation result authentication failed: {exc}") from exc
    return document


__all__ = [
    "ELEVATION_CAPABILITY", "ElevationError", "build_bootstrap_request", "build_consent", "build_elevation_grant",
    "request_elevation", "validate_bootstrap_request", "validate_consent", "validate_elevation_grant", "validate_elevation_result",
]
