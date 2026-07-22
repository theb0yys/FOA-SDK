"""Compatibility matrices for offline workflow receipt migration versions."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any

from .ci_migration import WorkflowPolicyReceiptV2
from .ci_migration_history import (
    WorkflowPolicyReceiptV3,
    WorkflowReceiptMigrationHistory,
    verify_workflow_receipt_migration_history,
)
from .ci_policy import WorkflowPolicyReceipt
from .errors import WorkflowPolicyError


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


def _violations_sha(violations: tuple[str, ...]) -> str:
    return hashlib.sha256(
        _canonical({"violations": list(violations)})
    ).hexdigest()


@dataclass(frozen=True)
class WorkflowMigrationCompatibilityCell:
    source_policy_version: int
    target_policy_version: int
    compatible: bool
    migration_path: tuple[int, ...]
    disposition: str

    def __post_init__(self) -> None:
        for value in (
            self.source_policy_version,
            self.target_policy_version,
        ):
            if type(value) is not int or value <= 0:
                raise WorkflowPolicyError(
                    "workflow migration compatibility version is invalid"
                )
        if type(self.compatible) is not bool:
            raise WorkflowPolicyError(
                "workflow migration compatibility flag is invalid"
            )
        if any(type(value) is not int or value <= 0 for value in self.migration_path):
            raise WorkflowPolicyError(
                "workflow migration compatibility path is invalid"
            )
        allowed = {
            "identity-compatible",
            "forward-compatible",
            "downgrade-unsupported",
        }
        if self.disposition not in allowed:
            raise WorkflowPolicyError(
                "workflow migration compatibility disposition is unsupported"
            )
        if self.compatible:
            expected = tuple(
                range(
                    self.source_policy_version,
                    self.target_policy_version + 1,
                )
            )
            if self.migration_path != expected:
                raise WorkflowPolicyError(
                    "workflow migration compatibility path is not contiguous"
                )
            if (
                self.source_policy_version == self.target_policy_version
                and self.disposition != "identity-compatible"
            ):
                raise WorkflowPolicyError(
                    "workflow migration identity disposition mismatch"
                )
            if (
                self.source_policy_version < self.target_policy_version
                and self.disposition != "forward-compatible"
            ):
                raise WorkflowPolicyError(
                    "workflow migration forward disposition mismatch"
                )
        elif (
            self.source_policy_version <= self.target_policy_version
            or self.migration_path
            or self.disposition != "downgrade-unsupported"
        ):
            raise WorkflowPolicyError(
                "workflow migration unsupported disposition mismatch"
            )

    def to_dict(self) -> dict[str, Any]:
        return {
            "source_policy_version": self.source_policy_version,
            "target_policy_version": self.target_policy_version,
            "compatible": self.compatible,
            "migration_path": list(self.migration_path),
            "disposition": self.disposition,
        }

    @classmethod
    def from_dict(
        cls,
        doc: Any,
    ) -> "WorkflowMigrationCompatibilityCell":
        expected = {
            "source_policy_version",
            "target_policy_version",
            "compatible",
            "migration_path",
            "disposition",
        }
        if (
            not isinstance(doc, dict)
            or set(doc) != expected
            or not isinstance(doc["migration_path"], list)
        ):
            raise WorkflowPolicyError(
                "workflow migration compatibility cell is invalid"
            )
        return cls(
            doc["source_policy_version"],
            doc["target_policy_version"],
            doc["compatible"],
            tuple(doc["migration_path"]),
            doc["disposition"],
        )


@dataclass(frozen=True)
class WorkflowMigrationCompatibilityMatrix:
    workflow_sha256: str
    status: str
    violations_sha256: str
    policy_versions: tuple[int, ...]
    cells: tuple[WorkflowMigrationCompatibilityCell, ...]

    def __post_init__(self) -> None:
        for value in (self.workflow_sha256, self.violations_sha256):
            if not isinstance(value, str) or len(value) != 64:
                raise WorkflowPolicyError(
                    "workflow migration compatibility matrix hash is invalid"
                )
        if self.status not in {"valid", "invalid"}:
            raise WorkflowPolicyError(
                "workflow migration compatibility matrix status is invalid"
            )
        if (
            not self.policy_versions
            or self.policy_versions
            != tuple(sorted(set(self.policy_versions)))
        ):
            raise WorkflowPolicyError(
                "workflow migration compatibility versions are invalid"
            )
        expected_pairs = tuple(
            (source, target)
            for source in self.policy_versions
            for target in self.policy_versions
        )
        actual_pairs = tuple(
            (cell.source_policy_version, cell.target_policy_version)
            for cell in self.cells
        )
        if actual_pairs != expected_pairs:
            raise WorkflowPolicyError(
                "workflow migration compatibility matrix is incomplete"
            )

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "offline-workflow-migration-compatibility-matrix-only",
            "runtime_authority": "none",
            "promotion": "none",
            "migration_execution_authority": "none",
            "workflow_sha256": self.workflow_sha256,
            "status": self.status,
            "violations_sha256": self.violations_sha256,
            "policy_versions": list(self.policy_versions),
            "cells": [cell.to_dict() for cell in self.cells],
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.to_bytes()).hexdigest()

    @classmethod
    def from_bytes(
        cls,
        data: bytes,
    ) -> "WorkflowMigrationCompatibilityMatrix":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise WorkflowPolicyError(
                "invalid workflow migration compatibility matrix"
            ) from exc
        expected = {
            "schema_version",
            "authority",
            "runtime_authority",
            "promotion",
            "migration_execution_authority",
            "workflow_sha256",
            "status",
            "violations_sha256",
            "policy_versions",
            "cells",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise WorkflowPolicyError(
                "workflow migration compatibility matrix has unexpected fields"
            )
        if (
            doc["schema_version"] != 1
            or doc["authority"]
            != "offline-workflow-migration-compatibility-matrix-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or doc["migration_execution_authority"] != "none"
            or not isinstance(doc["policy_versions"], list)
            or not isinstance(doc["cells"], list)
        ):
            raise WorkflowPolicyError(
                "workflow migration compatibility matrix boundary is invalid"
            )
        return cls(
            doc["workflow_sha256"],
            doc["status"],
            doc["violations_sha256"],
            tuple(doc["policy_versions"]),
            tuple(
                WorkflowMigrationCompatibilityCell.from_dict(item)
                for item in doc["cells"]
            ),
        )


def build_workflow_migration_compatibility_matrix(
    source_v1: WorkflowPolicyReceipt,
    target_v2: WorkflowPolicyReceiptV2,
    target_v3: WorkflowPolicyReceiptV3,
    history: WorkflowReceiptMigrationHistory,
) -> WorkflowMigrationCompatibilityMatrix:
    verify_workflow_receipt_migration_history(
        source_v1,
        target_v2,
        target_v3,
        history,
    )
    versions = tuple(range(history.initial_policy_version, history.final_policy_version + 1))
    if versions != (1, 2, 3):
        raise WorkflowPolicyError(
            "workflow migration compatibility versions are unsupported"
        )
    expected_violations_sha = _violations_sha(source_v1.violations)
    if (
        history.workflow_sha256 != source_v1.workflow_sha256
        or history.status != source_v1.status
        or history.violations_sha256 != expected_violations_sha
    ):
        raise WorkflowPolicyError(
            "workflow migration compatibility history boundary mismatch"
        )

    cells: list[WorkflowMigrationCompatibilityCell] = []
    for source in versions:
        for target in versions:
            if source == target:
                cells.append(
                    WorkflowMigrationCompatibilityCell(
                        source,
                        target,
                        True,
                        (source,),
                        "identity-compatible",
                    )
                )
            elif source < target:
                cells.append(
                    WorkflowMigrationCompatibilityCell(
                        source,
                        target,
                        True,
                        tuple(range(source, target + 1)),
                        "forward-compatible",
                    )
                )
            else:
                cells.append(
                    WorkflowMigrationCompatibilityCell(
                        source,
                        target,
                        False,
                        (),
                        "downgrade-unsupported",
                    )
                )
    return WorkflowMigrationCompatibilityMatrix(
        source_v1.workflow_sha256,
        source_v1.status,
        expected_violations_sha,
        versions,
        tuple(cells),
    )


def verify_workflow_migration_compatibility_matrix(
    source_v1: WorkflowPolicyReceipt,
    target_v2: WorkflowPolicyReceiptV2,
    target_v3: WorkflowPolicyReceiptV3,
    history: WorkflowReceiptMigrationHistory,
    matrix: WorkflowMigrationCompatibilityMatrix,
) -> bool:
    rebuilt = build_workflow_migration_compatibility_matrix(
        source_v1,
        target_v2,
        target_v3,
        history,
    )
    if rebuilt != matrix:
        raise WorkflowPolicyError(
            "workflow migration compatibility matrix mismatch"
        )
    return True
