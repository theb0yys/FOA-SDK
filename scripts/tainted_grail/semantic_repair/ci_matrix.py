"""Cross-version fixtures for byte-identical offline workflow policy receipts."""
from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from typing import Any, Iterable

from .ci_policy import WorkflowPolicyReceipt, build_offline_workflow_receipt
from .errors import WorkflowPolicyError

_EXPECTED_VERSIONS = ("3.11", "3.12", "3.13")


def _canonical(doc: dict[str, Any]) -> bytes:
    return (json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


@dataclass(frozen=True)
class CrossVersionPolicyFixture:
    python_version: str
    workflow_sha256: str
    policy_receipt_sha256: str
    status: str

    def __post_init__(self) -> None:
        if not re.fullmatch(r"3\.\d+", self.python_version):
            raise WorkflowPolicyError("cross-version fixture Python version is invalid")
        if any(len(value) != 64 for value in (self.workflow_sha256, self.policy_receipt_sha256)):
            raise WorkflowPolicyError("cross-version fixture hash is invalid")
        if self.status not in {"valid", "invalid"}:
            raise WorkflowPolicyError("cross-version fixture status is invalid")

    def to_bytes(self) -> bytes:
        return _canonical(
            {
                "schema_version": 1,
                "authority": "offline-workflow-cross-version-fixture-only",
                "runtime_authority": "none",
                "promotion": "none",
                "python_version": self.python_version,
                "workflow_sha256": self.workflow_sha256,
                "policy_receipt_sha256": self.policy_receipt_sha256,
                "status": self.status,
            }
        )

    @classmethod
    def from_bytes(cls, data: bytes) -> "CrossVersionPolicyFixture":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise WorkflowPolicyError("invalid cross-version workflow fixture") from exc
        expected = {
            "schema_version",
            "authority",
            "runtime_authority",
            "promotion",
            "python_version",
            "workflow_sha256",
            "policy_receipt_sha256",
            "status",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise WorkflowPolicyError("cross-version fixture has unexpected fields")
        if (
            doc["schema_version"] != 1
            or doc["authority"] != "offline-workflow-cross-version-fixture-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
        ):
            raise WorkflowPolicyError("cross-version fixture boundary is invalid")
        return cls(
            doc["python_version"],
            doc["workflow_sha256"],
            doc["policy_receipt_sha256"],
            doc["status"],
        )


def build_cross_version_policy_fixture(
    workflow_text: str,
    python_version: str,
) -> CrossVersionPolicyFixture:
    receipt = build_offline_workflow_receipt(workflow_text)
    return CrossVersionPolicyFixture(
        python_version,
        receipt.workflow_sha256,
        receipt.sha256,
        receipt.status,
    )


def verify_cross_version_policy_fixtures(
    fixtures: Iterable[CrossVersionPolicyFixture],
    receipt: WorkflowPolicyReceipt,
    *,
    expected_versions: tuple[str, ...] = _EXPECTED_VERSIONS,
) -> bool:
    supplied = tuple(sorted(fixtures, key=lambda item: item.python_version))
    if tuple(item.python_version for item in supplied) != tuple(sorted(expected_versions)):
        raise WorkflowPolicyError("cross-version fixtures do not cover the expected matrix")
    for item in supplied:
        if (
            item.workflow_sha256 != receipt.workflow_sha256
            or item.policy_receipt_sha256 != receipt.sha256
            or item.status != receipt.status
        ):
            raise WorkflowPolicyError("cross-version workflow receipt fixture mismatch")
    return True
