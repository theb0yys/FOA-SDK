#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Explicit-consent, capability-gated Windows elevation helper."""
from __future__ import annotations

import ctypes
import datetime as dt
import os
import re
import sys
from pathlib import Path
from typing import Callable, Mapping

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
ENGINE_SOURCE = INSTALLER_ROOT / "Bootstrapper" / "PackageEngine" / "Source"
LAUNCHER_SOURCE = INSTALLER_ROOT / "Bootstrapper" / "ProcessLauncher" / "Source"
VIEW_MODEL_SOURCE = INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source"
for root in (ENGINE_SOURCE, LAUNCHER_SOURCE, VIEW_MODEL_SOURCE):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from capability_process_launcher import ProcessLaunchError, validate_launch_grant  # noqa: E402
from package_engine import PackageEngineError, validate_engine_session  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS, sha256  # noqa: E402

ELEVATION_CAPABILITY = "package-engine.elevation"
GRANT_SCOPE = "package-elevation-grant"
RESULT_SCOPE = "package-elevation-result"
MAX_GRANT_SECONDS = 300
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
CONSENT_STATEMENT = (
    "The user explicitly approved one exact elevated process request after reviewing the "
    "executable identity, argument fingerprint, target operation, and elevation rationale."
)
GRANT_STATEMENT = (
    "This grant authorizes one operating-system elevation prompt for one exact verified "
    "package-engine session and one exact process-launch grant. It contains no credential, "
    "cannot suppress consent UI, and grants no lifecycle finalization or later authority."
)
RESULT_STATEMENT = (
    "This result records whether the operating system accepted or rejected one explicit "
    "elevation request. It does not prove installer success or grant later authority."
)


class ElevationError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ElevationError(f"{label} must be an object.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ElevationError(f"{label} must be a non-empty trimmed string of at most {maximum} characters.")
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise ElevationError(f"{label} contains a forbidden control character.")
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
    result = _text(value, label, 64)
    if not result.endswith("Z") or "T" not in result:
        raise ElevationError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise ElevationError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise ElevationError(f"{label} must use UTC.")
    return result


def _utc_dt(value: object, label: str) -> dt.datetime:
    return dt.datetime.fromisoformat(_utc(value, label)[:-1] + "+00:00")


def _authority() -> dict[str, bool]:
    return {field: field == "elevation" for field in AUTHORITY_FIELDS}


def _validate_authority(value: object, label: str) -> None:
    authority = _object(value, label)
    expected = _authority()
    if authority != expected:
        raise ElevationError(f"{label} must grant elevation only and no other authority.")


def build_consent(
    *, consent_reference: str, approved_by: str, approved_at_utc: str,
    executable_reference: str, launch_grant_sha256: str, rationale: str,
) -> dict[str, object]:
    base = {
        "schema_version": 1,
        "consent_reference": _reference(consent_reference, "consent_reference"),
        "approved": True,
        "approved_by": _text(approved_by, "approved_by", 160),
        "approved_at_utc": _utc(approved_at_utc, "approved_at_utc"),
        "executable_reference": _reference(executable_reference, "executable_reference"),
        "launch_grant_sha256": _hash(launch_grant_sha256, "launch_grant_sha256"),
        "rationale": _text(rationale, "rationale", 512),
        "statement": CONSENT_STATEMENT,
    }
    return {**base, "consent_sha256": sha256(base)}


def validate_consent(consent: Mapping[str, object]) -> dict[str, object]:
    document = dict(consent)
    if document.get("schema_version") != 1 or document.get("approved") is not True:
        raise ElevationError("Elevation consent must be schema 1 and explicitly approved.")
    if document.get("statement") != CONSENT_STATEMENT:
        raise ElevationError("Elevation consent statement was altered.")
    declared = _hash(document.get("consent_sha256"), "consent.consent_sha256")
    unsigned = {key: value for key, value in document.items() if key != "consent_sha256"}
    if sha256(unsigned) != declared:
        raise ElevationError("Elevation consent fingerprint does not match its content.")
    expected = build_consent(
        consent_reference=_reference(document.get("consent_reference"), "consent.consent_reference"),
        approved_by=_text(document.get("approved_by"), "consent.approved_by", 160),
        approved_at_utc=_utc(document.get("approved_at_utc"), "consent.approved_at_utc"),
        executable_reference=_reference(document.get("executable_reference"), "consent.executable_reference"),
        launch_grant_sha256=_hash(document.get("launch_grant_sha256"), "consent.launch_grant_sha256"),
        rationale=_text(document.get("rationale"), "consent.rationale", 512),
    )
    if document != expected:
        raise ElevationError("Elevation consent is stale, altered, or not canonically derived.")
    return document


def build_elevation_grant(
    session: Mapping[str, object], launch_grant: Mapping[str, object], consent: Mapping[str, object], *,
    issuer: str, issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    try:
        checked_session = validate_engine_session(session)
    except PackageEngineError as exc:
        raise ElevationError(f"Package-engine session verification failed: {exc}") from exc
    try:
        checked_launch = validate_launch_grant(launch_grant)
    except ProcessLaunchError as exc:
        raise ElevationError(f"Process-launch grant verification failed: {exc}") from exc
    checked_consent = validate_consent(consent)
    capabilities = checked_session.get("authorized_capabilities")
    if not isinstance(capabilities, list) or ELEVATION_CAPABILITY not in capabilities:
        raise ElevationError("The verified package-engine session does not grant elevation capability.")
    if checked_launch["session_sha256"] != checked_session["session_sha256"]:
        raise ElevationError("Launch grant is not bound to the exact elevation session.")
    if checked_consent["launch_grant_sha256"] != checked_launch["grant_sha256"]:
        raise ElevationError("Consent is not bound to the exact launch grant.")
    if checked_consent["executable_reference"] != checked_launch["executable_reference"]:
        raise ElevationError("Consent executable identity does not match the launch grant.")
    issued = _utc_dt(issued_at_utc, "issued_at_utc")
    expires = _utc_dt(expires_at_utc, "expires_at_utc")
    approved = _utc_dt(checked_consent["approved_at_utc"], "consent.approved_at_utc")
    if issued < approved:
        raise ElevationError("issued_at_utc must not precede explicit consent.")
    if expires <= issued or expires - issued > dt.timedelta(seconds=MAX_GRANT_SECONDS):
        raise ElevationError("Elevation grant expiry must be after issuance and within five minutes.")
    base = {
        "schema_version": 1,
        "grant_scope": GRANT_SCOPE,
        "capability": ELEVATION_CAPABILITY,
        "session_sha256": checked_session["session_sha256"],
        "launch_grant_sha256": checked_launch["grant_sha256"],
        "consent_sha256": checked_consent["consent_sha256"],
        "executable_reference": checked_launch["executable_reference"],
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
    return {**base, "grant_sha256": sha256(base)}


def validate_elevation_grant(grant: Mapping[str, object]) -> dict[str, object]:
    document = dict(grant)
    if document.get("schema_version") != 1 or document.get("grant_scope") != GRANT_SCOPE:
        raise ElevationError("Elevation grant schema or scope is invalid.")
    if document.get("capability") != ELEVATION_CAPABILITY or document.get("statement") != GRANT_STATEMENT:
        raise ElevationError("Elevation grant capability or statement is invalid.")
    _validate_authority(document.get("authority"), "grant.authority")
    declared = _hash(document.get("grant_sha256"), "grant.grant_sha256")
    unsigned = {key: value for key, value in document.items() if key != "grant_sha256"}
    if sha256(unsigned) != declared:
        raise ElevationError("Elevation grant fingerprint does not match its content.")
    expected = build_elevation_grant(
        _object(document.get("session"), "grant.session"),
        _object(document.get("launch_grant"), "grant.launch_grant"),
        _object(document.get("consent"), "grant.consent"),
        issuer=_text(document.get("issuer"), "grant.issuer", 160),
        issued_at_utc=_utc(document.get("issued_at_utc"), "grant.issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "grant.expires_at_utc"),
        nonce=_reference(document.get("nonce"), "grant.nonce"),
    )
    if document != expected:
        raise ElevationError("Elevation grant is stale, altered, or not canonically derived.")
    return document


def _windows_runas(executable: Path, parameters: str, working_directory: Path) -> int:
    if os.name != "nt":
        raise ElevationError("The production elevation backend is available only on Windows.")
    result = ctypes.windll.shell32.ShellExecuteW(
        None, "runas", str(executable), parameters, str(working_directory), 1
    )
    code = int(result)
    if code <= 32:
        raise ElevationError(f"Windows rejected the elevation request with ShellExecute code {code}.")
    return code


def request_elevation(
    grant: Mapping[str, object], executable_path: Path, working_directory: Path, *,
    requested_at_utc: str, backend: Callable[[Path, str, Path], int] | None = None,
) -> dict[str, object]:
    checked = validate_elevation_grant(grant)
    requested = _utc_dt(requested_at_utc, "requested_at_utc")
    if requested < _utc_dt(checked["issued_at_utc"], "grant.issued_at_utc"):
        raise ElevationError("requested_at_utc must not precede grant issuance.")
    if requested > _utc_dt(checked["expires_at_utc"], "grant.expires_at_utc"):
        raise ElevationError("Elevation grant expired before the operating-system prompt.")
    executable = Path(executable_path)
    cwd = Path(working_directory)
    if not executable.is_absolute() or executable.is_symlink() or not executable.is_file():
        raise ElevationError("Executable must be an absolute regular non-symlink file.")
    if not cwd.is_absolute() or cwd.is_symlink() or not cwd.is_dir():
        raise ElevationError("Working directory must be an absolute existing non-symlink directory.")
    from capability_process_launcher import _file_sha256, _reject_symlink_components
    _reject_symlink_components(executable.parent, "Executable path")
    _reject_symlink_components(cwd, "Working directory")
    actual_hash, executable_size = _file_sha256(executable)
    if actual_hash != checked["executable_sha256"]:
        raise ElevationError("Executable SHA-256 does not match the elevation grant.")
    launch = checked["launch_grant"]
    argv = launch["argv"]
    if os.name == "nt":
        parameters = subprocess_list2cmdline(argv)
    else:
        parameters = " ".join(argv)
    selected_backend = backend or _windows_runas
    backend_code = selected_backend(executable, parameters, cwd)
    base = {
        "schema_version": 1,
        "result_scope": RESULT_SCOPE,
        "session_sha256": checked["session_sha256"],
        "launch_grant_sha256": checked["launch_grant_sha256"],
        "elevation_grant_sha256": checked["grant_sha256"],
        "consent_sha256": checked["consent_sha256"],
        "executable_reference": checked["executable_reference"],
        "executable_sha256": actual_hash,
        "executable_size_bytes": executable_size,
        "argv_sha256": checked["argv_sha256"],
        "environment_sha256": checked["environment_sha256"],
        "requested_at_utc": _utc(requested_at_utc, "requested_at_utc"),
        "backend_code": backend_code,
        "elevation_requested": True,
        "consent_ui_suppressed": False,
        "credentials_collected": False,
        "process_completion_observed": False,
        "statement": RESULT_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "result_sha256": sha256(base)}


def subprocess_list2cmdline(argv: object) -> str:
    import subprocess
    if not isinstance(argv, list) or any(not isinstance(item, str) for item in argv):
        raise ElevationError("Launch argv must be an array of strings.")
    return subprocess.list2cmdline(argv)
