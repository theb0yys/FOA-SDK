#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only SDK-owned installation-state registry and lifecycle eligibility gate."""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Mapping

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
for root in (
    INSTALLER_ROOT / "Bootstrapper" / "InstallationStatePublisher" / "Source",
    INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source",
):
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))

from installation_state_publisher import (  # noqa: E402
    InstallationStatePublisherError,
    validate_state_record,
)
from wizard_view_model import AUTHORITY_FIELDS, canonical_json, sha256  # noqa: E402

QUERY_CAPABILITY = "package-engine.query-installation-state"
SNAPSHOT_SCOPE = "package-installation-state-registry-snapshot"
ELIGIBILITY_SCOPE = "package-installation-state-lifecycle-eligibility"
OPERATIONS = ("install", "repair", "upgrade", "rollback", "uninstall")
REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
SNAPSHOT_STATEMENT = (
    "This snapshot records read-only discovery of SDK-owned installation-state files. "
    "It grants no copy, process-launch, elevation, lifecycle execution, installation-state "
    "publication, product or game directory mutation, runtime execution, save mutation, signing, "
    "network publication, catalogue mutation, or evidence-promotion authority."
)
ELIGIBILITY_STATEMENT = (
    "This eligibility decision is a read-only recommendation derived from a validated "
    "installation-state registry snapshot. It does not create a lifecycle handoff, launch a process, "
    "request elevation, publish state, mutate products, or grant later authority."
)


class InstallationStateRegistryError(RuntimeError):
    pass


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise InstallationStateRegistryError(f"{label} must be an object.")
    return value


def _array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise InstallationStateRegistryError(f"{label} must be an array.")
    return value


def _text(value: object, label: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or value != value.strip() or len(value) > maximum:
        raise InstallationStateRegistryError(f"{label} must be a non-empty trimmed string of at most {maximum} characters.")
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in value):
        raise InstallationStateRegistryError(f"{label} contains a forbidden control character.")
    return value


def _reference(value: object, label: str) -> str:
    result = _text(value, label, 128)
    if REFERENCE_RE.fullmatch(result) is None:
        raise InstallationStateRegistryError(f"{label} must be a stable namespaced logical ID.")
    return result


def _hash(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if SHA256_RE.fullmatch(result) is None:
        raise InstallationStateRegistryError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _optional_reference(value: object, label: str) -> str | None:
    if value is None:
        return None
    return _reference(value, label)


def _utc(value: object, label: str) -> str:
    result = _text(value, label, 64)
    if not result.endswith("Z") or "T" not in result:
        raise InstallationStateRegistryError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise InstallationStateRegistryError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise InstallationStateRegistryError(f"{label} must use UTC.")
    return result


def _operation(value: object, label: str = "operation") -> str:
    result = _text(value, label, 32)
    if result not in OPERATIONS:
        raise InstallationStateRegistryError("operation must be one of: " + ", ".join(OPERATIONS) + ".")
    return result


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _validate_authority(value: object, label: str) -> None:
    authority = _object(value, label)
    if authority != _authority():
        raise InstallationStateRegistryError(f"{label} must contain the exact all-false authority record.")


def _reject_symlink_components(path: Path, label: str) -> None:
    current = path
    while True:
        if current.is_symlink():
            raise InstallationStateRegistryError(f"{label} contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def _state_file_name(state_reference: str) -> str:
    return f"{state_reference}.json"


def _load_state_record(path: Path) -> tuple[dict[str, object], str]:
    if path.is_symlink() or not path.is_file():
        raise InstallationStateRegistryError(f"State registry entry is not a regular non-symlink file: {path.name}")
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    try:
        raw = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise InstallationStateRegistryError(f"State registry entry is not valid UTF-8 JSON: {path.name}") from exc
    try:
        record = validate_state_record(_object(raw, f"{path.name}.record"))
    except InstallationStatePublisherError as exc:
        raise InstallationStateRegistryError(f"State registry entry failed validation: {path.name}: {exc}") from exc
    canonical = canonical_json(record)
    if canonical != data:
        raise InstallationStateRegistryError(f"State registry entry is not canonical JSON: {path.name}")
    if path.name != _state_file_name(str(record["state_reference"])):
        raise InstallationStateRegistryError("State registry file name does not match its state_reference.")
    return record, digest


def _record_summary(record: Mapping[str, object], state_file_sha256: str) -> dict[str, object]:
    summary = _object(record.get("summary"), "state_record.summary")
    row = {
        "state_reference": _reference(record.get("state_reference"), "state_record.state_reference"),
        "state_status": _text(record.get("state_status"), "state_record.state_status", 32),
        "operation": _operation(record.get("operation"), "state_record.operation"),
        "target_reference": _reference(record.get("target_reference"), "state_record.target_reference"),
        "prior_installation_reference": _optional_reference(
            record.get("prior_installation_reference"),
            "state_record.prior_installation_reference",
        ),
        "published_at_utc": _utc(record.get("published_at_utc"), "state_record.published_at_utc"),
        "session_sha256": _hash(record.get("session_sha256"), "state_record.session_sha256"),
        "handoff_sha256": _hash(record.get("handoff_sha256"), "state_record.handoff_sha256"),
        "lifecycle_result_sha256": _hash(
            record.get("lifecycle_result_sha256"),
            "state_record.lifecycle_result_sha256",
        ),
        "copy_receipt_sha256": None if record.get("copy_receipt_sha256") is None else _hash(
            record.get("copy_receipt_sha256"),
            "state_record.copy_receipt_sha256",
        ),
        "launch_result_sha256": None if record.get("launch_result_sha256") is None else _hash(
            record.get("launch_result_sha256"),
            "state_record.launch_result_sha256",
        ),
        "state_record_sha256": _hash(record.get("state_record_sha256"), "state_record.state_record_sha256"),
        "state_file_sha256": _hash(state_file_sha256, "state_file_sha256"),
        "package_order": [
            _reference(item, f"state_record.package_order[{index}]")
            for index, item in enumerate(_array(record.get("package_order"), "state_record.package_order"))
        ],
        "summary": {
            "package_count": summary.get("package_count"),
            "payload_file_count": summary.get("payload_file_count"),
            "payload_size_bytes": summary.get("payload_size_bytes"),
        },
    }
    if row["state_status"] not in {"active", "removed", "rolled-back"}:
        raise InstallationStateRegistryError("state_record.state_status is not a supported registry status.")
    return row


def build_registry_snapshot(
    records: list[Mapping[str, object]], *, state_root_reference: str, observed_at_utc: str,
) -> dict[str, object]:
    summaries: list[dict[str, object]] = []
    seen_state: set[str] = set()
    seen_file: set[str] = set()
    for index, raw in enumerate(records):
        record = dict(_object(raw, f"records[{index}]"))
        state_file_hash = _hash(record.pop("state_file_sha256", None), f"records[{index}].state_file_sha256")
        try:
            checked_record = validate_state_record(record)
        except InstallationStatePublisherError as exc:
            raise InstallationStateRegistryError(f"Registry snapshot input record failed validation: {exc}") from exc
        row = _record_summary(checked_record, state_file_hash)
        if row["state_reference"] in seen_state:
            raise InstallationStateRegistryError("Duplicate state_reference in registry snapshot input.")
        if row["state_file_sha256"] in seen_file:
            raise InstallationStateRegistryError("Duplicate state file fingerprint in registry snapshot input.")
        seen_state.add(str(row["state_reference"]))
        seen_file.add(str(row["state_file_sha256"]))
        summaries.append(row)
    summaries.sort(
        key=lambda row: (
            str(row["target_reference"]),
            str(row["published_at_utc"]),
            str(row["state_reference"]),
            str(row["state_file_sha256"]),
        )
    )
    active = sum(1 for row in summaries if row["state_status"] == "active")
    removed = sum(1 for row in summaries if row["state_status"] == "removed")
    rolled_back = sum(1 for row in summaries if row["state_status"] == "rolled-back")
    base = {
        "schema_version": 1,
        "snapshot_scope": SNAPSHOT_SCOPE,
        "capability": QUERY_CAPABILITY,
        "state_root_reference": _reference(state_root_reference, "state_root_reference"),
        "observed_at_utc": _utc(observed_at_utc, "observed_at_utc"),
        "record_count": len(summaries),
        "active_count": active,
        "removed_count": removed,
        "rolled_back_count": rolled_back,
        "records": summaries,
        "product_or_game_directory_mutated": False,
        "runtime_executed": False,
        "save_mutated": False,
        "signing_performed": False,
        "network_publication_performed": False,
        "catalog_mutated": False,
        "evidence_promoted": False,
        "statement": SNAPSHOT_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "snapshot_sha256": sha256(base)}


def query_state_registry(state_root: Path, *, state_root_reference: str, observed_at_utc: str) -> dict[str, object]:
    root = Path(state_root)
    if not root.is_absolute() or root.is_symlink() or not root.is_dir():
        raise InstallationStateRegistryError("State root must be an absolute existing non-symlink directory.")
    _reject_symlink_components(root, "State root")
    rows: list[dict[str, object]] = []
    for entry in sorted(root.glob("*.json"), key=lambda item: item.name.casefold()):
        record, state_file_hash = _load_state_record(entry)
        row = dict(record)
        row["state_file_sha256"] = state_file_hash
        rows.append(row)
    return build_registry_snapshot(
        rows,
        state_root_reference=state_root_reference,
        observed_at_utc=observed_at_utc,
    )


def validate_registry_snapshot(snapshot: Mapping[str, object]) -> dict[str, object]:
    document = dict(snapshot)
    if document.get("schema_version") != 1 or document.get("snapshot_scope") != SNAPSHOT_SCOPE:
        raise InstallationStateRegistryError("Registry snapshot schema or scope is invalid.")
    if document.get("capability") != QUERY_CAPABILITY or document.get("statement") != SNAPSHOT_STATEMENT:
        raise InstallationStateRegistryError("Registry snapshot capability or statement is invalid.")
    _validate_authority(document.get("authority"), "snapshot.authority")
    for field in (
        "product_or_game_directory_mutated", "runtime_executed", "save_mutated",
        "signing_performed", "network_publication_performed", "catalog_mutated", "evidence_promoted",
    ):
        if document.get(field) is not False:
            raise InstallationStateRegistryError(f"Registry snapshot forbidden side-effect flag {field} must be false.")
    records = _array(document.get("records"), "snapshot.records")
    if document.get("record_count") != len(records):
        raise InstallationStateRegistryError("snapshot.record_count does not match records.")
    if document.get("active_count") != sum(1 for row in records if _object(row, "snapshot.records[]").get("state_status") == "active"):
        raise InstallationStateRegistryError("snapshot.active_count does not match records.")
    if document.get("removed_count") != sum(1 for row in records if _object(row, "snapshot.records[]").get("state_status") == "removed"):
        raise InstallationStateRegistryError("snapshot.removed_count does not match records.")
    if document.get("rolled_back_count") != sum(1 for row in records if _object(row, "snapshot.records[]").get("state_status") == "rolled-back"):
        raise InstallationStateRegistryError("snapshot.rolled_back_count does not match records.")
    declared = _hash(document.get("snapshot_sha256"), "snapshot.snapshot_sha256")
    unsigned = {key: value for key, value in document.items() if key != "snapshot_sha256"}
    if sha256(unsigned) != declared:
        raise InstallationStateRegistryError("Registry snapshot fingerprint does not match its content.")
    return document


def _current_record(snapshot: Mapping[str, object], target_reference: str) -> dict[str, object] | None:
    target = _reference(target_reference, "target_reference")
    matches = [
        _object(row, "snapshot.records[]")
        for row in _array(snapshot.get("records"), "snapshot.records")
        if row.get("target_reference") == target
    ]
    if not matches:
        return None
    matches.sort(
        key=lambda row: (
            _utc(row.get("published_at_utc"), "state_record.published_at_utc"),
            _reference(row.get("state_reference"), "state_record.state_reference"),
            _hash(row.get("state_file_sha256"), "state_record.state_file_sha256"),
        )
    )
    return matches[-1]


def _eligibility_for(operation: str, current: Mapping[str, object] | None) -> tuple[bool, str, str | None, str | None]:
    if current is None:
        if operation == "install":
            return True, "eligible-no-current-state", None, None
        return False, "blocked-no-current-installation-state", None, None
    current_state = _text(current.get("state_status"), "current.state_status", 32)
    state_reference = _reference(current.get("state_reference"), "current.state_reference")
    prior = _optional_reference(current.get("prior_installation_reference"), "current.prior_installation_reference")
    if current_state == "removed":
        if operation == "install":
            return True, "eligible-after-removed-state", None, state_reference
        return False, "blocked-current-state-removed", None, state_reference
    if current_state in {"active", "rolled-back"}:
        if operation == "install":
            return False, "blocked-current-installation-state-exists", state_reference, state_reference
        if operation in {"repair", "upgrade", "uninstall"}:
            return True, f"eligible-current-state-{current_state}", state_reference, state_reference
        if operation == "rollback":
            if prior is None:
                return False, "blocked-no-rollback-base", None, state_reference
            return True, "eligible-rollback-base-present", state_reference, state_reference
    raise InstallationStateRegistryError("Unsupported current state status.")


def build_lifecycle_eligibility(
    snapshot: Mapping[str, object], *, operation: str, target_reference: str, decided_at_utc: str,
) -> dict[str, object]:
    checked = validate_registry_snapshot(snapshot)
    checked_operation = _operation(operation)
    current = _current_record(checked, target_reference)
    eligible, reason, prior_reference, current_reference = _eligibility_for(checked_operation, current)
    base = {
        "schema_version": 1,
        "eligibility_scope": ELIGIBILITY_SCOPE,
        "capability": QUERY_CAPABILITY,
        "snapshot_sha256": checked["snapshot_sha256"],
        "operation": checked_operation,
        "target_reference": _reference(target_reference, "target_reference"),
        "eligible": eligible,
        "reason": reason,
        "current_state_reference": current_reference,
        "recommended_prior_installation_reference": prior_reference,
        "current_state_status": None if current is None else current["state_status"],
        "current_state_file_sha256": None if current is None else current["state_file_sha256"],
        "decided_at_utc": _utc(decided_at_utc, "decided_at_utc"),
        "lifecycle_handoff_created": False,
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
        "statement": ELIGIBILITY_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "eligibility_sha256": sha256(base)}


def validate_lifecycle_eligibility(eligibility: Mapping[str, object]) -> dict[str, object]:
    document = dict(eligibility)
    if document.get("schema_version") != 1 or document.get("eligibility_scope") != ELIGIBILITY_SCOPE:
        raise InstallationStateRegistryError("Lifecycle eligibility schema or scope is invalid.")
    if document.get("capability") != QUERY_CAPABILITY or document.get("statement") != ELIGIBILITY_STATEMENT:
        raise InstallationStateRegistryError("Lifecycle eligibility capability or statement is invalid.")
    _validate_authority(document.get("authority"), "eligibility.authority")
    for field in (
        "lifecycle_handoff_created", "copy_performed", "process_launched", "elevation_requested",
        "state_published", "product_or_game_directory_mutated", "runtime_executed", "save_mutated",
        "signing_performed", "network_publication_performed", "catalog_mutated", "evidence_promoted",
    ):
        if document.get(field) is not False:
            raise InstallationStateRegistryError(f"Lifecycle eligibility forbidden side-effect flag {field} must be false.")
    _operation(document.get("operation"), "eligibility.operation")
    _reference(document.get("target_reference"), "eligibility.target_reference")
    _hash(document.get("snapshot_sha256"), "eligibility.snapshot_sha256")
    if document.get("current_state_reference") is not None:
        _reference(document.get("current_state_reference"), "eligibility.current_state_reference")
    if document.get("recommended_prior_installation_reference") is not None:
        _reference(
            document.get("recommended_prior_installation_reference"),
            "eligibility.recommended_prior_installation_reference",
        )
    if document.get("current_state_file_sha256") is not None:
        _hash(document.get("current_state_file_sha256"), "eligibility.current_state_file_sha256")
    if type(document.get("eligible")) is not bool:
        raise InstallationStateRegistryError("eligibility.eligible must be a boolean.")
    _text(document.get("reason"), "eligibility.reason", 128)
    declared = _hash(document.get("eligibility_sha256"), "eligibility.eligibility_sha256")
    unsigned = {key: value for key, value in document.items() if key != "eligibility_sha256"}
    if sha256(unsigned) != declared:
        raise InstallationStateRegistryError("Lifecycle eligibility fingerprint does not match its content.")
    return document


def canonical_snapshot_bytes(snapshot: Mapping[str, object]) -> bytes:
    return canonical_json(validate_registry_snapshot(snapshot))


def canonical_eligibility_bytes(eligibility: Mapping[str, object]) -> bytes:
    return canonical_json(validate_lifecycle_eligibility(eligibility))
