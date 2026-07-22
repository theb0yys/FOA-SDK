#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Authenticated package-engine intake requiring state-backed admission-bound handoffs."""
from __future__ import annotations

import argparse
import datetime as dt
import re
import sys
from pathlib import Path
from typing import Mapping, Sequence

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for source_root in (
    INSTALLER_ROOT / "Bootstrapper" / "AdmissionBoundExecutionHandoff" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "ExecutionHandoff" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "Security" / "Source",
    INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source",
):
    if str(source_root) not in sys.path:
        sys.path.insert(0, str(source_root))

from execution_security import (  # noqa: E402
    ExecutionSecurityError,
    canonical_json,
    claim_once,
    publish_bytes_create_once,
    read_strict_json_file,
    seal_authenticated_record,
    sha256,
    utc_datetime,
    validate_authority_proof,
    validate_operation_plan,
    verify_sealed_record,
)
from receipt_execution_handoff import ExecutionHandoffError, validate_handoff  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS  # noqa: E402

TOKEN_SCOPE = "package-engine-capability-token"
SESSION_SCOPE = "package-engine-capability-session"
AUDIENCE = "foa-sdk.package-engine"
MAX_TOKEN_SECONDS = 3600
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
CAPABILITY_RE = re.compile(r"^package-engine\.[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$")
TOKEN_STATEMENT = (
    "This authenticated token grants only the exact capabilities in one reviewed operation plan for one "
    "exact admission-bound execution handoff. Authority is verified with the configured installer trust key."
)
SESSION_STATEMENT = (
    "This package-engine session records authenticated one-shot token consumption for one exact "
    "admission-bound execution handoff and performs no payload copy, process launch, elevation, lifecycle "
    "finalization, runtime execution, or publication."
)
EFFECT_FIELDS = (
    "package_copy_performed", "process_launched", "elevation_requested", "installation_performed",
    "repair_performed", "upgrade_performed", "rollback_performed", "uninstall_performed",
    "runtime_executed", "deployment_performed", "save_mutated", "signing_performed",
    "publication_performed", "catalog_mutated", "evidence_promoted",
)


class PackageEngineError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise PackageEngineError(f"{label} must be an object.")
    return value


def _array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise PackageEngineError(f"{label} must be an array.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise PackageEngineError(f"{label} must be non-empty trimmed text of at most {maximum} characters.")
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise PackageEngineError(f"{label} contains a forbidden control character.")
    return value


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise PackageEngineError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise PackageEngineError(f"{label} must be a stable namespaced logical ID.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label, 64)
    try:
        utc_datetime(result, label)
    except ExecutionSecurityError as exc:
        raise PackageEngineError(str(exc)) from exc
    return result


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _effects() -> dict[str, bool]:
    return {field: False for field in EFFECT_FIELDS}


def _validate_exact_false(value: object, fields: Sequence[str], label: str) -> None:
    record = _object(value, label)
    if set(record) != set(fields) or any(record.get(field) is not False for field in fields):
        raise PackageEngineError(f"{label} must contain the exact all-false record.")


def _capabilities(value: object, label: str) -> list[str]:
    result = [_text(item, f"{label}[{index}]", 128) for index, item in enumerate(_array(value, label))]
    if not result or sorted(set(result)) != result:
        raise PackageEngineError(f"{label} must be a non-empty unique sorted array.")
    if any(CAPABILITY_RE.fullmatch(item) is None for item in result):
        raise PackageEngineError(f"{label} contains an invalid package-engine capability.")
    return result


def _checked_binding(binding: Mapping[str, object]) -> dict[str, object]:
    # Lazy import avoids the legacy ProcessLauncher -> PackageEngine import cycle during module discovery.
    from admission_bound_execution_handoff import (  # noqa: PLC0415
        AdmissionBoundExecutionHandoffError,
        validate_admission_bound_handoff,
    )
    try:
        return validate_admission_bound_handoff(binding)
    except AdmissionBoundExecutionHandoffError as exc:
        raise PackageEngineError(f"Admission-bound handoff verification failed: {exc}") from exc


def _checked_handoff_from_binding(binding: Mapping[str, object]) -> dict[str, object]:
    checked_binding = _checked_binding(binding)
    try:
        handoff = validate_handoff(_object(checked_binding.get("execution_handoff"), "admission_bound_handoff.execution_handoff"))
    except ExecutionHandoffError as exc:
        raise PackageEngineError(f"Execution handoff verification failed: {exc}") from exc
    if checked_binding.get("handoff_sha256") != handoff.get("handoff_sha256"):
        raise PackageEngineError("Admission-bound handoff does not bind the embedded execution handoff.")
    return handoff


def build_capability_token(
    admission_bound_handoff: Mapping[str, object], operation_plan: Mapping[str, object],
    authority_proof: Mapping[str, object], *, authority_key_path: Path, subject: str,
    issued_at_utc: str, expires_at_utc: str, nonce: str,
) -> dict[str, object]:
    try:
        checked_binding = _checked_binding(admission_bound_handoff)
        checked_handoff = _checked_handoff_from_binding(checked_binding)
        checked_plan = validate_operation_plan(operation_plan)
        checked_proof = validate_authority_proof(
            authority_proof, checked_handoff, checked_plan, authority_key_path=authority_key_path
        )
    except (ExecutionSecurityError, PackageEngineError) as exc:
        raise PackageEngineError(f"Authenticated admitted operation intake failed: {exc}") from exc
    if checked_plan["operation"] != checked_handoff["operation"]:
        raise PackageEngineError("Reviewed operation plan does not match the admitted handoff operation.")
    requested = _capabilities(checked_handoff.get("required_capabilities"), "handoff.required_capabilities")
    granted = _capabilities(checked_plan.get("capabilities"), "operation_plan.capabilities")
    if not set(requested).issubset(granted):
        raise PackageEngineError("Reviewed operation plan does not include every handoff-required capability.")
    issued = utc_datetime(issued_at_utc, "issued_at_utc")
    expires = utc_datetime(expires_at_utc, "expires_at_utc")
    requested_at = utc_datetime(checked_handoff["requested_at_utc"], "handoff.requested_at_utc")
    bound_at = utc_datetime(checked_binding["bound_at_utc"], "admission_bound_handoff.bound_at_utc")
    proof_issued = utc_datetime(checked_proof["issued_at_utc"], "authority_proof.issued_at_utc")
    proof_expires = utc_datetime(checked_proof["expires_at_utc"], "authority_proof.expires_at_utc")
    if issued < requested_at or issued < bound_at or issued < proof_issued:
        raise PackageEngineError("Token issuance must follow the admitted handoff binding and authority proof.")
    if expires <= issued or expires - issued > dt.timedelta(seconds=MAX_TOKEN_SECONDS):
        raise PackageEngineError("Capability token expiry must be after issuance and within one hour.")
    if expires > proof_expires:
        raise PackageEngineError("Capability token must not outlive its authority proof.")
    base = {
        "schema_version": 2,
        "token_scope": TOKEN_SCOPE,
        "audience": AUDIENCE,
        "admission_bound_handoff_sha256": checked_binding["binding_sha256"],
        "admission_receipt_sha256": checked_binding["admission_receipt_sha256"],
        "eligibility_sha256": checked_binding["eligibility_sha256"],
        "snapshot_sha256": checked_binding["snapshot_sha256"],
        "handoff_sha256": checked_handoff["handoff_sha256"],
        "operation_plan_sha256": checked_plan["operation_plan_sha256"],
        "authority_proof_sha256": checked_proof["authority_proof_sha256"],
        "authority_key_id": checked_proof["key_id"],
        "operation": checked_handoff["operation"],
        "target_reference": checked_handoff["target_reference"],
        "prior_installation_reference": checked_handoff["prior_installation_reference"],
        "required_capabilities": requested,
        "granted_capabilities": granted,
        "subject": _text(subject, "subject", 160),
        "issued_at_utc": _utc(issued_at_utc, "issued_at_utc"),
        "expires_at_utc": _utc(expires_at_utc, "expires_at_utc"),
        "nonce": _reference(nonce, "nonce"),
        "admission_bound_handoff": checked_binding,
        "handoff": checked_handoff,
        "operation_plan": checked_plan,
        "authority_proof": checked_proof,
        "statement": TOKEN_STATEMENT,
        "authority": _authority(),
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="token_sha256")
    except ExecutionSecurityError as exc:
        raise PackageEngineError(f"Token authentication failed: {exc}") from exc


def validate_capability_token(token: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(token)
    if document.get("schema_version") != 2 or document.get("token_scope") != TOKEN_SCOPE or document.get("audience") != AUDIENCE:
        raise PackageEngineError("Token schema, scope, or audience is invalid.")
    if document.get("statement") != TOKEN_STATEMENT:
        raise PackageEngineError("Token statement was altered.")
    _validate_exact_false(document.get("authority"), AUTHORITY_FIELDS, "token.authority")
    _hash(document.get("admission_bound_handoff_sha256"), "token.admission_bound_handoff_sha256")
    _hash(document.get("admission_receipt_sha256"), "token.admission_receipt_sha256")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="token_sha256")
    except ExecutionSecurityError as exc:
        raise PackageEngineError(f"Token authentication failed: {exc}") from exc
    expected = build_capability_token(
        _object(document.get("admission_bound_handoff"), "token.admission_bound_handoff"),
        _object(document.get("operation_plan"), "token.operation_plan"),
        _object(document.get("authority_proof"), "token.authority_proof"),
        authority_key_path=authority_key_path,
        subject=_text(document.get("subject"), "token.subject", 160),
        issued_at_utc=_utc(document.get("issued_at_utc"), "token.issued_at_utc"),
        expires_at_utc=_utc(document.get("expires_at_utc"), "token.expires_at_utc"),
        nonce=_reference(document.get("nonce"), "token.nonce"),
    )
    if document != expected:
        raise PackageEngineError("Capability token is stale, altered, or not canonically derived.")
    return document


def verify_token_for_admission_bound_handoff(
    token: Mapping[str, object], admission_bound_handoff: Mapping[str, object], *, authority_key_path: Path
) -> dict[str, object]:
    checked = validate_capability_token(token, authority_key_path=authority_key_path)
    current = _checked_binding(admission_bound_handoff)
    if checked["admission_bound_handoff"] != current or checked["admission_bound_handoff_sha256"] != current["binding_sha256"]:
        raise PackageEngineError("Capability token is not bound to the exact current admission-bound handoff.")
    return checked


def verify_token_for_handoff(
    token: Mapping[str, object], handoff: Mapping[str, object], *, authority_key_path: Path
) -> dict[str, object]:
    """Legacy read-only verification helper; it never authorizes token issuance or session creation."""
    checked = validate_capability_token(token, authority_key_path=authority_key_path)
    try:
        current = validate_handoff(handoff)
    except ExecutionHandoffError as exc:
        raise PackageEngineError(f"Handoff verification failed: {exc}") from exc
    if checked["handoff"] != current:
        raise PackageEngineError("Capability token is not bound to the exact current handoff.")
    return checked


def _derive_engine_session(
    admission_bound_handoff: Mapping[str, object], token: Mapping[str, object], *, authority_key_path: Path,
    session_reference: str, accepted_by: str, accepted_at_utc: str, token_claim: Mapping[str, object],
) -> dict[str, object]:
    checked_binding = _checked_binding(admission_bound_handoff)
    checked_handoff = _checked_handoff_from_binding(checked_binding)
    checked_token = verify_token_for_admission_bound_handoff(token, checked_binding, authority_key_path=authority_key_path)
    accepted = utc_datetime(accepted_at_utc, "accepted_at_utc")
    if accepted < utc_datetime(checked_token["issued_at_utc"], "token.issued_at_utc"):
        raise PackageEngineError("Session acceptance must not precede token issuance.")
    if accepted > utc_datetime(checked_token["expires_at_utc"], "token.expires_at_utc"):
        raise PackageEngineError("Capability token expired before package-engine intake.")
    claim = _object(dict(token_claim), "token_claim")
    try:
        verify_sealed_record(claim, authority_key_path=authority_key_path, digest_field="claim_sha256")
    except ExecutionSecurityError as exc:
        raise PackageEngineError(f"Token claim authentication failed: {exc}") from exc
    if (
        claim.get("claim_kind") != "claim.package-engine-token"
        or claim.get("artifact_sha256") != checked_token["token_sha256"]
        or claim.get("nonce") != checked_token["nonce"]
        or claim.get("claimed_at_utc") != accepted_at_utc
    ):
        raise PackageEngineError("Token claim is not bound to the exact token, nonce, and acceptance time.")
    base = {
        "schema_version": 2,
        "session_scope": SESSION_SCOPE,
        "session_state": "authenticated-admission-bound-capability-accepted-no-effects",
        "session_reference": _reference(session_reference, "session_reference"),
        "admission_bound_handoff_sha256": checked_binding["binding_sha256"],
        "admission_receipt_sha256": checked_binding["admission_receipt_sha256"],
        "eligibility_sha256": checked_binding["eligibility_sha256"],
        "snapshot_sha256": checked_binding["snapshot_sha256"],
        "handoff_sha256": checked_handoff["handoff_sha256"],
        "token_sha256": checked_token["token_sha256"],
        "token_claim_sha256": _hash(claim.get("claim_sha256"), "token_claim.claim_sha256"),
        "operation_plan_sha256": checked_token["operation_plan_sha256"],
        "authority_proof_sha256": checked_token["authority_proof_sha256"],
        "authority_key_id": checked_token["authority_key_id"],
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
        "operation_plan": dict(checked_token["operation_plan"]),
        "admission_bound_handoff": checked_binding,
        "accepted_by": _text(accepted_by, "accepted_by", 160),
        "accepted_at_utc": _utc(accepted_at_utc, "accepted_at_utc"),
        "handoff": checked_handoff,
        "capability_token": checked_token,
        "token_claim": claim,
        "effects": _effects(),
        "statement": SESSION_STATEMENT,
        "authority": _authority(),
    }
    try:
        return seal_authenticated_record(base, authority_key_path=authority_key_path, digest_field="session_sha256")
    except ExecutionSecurityError as exc:
        raise PackageEngineError(f"Session authentication failed: {exc}") from exc


def build_engine_session(
    admission_bound_handoff: Mapping[str, object], token: Mapping[str, object], *, authority_key_path: Path,
    claim_root: Path, session_reference: str, accepted_by: str, accepted_at_utc: str,
) -> dict[str, object]:
    # Validate every caller-authored field and all chronology before consuming the one-shot token.
    checked_token = verify_token_for_admission_bound_handoff(token, admission_bound_handoff, authority_key_path=authority_key_path)
    checked_reference = _reference(session_reference, "session_reference")
    checked_actor = _text(accepted_by, "accepted_by", 160)
    checked_time = _utc(accepted_at_utc, "accepted_at_utc")
    accepted = utc_datetime(checked_time, "accepted_at_utc")
    if accepted < utc_datetime(checked_token["issued_at_utc"], "token.issued_at_utc") or accepted > utc_datetime(checked_token["expires_at_utc"], "token.expires_at_utc"):
        raise PackageEngineError("Capability token is not valid at package-engine intake.")
    try:
        claim = claim_once(
            claim_root, authority_key_path=authority_key_path, claim_kind="claim.package-engine-token",
            artifact_sha256=str(checked_token["token_sha256"]), nonce=str(checked_token["nonce"]),
            claimed_at_utc=checked_time,
        )
    except ExecutionSecurityError as exc:
        raise PackageEngineError(f"Token consumption failed: {exc}") from exc
    return _derive_engine_session(
        admission_bound_handoff, checked_token, authority_key_path=authority_key_path,
        session_reference=checked_reference, accepted_by=checked_actor, accepted_at_utc=checked_time,
        token_claim=claim,
    )


def validate_engine_session(session: Mapping[str, object], *, authority_key_path: Path) -> dict[str, object]:
    document = dict(session)
    if document.get("schema_version") != 2 or document.get("session_scope") != SESSION_SCOPE:
        raise PackageEngineError("Session schema or scope is invalid.")
    if document.get("session_state") != "authenticated-admission-bound-capability-accepted-no-effects" or document.get("statement") != SESSION_STATEMENT:
        raise PackageEngineError("Session state or statement is invalid.")
    _validate_exact_false(document.get("authority"), AUTHORITY_FIELDS, "session.authority")
    _validate_exact_false(document.get("effects"), EFFECT_FIELDS, "session.effects")
    _hash(document.get("admission_bound_handoff_sha256"), "session.admission_bound_handoff_sha256")
    try:
        verify_sealed_record(document, authority_key_path=authority_key_path, digest_field="session_sha256")
    except ExecutionSecurityError as exc:
        raise PackageEngineError(f"Session authentication failed: {exc}") from exc
    expected = _derive_engine_session(
        _object(document.get("admission_bound_handoff"), "session.admission_bound_handoff"),
        _object(document.get("capability_token"), "session.capability_token"),
        authority_key_path=authority_key_path,
        session_reference=_reference(document.get("session_reference"), "session.session_reference"),
        accepted_by=_text(document.get("accepted_by"), "session.accepted_by", 160),
        accepted_at_utc=_utc(document.get("accepted_at_utc"), "session.accepted_at_utc"),
        token_claim=_object(document.get("token_claim"), "session.token_claim"),
    )
    if document != expected:
        raise PackageEngineError("Engine session is stale, altered, escalated, or not canonically derived.")
    return document


def canonical_token_bytes(token: Mapping[str, object], *, authority_key_path: Path) -> bytes:
    return canonical_json(validate_capability_token(token, authority_key_path=authority_key_path))


def canonical_session_bytes(session: Mapping[str, object], *, authority_key_path: Path) -> bytes:
    return canonical_json(validate_engine_session(session, authority_key_path=authority_key_path))


def _publish(path: Path, payload: bytes, suffix: str, label: str) -> tuple[str, Path]:
    target = Path(path)
    if not target.name.endswith(suffix) or not target.parent.is_dir() or target.parent.is_symlink():
        raise PackageEngineError(f"{label} destination must have suffix {suffix} beneath an existing non-symlink directory.")
    if target.is_symlink():
        raise PackageEngineError(f"{label} destination must not be a symbolic link.")
    if target.exists():
        try:
            existing = target.read_bytes() if target.is_file() and not target.is_symlink() else b""
        except OSError as exc:
            raise PackageEngineError(f"{label} destination could not be read: {exc}") from exc
        if existing != payload:
            raise PackageEngineError(f"{label} destination already exists with different bytes.")
        return "already-current", target
    try:
        return "published", publish_bytes_create_once(target, payload, label=label)
    except ExecutionSecurityError as exc:
        raise PackageEngineError(str(exc)) from exc


def load_token(path: Path, *, authority_key_path: Path) -> dict[str, object]:
    try:
        value = read_strict_json_file(Path(path), "Token", require_canonical=True)
        return validate_capability_token(_object(value, "token"), authority_key_path=authority_key_path)
    except ExecutionSecurityError as exc:
        raise PackageEngineError(str(exc)) from exc


def load_engine_session(path: Path, *, authority_key_path: Path) -> dict[str, object]:
    try:
        value = read_strict_json_file(Path(path), "Session", require_canonical=True)
        return validate_engine_session(_object(value, "session"), authority_key_path=authority_key_path)
    except ExecutionSecurityError as exc:
        raise PackageEngineError(str(exc)) from exc


def publish_token(
    destination: Path, admission_bound_handoff: Mapping[str, object], operation_plan: Mapping[str, object],
    authority_proof: Mapping[str, object], **kwargs: object,
) -> dict[str, object]:
    token = build_capability_token(admission_bound_handoff, operation_plan, authority_proof, **kwargs)
    payload = canonical_token_bytes(token, authority_key_path=kwargs["authority_key_path"])
    status, target = _publish(destination, payload, ".foa-package-engine-token.json", "Token")
    return {
        "status": status, "path": str(target), "token_sha256": token["token_sha256"],
        "admission_bound_handoff_sha256": token["admission_bound_handoff_sha256"],
        "authority_key_id": token["authority_key_id"], "size_bytes": len(payload),
    }


def publish_engine_session(
    destination: Path, admission_bound_handoff: Mapping[str, object], token: Mapping[str, object], **kwargs: object,
) -> dict[str, object]:
    session = build_engine_session(admission_bound_handoff, token, **kwargs)
    payload = canonical_session_bytes(session, authority_key_path=kwargs["authority_key_path"])
    status, target = _publish(destination, payload, ".foa-package-engine-session.json", "Session")
    return {
        "status": status, "path": str(target), "session_sha256": session["session_sha256"],
        "admission_bound_handoff_sha256": session["admission_bound_handoff_sha256"],
        "token_claim_sha256": session["token_claim_sha256"], "size_bytes": len(payload),
    }


def _load_json(path: Path, label: str) -> dict[str, object]:
    try:
        return _object(read_strict_json_file(Path(path), label), label)
    except ExecutionSecurityError as exc:
        raise PackageEngineError(str(exc)) from exc


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    token = sub.add_parser("token")
    for name in ("admission-bound-handoff", "operation-plan", "authority-proof", "authority-key", "output"):
        token.add_argument(f"--{name}", type=Path, required=True)
    token.add_argument("--subject", required=True)
    token.add_argument("--issued-at-utc", required=True)
    token.add_argument("--expires-at-utc", required=True)
    token.add_argument("--nonce", required=True)
    session = sub.add_parser("session")
    for name in ("admission-bound-handoff", "token", "authority-key", "claim-root", "output"):
        session.add_argument(f"--{name}", type=Path, required=True)
    session.add_argument("--session-reference", required=True)
    session.add_argument("--accepted-by", required=True)
    session.add_argument("--accepted-at-utc", required=True)
    verify_token = sub.add_parser("verify-token")
    verify_token.add_argument("--token", type=Path, required=True)
    verify_token.add_argument("--authority-key", type=Path, required=True)
    verify_session = sub.add_parser("verify-session")
    verify_session.add_argument("--session", type=Path, required=True)
    verify_session.add_argument("--authority-key", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        if args.command == "token":
            result = publish_token(
                args.output,
                _load_json(args.admission_bound_handoff, "admission_bound_handoff"),
                _load_json(args.operation_plan, "operation_plan"),
                _load_json(args.authority_proof, "authority_proof"),
                authority_key_path=args.authority_key, subject=args.subject,
                issued_at_utc=args.issued_at_utc, expires_at_utc=args.expires_at_utc, nonce=args.nonce,
            )
        elif args.command == "session":
            result = publish_engine_session(
                args.output,
                _load_json(args.admission_bound_handoff, "admission_bound_handoff"),
                load_token(args.token, authority_key_path=args.authority_key),
                authority_key_path=args.authority_key, claim_root=args.claim_root,
                session_reference=args.session_reference, accepted_by=args.accepted_by,
                accepted_at_utc=args.accepted_at_utc,
            )
        elif args.command == "verify-token":
            token = load_token(args.token, authority_key_path=args.authority_key)
            result = {
                "status": "verified", "token_sha256": token["token_sha256"],
                "admission_bound_handoff_sha256": token["admission_bound_handoff_sha256"],
                "authority_key_id": token["authority_key_id"],
            }
        else:
            session = load_engine_session(args.session, authority_key_path=args.authority_key)
            result = {
                "status": "verified", "session_sha256": session["session_sha256"],
                "admission_bound_handoff_sha256": session["admission_bound_handoff_sha256"],
                "token_claim_sha256": session["token_claim_sha256"],
            }
        sys.stdout.buffer.write(canonical_json(result))
        return 0
    except (OSError, ExecutionHandoffError, ExecutionSecurityError, PackageEngineError) as exc:
        print(f"Package engine intake failed: {exc}", file=sys.stderr)
        return 1


__all__ = [
    "AUDIENCE", "PackageEngineError", "SESSION_SCOPE", "TOKEN_SCOPE", "build_capability_token",
    "build_engine_session", "canonical_session_bytes", "canonical_token_bytes", "load_engine_session", "load_token",
    "publish_engine_session", "publish_token", "validate_capability_token", "validate_engine_session",
    "verify_token_for_admission_bound_handoff", "verify_token_for_handoff", "sha256",
]

if __name__ == "__main__":
    raise SystemExit(main())
