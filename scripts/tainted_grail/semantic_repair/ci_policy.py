"""Negative policy checks and reproducible receipts for the offline workflow."""
from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .errors import WorkflowPolicyError

_POLICY_VERSION = 1
_FORBIDDEN_TOKENS = (
    "actions/upload-artifact",
    "actions/upload-pages-artifact",
    "actions/attest",
    "softprops/action-gh-release",
    "docker/build-push-action",
    "gh release",
    "packages: write",
    "id-token: write",
    "actions: write",
    "contents: write",
    "pull-requests: write",
    "continue-on-error: true",
)


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


def _top_level_permissions(text: str) -> dict[str, str]:
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if line == "permissions:":
            start = index + 1
            break
    if start is None:
        raise WorkflowPolicyError("workflow must declare top-level permissions")
    permissions: dict[str, str] = {}
    for line in lines[start:]:
        if line and not line.startswith(" "):
            break
        match = re.fullmatch(r"  ([A-Za-z0-9_-]+):\s*(\S+)\s*", line)
        if match:
            permissions[match.group(1)] = match.group(2)
    return permissions


def evaluate_offline_workflow(text: str) -> tuple[str, ...]:
    lowered = text.lower()
    violations: list[str] = []
    try:
        permissions = _top_level_permissions(text)
    except WorkflowPolicyError as exc:
        violations.append(str(exc))
    else:
        if permissions != {"contents": "read"}:
            violations.append("workflow permissions must be exactly contents: read")
    if "persist-credentials: false" not in lowered:
        violations.append("checkout credentials must not persist")
    if "${{ secrets." in lowered:
        violations.append("offline workflow must not reference secrets")
    for token in _FORBIDDEN_TOKENS:
        if token in lowered:
            violations.append(
                f"offline workflow contains forbidden publication token: {token}"
            )
    if re.search(
        r"\b(curl|wget|pip\s+install|apt-get|npm\s+publish|twine\s+upload)\b",
        lowered,
    ):
        violations.append("offline workflow contains network or publication command")
    return tuple(dict.fromkeys(violations))


@dataclass(frozen=True)
class WorkflowPolicyReceipt:
    workflow_sha256: str
    status: str
    violations: tuple[str, ...]

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "policy_version": _POLICY_VERSION,
            "authority": "offline-workflow-policy-only",
            "runtime_authority": "none",
            "promotion": "none",
            "workflow_sha256": self.workflow_sha256,
            "status": self.status,
            "violations": list(self.violations),
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.to_bytes()).hexdigest()

    @classmethod
    def from_bytes(cls, data: bytes) -> "WorkflowPolicyReceipt":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise WorkflowPolicyError("invalid workflow policy receipt") from exc
        expected = {
            "schema_version",
            "policy_version",
            "authority",
            "runtime_authority",
            "promotion",
            "workflow_sha256",
            "status",
            "violations",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise WorkflowPolicyError("workflow policy receipt has unexpected fields")
        if (
            doc["schema_version"] != 1
            or doc["policy_version"] != _POLICY_VERSION
            or doc["authority"] != "offline-workflow-policy-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or doc["status"] not in {"valid", "invalid"}
            or not isinstance(doc["violations"], list)
        ):
            raise WorkflowPolicyError("workflow policy receipt boundary is invalid")
        receipt = cls(
            doc["workflow_sha256"],
            doc["status"],
            tuple(doc["violations"]),
        )
        if len(receipt.workflow_sha256) != 64:
            raise WorkflowPolicyError("workflow policy receipt hash is invalid")
        if (receipt.status == "valid") != (not receipt.violations):
            raise WorkflowPolicyError("workflow policy receipt status is inconsistent")
        return receipt


def build_offline_workflow_receipt(text: str) -> WorkflowPolicyReceipt:
    violations = evaluate_offline_workflow(text)
    return WorkflowPolicyReceipt(
        hashlib.sha256(text.encode("utf-8")).hexdigest(),
        "valid" if not violations else "invalid",
        violations,
    )


def validate_offline_workflow(text: str) -> None:
    receipt = build_offline_workflow_receipt(text)
    if receipt.violations:
        raise WorkflowPolicyError("; ".join(receipt.violations))


def validate_offline_workflow_file(path: Path) -> None:
    validate_offline_workflow(Path(path).read_text(encoding="utf-8"))
