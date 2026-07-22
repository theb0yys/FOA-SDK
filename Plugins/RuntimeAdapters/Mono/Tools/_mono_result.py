# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Externally supplied runtime-result evidence for the Mono adapter."""
from __future__ import annotations
from pathlib import Path
from typing import Mapping, Sequence
from _mono_contract import *
from _mono_gate import validate_gate

OUTCOMES = {"not_attempted", "succeeded", "failed"}


def build_result(gate: Mapping[str, object], *, outcome: str, captured_at_utc: str, started_at_utc: str | None, completed_at_utc: str | None, loader_seen: bool, framework_seen: bool, startup_marker_seen: bool, log_reference: str, failures: Sequence[str], package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    checked_gate = validate_gate(gate, package_root)
    if outcome not in OUTCOMES:
        raise MonoAdapterError("Mono runtime result outcome is invalid.")
    captured = utc(captured_at_utc, "captured_at_utc")
    requested = utc(checked_gate["requested_at_utc"], "gate.requested_at_utc")
    expires = utc(checked_gate["expires_at_utc"], "gate.expires_at_utc")
    if not requested <= captured <= expires:
        raise MonoAdapterError("Mono runtime result capture is outside the execution window.")
    started = utc(started_at_utc, "started_at_utc") if started_at_utc is not None else None
    completed = utc(completed_at_utc, "completed_at_utc") if completed_at_utc is not None else None
    if (started is None) != (completed is None) or (started is not None and not requested <= started <= completed <= captured):
        raise MonoAdapterError("Mono runtime result chronology is impossible.")
    if not all(isinstance(value, bool) for value in (loader_seen, framework_seen, startup_marker_seen)):
        raise MonoAdapterError("Mono runtime observations must be booleans.")
    checked_log = safe_reference(log_reference, "log_reference", True)
    checked_failures = sorted(text(value, f"failures[{index}]", 512) for index, value in enumerate(failures))
    if len(checked_failures) > 32 or len(set(checked_failures)) != len(checked_failures):
        raise MonoAdapterError("Mono runtime failures are duplicated or unbounded.")
    observations = (loader_seen, framework_seen, startup_marker_seen)
    if outcome == "not_attempted" and (started is not None or any(observations) or checked_log or checked_failures):
        raise MonoAdapterError("not_attempted results cannot contain runtime observations.")
    if outcome == "succeeded" and (started is None or not all(observations) or checked_failures):
        raise MonoAdapterError("A succeeded Mono result requires all exact startup observations and no failures.")
    if outcome == "failed" and (started is None or not checked_failures):
        raise MonoAdapterError("A failed Mono result requires chronology and at least one failure.")
    candidate = outcome == "succeeded" and all(observations)
    base = {
        "schema_version": 1, "result_kind": "foa-mono-runtime-result",
        "adapter_id": ADAPTER_ID, "package_id": PACKAGE_ID,
        "package_version": PACKAGE_VERSION, "gate_sha256": checked_gate["gate_sha256"],
        "binary_sha256": object_value(checked_gate["binary"], "gate.binary")["sha256"],
        "profile": PROFILE, "outcome": outcome,
        "started_at_utc": started_at_utc, "completed_at_utc": completed_at_utc,
        "captured_at_utc": captured_at_utc,
        "observations": {"loader_seen": loader_seen, "framework_seen": framework_seen, "startup_marker_seen": startup_marker_seen, "startup_marker": STARTUP_MARKER},
        "log_reference": checked_log, "failures": checked_failures,
        "candidate_evidence_eligible": candidate,
        "candidate_evidence_promoted": False, "authority": authority(),
    }
    return {**base, "result_sha256": fingerprint(base)}


def validate_result(result: Mapping[str, object], gate: Mapping[str, object], package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    document = dict(result)
    observations = object_value(document.get("observations"), "observations")
    rebuilt = build_result(
        gate, outcome=document.get("outcome"), captured_at_utc=document.get("captured_at_utc"),
        started_at_utc=document.get("started_at_utc"), completed_at_utc=document.get("completed_at_utc"),
        loader_seen=observations.get("loader_seen"), framework_seen=observations.get("framework_seen"),
        startup_marker_seen=observations.get("startup_marker_seen"), log_reference=document.get("log_reference"),
        failures=document.get("failures"), package_root=package_root,
    )
    if document != rebuilt:
        raise MonoAdapterError("Mono runtime result is stale, altered, or noncanonical.")
    return document
