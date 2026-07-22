"""Retention policies for replay-sequence compaction proofs."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any, Iterable

from .errors import RepairError
from .replay_sequence_compaction import ReplaySequenceCompactionProof

_ZERO_HASH = "0" * 64


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


@dataclass(frozen=True)
class ReplayCompactionRetentionPolicy:
    max_retained_compactions: int = 4

    def __post_init__(self) -> None:
        if (
            type(self.max_retained_compactions) is not int
            or self.max_retained_compactions <= 0
        ):
            raise RepairError(
                "replay compaction retention limit must be positive"
            )


@dataclass(frozen=True)
class ReplayCompactionRetentionDisposition:
    compaction_index: int
    proof_sha256: str
    source_sequence_sha256: str
    previous_compaction_sha256: str
    disposition: str

    def __post_init__(self) -> None:
        if type(self.compaction_index) is not int or self.compaction_index <= 0:
            raise RepairError(
                "replay compaction retention index must be positive"
            )
        for value in (
            self.proof_sha256,
            self.source_sequence_sha256,
            self.previous_compaction_sha256,
        ):
            if not isinstance(value, str) or len(value) != 64:
                raise RepairError(
                    "replay compaction retention hash is invalid"
                )
        if self.disposition not in {"retain", "tombstone-metadata"}:
            raise RepairError(
                "replay compaction retention disposition is unsupported"
            )

    def to_dict(self) -> dict[str, Any]:
        return {
            "compaction_index": self.compaction_index,
            "proof_sha256": self.proof_sha256,
            "source_sequence_sha256": self.source_sequence_sha256,
            "previous_compaction_sha256": self.previous_compaction_sha256,
            "disposition": self.disposition,
        }

    @classmethod
    def from_dict(
        cls,
        doc: Any,
    ) -> "ReplayCompactionRetentionDisposition":
        expected = {
            "compaction_index",
            "proof_sha256",
            "source_sequence_sha256",
            "previous_compaction_sha256",
            "disposition",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise RepairError(
                "replay compaction retention disposition is invalid"
            )
        return cls(
            doc["compaction_index"],
            doc["proof_sha256"],
            doc["source_sequence_sha256"],
            doc["previous_compaction_sha256"],
            doc["disposition"],
        )


@dataclass(frozen=True)
class ReplayCompactionRetentionPlan:
    policy: ReplayCompactionRetentionPolicy
    source_compaction_count: int
    source_final_compaction_sha256: str
    dispositions: tuple[ReplayCompactionRetentionDisposition, ...]

    def __post_init__(self) -> None:
        if (
            type(self.source_compaction_count) is not int
            or self.source_compaction_count <= 0
        ):
            raise RepairError(
                "replay compaction retention source count must be positive"
            )
        if (
            not isinstance(self.source_final_compaction_sha256, str)
            or len(self.source_final_compaction_sha256) != 64
        ):
            raise RepairError(
                "replay compaction retention final hash is invalid"
            )
        if len(self.dispositions) != self.source_compaction_count:
            raise RepairError(
                "replay compaction retention disposition count mismatch"
            )
        indexes = tuple(
            item.compaction_index for item in self.dispositions
        )
        if indexes != tuple(range(1, self.source_compaction_count + 1)):
            raise RepairError(
                "replay compaction retention indices are not contiguous"
            )
        if (
            sum(
                item.disposition == "retain"
                for item in self.dispositions
            )
            > self.policy.max_retained_compactions
        ):
            raise RepairError(
                "replay compaction retention plan exceeds policy"
            )

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-api-v2-replay-compaction-retention-only",
            "runtime_authority": "none",
            "promotion": "none",
            "deletion_authority": "none",
            "policy": {
                "max_retained_compactions": (
                    self.policy.max_retained_compactions
                )
            },
            "source_compaction_count": self.source_compaction_count,
            "source_final_compaction_sha256": (
                self.source_final_compaction_sha256
            ),
            "dispositions": [
                item.to_dict() for item in self.dispositions
            ],
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.to_bytes()).hexdigest()

    @classmethod
    def from_bytes(cls, data: bytes) -> "ReplayCompactionRetentionPlan":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise RepairError(
                "invalid replay compaction retention plan"
            ) from exc
        expected = {
            "schema_version",
            "authority",
            "runtime_authority",
            "promotion",
            "deletion_authority",
            "policy",
            "source_compaction_count",
            "source_final_compaction_sha256",
            "dispositions",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise RepairError(
                "replay compaction retention plan has unexpected fields"
            )
        if (
            doc["schema_version"] != 1
            or doc["authority"]
            != "synthetic-api-v2-replay-compaction-retention-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or doc["deletion_authority"] != "none"
            or not isinstance(doc["policy"], dict)
            or set(doc["policy"]) != {"max_retained_compactions"}
            or not isinstance(doc["dispositions"], list)
        ):
            raise RepairError(
                "replay compaction retention boundary is invalid"
            )
        return cls(
            ReplayCompactionRetentionPolicy(
                doc["policy"]["max_retained_compactions"]
            ),
            doc["source_compaction_count"],
            doc["source_final_compaction_sha256"],
            tuple(
                ReplayCompactionRetentionDisposition.from_dict(item)
                for item in doc["dispositions"]
            ),
        )


def _validate_compaction_chain(
    proofs: tuple[ReplaySequenceCompactionProof, ...],
) -> None:
    if not proofs:
        raise RepairError(
            "replay compaction retention requires proofs"
        )
    previous = _ZERO_HASH
    for proof in proofs:
        if proof.previous_compaction_sha256 != previous:
            raise RepairError(
                "replay compaction retention chain is broken"
            )
        previous = proof.sha256


def build_replay_compaction_retention_plan(
    proofs: Iterable[ReplaySequenceCompactionProof],
    *,
    policy: ReplayCompactionRetentionPolicy = (
        ReplayCompactionRetentionPolicy()
    ),
) -> ReplayCompactionRetentionPlan:
    supplied = tuple(proofs)
    _validate_compaction_chain(supplied)
    retain_from = max(
        1,
        len(supplied) - policy.max_retained_compactions + 1,
    )
    dispositions = tuple(
        ReplayCompactionRetentionDisposition(
            index,
            proof.sha256,
            proof.source_sequence_sha256,
            proof.previous_compaction_sha256,
            "retain" if index >= retain_from else "tombstone-metadata",
        )
        for index, proof in enumerate(supplied, start=1)
    )
    return ReplayCompactionRetentionPlan(
        policy,
        len(supplied),
        supplied[-1].sha256,
        dispositions,
    )


def verify_replay_compaction_retention_plan(
    proofs: Iterable[ReplaySequenceCompactionProof],
    plan: ReplayCompactionRetentionPlan,
) -> bool:
    rebuilt = build_replay_compaction_retention_plan(
        proofs,
        policy=plan.policy,
    )
    if rebuilt != plan:
        raise RepairError(
            "replay compaction retention plan mismatch"
        )
    return True
