"""Hash-chained sequence proofs for API v2 replay-window receipts."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any, Iterable

from .errors import RepairError
from .replay_window import SnapshotReplayWindowReceipt

_ZERO_HASH = "0" * 64
_ALLOWED = {
    "accepted",
    "already-current",
    "same-generation-conflict",
    "replayed-stale",
    "replay-window-exceeded",
}


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


@dataclass(frozen=True)
class ReplaySequenceEntry:
    index: int
    previous_entry_sha256: str
    receipt_sha256: str
    current_generation: int
    incoming_generation: int
    status: str
    resulting_generation: int
    entry_sha256: str

    def core_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "index": self.index,
            "previous_entry_sha256": self.previous_entry_sha256,
            "receipt_sha256": self.receipt_sha256,
            "current_generation": self.current_generation,
            "incoming_generation": self.incoming_generation,
            "status": self.status,
            "resulting_generation": self.resulting_generation,
        }

    def to_dict(self) -> dict[str, Any]:
        return {**self.core_dict(), "entry_sha256": self.entry_sha256}

    @classmethod
    def build(
        cls,
        index: int,
        previous_entry_sha256: str,
        receipt: SnapshotReplayWindowReceipt,
    ) -> "ReplaySequenceEntry":
        if type(index) is not int or index <= 0:
            raise RepairError("replay sequence index must be positive")
        if len(previous_entry_sha256) != 64:
            raise RepairError("replay sequence previous hash is invalid")
        if receipt.status not in _ALLOWED:
            raise RepairError(
                "replay sequence receipt status is unsupported"
            )
        resulting = (
            receipt.incoming_generation
            if receipt.status == "accepted"
            else receipt.current_generation
        )
        core = {
            "schema_version": 1,
            "index": index,
            "previous_entry_sha256": previous_entry_sha256,
            "receipt_sha256": receipt.sha256,
            "current_generation": receipt.current_generation,
            "incoming_generation": receipt.incoming_generation,
            "status": receipt.status,
            "resulting_generation": resulting,
        }
        return cls(
            index,
            previous_entry_sha256,
            receipt.sha256,
            receipt.current_generation,
            receipt.incoming_generation,
            receipt.status,
            resulting,
            hashlib.sha256(_canonical(core)).hexdigest(),
        )


@dataclass(frozen=True)
class ReplayWindowSequenceProof:
    initial_generation: int
    final_generation: int
    entries: tuple[ReplaySequenceEntry, ...]

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-api-v2-replay-sequence-proof-only",
            "runtime_authority": "none",
            "promotion": "none",
            "initial_generation": self.initial_generation,
            "final_generation": self.final_generation,
            "entries": [entry.to_dict() for entry in self.entries],
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.to_bytes()).hexdigest()


def build_replay_window_sequence_proof(
    receipts: Iterable[SnapshotReplayWindowReceipt],
) -> ReplayWindowSequenceProof:
    supplied = tuple(receipts)
    if not supplied:
        raise RepairError("replay sequence proof requires receipts")
    entries: list[ReplaySequenceEntry] = []
    expected_generation = supplied[0].current_generation
    previous = _ZERO_HASH
    for index, receipt in enumerate(supplied, start=1):
        if receipt.current_generation != expected_generation:
            raise RepairError(
                "replay sequence generation continuity failed"
            )
        entry = ReplaySequenceEntry.build(
            index,
            previous,
            receipt,
        )
        entries.append(entry)
        expected_generation = entry.resulting_generation
        previous = entry.entry_sha256
    return ReplayWindowSequenceProof(
        supplied[0].current_generation,
        expected_generation,
        tuple(entries),
    )


def verify_replay_window_sequence_proof(
    proof: ReplayWindowSequenceProof,
    receipts: Iterable[SnapshotReplayWindowReceipt],
) -> bool:
    rebuilt = build_replay_window_sequence_proof(receipts)
    if rebuilt != proof:
        raise RepairError(
            "replay sequence proof does not match receipts"
        )
    return True
