#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Observe one authenticated elevated bootstrap completion receipt for one elevation result."""
from __future__ import annotations

import datetime as dt
import re
import sys
from pathlib import Path
from typing import Mapping

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for root in (
    INSTALLER_ROOT / "Bootstrapper" / "ElevationHelper" / "Source",
    INSTALLER_ROOT / "Bootstrapper" / "Security" / "Source",
    INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from capability_elevation_helper import ElevationError, validate_elevation_result  # noqa: E402
from controlled_elevation_bootstrapper import (  # noqa: E402
    ControlledBootstrapperError,
    validate_completion_receipt,
)
from execution_security import (  # noqa: E402
    ExecutionSecurityError,
    canonical_json,
    seal_authenticated_record,
    utc_datetime,
    verify_sealed_record,
)
from wizard_view_model import AUTHORITY_FIELDS  # noqa: E402

OBSERVATION_CAPABILITY = "package-engine.observe-elevated-completion"
OBSERVATION_SCOPE = "package-elevated-completion-observation"
MAX_OBSERVATION_SECONDS = 900
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
OBSERVATION_STATEMENT = (
    "This observation binds one controlled elevation result to one authenticated elevated bootstrap "
    "completion receipt. It observes completion evidence only; it does not request elevation, launch "
    "processes, copy payloads, coordinate lifecycle completion, publish installation state, mutate products, "
    "run runtime code, sign artifacts, mutate catalogs, mutate saves, or promote evidence."
)


class ElevatedCompletionReceiptError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ElevatedCompletionReceiptError(f"{label} must be an object.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise ElevatedCompletionReceiptError(
            f"{label} must be non-empty trimmed text of at most {maximum} characters."
        )
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise ElevatedCompletionReceiptError(f"{label} contains a forbidden control character.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise ElevatedCompletionReceiptError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise ElevatedCompletionReceiptError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _utc(value: object, label: str) -> str:
    try:
        utc_datetime(value, label)
    except ExecutionSecurityError as exc:
        raise ElevatedCompletionReceiptError(str(exc)) from exc
    return str(value)


def _utc_dt(value: object, label: str) -> dt.datetime:
    return utc_datetime(_utc(value, label), label)


def _bool(value: object, label: str) -> bool:
    if type(value) is not bool:
        raise ElevatedCompletionReceiptError(f"{label} must be a boolean.")
    return bool(value)


def _int(value: object, label: str) -> int:
    if type(value) is not int:
        raise ElevatedCompletionReceiptError(f"{label} must be an integer.")
    return int(value)


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _status(completion: Mapping[str, object]) -> tuple[str, bool]:
    if _bool(completion.get("timed_out"), "completion.timed_out"):
        return "blocked-timeout", False
    if _bool(completion.get("output_limit_exceeded"), "completion.output_limit_exceeded"):
        return "blocked-output-limit", False
    return_code = _int(completion.get("return_code"), "completion.return_code")
    if return_code != 0:
        return "blocked-nonzero-return", False
    return "completed", True


def _validate_source_binding(elevation: Mapping[str, object], completion: Mapping[str, object]) -> None:
    if completion.get("request_sha256") != elevation.get("request_sha256"):
        raise ElevatedCompletionReceiptError("Completion receipt is not bound to the elevation request.")
    if completion.get("elevation_grant_sha256") != elevation.get("elevation_grant_sha256"):
        raise ElevatedCompletionReceiptError("Completion receipt is not bound to the elevation grant.")
    if completion.get("launch_grant_sha256") != elevation.get("launch_grant_sha256"):
        raise ElevatedCompletionReceiptError("Completion receipt is not bound to the elevation launch grant.")
    if _object(completion.get("bootstrap_request"), "completion.bootstrap_request") != _object(
        elevation.get("bootstrap_request"), "elevation.bootstrap_request"
    ):
        raise ElevatedCompletionReceiptError("Completion receipt is not bound to the exact bootstrap request.")
    if elevation.get("process_completion_observed") is not False:
        raise ElevatedCompletionReceiptError("Elevation result must not already claim process completion.")
    if completion.get("process_completion_observed") is not True:
        raise ElevatedCompletionReceiptError("Completion receipt must record process_completion_observed=true.")


def observe_elevated_completion(
    elevation_result: Mapping[str, object],
    completion_receipt: Mapping[str, object],
    *,
    authority_key_path: Path,
    observed_by: str,
    observed_at_utc: str,
    nonce: str,
) -> dict[str, object]:
    try:
        checked_elevation = validate_elevation_result(
            elevation_result,
            authority_key_path=authority_key_path,
        )
        checked_completion = validate_completion_receipt(
            completion_receipt,
            authority_key_path=authority_key_path,
        )
    except (ElevationError, ControlledBootstrapperError) as exc:
        raise ElevatedCompletionReceiptError(f"Elevated completion intake failed: {exc}") from exc
    _validate_source_binding(checked_elevation, checked_completion)
    requested = _utc_dt(checked_elevation.get("requested_at_utc"), "elevation.requested_at_utc")
    completed = _utc_dt(checked_completion.get("completed_at_utc"), "completion.completed_at_utc")
    observed = _utc_dt(observed_at_utc, "observed_at_utc")
    if completed < requested:
        raise ElevatedCompletionReceiptError("Completion receipt must not precede the elevation request.")
    if observed < completed or observed - completed > dt.timedelta(seconds=MAX_OBSERVATION_SECONDS):
        raise ElevatedCompletionReceiptError("Observation must follow completion and occur within 15 minutes.")
    session = _object(_object(checked_elevation.get("elevation_grant"), "elevation.elevation_grant").get("session"), "elevation.session")
    status, completed_ok = _status(checked_completion)
    base = {
        "schema_version": 1,
        "observation_scope": OBSERVATION_SCOPE,
        "capability": OBSERVATION_CAPABILITY,
        "session_sha256": _hash(checked_elevation.get("session_sha256"), "elevation.session_sha256"),
        "operation": _text(session.get("operation"), "session.operation", 32),
        "target_reference": _reference(session.get("target_reference"), "session.target_reference"),
        "prior_installation_reference": session.get("prior_installation_reference"),
        "elevation_result_sha256": _hash(checked_elevation.get("result_sha256"), "elevation.result_sha256"),
        "completion_sha256": _hash(checked_completion.get("completion_sha256"), "completion.completion_sha256"),
        "request_sha256": _hash(checked_elevation.get("request_sha256"), "elevation.request_sha256"),
        "elevation_grant_sha256": _hash(
            checked_elevation.get("elevation_grant_sha256"),
            "elevation.elevation_grant_sha256",
        ),
        "launch_grant_sha256": _hash(checked_elevation.get("launch_grant_sha256"), "elevation.launch_grant_sha256"),
        "bootstrapper_reference": _reference(
            checked_elevation.get("bootstrapper_reference"),
            "elevation.bootstrapper_reference",
        ),
        "helper_reference": _reference(checked_completion.get("helper_reference"), "completion.helper_reference"),
        "status": status,
        "completed": completed_ok,
        "lifecycle_completion_observed": completed_ok,
        "elevation_request_confirmed": True,
        "process_completion_observed": True,
        "return_code": _int(checked_completion.get("return_code"), "completion.return_code"),
        "timed_out": _bool(checked_completion.get("timed_out"), "completion.timed_out"),
        "output_limit_exceeded": _bool(
            checked_completion.get("output_limit_exceeded"),
            "completion.output_limit_exceeded",
        ),
        "requested_at_utc": _utc(checked_elevation.get("requested_at_utc"), "elevation.requested_at_utc"),
        "completed_at_utc": _utc(checked_completion.get("completed_at_utc"), "completion.completed_at_utc"),
        "observed_by": _text(observed_by, "observed_by", 160),
        "observed_at_utc": _utc(observed_at_utc, "observed_at_utc"),
        "nonce": _reference(nonce, "nonce"),
        "elevation_result": checked_elevation,
        "completion_receipt": checked_completion,
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
        "statement": OBSERVATION_STATEMENT,
        "authority": _authority(),
    }
    try:
        return seal_authenticated_record(
            base,
            authority_key_path=authority_key_path,
            digest_field="observation_sha256",
        )
    except ExecutionSecurityError as exc:
        raise ElevatedCompletionReceiptError(f"Elevated completion observation authentication failed: {exc}") from exc


def validate_elevated_completion_observation(
    observation: Mapping[str, object],
    *,
    authority_key_path: Path,
) -> dict[str, object]:
    document = dict(observation)
    if (
        document.get("schema_version") != 1
        or document.get("observation_scope") != OBSERVATION_SCOPE
        or document.get("capability") != OBSERVATION_CAPABILITY
        or document.get("statement") != OBSERVATION_STATEMENT
    ):
        raise ElevatedCompletionReceiptError("Elevated completion observation contract is invalid.")
    if document.get("authority") != _authority():
        raise ElevatedCompletionReceiptError("Elevated completion observation authority must remain all false.")
    for field in (
        "copy_performed",
        "process_launched",
        "elevation_requested",
        "state_published",
        "product_or_game_directory_mutated",
        "runtime_executed",
        "save_mutated",
        "signing_performed",
        "network_publication_performed",
        "catalog_mutated",
        "evidence_promoted",
    ):
        if document.get(field) is not False:
            raise ElevatedCompletionReceiptError(f"Elevated completion observation flag {field} must remain false.")
    try:
        verify_sealed_record(
            document,
            authority_key_path=authority_key_path,
            digest_field="observation_sha256",
        )
    except ExecutionSecurityError as exc:
        raise ElevatedCompletionReceiptError(f"Elevated completion observation authentication failed: {exc}") from exc
    expected = observe_elevated_completion(
        _object(document.get("elevation_result"), "observation.elevation_result"),
        _object(document.get("completion_receipt"), "observation.completion_receipt"),
        authority_key_path=authority_key_path,
        observed_by=_text(document.get("observed_by"), "observation.observed_by", 160),
        observed_at_utc=_utc(document.get("observed_at_utc"), "observation.observed_at_utc"),
        nonce=_reference(document.get("nonce"), "observation.nonce"),
    )
    if document != expected:
        raise ElevatedCompletionReceiptError(
            "Elevated completion observation is stale, altered, or not canonically derived."
        )
    return document


def canonical_observation_bytes(observation: Mapping[str, object], *, authority_key_path: Path) -> bytes:
    return canonical_json(validate_elevated_completion_observation(observation, authority_key_path=authority_key_path))


__all__ = [
    "ElevatedCompletionReceiptError",
    "OBSERVATION_CAPABILITY",
    "OBSERVATION_SCOPE",
    "canonical_observation_bytes",
    "observe_elevated_completion",
    "validate_elevated_completion_observation",
]
