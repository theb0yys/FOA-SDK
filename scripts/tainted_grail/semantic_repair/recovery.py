"""Deterministic, read-only recovery planning for synthetic journals."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from enum import Enum
from typing import Any, TYPE_CHECKING

if TYPE_CHECKING:
    from .journal import CrashRecoveryJournal, JournalRecord

_SCHEMA_VERSION = 1
_TERMINAL = {
    "committed",
    "rolled-back",
    "cancelled",
    "intent-committed",
    "intent-rolled-back",
    "compacted-checkpoint",
}
_ROLLBACK = {
    "preparing",
    "prepared",
    "committing",
    "failed",
    "rolling-back",
    "cancel-requested",
    "cancelling",
    "intent-prepared",
    "intent-file-published",
}


class RecoveryAction(str, Enum):
    TRUNCATE_TORN_TAIL = "truncate-torn-tail"
    ROLLBACK = "rollback"
    NONE = "none"
    MANUAL_REVIEW = "manual-review"


@dataclass(frozen=True)
class RecoveryStep:
    order: int
    transaction_id: str
    action: RecoveryAction
    reason: str
    latest_phase: str | None = None
    owner_id: str | None = None
    lock_generation: int | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "order": self.order,
            "transaction_id": self.transaction_id,
            "action": self.action.value,
            "reason": self.reason,
            "latest_phase": self.latest_phase,
            "owner_id": self.owner_id,
            "lock_generation": self.lock_generation,
        }


@dataclass(frozen=True)
class RecoveryPlan:
    journal_sha256: str
    torn_tail_bytes: int
    steps: tuple[RecoveryStep, ...]

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": _SCHEMA_VERSION,
            "authority": "synthetic-recovery-plan-only",
            "runtime_authority": "none",
            "promotion": "none",
            "journal_sha256": self.journal_sha256,
            "torn_tail_bytes": self.torn_tail_bytes,
            "steps": [step.to_dict() for step in self.steps],
        }

    def to_bytes(self) -> bytes:
        return (
            json.dumps(self.to_dict(), sort_keys=True, separators=(",", ":")) + "\n"
        ).encode("utf-8")

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.to_bytes()).hexdigest()


def _ownership(records: list["JournalRecord"]) -> tuple[str | None, int | None, bool]:
    identities: set[tuple[str, int]] = set()
    for record in records:
        owner = record.payload.get("journal_owner_id")
        generation = record.payload.get("journal_lock_generation")
        if owner is None and generation is None:
            continue
        if not isinstance(owner, str) or type(generation) is not int:
            return None, None, True
        identities.add((owner, generation))
    if len(identities) > 1:
        return None, None, True
    if not identities:
        return None, None, False
    owner, generation = next(iter(identities))
    return owner, generation, False


def build_recovery_plan(journal: "CrashRecoveryJournal") -> RecoveryPlan:
    raw = journal.path.read_bytes() if journal.path.exists() else b""
    records, torn = journal.read_records(allow_torn_tail=True)
    grouped: dict[str, list["JournalRecord"]] = {}
    for record in records:
        grouped.setdefault(record.transaction_id, []).append(record)

    steps: list[RecoveryStep] = []
    order = 1
    if torn:
        steps.append(
            RecoveryStep(
                order,
                "__journal__",
                RecoveryAction.TRUNCATE_TORN_TAIL,
                "journal contains a torn final record",
            )
        )
        order += 1

    for transaction_id in sorted(grouped):
        tx_records = grouped[transaction_id]
        latest = tx_records[-1]
        owner, generation, ownership_conflict = _ownership(tx_records)
        if ownership_conflict:
            action = RecoveryAction.MANUAL_REVIEW
            reason = "transaction ownership or generation changed within one journal history"
        elif latest.phase in _TERMINAL:
            action = RecoveryAction.NONE
            reason = "transaction reached a terminal phase"
        elif latest.phase in _ROLLBACK:
            action = RecoveryAction.ROLLBACK
            reason = "transaction did not reach a terminal phase"
        else:
            action = RecoveryAction.MANUAL_REVIEW
            reason = "transaction phase is not recognized by the recovery planner"
        steps.append(
            RecoveryStep(
                order,
                transaction_id,
                action,
                reason,
                latest.phase,
                owner,
                generation,
            )
        )
        order += 1

    return RecoveryPlan(
        hashlib.sha256(raw).hexdigest(),
        len(torn),
        tuple(steps),
    )
