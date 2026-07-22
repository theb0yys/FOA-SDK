#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Capability-token gated package-engine intake without lifecycle execution."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import sys
from pathlib import Path
from typing import Mapping, Sequence

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
HANDOFF_SOURCE = INSTALLER_ROOT / "Bootstrapper" / "ExecutionHandoff" / "Source"
VIEW_MODEL_SOURCE = INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source"
for source_root in (HANDOFF_SOURCE, VIEW_MODEL_SOURCE):
    if str(source_root) not in sys.path:
        sys.path.insert(0, str(source_root))

from receipt_execution_handoff import (  # noqa: E402
    ExecutionHandoffError,
    load_handoff,
    validate_handoff,
)
from wizard_view_model import (  # noqa: E402
    AUTHORITY_FIELDS,
    canonical_json,
    sha256,
)

TOKEN_SCOPE = "package-engine-capability-token"
SESSION_SCOPE = "package-engine-capability-session"
AUDIENCE = "foa-sdk.package-engine"
MAX_TOKEN_SECONDS = 3600
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
CAPABILITY_RE = re.compile(r"^package-engine\.[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$")
TOKEN_STATEMENT = (
    "This token grants only the listed FOA-SDK package-engine intake capabilities "
    "for one exact handoff. It is not a credential or secret, serializes no private "
    "path or executable, and grants no copy, process-launch, elevation, runtime, "
    "deployment, save-mutation, signing, publication, catalog-mutation, or "
    "evidence-promotion authority."
)
SESSION_STATEMENT = (
    "This package-engine session records successful handoff and capability-token "
    "verification only. It performs no package copy, process launch, elevation, "
    "installation, repair, upgrade, rollback, uninstall, runtime execution, "
    "deployment, save mutation, signing, publication, catalog mutation, or "
    "evidence promotion."
)
EFFECT_FIELDS = (
    "package_copy_performed",
    "process_launched",
    "elevation_requested",
    "installation_performed",
    "repair_performed",
    "upgrade_performed",
    "rollback_performed",
    "uninstall_performed",
    "runtime_executed",
    "deployment_performed",
    "save_mutated",
    "signing_performed",
    "publication_performed",
    "catalog_mutated",
    "evidence_promoted",
)


class PackageEngineError(RuntimeError):
    """Raised when package-engine intake or capability validation fails."""


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise PackageEngineError(f"{label} must be an object.")
    return value


def _array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise PackageEngineError(f"{label} must be an array.")
    return value


def _text(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise PackageEngineError(f"{label} must be a non-empty string.")
    return value


def _hash(value: object, label: str) -> str:
    result = _text(value, label)
    if SHA256_RE.fullmatch(result) is None:
        raise PackageEngineError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _actor(value: object, label: str) -> str:
    result = _text(value, label)
    if result != result.strip() or len(result) > 160:
        raise PackageEngineError(f"{label} must be trimmed and at most 160 characters.")
    if any(ord(character) < 32 or ord(character) == 127 for character in result):
        raise PackageEngineError(f"{label} must not contain control characters.")
    return result


def _reference(value: object, label: str) -> str:
    result = _text(value, label)
    if len(result) > 128 or REFERENCE_RE.fullmatch(result) is None:
        raise PackageEngineError(f"{label} must be a stable namespaced logical ID.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label)
    if len(result) > 64:
        raise PackageEngineError(f"{label} must not exceed 64 characters.")
    if not result.endswith("Z") or "T" not in result:
        raise PackageEngineError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise PackageEngineError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise PackageEngineError(f"{label} must use UTC.")
    return result


def _utc_datetime(value: object, label: str) -> dt.datetime:
    return dt.datetime.fromisoformat(_utc(value, label)[:-1] + "+00:00")


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _effects() -> dict[str, bool]:
    return {field: False for field in EFFECT_FIELDS}


def _validate_authority(value: object, label: str) -> None:
    authority = _object(value, label)
    if set(authority) != set(AUTHORITY_FIELDS) or any(
        authority.get(field) is not False for field in AUTHORITY_FIELDS
    ):
        raise PackageEngineError(f"{label} must contain the exact all-false authority record.")


def _validate_effects(value: object, label: str) -> None:
    effects = _object(value, label)
    if set(effects) != set(EFFECT_FIELDS) or any(
        effects.get(field) is not False for field in EFFECT_FIELDS
    ):
        raise PackageEngineError(f"{label} must contain the exact all-false effect record.")


def _capabilities(value: object, label: str) -> list[str]:
    result = [_text(item, f"{label}[{index}]") for index, item in enumerate(_array(value, label))]
    if not result:
        raise PackageEngineError(f"{label} must not be empty.")
    if sorted(set(result)) != result:
        raise PackageEngineError(f"{label} must be unique and sorted.")
    invalid = [item for item in result if CAPABILITY_RE.fullmatch(item) is None]
    if invalid:
        raise PackageEngineError(f"{label} contains invalid package-engine capabilities: {', '.join(invalid)}")
    return result


def build_capability_token(
    handoff: Mapping[str, object],
    *,
    issuer: str,
    subject: str,
    issued_at_utc: str,
    expires_at_utc: str,
    nonce: str,
) -> dict[str, object]:
    """Build a deterministic capability token bound to one exact handoff."""
    try:
        checked_handoff = validate_handoff(handoff)
    except ExecutionHandoffError as exc:
        raise PackageEngineError(f"Handoff verification failed: {exc}") from exc
    issued_at = _utc(issued_at_utc, "issued_at_utc")
    expires_at = _utc(expires_at_utc, "expires_at_utc")
    issued = _utc_datetime(issued_at, "issued_at_utc")
    expires = _utc_datetime(expires_at, "expires_at_utc")
    requested = _utc_datetime(checked_handoff["requested_at_utc"], "handoff.requested_at_utc")
    if issued < requested:
        raise PackageEngineError("issued_at_utc must not precede the handoff request time.")
    if expires <= issued:
        raise PackageEngineError("expires_at_utc must be after issued_at_utc.")
    if expires - issued > dt.timedelta(seconds=MAX_TOKEN_SECONDS):
        raise PackageEngineError("capability token lifetime must not exceed one hour.")
    required_capabilities = _capabilities(
        checked_handoff.get("required_capabilities"),
        "handoff.required_capabilities",
    )
    base = {
        "schema_version": 1,
        "token_scope": TOKEN_SCOPE,
        "audience": AUDIENCE,
        "handoff_sha256": checked_handoff["handoff_sha256"],
        "operation": checked_handoff["operation"],
        "target_reference": checked_handoff["target_reference"],
        "required_capabilities": required_capabilities,
        "granted_capabilities": required_capabilities,
        "issuer": _actor(issuer, "issuer"),
        "subject": _actor(subject, "subject"),
        "issued_at_utc": issued_at,
        "expires_at_utc": expires_at,
        "nonce": _reference(nonce, "nonce"),
        "handoff": checked_handoff,
        "statement": TOKEN_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "token_sha256": sha256(base)}


def validate_capability_token(token: Mapping[str, object]) -> dict[str, object]:
    """Validate a self-contained capability token without consuming it."""
    document = dict(token)
    if document.get("schema_version") != 1:
        raise PackageEngineError("token.schema_version must be exactly 1.")
    if document.get("token_scope") != TOKEN_SCOPE:
        raise PackageEngineError("token.token_scope is invalid.")
    if document.get("audience") != AUDIENCE:
        raise PackageEngineError("token.audience is invalid.")
    if document.get("statement") != TOKEN_STATEMENT:
        raise PackageEngineError("token statement was altered.")
    _validate_authority(document.get("authority"), "token.authority")
    required = _capabilities(document.get("required_capabilities"), "token.required_capabilities")
    granted = _capabilities(document.get("granted_capabilities"), "token.granted_capabilities")
    if granted != required:
        raise PackageEngineError("token.granted_capabilities must exactly match required_capabilities.")
    declared = _hash(document.get("token_sha256"), "token.token_sha256")
    unsigned = {key: value for key, value in document.items() if key != "token_sha256"}
    if sha256(unsigned) != declared:
        raise PackageEngineError("token fingerprint does not match its content.")
    expected = build_capability_token(
        _object(document.get("handoff"), "token.handoff"),
        issuer=_actor(document.get("issuer"), "token.issuer"),
        subject=_actor(document.get("subject"), "token.subject"),
        issued_at_utc=_utc(document.get("issued_at_utc"), "token.issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "token.expires_at_utc"),
        nonce=_reference(document.get("nonce"), "token.nonce"),
    )
    if document != expected:
        raise PackageEngineError("capability token is stale, altered, or not canonically derived.")
    return document


def verify_token_for_handoff(
    token: Mapping[str, object],
    handoff: Mapping[str, object],
) -> dict[str, object]:
    checked_token = validate_capability_token(token)
    try:
        checked_handoff = validate_handoff(handoff)
    except ExecutionHandoffError as exc:
        raise PackageEngineError(f"Handoff verification failed: {exc}") from exc
    if checked_token["handoff"] != checked_handoff:
        raise PackageEngineError("capability token is not bound to the exact current handoff.")
    if checked_token["granted_capabilities"] != checked_handoff["required_capabilities"]:
        raise PackageEngineError("capability token does not grant the exact required capabilities.")
    return checked_token


def build_engine_session(
    handoff: Mapping[str, object],
    token: Mapping[str, object],
    *,
    session_reference: str,
    accepted_by: str,
    accepted_at_utc: str,
) -> dict[str, object]:
    """Consume a valid token into a package-engine intake session without effects."""
    try:
        checked_handoff = validate_handoff(handoff)
    except ExecutionHandoffError as exc:
        raise PackageEngineError(f"Handoff verification failed: {exc}") from exc
    checked_token = verify_token_for_handoff(token, checked_handoff)
    accepted_at = _utc(accepted_at_utc, "accepted_at_utc")
    accepted = _utc_datetime(accepted_at, "accepted_at_utc")
    issued = _utc_datetime(checked_token["issued_at_utc"], "token.issued_at_utc")
    expires = _utc_datetime(checked_token["expires_at_utc"], "token.expires_at_utc")
    if accepted < issued:
        raise PackageEngineError("accepted_at_utc must not precede token issuance.")
    if accepted > expires:
        raise PackageEngineError("capability token expired before package-engine intake.")
    base = {
        "schema_version": 1,
        "session_scope": SESSION_SCOPE,
        "session_state": "capability-accepted-no-effects",
        "session_reference": _reference(session_reference, "session_reference"),
        "handoff_sha256": checked_handoff["handoff_sha256"],
        "token_sha256": checked_token["token_sha256"],
        "receipt_sha256": checked_handoff["receipt_sha256"],
        "plan_sha256": checked_handoff["plan_sha256"],
        "view_model_sha256": checked_handoff["view_model_sha256"],
        "confirmation_sha256": checked_handoff["confirmation_sha256"],
        "operation": checked_handoff["operation"],
        "target_reference": checked_handoff["target_reference"],
        "prior_installation_reference": checked_handoff["prior_installation_reference"],
        "package_order": list(checked_handoff["package_order"]),
        "summary": dict(checked_handoff["summary"]),
        "authorized_capabilities": list(checked_token["granted_capabilities"]),
        "accepted_by": _actor(accepted_by, "accepted_by"),
        "accepted_at_utc": accepted_at,
        "handoff": checked_handoff,
        "capability_token": checked_token,
        "effects": _effects(),
        "statement": SESSION_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "session_sha256": sha256(base)}


def validate_engine_session(session: Mapping[str, object]) -> dict[str, object]:
    """Validate a self-contained package-engine intake session."""
    document = dict(session)
    if document.get("schema_version") != 1:
        raise PackageEngineError("session.schema_version must be exactly 1.")
    if document.get("session_scope") != SESSION_SCOPE:
        raise PackageEngineError("session.session_scope is invalid.")
    if document.get("session_state") != "capability-accepted-no-effects":
        raise PackageEngineError("session.session_state is invalid.")
    if document.get("statement") != SESSION_STATEMENT:
        raise PackageEngineError("session statement was altered.")
    _validate_authority(document.get("authority"), "session.authority")
    _validate_effects(document.get("effects"), "session.effects")
    declared = _hash(document.get("session_sha256"), "session.session_sha256")
    unsigned = {key: value for key, value in document.items() if key != "session_sha256"}
    if sha256(unsigned) != declared:
        raise PackageEngineError("session fingerprint does not match its content.")
    expected = build_engine_session(
        _object(document.get("handoff"), "session.handoff"),
        _object(document.get("capability_token"), "session.capability_token"),
        session_reference=_reference(document.get("session_reference"), "session.session_reference"),
        accepted_by=_actor(document.get("accepted_by"), "session.accepted_by"),
        accepted_at_utc=_utc(document.get("accepted_at_utc"), "session.accepted_at_utc"),
    )
    if document != expected:
        raise PackageEngineError("engine session is stale, altered, escalated, or not canonically derived.")
    return document


def canonical_token_bytes(token: Mapping[str, object]) -> bytes:
    return canonical_json(validate_capability_token(token))


def canonical_session_bytes(session: Mapping[str, object]) -> bytes:
    return canonical_json(validate_engine_session(session))


def _reject_symlink_components(path: Path) -> None:
    current = path
    while True:
        if current.is_symlink():
            raise PackageEngineError(f"Package-engine path contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def _validated_destination(destination: Path, suffix: str, label: str) -> Path:
    target = Path(destination)
    if target.name in {"", ".", ".."} or not target.name.endswith(suffix):
        raise PackageEngineError(f"{label} destination must end with {suffix!r}.")
    if not target.parent.exists() or not target.parent.is_dir():
        raise PackageEngineError(f"{label} destination parent must be an existing directory.")
    _reject_symlink_components(target.parent)
    if target.is_symlink():
        raise PackageEngineError(f"{label} destination must not be a symbolic link.")
    if target.exists() and not target.is_file():
        raise PackageEngineError(f"{label} destination must be a regular file path.")
    return target


def _read_exact_file(path: Path, label: str) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise PackageEngineError(f"{label} path is not a regular non-symlink file: {path}")
    return path.read_bytes()


def load_token(path: Path) -> dict[str, object]:
    target = Path(path)
    _reject_symlink_components(target.parent)
    payload = _read_exact_file(target, "Token")
    try:
        value = json.loads(payload.decode("utf-8", errors="strict"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise PackageEngineError(f"Token is not strict UTF-8 JSON: {target}") from exc
    document = validate_capability_token(_object(value, "token"))
    if canonical_json(document) != payload:
        raise PackageEngineError("Token file bytes are not canonical JSON.")
    return document


def load_engine_session(path: Path) -> dict[str, object]:
    target = Path(path)
    _reject_symlink_components(target.parent)
    payload = _read_exact_file(target, "Session")
    try:
        value = json.loads(payload.decode("utf-8", errors="strict"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise PackageEngineError(f"Session is not strict UTF-8 JSON: {target}") from exc
    document = validate_engine_session(_object(value, "session"))
    if canonical_json(document) != payload:
        raise PackageEngineError("Session file bytes are not canonical JSON.")
    return document


def _publish_bytes(
    destination: Path,
    payload: bytes,
    digest: str,
    suffix: str,
    label: str,
) -> tuple[str, Path]:
    target = _validated_destination(destination, suffix, label)
    if target.exists():
        existing = _read_exact_file(target, label)
        if existing != payload:
            raise PackageEngineError(f"{label} destination already exists with different bytes.")
        return "already-current", target
    temporary = target.parent / f".{target.name}.{digest}.tmp"
    if temporary.exists() or temporary.is_symlink():
        raise PackageEngineError(f"Deterministic temporary {label} path already exists: {temporary}")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor: int | None = None
    try:
        descriptor = os.open(temporary, flags, 0o600)
        view = memoryview(payload)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise PackageEngineError(f"{label} write made no forward progress.")
            view = view[written:]
        os.fsync(descriptor)
        os.close(descriptor)
        descriptor = None
        if _read_exact_file(temporary, label) != payload:
            raise PackageEngineError(f"Temporary {label} bytes changed before publication.")
        try:
            os.link(temporary, target)
        except FileExistsError as exc:
            raise PackageEngineError(f"{label} destination appeared during publication.") from exc
        os.unlink(temporary)
        if _read_exact_file(target, label) != payload:
            raise PackageEngineError(f"Published {label} bytes do not match canonical bytes.")
        return "published", target
    except (OSError, PackageEngineError) as exc:
        if descriptor is not None:
            os.close(descriptor)
        if temporary.exists() and not temporary.is_symlink():
            try:
                temporary.unlink()
            except OSError:
                pass
        if isinstance(exc, PackageEngineError):
            raise
        raise PackageEngineError(f"{label} publication failed: {exc}") from exc


def publish_token(
    destination: Path,
    handoff: Mapping[str, object],
    *,
    issuer: str,
    subject: str,
    issued_at_utc: str,
    expires_at_utc: str,
    nonce: str,
) -> dict[str, object]:
    token = build_capability_token(
        handoff,
        issuer=issuer,
        subject=subject,
        issued_at_utc=issued_at_utc,
        expires_at_utc=expires_at_utc,
        nonce=nonce,
    )
    payload = canonical_token_bytes(token)
    status, target = _publish_bytes(
        destination,
        payload,
        str(token["token_sha256"]),
        ".foa-package-engine-token.json",
        "Token",
    )
    loaded = load_token(target)
    return {
        "status": status,
        "path": str(target),
        "token_sha256": loaded["token_sha256"],
        "handoff_sha256": loaded["handoff_sha256"],
        "granted_capabilities": list(loaded["granted_capabilities"]),
        "authority": _authority(),
        "size_bytes": len(payload),
    }


def publish_engine_session(
    destination: Path,
    handoff: Mapping[str, object],
    token: Mapping[str, object],
    *,
    session_reference: str,
    accepted_by: str,
    accepted_at_utc: str,
) -> dict[str, object]:
    session = build_engine_session(
        handoff,
        token,
        session_reference=session_reference,
        accepted_by=accepted_by,
        accepted_at_utc=accepted_at_utc,
    )
    payload = canonical_session_bytes(session)
    status, target = _publish_bytes(
        destination,
        payload,
        str(session["session_sha256"]),
        ".foa-package-engine-session.json",
        "Session",
    )
    loaded = load_engine_session(target)
    return {
        "status": status,
        "path": str(target),
        "session_sha256": loaded["session_sha256"],
        "handoff_sha256": loaded["handoff_sha256"],
        "token_sha256": loaded["token_sha256"],
        "authorized_capabilities": list(loaded["authorized_capabilities"]),
        "effects": _effects(),
        "authority": _authority(),
        "size_bytes": len(payload),
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    token = subparsers.add_parser("token", help="Create a handoff-bound capability token.")
    token.add_argument("--handoff", type=Path, required=True)
    token.add_argument("--issuer", required=True)
    token.add_argument("--subject", required=True)
    token.add_argument("--issued-at-utc", required=True)
    token.add_argument("--expires-at-utc", required=True)
    token.add_argument("--nonce", required=True)
    token.add_argument("--output", type=Path, required=True)

    session = subparsers.add_parser("session", help="Create a non-executing package-engine session.")
    session.add_argument("--handoff", type=Path, required=True)
    session.add_argument("--token", type=Path, required=True)
    session.add_argument("--session-reference", required=True)
    session.add_argument("--accepted-by", required=True)
    session.add_argument("--accepted-at-utc", required=True)
    session.add_argument("--output", type=Path, required=True)

    verify_token = subparsers.add_parser("verify-token", help="Verify a capability token.")
    verify_token.add_argument("--token", type=Path, required=True)
    verify_token.add_argument("--handoff", type=Path)

    verify_session = subparsers.add_parser("verify-session", help="Verify a package-engine session.")
    verify_session.add_argument("--session", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        if arguments.command == "token":
            result = publish_token(
                arguments.output,
                load_handoff(arguments.handoff),
                issuer=arguments.issuer,
                subject=arguments.subject,
                issued_at_utc=arguments.issued_at_utc,
                expires_at_utc=arguments.expires_at_utc,
                nonce=arguments.nonce,
            )
        elif arguments.command == "session":
            result = publish_engine_session(
                arguments.output,
                load_handoff(arguments.handoff),
                load_token(arguments.token),
                session_reference=arguments.session_reference,
                accepted_by=arguments.accepted_by,
                accepted_at_utc=arguments.accepted_at_utc,
            )
        elif arguments.command == "verify-token":
            token = load_token(arguments.token)
            if arguments.handoff is not None:
                token = verify_token_for_handoff(token, load_handoff(arguments.handoff))
            result = {
                "status": "verified",
                "path": str(arguments.token),
                "token_sha256": token["token_sha256"],
                "handoff_sha256": token["handoff_sha256"],
                "granted_capabilities": list(token["granted_capabilities"]),
                "authority": _authority(),
            }
        else:
            session = load_engine_session(arguments.session)
            result = {
                "status": "verified",
                "path": str(arguments.session),
                "session_sha256": session["session_sha256"],
                "handoff_sha256": session["handoff_sha256"],
                "token_sha256": session["token_sha256"],
                "authorized_capabilities": list(session["authorized_capabilities"]),
                "effects": _effects(),
                "authority": _authority(),
            }
        sys.stdout.buffer.write(canonical_json(result))
        return 0
    except (OSError, ExecutionHandoffError, PackageEngineError) as exc:
        print(f"Package engine intake failed: {exc}", file=sys.stderr)
        return 1


__all__ = [
    "AUDIENCE",
    "PackageEngineError",
    "SESSION_SCOPE",
    "TOKEN_SCOPE",
    "build_capability_token",
    "build_engine_session",
    "canonical_session_bytes",
    "canonical_token_bytes",
    "load_engine_session",
    "load_token",
    "publish_engine_session",
    "publish_token",
    "validate_capability_token",
    "validate_engine_session",
    "verify_token_for_handoff",
]


if __name__ == "__main__":
    raise SystemExit(main())
