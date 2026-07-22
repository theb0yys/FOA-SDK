# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Non-authorizing external-executor review gate for the IL2CPP adapter."""
from __future__ import annotations
from pathlib import Path
from typing import Mapping, Sequence
from _il2cpp_contract import *

def exact_profile(value: object) -> dict[str, object]:
    profile=object_value(value,"profile")
    if profile != PROFILE: raise Il2CppAdapterError("IL2CPP execution gate requires the exact IL2CPP profile.")
    return profile

def evaluate_execution_gate(*, binary_sha256: str, binary_bytes: int, profile: Mapping[str, object], interop_manifest: Mapping[str, object], identity_evidence_ids: Sequence[str], parity_evidence_ids: Sequence[str], interop_evidence_ids: Sequence[str], confirmation_receipt_sha256: str, requested_by: str, requested_at_utc: str, expires_at_utc: str, package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    checked_profile=exact_profile(profile); checked_binary=sha256(binary_sha256,"binary_sha256"); positive_int(binary_bytes,"binary_bytes")
    interop=validate_interop_manifest(interop_manifest)
    identity=unique_ids(list(identity_evidence_ids),"identity_evidence_ids"); parity=unique_ids(list(parity_evidence_ids),"parity_evidence_ids"); interop_ids=unique_ids(list(interop_evidence_ids),"interop_evidence_ids")
    if (set(identity)&set(parity)) or (set(identity)&set(interop_ids)) or (set(parity)&set(interop_ids)): raise Il2CppAdapterError("Evidence IDs cannot satisfy multiple evidence classes.")
    receipt=sha256(confirmation_receipt_sha256,"confirmation_receipt_sha256"); requester=stable_id(requested_by,"requested_by")
    requested=utc(requested_at_utc,"requested_at_utc"); expires=utc(expires_at_utc,"expires_at_utc")
    if requested >= expires or (expires-requested).total_seconds() > 24*60*60: raise Il2CppAdapterError("The external review window is invalid or exceeds 24 hours.")
    base={"schema_version":1,"gate_kind":"foa-il2cpp-external-execution-gate","adapter_id":ADAPTER_ID,"package_id":PACKAGE_ID,"package_version":PACKAGE_VERSION,"build_plan_sha256":build_plan(interop,package_root)["build_plan_sha256"],"interop_manifest":interop,"binary":{"relative_path":"FOA.SDK.RuntimeAdapter.IL2CPP.dll","sha256":checked_binary,"bytes":binary_bytes},"profile":checked_profile,"identity_evidence_ids":identity,"install_parity_evidence_ids":parity,"interop_evidence_ids":interop_ids,"confirmation_receipt_sha256":receipt,"requested_by":requester,"requested_at_utc":requested_at_utc,"expires_at_utc":expires_at_utc,"ready_for_external_executor_review":True,"execution_authorized":False,"reasons":[],"authority":authority()}
    return {**base,"gate_sha256":fingerprint(base)}

def validate_gate(gate: Mapping[str, object], package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    document=dict(gate); binary=object_value(document.get("binary"),"binary"); interop=object_value(document.get("interop_manifest"),"interop_manifest")
    rebuilt=evaluate_execution_gate(binary_sha256=binary.get("sha256"),binary_bytes=binary.get("bytes"),profile=document.get("profile"),interop_manifest=interop,identity_evidence_ids=document.get("identity_evidence_ids"),parity_evidence_ids=document.get("install_parity_evidence_ids"),interop_evidence_ids=document.get("interop_evidence_ids"),confirmation_receipt_sha256=document.get("confirmation_receipt_sha256"),requested_by=document.get("requested_by"),requested_at_utc=document.get("requested_at_utc"),expires_at_utc=document.get("expires_at_utc"),package_root=package_root)
    if document != rebuilt or document.get("execution_authorized") is not False: raise Il2CppAdapterError("IL2CPP execution gate is stale, altered, or authorizing.")
    return document
