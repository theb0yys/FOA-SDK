#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Bind an inert execution handoff to a state-backed lifecycle admission receipt."""
from __future__ import annotations

import datetime as dt
import re
import sys
from pathlib import Path
from typing import Mapping

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for root in (
    INSTALLER_ROOT / "Bootstrapper" / "ExecutionHandoff" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "LifecycleOperationAdmission" / "Source",
    INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from lifecycle_operation_admission import (  # noqa: E402
    LifecycleOperationAdmissionError,
    validate_admission_receipt,
)
from receipt_execution_handoff import ExecutionHandoffError, validate_handoff  # noqa: E402
from wizard_view_model import AUTHORITY_FIELDS, canonical_json, sha256  # noqa: E402

BINDING_CAPABILITY = "package-engine.bind-admitted-handoff"
BINDING_SCOPE = "package-admission-bound-execution-handoff"
MAX_BIND_SECONDS = 900
OPERATIONS = ("install", "repair", "upgrade", "rollback", "uninstall")
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
BINDING_STATEMENT = (
    "This binding records that one exact inert execution handoff is backed by one exact state "
    "lifecycle admission receipt. It does not create a package-engine session, copy payloads, "
    "start processes, request elevation, publish state, mutate products, run runtime code, sign "
    "artifacts, mutate catalogs, mutate saves, or promote evidence."
)


class AdmissionBoundExecutionHandoffError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise AdmissionBoundExecutionHandoffError(f"{label} must be an object.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise AdmissionBoundExecutionHandoffError(
            f"{label} must be a non-empty trimmed string of at most {maximum} characters."
        )
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise AdmissionBoundExecutionHandoffError(f"{label} contains a forbidden control character.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise AdmissionBoundExecutionHandoffError(f"{label} must be a stable namespaced logical ID.")
    return result


def _optional_reference(value: object, label: str) -> str | None:
    if value is None:
        return None
    return _reference(value, label)


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise AdmissionBoundExecutionHandoffError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if not result.endswith("Z") or "T" not in result:
        raise AdmissionBoundExecutionHandoffError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise AdmissionBoundExecutionHandoffError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise AdmissionBoundExecutionHandoffError(f"{label} must use UTC.")
    return result


def _utc_dt(value: object, label: str) -> dt.datetime:
    return dt.datetime.fromisoformat(_utc(value, label)[:-1] + "+00:00")


def _operation(value: object, label: str = "operation") -> str:
    result = _text(value, label, 32)
    if result not in OPERATIONS:
        raise AdmissionBoundExecutionHandoffError("operation must be one of: " + ", ".join(OPERATIONS) + ".")
    return result


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _validate_authority(value: object, label: str) -> None:
    authority = _object(value, label)
    if authority != _authority():
        raise AdmissionBoundExecutionHandoffError(f"{label} must contain the exact all-false authority record.")


def _checked_handoff(handoff: Mapping[str, object]) -> dict[str, object]:
    try:
        return validate_handoff(handoff)
    except ExecutionHandoffError as exc:
        raise AdmissionBoundExecutionHandoffError(f"Execution handoff verification failed: {exc}") from exc


def _checked_admission(receipt: Mapping[str, object]) -> dict[str, object]:
    try:
        return validate_admission_receipt(receipt)
    except LifecycleOperationAdmissionError as exc:
        raise AdmissionBoundExecutionHandoffError(f"Lifecycle admission receipt verification failed: {exc}") from exc


def _match_handoff_to_admission(handoff: Mapping[str, object], admission: Mapping[str, object]) -> None:
    handoff_operation = _operation(handoff.get("operation"), "handoff.operation")
    admission_operation = _operation(admission.get("operation"), "admission.operation")
    if handoff_operation != admission_operation:
        raise AdmissionBoundExecutionHandoffError("Execution handoff operation is not admitted by the receipt.")
    handoff_target = _reference(handoff.get("target_reference"), "handoff.target_reference")
    admission_target = _reference(admission.get("target_reference"), "admission.target_reference")
    if handoff_target != admission_target:
        raise AdmissionBoundExecutionHandoffError("Execution handoff target is not admitted by the receipt.")
    handoff_prior = _optional_reference(
        handoff.get("prior_installation_reference"),
        "handoff.prior_installation_reference",
    )
    admission_prior = _optional_reference(
        admission.get("recommended_prior_installation_reference"),
        "admission.recommended_prior_installation_reference",
    )
    if handoff_prior != admission_prior:
        raise AdmissionBoundExecutionHandoffError("Execution handoff prior reference does not match state-backed admission.")


def build_admission_bound_handoff(
    handoff: Mapping[str, object],
    admission_receipt: Mapping[str, object],
    *,
    bound_by: str,
    bound_at_utc: str,
    nonce: str,
) -> dict[str, object]:
    checked_handoff = _checked_handoff(handoff)
    checked_admission = _checked_admission(admission_receipt)
    _match_handoff_to_admission(checked_handoff, checked_admission)
    bound = _utc_dt(bound_at_utc, "bound_at_utc")
    requested = _utc_dt(checked_handoff["requested_at_utc"], "handoff.requested_at_utc")
    admitted = _utc_dt(checked_admission["admitted_at_utc"], "admission.admitted_at_utc")
    if requested < admitted:
        raise AdmissionBoundExecutionHandoffError("Execution handoff request must not precede state-backed admission.")
    if bound < requested:
        raise AdmissionBoundExecutionHandoffError("bound_at_utc must not precede the execution handoff request.")
    if bound - admitted > dt.timedelta(seconds=MAX_BIND_SECONDS):
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff binding must occur within 15 minutes of admission.")
    base = {
        "schema_version": 1,
        "binding_scope": BINDING_SCOPE,
        "capability": BINDING_CAPABILITY,
        "handoff_sha256": _hash(checked_handoff.get("handoff_sha256"), "handoff.handoff_sha256"),
        "admission_receipt_sha256": _hash(
            checked_admission.get("receipt_sha256"),
            "admission.receipt_sha256",
        ),
        "eligibility_sha256": _hash(checked_admission.get("eligibility_sha256"), "admission.eligibility_sha256"),
        "snapshot_sha256": _hash(checked_admission.get("snapshot_sha256"), "admission.snapshot_sha256"),
        "operation": _operation(checked_handoff.get("operation"), "handoff.operation"),
        "target_reference": _reference(checked_handoff.get("target_reference"), "handoff.target_reference"),
        "prior_installation_reference": _optional_reference(
            checked_handoff.get("prior_installation_reference"),
            "handoff.prior_installation_reference",
        ),
        "current_state_reference": _optional_reference(
            checked_admission.get("current_state_reference"),
            "admission.current_state_reference",
        ),
        "current_state_status": checked_admission.get("current_state_status"),
        "current_state_file_sha256": None if checked_admission.get("current_state_file_sha256") is None else _hash(
            checked_admission.get("current_state_file_sha256"),
            "admission.current_state_file_sha256",
        ),
        "eligibility_reason": _text(checked_admission.get("eligibility_reason"), "admission.eligibility_reason", 128),
        "requested_at_utc": _utc(checked_handoff.get("requested_at_utc"), "handoff.requested_at_utc"),
        "admitted_at_utc": _utc(checked_admission.get("admitted_at_utc"), "admission.admitted_at_utc"),
        "bound_by": _text(bound_by, "bound_by", 160),
        "bound_at_utc": _utc(bound_at_utc, "bound_at_utc"),
        "nonce": _reference(nonce, "nonce"),
        "execution_handoff": checked_handoff,
        "admission_receipt": checked_admission,
        "execution_handoff_verified": True,
        "admission_receipt_verified": True,
        "package_engine_session_created": False,
        "copy_performed": False,
        "process_launched": False,
        "elevation_requested": False,
        "state_published": False,
        "product_or_game_directory_mutated": False,
        "runtime_executed": False,
        "save_mutated": False,
        "signing_performed": False,
        "network_publication_performed": False,
        "catalog_mutated": False,
        "evidence_promoted": False,
        "statement": BINDING_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "binding_sha256": sha256(base)}


def validate_admission_bound_handoff(binding: Mapping[str, object]) -> dict[str, object]:
    document = dict(binding)
    if document.get("schema_version") != 1 or document.get("binding_scope") != BINDING_SCOPE:
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff schema or scope is invalid.")
    if document.get("capability") != BINDING_CAPABILITY or document.get("statement") != BINDING_STATEMENT:
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff capability or statement is invalid.")
    _validate_authority(document.get("authority"), "binding.authority")
    if document.get("execution_handoff_verified") is not True:
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff must record execution_handoff_verified=true.")
    if document.get("admission_receipt_verified") is not True:
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff must record admission_receipt_verified=true.")
    for field in (
        "package_engine_session_created", "copy_performed", "process_launched", "elevation_requested",
        "state_published", "product_or_game_directory_mutated", "runtime_executed", "save_mutated",
        "signing_performed", "network_publication_performed", "catalog_mutated", "evidence_promoted",
    ):
        if document.get(field) is not False:
            raise AdmissionBoundExecutionHandoffError(f"Admission-bound handoff forbidden side-effect flag {field} must be false.")
    declared = _hash(document.get("binding_sha256"), "binding.binding_sha256")
    unsigned = {key: value for key, value in document.items() if key != "binding_sha256"}
    if sha256(unsigned) != declared:
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff fingerprint does not match its content.")
    expected = build_admission_bound_handoff(
        _object(document.get("execution_handoff"), "binding.execution_handoff"),
        _object(document.get("admission_receipt"), "binding.admission_receipt"),
        bound_by=_text(document.get("bound_by"), "binding.bound_by", 160),
        bound_at_utc=_utc(document.get("bound_at_utc"), "binding.bound_at_utc"),
        nonce=_reference(document.get("nonce"), "binding.nonce"),
    )
    if document != expected:
        raise AdmissionBoundExecutionHandoffError("Admission-bound handoff is stale, altered, or not canonically derived.")
    return document


def verify_bound_handoff(
    binding: Mapping[str, object],
    handoff: Mapping[str, object],
    admission_receipt: Mapping[str, object],
) -> dict[str, object]:
    checked = validate_admission_bound_handoff(binding)
    if checked["execution_handoff"] != _checked_handoff(handoff):
        raise AdmissionBoundExecutionHandoffError("Binding is not attached to the exact current execution handoff.")
    if checked["admission_receipt"] != _checked_admission(admission_receipt):
        raise AdmissionBoundExecutionHandoffError("Binding is not attached to the exact current admission receipt.")
    return checked


def canonical_binding_bytes(binding: Mapping[str, object]) -> bytes:
    return canonical_json(validate_admission_bound_handoff(binding))
