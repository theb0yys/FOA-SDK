#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Deterministic non-executing handoff from a verified receipt to a future package engine."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import sys
from pathlib import Path
from typing import Mapping, Sequence

INSTALLER_ROOT = Path(__file__).resolve().parents[3]
RECEIPT_SOURCE = INSTALLER_ROOT / "SuiteWizard" / "Receipt" / "Source"
VIEW_MODEL_SOURCE = INSTALLER_ROOT / "SuiteWizard" / "ViewModel" / "Source"
for source_root in (RECEIPT_SOURCE, VIEW_MODEL_SOURCE):
    if str(source_root) not in sys.path:
        sys.path.insert(0, str(source_root))

from confirmation_receipt import (  # noqa: E402
    ReceiptError,
    load_receipt,
    validate_receipt,
)
from wizard_view_model import (  # noqa: E402
    AUTHORITY_FIELDS,
    WizardContractError,
    canonical_json,
    sha256,
)

HANDOFF_SCOPE = "verified-receipt-to-package-engine"
OPERATIONS = ("install", "repair", "upgrade", "rollback", "uninstall")
LOGICAL_REFERENCE_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
HANDOFF_STATEMENT = (
    "This handoff records a requested FOA-SDK package lifecycle operation bound to "
    "an exact verified review receipt. It grants no package-engine capability and "
    "performs no acquisition, installation, repair, upgrade, rollback, uninstall, "
    "elevation, game launch, runtime execution, deployment, save mutation, signing, "
    "network publication, catalog mutation, or evidence promotion."
)


class ExecutionHandoffError(RuntimeError):
    """Raised when the receipt-to-package-engine handoff is invalid."""


def _object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ExecutionHandoffError(f"{label} must be an object.")
    return value


def _array(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise ExecutionHandoffError(f"{label} must be an array.")
    return value


def _text(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ExecutionHandoffError(f"{label} must be a non-empty string.")
    return value


def _integer(value: object, label: str) -> int:
    if type(value) is not int:
        raise ExecutionHandoffError(f"{label} must be an integer.")
    return value


def _hash(value: object, label: str) -> str:
    result = _text(value, label)
    if SHA256_RE.fullmatch(result) is None:
        raise ExecutionHandoffError(f"{label} must be a lowercase SHA-256 value.")
    return result


def _actor(value: object, label: str) -> str:
    result = _text(value, label)
    if result != result.strip() or len(result) > 160:
        raise ExecutionHandoffError(
            f"{label} must be trimmed and must not exceed 160 characters."
        )
    if any(ord(character) < 32 or ord(character) == 127 for character in result):
        raise ExecutionHandoffError(f"{label} must not contain control characters.")
    return result


def _utc(value: object, label: str) -> str:
    result = _text(value, label)
    if len(result) > 64:
        raise ExecutionHandoffError(f"{label} must not exceed 64 characters.")
    if not result.endswith("Z") or "T" not in result:
        raise ExecutionHandoffError(
            f"{label} must be an ISO-8601 UTC timestamp ending in Z."
        )
    try:
        parsed = dt.datetime.fromisoformat(result[:-1] + "+00:00")
    except ValueError as exc:
        raise ExecutionHandoffError(f"{label} is not a valid ISO-8601 timestamp.") from exc
    if parsed.utcoffset() != dt.timedelta(0):
        raise ExecutionHandoffError(f"{label} must use UTC.")
    return result


def _logical_reference(value: object, label: str) -> str:
    result = _text(value, label)
    if len(result) > 128 or LOGICAL_REFERENCE_RE.fullmatch(result) is None:
        raise ExecutionHandoffError(
            f"{label} must be a stable namespaced logical ID, not a filesystem path."
        )
    return result


def _operation(value: object) -> str:
    result = _text(value, "operation")
    if result not in OPERATIONS:
        raise ExecutionHandoffError(
            "operation must be one of: " + ", ".join(OPERATIONS) + "."
        )
    return result


def _authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}


def _validate_authority(value: object) -> None:
    authority = _object(value, "handoff.authority")
    if set(authority) != set(AUTHORITY_FIELDS) or any(
        authority.get(field) is not False for field in AUTHORITY_FIELDS
    ):
        raise ExecutionHandoffError(
            "handoff.authority must contain the exact all-false authority record."
        )


def _prior_reference(operation: str, value: object) -> str | None:
    if operation == "install":
        if value is not None:
            raise ExecutionHandoffError(
                "install handoffs must not declare prior_installation_reference."
            )
        return None
    if value is None:
        raise ExecutionHandoffError(
            f"{operation} handoffs require prior_installation_reference."
        )
    return _logical_reference(value, "prior_installation_reference")


def _required_capabilities(operation: str, requires_elevation: bool) -> list[str]:
    result = [f"package-engine.execute.{operation}"]
    if requires_elevation:
        result.append("package-engine.elevation")
    return sorted(result)


def _validate_operation_support(plan: Mapping[str, object], operation: str) -> None:
    if operation == "install":
        return
    support_field = {
        "repair": "repair_supported",
        "upgrade": "upgrade_supported",
        "uninstall": "uninstall_supported",
        "rollback": "rollback_required",
    }[operation]
    unsupported: list[str] = []
    for index, raw_package in enumerate(_array(plan.get("packages"), "receipt.plan.packages")):
        package = _object(raw_package, f"receipt.plan.packages[{index}]")
        package_id = _text(
            package.get("package_id"), f"receipt.plan.packages[{index}].package_id"
        )
        lifecycle = _object(package.get("lifecycle"), f"{package_id}.lifecycle")
        if lifecycle.get(support_field) is not True:
            unsupported.append(package_id)
    if unsupported:
        label = "rollback protection" if operation == "rollback" else f"{operation} support"
        raise ExecutionHandoffError(
            f"Selected packages do not declare {label}: " + ", ".join(unsupported)
        )


def build_handoff(
    receipt: Mapping[str, object],
    *,
    operation: str,
    target_reference: str,
    prior_installation_reference: str | None,
    requested_by: str,
    requested_at_utc: str,
) -> dict[str, object]:
    """Build a deterministic inert request after reverifying the complete receipt."""
    try:
        checked_receipt = validate_receipt(receipt)
    except (ReceiptError, WizardContractError) as exc:
        raise ExecutionHandoffError(f"Receipt verification failed: {exc}") from exc

    checked_operation = _operation(operation)
    checked_target = _logical_reference(target_reference, "target_reference")
    checked_prior = _prior_reference(checked_operation, prior_installation_reference)
    actor = _actor(requested_by, "requested_by")
    requested_at = _utc(requested_at_utc, "requested_at_utc")

    plan = _object(checked_receipt.get("plan"), "receipt.plan")
    _validate_operation_support(plan, checked_operation)
    requires_elevation = plan.get("requires_elevation")
    if type(requires_elevation) is not bool:
        raise ExecutionHandoffError("receipt.plan.requires_elevation must be a boolean.")
    package_order = [
        _logical_reference(value, f"receipt.plan.package_order[{index}]")
        for index, value in enumerate(
            _array(plan.get("package_order"), "receipt.plan.package_order")
        )
    ]
    summary = _object(plan.get("summary"), "receipt.plan.summary")
    summary_row = {
        "package_count": _integer(
            summary.get("package_count"), "receipt.plan.summary.package_count"
        ),
        "payload_file_count": _integer(
            summary.get("payload_file_count"), "receipt.plan.summary.payload_file_count"
        ),
        "payload_size_bytes": _integer(
            summary.get("payload_size_bytes"), "receipt.plan.summary.payload_size_bytes"
        ),
    }

    base = {
        "schema_version": 1,
        "handoff_scope": HANDOFF_SCOPE,
        "operation": checked_operation,
        "target_reference": checked_target,
        "prior_installation_reference": checked_prior,
        "receipt_sha256": checked_receipt["receipt_sha256"],
        "plan_sha256": checked_receipt["plan_sha256"],
        "view_model_sha256": checked_receipt["view_model_sha256"],
        "confirmation_sha256": checked_receipt["confirmation_sha256"],
        "package_order": package_order,
        "summary": summary_row,
        "required_capabilities": _required_capabilities(
            checked_operation, requires_elevation
        ),
        "granted_capabilities": [],
        "requested_by": actor,
        "requested_at_utc": requested_at,
        "receipt": checked_receipt,
        "statement": HANDOFF_STATEMENT,
        "authority": _authority(),
    }
    return {**base, "handoff_sha256": sha256(base)}


def validate_handoff(handoff: Mapping[str, object]) -> dict[str, object]:
    """Rebuild the exact handoff and reject drift, escalation, or extra fields."""
    document = dict(handoff)
    if _integer(document.get("schema_version"), "handoff.schema_version") != 1:
        raise ExecutionHandoffError("handoff.schema_version must be exactly 1.")
    if document.get("handoff_scope") != HANDOFF_SCOPE:
        raise ExecutionHandoffError("handoff scope is invalid.")
    if document.get("statement") != HANDOFF_STATEMENT:
        raise ExecutionHandoffError("handoff statement was altered.")
    _validate_authority(document.get("authority"))
    if _array(document.get("granted_capabilities"), "handoff.granted_capabilities"):
        raise ExecutionHandoffError("handoff.granted_capabilities must remain empty.")

    declared = _hash(document.get("handoff_sha256"), "handoff.handoff_sha256")
    unsigned = {key: value for key, value in document.items() if key != "handoff_sha256"}
    if sha256(unsigned) != declared:
        raise ExecutionHandoffError("handoff fingerprint does not match its content.")

    receipt = _object(document.get("receipt"), "handoff.receipt")
    expected = build_handoff(
        receipt,
        operation=_operation(document.get("operation")),
        target_reference=_logical_reference(
            document.get("target_reference"), "handoff.target_reference"
        ),
        prior_installation_reference=document.get("prior_installation_reference"),
        requested_by=_actor(document.get("requested_by"), "handoff.requested_by"),
        requested_at_utc=_utc(
            document.get("requested_at_utc"), "handoff.requested_at_utc"
        ),
    )
    if document != expected:
        raise ExecutionHandoffError(
            "handoff is stale, altered, escalated, or not canonically derived."
        )
    return document


def verify_handoff_against_receipt(
    handoff: Mapping[str, object],
    receipt: Mapping[str, object],
) -> dict[str, object]:
    """Require the handoff to contain the exact caller-supplied current receipt."""
    checked_handoff = validate_handoff(handoff)
    try:
        checked_receipt = validate_receipt(receipt)
    except (ReceiptError, WizardContractError) as exc:
        raise ExecutionHandoffError(f"Current receipt verification failed: {exc}") from exc
    if checked_handoff["receipt"] != checked_receipt:
        raise ExecutionHandoffError(
            "handoff does not match the exact current verified receipt."
        )
    return checked_handoff


def canonical_handoff_bytes(handoff: Mapping[str, object]) -> bytes:
    return canonical_json(validate_handoff(handoff))


def _reject_symlink_components(path: Path) -> None:
    current = path
    while True:
        if current.is_symlink():
            raise ExecutionHandoffError(f"Handoff path contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def _validated_destination(destination: Path) -> Path:
    target = Path(destination)
    if target.name in {"", ".", ".."} or not target.name.endswith(
        ".foa-execution-handoff.json"
    ):
        raise ExecutionHandoffError(
            "Handoff destination must end with '.foa-execution-handoff.json'."
        )
    if not target.parent.exists() or not target.parent.is_dir():
        raise ExecutionHandoffError(
            "Handoff destination parent must be an existing directory."
        )
    _reject_symlink_components(target.parent)
    if target.is_symlink():
        raise ExecutionHandoffError("Handoff destination must not be a symbolic link.")
    if target.exists() and not target.is_file():
        raise ExecutionHandoffError("Handoff destination must be a regular file path.")
    return target


def _read_exact_file(path: Path) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise ExecutionHandoffError(
            f"Handoff path is not a regular non-symlink file: {path}"
        )
    return path.read_bytes()


def load_handoff(path: Path) -> dict[str, object]:
    target = Path(path)
    _reject_symlink_components(target.parent)
    payload = _read_exact_file(target)
    try:
        value = json.loads(payload.decode("utf-8", errors="strict"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ExecutionHandoffError(f"Handoff is not strict UTF-8 JSON: {target}") from exc
    document = validate_handoff(_object(value, "handoff"))
    if canonical_json(document) != payload:
        raise ExecutionHandoffError("Handoff file bytes are not canonical JSON.")
    return document


def publish_handoff(
    destination: Path,
    receipt: Mapping[str, object],
    *,
    operation: str,
    target_reference: str,
    prior_installation_reference: str | None,
    requested_by: str,
    requested_at_utc: str,
) -> dict[str, object]:
    """Create canonical handoff bytes atomically without granting capability."""
    target = _validated_destination(destination)
    handoff = build_handoff(
        receipt,
        operation=operation,
        target_reference=target_reference,
        prior_installation_reference=prior_installation_reference,
        requested_by=requested_by,
        requested_at_utc=requested_at_utc,
    )
    payload = canonical_handoff_bytes(handoff)

    if target.exists():
        existing = _read_exact_file(target)
        if existing != payload:
            raise ExecutionHandoffError(
                "Handoff destination already exists with different bytes."
            )
        load_handoff(target)
        return _publication_result("already-current", target, handoff, len(payload))

    temporary = target.parent / f".{target.name}.{handoff['handoff_sha256']}.tmp"
    if temporary.exists() or temporary.is_symlink():
        raise ExecutionHandoffError(
            f"Deterministic temporary handoff path already exists: {temporary}"
        )

    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor: int | None = None
    try:
        descriptor = os.open(temporary, flags, 0o600)
        view = memoryview(payload)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise ExecutionHandoffError("Handoff write made no forward progress.")
            view = view[written:]
        os.fsync(descriptor)
        os.close(descriptor)
        descriptor = None

        if _read_exact_file(temporary) != payload:
            raise ExecutionHandoffError(
                "Temporary handoff bytes changed before publication."
            )
        validate_handoff(_object(json.loads(payload.decode("utf-8")), "handoff"))
        try:
            os.link(temporary, target)
        except FileExistsError as exc:
            raise ExecutionHandoffError(
                "Handoff destination appeared during publication."
            ) from exc
        os.unlink(temporary)

        if _read_exact_file(target) != payload:
            raise ExecutionHandoffError(
                "Published handoff bytes do not match canonical bytes."
            )
        load_handoff(target)
        return _publication_result("published", target, handoff, len(payload))
    except (OSError, ExecutionHandoffError, json.JSONDecodeError) as exc:
        if descriptor is not None:
            os.close(descriptor)
        if temporary.exists() and not temporary.is_symlink():
            try:
                temporary.unlink()
            except OSError:
                pass
        if isinstance(exc, ExecutionHandoffError):
            raise
        raise ExecutionHandoffError(f"Handoff publication failed: {exc}") from exc


def _publication_result(
    status: str,
    target: Path,
    handoff: Mapping[str, object],
    size_bytes: int,
) -> dict[str, object]:
    return {
        "status": status,
        "path": str(target),
        "handoff_sha256": handoff["handoff_sha256"],
        "receipt_sha256": handoff["receipt_sha256"],
        "required_capabilities": list(handoff["required_capabilities"]),
        "granted_capabilities": [],
        "size_bytes": size_bytes,
        "authority": _authority(),
    }


def verify_published_handoff(
    path: Path,
    receipt: Mapping[str, object],
) -> dict[str, object]:
    return verify_handoff_against_receipt(load_handoff(path), receipt)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser(
        "create",
        help="Reverify a receipt and create an inert package-engine handoff.",
    )
    create.add_argument("--receipt", type=Path, required=True)
    create.add_argument("--operation", choices=OPERATIONS, required=True)
    create.add_argument("--target-reference", required=True)
    create.add_argument("--prior-installation-reference")
    create.add_argument("--requested-by", required=True)
    create.add_argument("--requested-at-utc", required=True)
    create.add_argument("--output", type=Path, required=True)

    verify = subparsers.add_parser(
        "verify",
        help="Verify a handoff and optionally bind it to a current receipt.",
    )
    verify.add_argument("--handoff", type=Path, required=True)
    verify.add_argument("--receipt", type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        if arguments.command == "create":
            receipt = load_receipt(arguments.receipt)
            result = publish_handoff(
                arguments.output,
                receipt,
                operation=arguments.operation,
                target_reference=arguments.target_reference,
                prior_installation_reference=arguments.prior_installation_reference,
                requested_by=arguments.requested_by,
                requested_at_utc=arguments.requested_at_utc,
            )
        else:
            handoff = load_handoff(arguments.handoff)
            if arguments.receipt is not None:
                handoff = verify_handoff_against_receipt(
                    handoff, load_receipt(arguments.receipt)
                )
            result = {
                "status": "verified",
                "path": str(arguments.handoff),
                "handoff_sha256": handoff["handoff_sha256"],
                "receipt_sha256": handoff["receipt_sha256"],
                "required_capabilities": list(handoff["required_capabilities"]),
                "granted_capabilities": [],
                "size_bytes": len(canonical_json(handoff)),
                "authority": _authority(),
            }
        sys.stdout.buffer.write(canonical_json(result))
        return 0
    except (OSError, ReceiptError, WizardContractError, ExecutionHandoffError) as exc:
        print(f"Installer execution handoff failed: {exc}", file=sys.stderr)
        return 1


__all__ = [
    "ExecutionHandoffError",
    "HANDOFF_SCOPE",
    "HANDOFF_STATEMENT",
    "OPERATIONS",
    "build_handoff",
    "canonical_handoff_bytes",
    "load_handoff",
    "publish_handoff",
    "validate_handoff",
    "verify_handoff_against_receipt",
    "verify_published_handoff",
]


if __name__ == "__main__":
    raise SystemExit(main())
