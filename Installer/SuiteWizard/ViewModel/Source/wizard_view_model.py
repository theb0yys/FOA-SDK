#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Pure deterministic suite-wizard view-model and review confirmation contract."""

from __future__ import annotations

import datetime as dt
import hashlib
import json
import re
from typing import Iterable, Mapping

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
ACK_ID_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
STATEMENT = (
    "I reviewed the exact FOA-SDK installer plan and understand that this "
    "confirmation grants no acquisition, installation, elevation, launch, "
    "deployment, save-mutation, signing, publication, catalog-mutation, or "
    "evidence-promotion authority."
)
AUTHORITY_FIELDS = (
    "acquisition", "installation", "elevation", "game_launch",
    "runtime_execution", "deployment", "save_mutation", "signing",
    "publication", "catalog_mutation", "evidence_promotion",
)


class WizardContractError(RuntimeError):
    pass


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def sha256(value: object) -> str:
    return hashlib.sha256(canonical_json(value)).hexdigest()


def _obj(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise WizardContractError(f"{label} must be an object.")
    return value


def _arr(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise WizardContractError(f"{label} must be an array.")
    return value


def _text(value: object, label: str, *, empty: bool = False) -> str:
    if not isinstance(value, str) or (not empty and not value.strip()):
        raise WizardContractError(f"{label} must be a non-empty string.")
    return value


def _bool(value: object, label: str) -> bool:
    if type(value) is not bool:
        raise WizardContractError(f"{label} must be a boolean.")
    return value


def _int(value: object, label: str) -> int:
    if type(value) is not int:
        raise WizardContractError(f"{label} must be an integer.")
    return value


def _hash(value: object, label: str) -> str:
    result = _text(value, label)
    if not SHA256_RE.fullmatch(result):
        raise WizardContractError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _strings(value: object, label: str) -> tuple[str, ...]:
    result = tuple(_text(item, f"{label}[{index}]") for index, item in enumerate(_arr(value, label)))
    if len(set(result)) != len(result):
        raise WizardContractError(f"{label} must not contain duplicates.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label)
    if not result.endswith("Z") or "T" not in result:
        raise WizardContractError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise WizardContractError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise WizardContractError(f"{label} must use UTC.")
    return result


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _validate_authority(value: object, label: str) -> None:
    authority = _obj(value, label)
    if set(authority) != set(AUTHORITY_FIELDS):
        raise WizardContractError(f"{label} contains the wrong authority fields.")
    for field in AUTHORITY_FIELDS:
        if _bool(authority.get(field), f"{label}.{field}"):
            raise WizardContractError(f"{label}.{field} must remain false.")


def validate_plan(plan: Mapping[str, object]) -> dict[str, object]:
    document = dict(plan)
    if _int(document.get("schema_version"), "plan.schema_version") != 1:
        raise WizardContractError("plan.schema_version must be exactly 1.")
    declared = _hash(document.get("plan_sha256"), "plan.plan_sha256")
    unsigned = {key: value for key, value in document.items() if key != "plan_sha256"}
    calculated = sha256(unsigned)
    if declared != calculated:
        raise WizardContractError(
            f"plan.plan_sha256 mismatch: declared {declared}, calculated {calculated}."
        )

    suite = _obj(document.get("suite"), "plan.suite")
    for field in ("suite_id", "display_name", "version", "channel"):
        _text(suite.get(field), f"plan.suite.{field}")
    _hash(suite.get("manifest_sha256"), "plan.suite.manifest_sha256")

    context = _obj(document.get("context"), "plan.context")
    for field in ("platform", "architecture", "runtime_target"):
        _text(context.get(field), f"plan.context.{field}")
    for field in ("game_version", "branch"):
        _text(context.get(field), f"plan.context.{field}", empty=True)

    selection = _obj(document.get("selection"), "plan.selection")
    for field in ("selected_package_ids", "excluded_package_ids", "selected_feature_ids"):
        _strings(selection.get(field), f"plan.selection.{field}")

    policies = _obj(document.get("policies"), "plan.policies")
    for field in ("network_allowed", "elevation_allowed", "unreviewed_packages_allowed", "silent_install_allowed"):
        _bool(policies.get(field), f"plan.policies.{field}")
    if policies["unreviewed_packages_allowed"] is not False:
        raise WizardContractError("plan must keep unreviewed_packages_allowed false.")

    order = _strings(document.get("package_order"), "plan.package_order")
    packages = _arr(document.get("packages"), "plan.packages")
    if len(packages) != len(order):
        raise WizardContractError("plan.packages must match plan.package_order length.")

    ids: list[str] = []
    dependencies: dict[str, tuple[str, ...]] = {}
    file_count = byte_count = 0
    elevation = False
    for index, raw in enumerate(packages):
        row = _obj(raw, f"plan.packages[{index}]")
        package_id = _text(row.get("package_id"), f"plan.packages[{index}].package_id")
        ids.append(package_id)
        for field in ("display_name", "version", "kind", "status"):
            _text(row.get(field), f"{package_id}.{field}")
        if row["status"] in {"planned", "blocked"}:
            raise WizardContractError(
                f"{package_id} is not reviewable while status is {row['status']!r}."
            )
        _strings(row.get("selection_reasons"), f"{package_id}.selection_reasons")
        dependencies[package_id] = _strings(row.get("dependency_ids"), f"{package_id}.dependency_ids")
        _strings(row.get("capabilities"), f"{package_id}.capabilities")
        _hash(row.get("manifest_sha256"), f"{package_id}.manifest_sha256")

        source = _obj(row.get("source"), f"{package_id}.source")
        for field in ("kind", "repository", "commit", "path"):
            _text(source.get(field), f"{package_id}.source.{field}")
        if "fingerprint_sha256" in source:
            _hash(source["fingerprint_sha256"], f"{package_id}.source.fingerprint_sha256")

        lifecycle = _obj(row.get("lifecycle"), f"{package_id}.lifecycle")
        _text(lifecycle.get("install_scope"), f"{package_id}.lifecycle.install_scope")
        for field in (
            "elevation_required", "repair_supported", "upgrade_supported",
            "uninstall_supported", "rollback_required", "preserve_external_workspaces",
        ):
            _bool(lifecycle.get(field), f"{package_id}.lifecycle.{field}")
        if lifecycle["preserve_external_workspaces"] is not True:
            raise WizardContractError(f"{package_id} must preserve external workspaces.")
        elevation = elevation or lifecycle["elevation_required"]

        legal = _obj(row.get("legal"), f"{package_id}.legal")
        _text(legal.get("license_expression"), f"{package_id}.legal.license_expression")
        if _text(legal.get("redistribution_review"), f"{package_id}.legal.redistribution_review") not in {
            "approved", "not-applicable",
        }:
            raise WizardContractError(f"{package_id} lacks accepted redistribution review.")
        _strings(legal.get("notice_files"), f"{package_id}.legal.notice_files")

        for item_index, raw_item in enumerate(_arr(row.get("payload"), f"{package_id}.payload")):
            item = _obj(raw_item, f"{package_id}.payload[{item_index}]")
            for field in ("source", "destination", "redistribution"):
                _text(item.get(field), f"{package_id}.payload[{item_index}].{field}")
            if item["redistribution"] not in {"project-owned", "approved", "notice-required"}:
                raise WizardContractError(
                    f"{package_id}.payload[{item_index}].redistribution is not accepted."
                )
            _hash(item.get("sha256"), f"{package_id}.payload[{item_index}].sha256")
            size = _int(item.get("size_bytes"), f"{package_id}.payload[{item_index}].size_bytes")
            if size < 0:
                raise WizardContractError(
                    f"{package_id}.payload[{item_index}].size_bytes must be non-negative."
                )
            file_count += 1
            byte_count += size

    if tuple(ids) != order:
        raise WizardContractError("plan.packages must be in exact plan.package_order.")
    if len(set(ids)) != len(ids):
        raise WizardContractError("plan contains duplicate package IDs.")
    selected_ids = set(ids)
    for package_id, required_ids in dependencies.items():
        unknown = sorted(set(required_ids) - selected_ids)
        if unknown:
            raise WizardContractError(
                f"{package_id} references dependencies outside the resolved plan: "
                + ", ".join(unknown)
            )

    summary = _obj(document.get("summary"), "plan.summary")
    actual = (
        _int(summary.get("package_count"), "plan.summary.package_count"),
        _int(summary.get("payload_file_count"), "plan.summary.payload_file_count"),
        _int(summary.get("payload_size_bytes"), "plan.summary.payload_size_bytes"),
    )
    if actual != (len(packages), file_count, byte_count):
        raise WizardContractError("plan.summary does not match the resolved package payload.")
    if _bool(document.get("requires_elevation"), "plan.requires_elevation") != elevation:
        raise WizardContractError("plan.requires_elevation does not match package lifecycle data.")
    _strings(document.get("warnings"), "plan.warnings")
    return document


def _ack(ack_id: str, title: str, detail: str, severity: str) -> dict[str, object]:
    if not ACK_ID_RE.fullmatch(ack_id):
        raise WizardContractError(f"Invalid acknowledgement ID: {ack_id!r}.")
    return {
        "acknowledgement_id": ack_id,
        "title": title,
        "detail": detail,
        "severity": severity,
        "required": True,
    }


def _required_acknowledgements(plan: dict[str, object]) -> list[dict[str, object]]:
    rows = [
        _ack("review.exact-plan", "Review the exact plan",
             "The confirmation applies only to the displayed plan fingerprint.", "required"),
        _ack("review.licenses", "Review licences and notices",
             "The selected packages retain their declared licence and notice obligations.", "required"),
        _ack("review.external-workspaces", "Preserve external workspaces",
             "Installer lifecycle operations must preserve workspaces and user-authored content.", "required"),
    ]
    if _obj(plan["summary"], "plan.summary")["payload_file_count"] > 0:
        rows.append(_ack("review.payload", "Review planned files",
                         "The displayed payload paths, hashes, sizes, and redistribution states were reviewed.",
                         "required"))
    if _obj(plan["policies"], "plan.policies")["network_allowed"]:
        rows.append(_ack("review.network-policy", "Review network policy",
                         "The suite permits a later capability-gated acquisition phase to use the network.",
                         "warning"))
    if plan["requires_elevation"]:
        rows.append(_ack("review.elevation", "Review elevation requirement",
                         "At least one package declares a later per-machine or elevated lifecycle operation.",
                         "warning"))
    if _strings(plan["warnings"], "plan.warnings"):
        rows.append(_ack("review.warnings", "Review resolver warnings",
                         "All lifecycle and compatibility warnings displayed by the resolver were reviewed.",
                         "warning"))
    packages = [_obj(item, "plan.package") for item in _arr(plan["packages"], "plan.packages")]
    if any(_obj(item["lifecycle"], "package.lifecycle")["rollback_required"] for item in packages):
        rows.append(_ack("review.rollback", "Review rollback requirements",
                         "At least one selected package requires rollback preparation before execution.",
                         "required"))
    if any(item["kind"] == "runtime-adapter" for item in packages):
        rows.append(_ack("review.runtime-adapter", "Review runtime-adapter scope",
                         "A runtime adapter is selected, but this confirmation does not enable or execute it.",
                         "warning"))
    return sorted(rows, key=lambda item: item["acknowledgement_id"])


def build_view_model(plan: Mapping[str, object]) -> dict[str, object]:
    checked = validate_plan(plan)
    package_rows: list[dict[str, object]] = []
    payload_rows: list[dict[str, object]] = []
    for order, raw in enumerate(_arr(checked["packages"], "plan.packages")):
        row = _obj(raw, f"plan.packages[{order}]")
        package_id = row["package_id"]
        lifecycle = _obj(row["lifecycle"], f"{package_id}.lifecycle")
        legal = _obj(row["legal"], f"{package_id}.legal")
        payload = [_obj(item, f"{package_id}.payload") for item in _arr(row["payload"], f"{package_id}.payload")]
        package_rows.append({
            "order": order,
            "package_id": package_id,
            "display_name": row["display_name"],
            "version": row["version"],
            "kind": row["kind"],
            "status": row["status"],
            "selection_reasons": sorted(row["selection_reasons"]),
            "dependency_ids": sorted(row["dependency_ids"]),
            "capabilities": sorted(row["capabilities"]),
            "manifest_sha256": row["manifest_sha256"],
            "source": dict(sorted(_obj(row["source"], f"{package_id}.source").items())),
            "install_scope": lifecycle["install_scope"],
            "elevation_required": lifecycle["elevation_required"],
            "repair_supported": lifecycle["repair_supported"],
            "upgrade_supported": lifecycle["upgrade_supported"],
            "uninstall_supported": lifecycle["uninstall_supported"],
            "rollback_required": lifecycle["rollback_required"],
            "license_expression": legal["license_expression"],
            "redistribution_review": legal["redistribution_review"],
            "notice_files": sorted(legal["notice_files"]),
            "payload_file_count": len(payload),
            "payload_size_bytes": sum(item["size_bytes"] for item in payload),
        })
        for item in sorted(payload, key=lambda value: (
            str(value["destination"]).casefold(), str(value["destination"]), str(value["source"])
        )):
            payload_rows.append({
                "package_id": package_id,
                "source": item["source"],
                "destination": item["destination"],
                "sha256": item["sha256"],
                "size_bytes": item["size_bytes"],
                "redistribution": item["redistribution"],
            })

    base = {
        "schema_version": 1,
        "plan_sha256": checked["plan_sha256"],
        "suite": dict(sorted(_obj(checked["suite"], "plan.suite").items())),
        "context": dict(sorted(_obj(checked["context"], "plan.context").items())),
        "selection": {
            key: sorted(value)
            for key, value in sorted(_obj(checked["selection"], "plan.selection").items())
        },
        "policies": dict(sorted(_obj(checked["policies"], "plan.policies").items())),
        "review_state": "awaiting-acknowledgements",
        "requires_elevation": checked["requires_elevation"],
        "summary": dict(sorted(_obj(checked["summary"], "plan.summary").items())),
        "packages": package_rows,
        "payload": payload_rows,
        "warnings": sorted(checked["warnings"]),
        "required_acknowledgements": _required_acknowledgements(checked),
        "authority": _authority(),
    }
    return {**base, "view_model_sha256": sha256(base)}


def validate_view_model(
    plan: Mapping[str, object], view_model: Mapping[str, object]
) -> dict[str, object]:
    checked_plan = validate_plan(plan)
    document = dict(view_model)
    declared = _hash(document.get("view_model_sha256"), "view_model.view_model_sha256")
    unsigned = {key: value for key, value in document.items() if key != "view_model_sha256"}
    if sha256(unsigned) != declared:
        raise WizardContractError("view_model.view_model_sha256 does not match its content.")
    if document != build_view_model(checked_plan):
        raise WizardContractError("view-model is stale or was not canonically derived from the plan.")
    return document


def required_acknowledgement_ids(view_model: Mapping[str, object]) -> tuple[str, ...]:
    result = tuple(
        _text(_obj(row, f"view_model.required_acknowledgements[{index}]").get("acknowledgement_id"),
              f"view_model.required_acknowledgements[{index}].acknowledgement_id")
        for index, row in enumerate(_arr(
            view_model.get("required_acknowledgements"),
            "view_model.required_acknowledgements",
        ))
    )
    if tuple(sorted(result)) != result or len(set(result)) != len(result):
        raise WizardContractError(
            "view_model.required_acknowledgements must be uniquely sorted by ID."
        )
    return result


def evaluate_acknowledgements(
    view_model: Mapping[str, object], acknowledged_ids: Iterable[str]
) -> dict[str, object]:
    required = set(required_acknowledgement_ids(view_model))
    supplied = tuple(_text(value, "acknowledgement ID") for value in acknowledged_ids)
    if len(set(supplied)) != len(supplied):
        raise WizardContractError("Acknowledgement IDs must not contain duplicates.")
    acknowledged = set(supplied)
    unknown = sorted(acknowledged - required)
    if unknown:
        raise WizardContractError("Unknown acknowledgement IDs: " + ", ".join(unknown))
    missing = sorted(required - acknowledged)
    return {
        "ready": not missing,
        "acknowledged_ids": sorted(acknowledged),
        "missing_acknowledgement_ids": missing,
    }


def create_confirmation(
    plan: Mapping[str, object],
    view_model: Mapping[str, object],
    *,
    expected_plan_sha256: str,
    acknowledged_ids: Iterable[str],
    confirmed_by: str,
    confirmed_at_utc: str,
) -> dict[str, object]:
    checked_plan = validate_plan(plan)
    checked_view = validate_view_model(checked_plan, view_model)
    if _hash(expected_plan_sha256, "expected_plan_sha256") != checked_plan["plan_sha256"]:
        raise WizardContractError("Expected plan hash does not match the current resolver plan.")
    evaluation = evaluate_acknowledgements(checked_view, acknowledged_ids)
    if not evaluation["ready"]:
        raise WizardContractError(
            "Missing required acknowledgements: "
            + ", ".join(evaluation["missing_acknowledgement_ids"])
        )
    actor = _text(confirmed_by, "confirmed_by")
    if len(actor) > 160:
        raise WizardContractError("confirmed_by must not exceed 160 characters.")
    base = {
        "schema_version": 1,
        "confirmation_scope": "review-only",
        "plan_sha256": checked_plan["plan_sha256"],
        "view_model_sha256": checked_view["view_model_sha256"],
        "acknowledged_ids": evaluation["acknowledged_ids"],
        "confirmed_by": actor,
        "confirmed_at_utc": _utc(confirmed_at_utc, "confirmed_at_utc"),
        "statement": STATEMENT,
        "authority": _authority(),
    }
    return {**base, "confirmation_sha256": sha256(base)}


def verify_confirmation(
    plan: Mapping[str, object],
    view_model: Mapping[str, object],
    confirmation: Mapping[str, object],
) -> dict[str, object]:
    checked_plan = validate_plan(plan)
    checked_view = validate_view_model(checked_plan, view_model)
    document = dict(confirmation)
    if _int(document.get("schema_version"), "confirmation.schema_version") != 1:
        raise WizardContractError("confirmation.schema_version must be exactly 1.")
    if document.get("confirmation_scope") != "review-only":
        raise WizardContractError("confirmation scope must remain review-only.")
    if _hash(document.get("plan_sha256"), "confirmation.plan_sha256") != checked_plan["plan_sha256"]:
        raise WizardContractError("confirmation is stale for the current plan.")
    if _hash(document.get("view_model_sha256"), "confirmation.view_model_sha256") != checked_view["view_model_sha256"]:
        raise WizardContractError("confirmation is stale for the current view-model.")
    acknowledged = _strings(document.get("acknowledged_ids"), "confirmation.acknowledged_ids")
    if acknowledged != required_acknowledgement_ids(checked_view):
        raise WizardContractError(
            "confirmation acknowledgements do not exactly match the required acknowledgement set."
        )
    actor = _text(document.get("confirmed_by"), "confirmation.confirmed_by")
    if len(actor) > 160:
        raise WizardContractError("confirmation.confirmed_by must not exceed 160 characters.")
    _utc(document.get("confirmed_at_utc"), "confirmation.confirmed_at_utc")
    if document.get("statement") != STATEMENT:
        raise WizardContractError("confirmation statement was altered.")
    _validate_authority(document.get("authority"), "confirmation.authority")
    declared = _hash(document.get("confirmation_sha256"), "confirmation.confirmation_sha256")
    unsigned = {key: value for key, value in document.items() if key != "confirmation_sha256"}
    if sha256(unsigned) != declared:
        raise WizardContractError("confirmation fingerprint does not match its content.")
    return document


def confirmation_is_current(
    plan: Mapping[str, object],
    view_model: Mapping[str, object],
    confirmation: Mapping[str, object],
) -> bool:
    try:
        verify_confirmation(plan, view_model, confirmation)
    except WizardContractError:
        return False
    return True
