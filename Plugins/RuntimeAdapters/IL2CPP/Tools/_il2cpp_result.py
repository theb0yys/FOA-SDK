# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Externally supplied runtime-result evidence for the IL2CPP adapter."""
from __future__ import annotations
from pathlib import Path
from typing import Mapping, Sequence
from _il2cpp_contract import *
from _il2cpp_gate import validate_gate
OUTCOMES={"not_attempted","succeeded","failed"}

def build_result(gate: Mapping[str, object], *, outcome: str, captured_at_utc: str, started_at_utc: str|None, completed_at_utc: str|None, loader_seen: bool, framework_seen: bool, interop_runtime_seen: bool, generated_interop_seen: bool, startup_marker_seen: bool, log_reference: str, failures: Sequence[str], package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    checked_gate=validate_gate(gate,package_root)
    if outcome not in OUTCOMES: raise Il2CppAdapterError("IL2CPP runtime result outcome is invalid.")
    captured=utc(captured_at_utc,"captured_at_utc"); requested=utc(checked_gate["requested_at_utc"],"gate.requested_at_utc"); expires=utc(checked_gate["expires_at_utc"],"gate.expires_at_utc")
    if not requested <= captured <= expires: raise Il2CppAdapterError("IL2CPP runtime result capture is outside the execution window.")
    started=utc(started_at_utc,"started_at_utc") if started_at_utc is not None else None; completed=utc(completed_at_utc,"completed_at_utc") if completed_at_utc is not None else None
    if (started is None)!=(completed is None) or (started is not None and not requested <= started <= completed <= captured): raise Il2CppAdapterError("IL2CPP runtime result chronology is impossible.")
    observations=(loader_seen,framework_seen,interop_runtime_seen,generated_interop_seen,startup_marker_seen)
    if not all(isinstance(value,bool) for value in observations): raise Il2CppAdapterError("IL2CPP runtime observations must be booleans.")
    checked_log=safe_reference(log_reference,"log_reference",True); checked_failures=sorted(text(value,f"failures[{index}]",512) for index,value in enumerate(failures))
    if len(checked_failures)>32 or len(set(checked_failures))!=len(checked_failures): raise Il2CppAdapterError("IL2CPP runtime failures are duplicated or unbounded.")
    if outcome=="not_attempted" and (started is not None or any(observations) or checked_log or checked_failures): raise Il2CppAdapterError("not_attempted results cannot contain runtime observations.")
    if outcome=="succeeded" and (started is None or not all(observations) or checked_failures): raise Il2CppAdapterError("A succeeded IL2CPP result requires loader, framework, interop, generated-input, and startup observations with no failures.")
    if outcome=="failed" and (started is None or not checked_failures): raise Il2CppAdapterError("A failed IL2CPP result requires chronology and at least one failure.")
    candidate=outcome=="succeeded" and all(observations)
    interop=object_value(checked_gate["interop_manifest"],"gate.interop_manifest")
    base={"schema_version":1,"result_kind":"foa-il2cpp-runtime-result","adapter_id":ADAPTER_ID,"package_id":PACKAGE_ID,"package_version":PACKAGE_VERSION,"gate_sha256":checked_gate["gate_sha256"],"binary_sha256":object_value(checked_gate["binary"],"gate.binary")["sha256"],"interop_manifest_sha256":interop["interop_manifest_sha256"],"profile":PROFILE,"outcome":outcome,"started_at_utc":started_at_utc,"completed_at_utc":completed_at_utc,"captured_at_utc":captured_at_utc,"observations":{"loader_seen":loader_seen,"framework_seen":framework_seen,"interop_runtime_seen":interop_runtime_seen,"generated_interop_seen":generated_interop_seen,"startup_marker_seen":startup_marker_seen,"startup_marker":STARTUP_MARKER},"log_reference":checked_log,"failures":checked_failures,"candidate_evidence_eligible":candidate,"candidate_evidence_promoted":False,"authority":authority()}
    return {**base,"result_sha256":fingerprint(base)}

def validate_result(result: Mapping[str, object], gate: Mapping[str, object], package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    document=dict(result); obs=object_value(document.get("observations"),"observations")
    rebuilt=build_result(gate,outcome=document.get("outcome"),captured_at_utc=document.get("captured_at_utc"),started_at_utc=document.get("started_at_utc"),completed_at_utc=document.get("completed_at_utc"),loader_seen=obs.get("loader_seen"),framework_seen=obs.get("framework_seen"),interop_runtime_seen=obs.get("interop_runtime_seen"),generated_interop_seen=obs.get("generated_interop_seen"),startup_marker_seen=obs.get("startup_marker_seen"),log_reference=document.get("log_reference"),failures=document.get("failures"),package_root=package_root)
    if document != rebuilt: raise Il2CppAdapterError("IL2CPP runtime result is stale, altered, or noncanonical.")
    return document
