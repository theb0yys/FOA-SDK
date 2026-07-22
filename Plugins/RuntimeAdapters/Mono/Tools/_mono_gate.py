# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Non-authorizing external-executor review gate for the Mono adapter."""
from __future__ import annotations
from pathlib import Path
from typing import Mapping, Sequence
from _mono_contract import *


def exact_profile(value: object) -> dict[str, object]:
    profile = object_value(value, "profile")
    if profile != PROFILE:
        raise MonoAdapterError("Mono execution gate requires the exact Mono profile.")
    return profile


def evaluate_execution_gate(*, binary_sha256: str, binary_bytes: int, profile: Mapping[str, object], identity_evidence_ids: Sequence[str], parity_evidence_ids: Sequence[str], confirmation_receipt_sha256: str, requested_by: str, requested_at_utc: str, expires_at_utc: str, package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    checked_profile = exact_profile(profile)
    checked_binary = sha256(binary_sha256, "binary_sha256")
    if not isinstance(binary_bytes, int) or isinstance(binary_bytes, bool) or binary_bytes <= 0:
        raise MonoAdapterError("binary_bytes must be a positive integer.")
    identity = unique_ids(list(identity_evidence_ids), "identity_evidence_ids")
    parity = unique_ids(list(parity_evidence_ids), "parity_evidence_ids")
    if set(identity) & set(parity):
        raise MonoAdapterError("Evidence IDs cannot satisfy multiple evidence classes.")
    receipt = sha256(confirmation_receipt_sha256, "confirmation_receipt_sha256")
    requester = stable_id(requested_by, "requested_by")
    requested = utc(requested_at_utc, "requested_at_utc")
    expires = utc(expires_at_utc, "expires_at_utc")
    if requested >= expires:
        raise MonoAdapterError("The external review window is invalid.")
    base = {
        "schema_version": 1, "gate_kind": "foa-mono-external-execution-gate",
        "adapter_id": ADAPTER_ID, "package_id": PACKAGE_ID,
        "package_version": PACKAGE_VERSION,
        "build_plan_sha256": build_plan(package_root)["build_plan_sha256"],
        "binary": {"relative_path": "FOA.SDK.RuntimeAdapter.Mono.dll", "sha256": checked_binary, "bytes": binary_bytes},
        "profile": checked_profile,
        "identity_evidence_ids": identity,
        "install_parity_evidence_ids": parity,
        "confirmation_receipt_sha256": receipt,
        "requested_by": requester, "requested_at_utc": requested_at_utc,
        "expires_at_utc": expires_at_utc,
        "ready_for_external_executor_review": True,
        "execution_authorized": False, "reasons": [], "authority": authority(),
    }
    return {**base, "gate_sha256": fingerprint(base)}


def validate_gate(gate: Mapping[str, object], package_root: Path = PACKAGE_ROOT) -> dict[str, object]:
    document = dict(gate)
    keys = {"schema_version", "gate_kind", "adapter_id", "package_id", "package_version", "build_plan_sha256", "binary", "profile", "identity_evidence_ids", "install_parity_evidence_ids", "confirmation_receipt_sha256", "requested_by", "requested_at_utc", "expires_at_utc", "ready_for_external_executor_review", "execution_authorized", "reasons", "authority", "gate_sha256"}
    if set(document) != keys:
        raise MonoAdapterError("Mono execution gate field set drifted.")
    binary = object_value(document["binary"], "binary")
    rebuilt = evaluate_execution_gate(
        binary_sha256=binary.get("sha256"), binary_bytes=binary.get("bytes"),
        profile=document["profile"], identity_evidence_ids=document["identity_evidence_ids"],
        parity_evidence_ids=document["install_parity_evidence_ids"],
        confirmation_receipt_sha256=document["confirmation_receipt_sha256"],
        requested_by=document["requested_by"], requested_at_utc=document["requested_at_utc"],
        expires_at_utc=document["expires_at_utc"], package_root=package_root,
    )
    if document != rebuilt or document.get("execution_authorized") is not False:
        raise MonoAdapterError("Mono execution gate is stale, altered, or authorizing.")
    return document
