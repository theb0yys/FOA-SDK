"""Append-only synthetic crash-recovery journal with hash chaining."""
from __future__ import annotations

import hashlib
import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .errors import JournalCorruptionError, SimulatedCrash

_SCHEMA_VERSION = 1
_ZERO_HASH = "0" * 64


def _canonical(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


@dataclass(frozen=True)
class JournalRecord:
    sequence: int
    transaction_id: str
    phase: str
    payload: dict[str, Any]
    previous_hash: str
    record_hash: str

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": _SCHEMA_VERSION,
            "sequence": self.sequence,
            "transaction_id": self.transaction_id,
            "phase": self.phase,
            "payload": self.payload,
            "previous_hash": self.previous_hash,
            "record_hash": self.record_hash,
        }


class CrashRecoveryJournal:
    """A deterministic JSONL journal that tolerates only a torn final line."""

    def __init__(self, path: Path):
        self.path = Path(path)

    def append(
        self,
        transaction_id: str,
        phase: str,
        payload: dict[str, Any] | None = None,
        *,
        crash_after_bytes: int | None = None,
    ) -> JournalRecord:
        if not transaction_id or not phase:
            raise ValueError("transaction_id and phase must be non-empty")
        records, torn = self.read_records(allow_torn_tail=True)
        if torn:
            records = list(self.recover())
        previous_hash = records[-1].record_hash if records else _ZERO_HASH
        sequence = records[-1].sequence + 1 if records else 1
        core = {
            "schema_version": _SCHEMA_VERSION,
            "sequence": sequence,
            "transaction_id": transaction_id,
            "phase": phase,
            "payload": dict(payload or {}),
            "previous_hash": previous_hash,
        }
        record_hash = hashlib.sha256(_canonical(core)).hexdigest()
        record = JournalRecord(
            sequence,
            transaction_id,
            phase,
            core["payload"],
            previous_hash,
            record_hash,
        )
        line = _canonical(record.to_dict()) + b"\n"
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("ab") as handle:
            if crash_after_bytes is not None:
                if crash_after_bytes < 0:
                    raise ValueError("crash_after_bytes must be non-negative")
                handle.write(line[:crash_after_bytes])
                handle.flush()
                os.fsync(handle.fileno())
                raise SimulatedCrash("synthetic crash during journal append")
            handle.write(line)
            handle.flush()
            os.fsync(handle.fileno())
        return record

    def read_records(self, *, allow_torn_tail: bool = False) -> tuple[list[JournalRecord], bytes]:
        if not self.path.exists():
            return [], b""
        data = self.path.read_bytes()
        lines = data.splitlines(keepends=True)
        torn = b""
        if lines and not lines[-1].endswith(b"\n"):
            torn = lines.pop()
            if not allow_torn_tail:
                raise JournalCorruptionError("journal has torn final record")

        records: list[JournalRecord] = []
        expected_previous = _ZERO_HASH
        expected_sequence = 1
        for index, raw in enumerate(lines, start=1):
            try:
                doc = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise JournalCorruptionError(f"journal line {index} is invalid JSON") from exc
            if not isinstance(doc, dict):
                raise JournalCorruptionError(f"journal line {index} is not an object")
            required = {
                "schema_version",
                "sequence",
                "transaction_id",
                "phase",
                "payload",
                "previous_hash",
                "record_hash",
            }
            if set(doc) != required:
                raise JournalCorruptionError(f"journal line {index} has unexpected fields")
            if doc["schema_version"] != _SCHEMA_VERSION:
                raise JournalCorruptionError(f"journal line {index} schema mismatch")
            if doc["sequence"] != expected_sequence:
                raise JournalCorruptionError(f"journal line {index} sequence mismatch")
            if doc["previous_hash"] != expected_previous:
                raise JournalCorruptionError(f"journal line {index} chain mismatch")
            core = {key: doc[key] for key in required if key != "record_hash"}
            actual_hash = hashlib.sha256(_canonical(core)).hexdigest()
            if doc["record_hash"] != actual_hash:
                raise JournalCorruptionError(f"journal line {index} hash mismatch")
            if not isinstance(doc["payload"], dict):
                raise JournalCorruptionError(f"journal line {index} payload must be an object")
            record = JournalRecord(
                doc["sequence"],
                doc["transaction_id"],
                doc["phase"],
                doc["payload"],
                doc["previous_hash"],
                doc["record_hash"],
            )
            records.append(record)
            expected_previous = record.record_hash
            expected_sequence += 1
        return records, torn

    def recover(self) -> tuple[JournalRecord, ...]:
        records, torn = self.read_records(allow_torn_tail=True)
        if torn:
            valid_bytes = b"".join(_canonical(record.to_dict()) + b"\n" for record in records)
            temp = self.path.with_name(f".{self.path.name}.recover")
            temp.write_bytes(valid_bytes)
            os.replace(temp, self.path)
        return tuple(records)

    def latest(self, transaction_id: str | None = None) -> JournalRecord | None:
        records, _ = self.read_records(allow_torn_tail=True)
        candidates: Iterable[JournalRecord] = records
        if transaction_id is not None:
            candidates = (record for record in records if record.transaction_id == transaction_id)
        latest: JournalRecord | None = None
        for latest in candidates:
            pass
        return latest
