#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Capability-gated handoff from reviewed installer plans to approved acquisition."""

from __future__ import annotations

import datetime as dt
import hashlib
import importlib.util
import json
import re
import sys
from pathlib import Path
from types import ModuleType
from typing import Mapping, Sequence

REPO_ROOT = Path(__file__).resolve().parents[4]
WIZARD_MODULE = REPO_ROOT / "Installer" / "SuiteWizard" / "ViewModel" / "Source" / "wizard_view_model.py"
PROVIDER_MODULE = REPO_ROOT / "Plugins" / "Integrations" / "ApprovedAcquisition" / "Tools" / "approved_acquisition.py"
CAPABILITY_PREFIX = "acquisition.approved."
REQUEST_STATEMENT = (
    "I explicitly authorize only the exact approved acquisition request bound to "
    "this reviewed installer plan and confirmation. This authorization grants no "
    "installation, elevation, launch, runtime execution, deployment, save mutation, "
    "signing, publication, catalog mutation, or evidence promotion authority."
)
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TYPED_SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
ID_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
AUTHORITY_FIELDS = (
    "acquisition", "network", "filesystem_write", "installation", "elevation",
    "game_launch", "runtime_execution", "deployment", "save_mutation", "signing",
    "publication", "catalog_mutation", "evidence_promotion",
)
EFFECT_FIELDS = (
    "acquisition_performed", "installation_performed", "elevation_performed",
    "game_launched", "runtime_executed", "deployment_performed", "save_mutated",
    "signing_performed", "publication_performed", "catalog_mutated", "evidence_promoted",
)


class BootstrapperAcquisitionError(RuntimeError):
    pass


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def sha256(value: object) -> str:
    return hashlib.sha256(canonical_json(value)).hexdigest()


def _load_module(name: str, path: Path) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise BootstrapperAcquisitionError(f"Unable to load required module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def load_wizard_api() -> ModuleType:
    return _load_module("foa_installer_wizard_view_model", WIZARD_MODULE)


def load_provider_api() -> ModuleType:
    return _load_module("foa_approved_acquisition_provider", PROVIDER_MODULE)


def _obj(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise BootstrapperAcquisitionError(f"{label} must be an object.")
    return value


def _arr(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise BootstrapperAcquisitionError(f"{label} must be an array.")
    return value


def _text(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise BootstrapperAcquisitionError(f"{label} must be a non-empty string.")
    return value


def _bool(value: object, label: str) -> bool:
    if type(value) is not bool:
        raise BootstrapperAcquisitionError(f"{label} must be a boolean.")
    return value


def _integer(value: object, label: str) -> int:
    if type(value) is not int:
        raise BootstrapperAcquisitionError(f"{label} must be an integer.")
    return value


def _hash(value: object, label: str) -> str:
    result = _text(value, label)
    if SHA256_RE.fullmatch(result) is None:
        raise BootstrapperAcquisitionError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _typed_hash(value: object, label: str) -> str:
    result = _text(value, label)
    if TYPED_SHA256_RE.fullmatch(result) is None:
        raise BootstrapperAcquisitionError(f"{label} must use sha256:<lowercase digest>.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label)
    if not result.endswith("Z") or "T" not in result:
        raise BootstrapperAcquisitionError(f"{label} must be an ISO-8601 UTC timestamp ending in Z.")
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise BootstrapperAcquisitionError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise BootstrapperAcquisitionError(f"{label} must use UTC.")
    return result


def _utc_datetime(value: object, label: str) -> dt.datetime:
    return dt.datetime.fromisoformat(_utc(value, label)[:-1] + "+00:00")


def _authority(route: str) -> dict[str, bool]:
    result = {field: False for field in AUTHORITY_FIELDS}
    result["acquisition"] = True
    result["filesystem_write"] = True
    result["network"] = route == "pinned-github"
    return result


def _validate_authority(value: object, route: str) -> None:
    authority = _obj(value, "request.authority")
    if authority != _authority(route):
        raise BootstrapperAcquisitionError("request authority differs from the exact acquisition-only boundary.")


def _effects() -> dict[str, bool]:
    result = {field: False for field in EFFECT_FIELDS}
    result["acquisition_performed"] = True
    return result


def _load_apis(wizard_api: ModuleType | None, provider_api: ModuleType | None) -> tuple[ModuleType, ModuleType]:
    return wizard_api or load_wizard_api(), provider_api or load_provider_api()


def _provider_bindings(plan: Mapping[str, object], provider_packages: Sequence[str]) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    seen: set[str] = set()
    for raw_package in _arr(plan.get("packages"), "plan.packages"):
        package = _obj(raw_package, "plan.package")
        installer_id = _text(package.get("package_id"), "plan.package.package_id")
        raw_capabilities = _arr(package.get("capabilities"), f"{installer_id}.capabilities")
        capabilities = sorted(_text(value, f"{installer_id}.capability") for value in raw_capabilities)
        for capability in capabilities:
            if not capability.startswith(CAPABILITY_PREFIX):
                continue
            provider_id = capability[len(CAPABILITY_PREFIX):]
            if ID_RE.fullmatch(provider_id) is None:
                raise BootstrapperAcquisitionError(
                    f"{installer_id} declares an invalid approved-acquisition package ID."
                )
            if provider_id in seen:
                raise BootstrapperAcquisitionError(
                    f"Approved-acquisition package {provider_id} is bound more than once."
                )
            seen.add(provider_id)
            rows.append({"installer_package_id": installer_id, "provider_package_id": provider_id})
    expected = sorted(provider_packages)
    actual = sorted(seen)
    if actual != expected:
        missing = sorted(set(expected) - seen)
        extra = sorted(seen - set(expected))
        details = []
        if missing:
            details.append("missing installer bindings: " + ", ".join(missing))
        if extra:
            details.append("undeclared provider packages: " + ", ".join(extra))
        raise BootstrapperAcquisitionError("Provider package binding mismatch (" + "; ".join(details) + ").")
    return rows


def build_request(
    plan: Mapping[str, object],
    view_model: Mapping[str, object],
    confirmation: Mapping[str, object],
    provider_plan: Mapping[str, object],
    *, expected_confirmation_sha256: str, output_reference: str,
    authorized_by: str, authorized_at_utc: str,
    wizard_api: ModuleType | None = None, provider_api: ModuleType | None = None,
) -> dict[str, object]:
    wizard, provider = _load_apis(wizard_api, provider_api)
    try:
        checked_confirmation = wizard.verify_confirmation(plan, view_model, confirmation)
        manifest = provider.load_manifest()
        provider.validate_plan(provider_plan, manifest)
    except Exception as exc:
        raise BootstrapperAcquisitionError(f"Upstream plan, confirmation, or provider plan is invalid: {exc}") from exc
    confirmation_hash = _hash(expected_confirmation_sha256, "expected_confirmation_sha256")
    if confirmation_hash != checked_confirmation["confirmation_sha256"]:
        raise BootstrapperAcquisitionError("Expected confirmation hash does not match the current confirmation.")
    route = _text(provider_plan.get("route"), "provider_plan.route")
    if route not in {"local", "pinned-github"}:
        raise BootstrapperAcquisitionError("Provider route must be local or pinned-github.")
    policies = _obj(plan.get("policies"), "plan.policies")
    if route == "pinned-github" and not _bool(policies.get("network_allowed"), "plan.policies.network_allowed"):
        raise BootstrapperAcquisitionError("Pinned-GitHub acquisition requires the reviewed suite network policy.")
    provider_packages = tuple(
        _text(value, "provider_plan.packages[]")
        for value in _arr(provider_plan.get("packages"), "provider_plan.packages")
    )
    if tuple(sorted(set(provider_packages))) != provider_packages:
        raise BootstrapperAcquisitionError("provider_plan.packages must be uniquely sorted.")
    logical_output = _text(output_reference, "output_reference")
    if ID_RE.fullmatch(logical_output) is None:
        raise BootstrapperAcquisitionError("output_reference must be a stable namespaced ID, not a path.")
    actor = _text(authorized_by, "authorized_by")
    if len(actor) > 160:
        raise BootstrapperAcquisitionError("authorized_by must not exceed 160 characters.")
    bindings = _provider_bindings(plan, provider_packages)
    base = {
        "schema_version": 1, "request_scope": "approved-acquisition-only",
        "plan_sha256": plan["plan_sha256"], "view_model_sha256": view_model["view_model_sha256"],
        "confirmation_sha256": checked_confirmation["confirmation_sha256"],
        "provider_id": provider_plan["provider_id"], "provider_plan_id": provider_plan["plan_id"],
        "route": route, "bindings": bindings, "output_reference": logical_output,
        "authorized_by": actor, "authorized_at_utc": _utc(authorized_at_utc, "authorized_at_utc"),
        "statement": REQUEST_STATEMENT, "authority": _authority(route),
    }
    return {**base, "request_sha256": sha256(base)}


def verify_request(
    plan: Mapping[str, object], view_model: Mapping[str, object], confirmation: Mapping[str, object],
    provider_plan: Mapping[str, object], request: Mapping[str, object], *,
    wizard_api: ModuleType | None = None, provider_api: ModuleType | None = None,
) -> dict[str, object]:
    document = dict(request)
    if _integer(document.get("schema_version"), "request.schema_version") != 1:
        raise BootstrapperAcquisitionError("request.schema_version must be exactly 1.")
    if document.get("request_scope") != "approved-acquisition-only":
        raise BootstrapperAcquisitionError("request scope must remain approved-acquisition-only.")
    declared = _hash(document.get("request_sha256"), "request.request_sha256")
    unsigned = {key: value for key, value in document.items() if key != "request_sha256"}
    if sha256(unsigned) != declared:
        raise BootstrapperAcquisitionError("request fingerprint does not match its content.")
    _typed_hash(document.get("provider_plan_id"), "request.provider_plan_id")
    route = _text(document.get("route"), "request.route")
    _validate_authority(document.get("authority"), route)
    if document.get("statement") != REQUEST_STATEMENT:
        raise BootstrapperAcquisitionError("request statement was altered.")
    expected = build_request(
        plan, view_model, confirmation, provider_plan,
        expected_confirmation_sha256=_hash(document.get("confirmation_sha256"), "request.confirmation_sha256"),
        output_reference=_text(document.get("output_reference"), "request.output_reference"),
        authorized_by=_text(document.get("authorized_by"), "request.authorized_by"),
        authorized_at_utc=_utc(document.get("authorized_at_utc"), "request.authorized_at_utc"),
        wizard_api=wizard_api, provider_api=provider_api,
    )
    if document != expected:
        raise BootstrapperAcquisitionError("request is stale or was not canonically derived.")
    return document


def execute_request(
    plan: Mapping[str, object], view_model: Mapping[str, object], confirmation: Mapping[str, object],
    provider_plan: Mapping[str, object], request: Mapping[str, object], *, output: Path,
    captured_at_utc: str, completed_at_utc: str, source_root: Path | None = None,
    wizard_api: ModuleType | None = None, provider_api: ModuleType | None = None, github_reader=None,
) -> dict[str, object]:
    wizard, provider = _load_apis(wizard_api, provider_api)
    checked = verify_request(
        plan, view_model, confirmation, provider_plan, request,
        wizard_api=wizard, provider_api=provider,
    )
    authorized_time = _utc_datetime(checked["authorized_at_utc"], "request.authorized_at_utc")
    captured_text = _utc(captured_at_utc, "captured_at_utc")
    completed_text = _utc(completed_at_utc, "completed_at_utc")
    captured_time = _utc_datetime(captured_text, "captured_at_utc")
    completed_time = _utc_datetime(completed_text, "completed_at_utc")
    if captured_time < authorized_time or completed_time < captured_time:
        raise BootstrapperAcquisitionError("Acquisition chronology must be authorization <= capture <= completion.")
    kwargs = {
        "provider": checked["route"], "output": output,
        "package_ids": tuple(provider_plan["packages"]), "source_root": source_root,
        "captured_at_utc": captured_text,
    }
    if github_reader is not None:
        kwargs["github_reader"] = github_reader
    try:
        receipt = provider.acquire(provider.load_manifest(), **kwargs)
        verified = provider.verify_bundle(output, provider.load_manifest())
    except Exception as exc:
        raise BootstrapperAcquisitionError(f"Approved acquisition execution failed: {exc}") from exc
    if receipt != verified:
        raise BootstrapperAcquisitionError("Provider verification did not reproduce the acquisition receipt.")
    if receipt.get("plan_id") != provider_plan.get("plan_id"):
        raise BootstrapperAcquisitionError("Provider receipt is not bound to the requested provider plan.")
    verification = _obj(receipt.get("verification"), "provider receipt verification")
    if receipt.get("captured_at_utc") != captured_text:
        raise BootstrapperAcquisitionError("Provider receipt capture time differs from the request.")
    base = {
        "schema_version": 1, "request_sha256": checked["request_sha256"],
        "plan_sha256": checked["plan_sha256"], "confirmation_sha256": checked["confirmation_sha256"],
        "provider_id": checked["provider_id"], "provider_plan_id": checked["provider_plan_id"],
        "provider_receipt_id": receipt["receipt_id"], "route": checked["route"],
        "output_reference": checked["output_reference"], "captured_at_utc": captured_text,
        "completed_at_utc": completed_text, "packages": list(provider_plan["packages"]),
        "verification": {
            "bundle_verified": True,
            "files": _integer(verification.get("files"), "provider verification files"),
            "bytes": _integer(verification.get("bytes"), "provider verification bytes"),
        },
        "candidate_evidence_created": False, "effects": _effects(),
        "remaining_authority": {field: False for field in AUTHORITY_FIELDS},
    }
    return {**base, "result_sha256": sha256(base)}


def verify_result(
    plan: Mapping[str, object], view_model: Mapping[str, object], confirmation: Mapping[str, object],
    provider_plan: Mapping[str, object], request: Mapping[str, object], result: Mapping[str, object], *,
    bundle: Path, wizard_api: ModuleType | None = None, provider_api: ModuleType | None = None,
) -> dict[str, object]:
    wizard, provider = _load_apis(wizard_api, provider_api)
    checked_request = verify_request(
        plan, view_model, confirmation, provider_plan, request,
        wizard_api=wizard, provider_api=provider,
    )
    try:
        receipt = provider.verify_bundle(bundle, provider.load_manifest())
    except Exception as exc:
        raise BootstrapperAcquisitionError(f"Approved acquisition bundle verification failed: {exc}") from exc
    document = dict(result)
    if _integer(document.get("schema_version"), "result.schema_version") != 1:
        raise BootstrapperAcquisitionError("result.schema_version must be exactly 1.")
    declared = _hash(document.get("result_sha256"), "result.result_sha256")
    unsigned = {key: value for key, value in document.items() if key != "result_sha256"}
    if sha256(unsigned) != declared:
        raise BootstrapperAcquisitionError("result fingerprint does not match its content.")
    if document.get("request_sha256") != checked_request["request_sha256"]:
        raise BootstrapperAcquisitionError("result is stale for the current acquisition request.")
    if document.get("provider_receipt_id") != receipt.get("receipt_id"):
        raise BootstrapperAcquisitionError("result is not bound to the verified provider receipt.")
    authorized_time = _utc_datetime(checked_request["authorized_at_utc"], "request.authorized_at_utc")
    captured_time = _utc_datetime(receipt.get("captured_at_utc"), "provider receipt captured_at_utc")
    completed_time = _utc_datetime(document.get("completed_at_utc"), "result.completed_at_utc")
    if captured_time < authorized_time or completed_time < captured_time:
        raise BootstrapperAcquisitionError("Acquisition chronology must be authorization <= capture <= completion.")
    if document.get("captured_at_utc") != receipt.get("captured_at_utc"):
        raise BootstrapperAcquisitionError("result capture time differs from the provider receipt.")
    if document.get("effects") != _effects():
        raise BootstrapperAcquisitionError("result effects exceed approved acquisition.")
    remaining = _obj(document.get("remaining_authority"), "result.remaining_authority")
    if set(remaining) != set(AUTHORITY_FIELDS) or any(
        _bool(value, "remaining authority") for value in remaining.values()
    ):
        raise BootstrapperAcquisitionError("result may not retain operational authority.")
    if document.get("candidate_evidence_created") is not False:
        raise BootstrapperAcquisitionError("result may not claim candidate-evidence creation.")
    verification = _obj(document.get("verification"), "result.verification")
    provider_verification = _obj(receipt.get("verification"), "provider receipt verification")
    if verification != {
        "bundle_verified": True,
        "files": provider_verification["files"],
        "bytes": provider_verification["bytes"],
    }:
        raise BootstrapperAcquisitionError("result verification differs from the provider receipt.")
    for key in (
        "plan_sha256", "confirmation_sha256", "provider_id", "provider_plan_id", "route", "output_reference",
    ):
        if document.get(key) != checked_request.get(key):
            raise BootstrapperAcquisitionError(f"result {key} differs from the acquisition request.")
    if document.get("packages") != list(provider_plan["packages"]):
        raise BootstrapperAcquisitionError("result packages differ from the provider plan.")
    return document
