"""Release records and deterministic compaction for repeated-compaction quota ledgers."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any, Iterable

from .compaction_quota import CompactionQuotaLedger, CompactionQuotaReservation
from .errors import CheckpointProofError

_ZERO_HASH = "0" * 64


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


@dataclass(frozen=True)
class CompactionQuotaReleaseRecord:
    release_index: int
    source_ledger_sha256: str
    operation_id: str
    operation_index: int
    released_backup_bytes: int
    operation_sha256: str
    previous_release_sha256: str
    reason: str

    def __post_init__(self) -> None:
        if type(self.release_index) is not int or self.release_index <= 0:
            raise CheckpointProofError("quota release index must be positive")
        if not isinstance(self.operation_id, str) or not self.operation_id.strip():
            raise CheckpointProofError("quota release operation_id must be non-empty")
        if type(self.operation_index) is not int or self.operation_index <= 0:
            raise CheckpointProofError("quota release operation index must be positive")
        if type(self.released_backup_bytes) is not int or self.released_backup_bytes < 0:
            raise CheckpointProofError("quota released bytes are invalid")
        if any(
            not isinstance(value, str) or len(value) != 64
            for value in (
                self.source_ledger_sha256,
                self.operation_sha256,
                self.previous_release_sha256,
            )
        ):
            raise CheckpointProofError("quota release hash is invalid")
        if not isinstance(self.reason, str) or not self.reason.strip():
            raise CheckpointProofError("quota release reason must be non-empty")

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-compaction-quota-release-only",
            "runtime_authority": "none",
            "promotion": "none",
            "effect": "quota-reservation-released-metadata-only",
            "release_index": self.release_index,
            "source_ledger_sha256": self.source_ledger_sha256,
            "operation_id": self.operation_id,
            "operation_index": self.operation_index,
            "released_backup_bytes": self.released_backup_bytes,
            "operation_sha256": self.operation_sha256,
            "previous_release_sha256": self.previous_release_sha256,
            "reason": self.reason,
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return _sha(self.to_bytes())

    @classmethod
    def from_bytes(cls, data: bytes) -> "CompactionQuotaReleaseRecord":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CheckpointProofError("invalid quota release record") from exc
        expected = {
            "schema_version", "authority", "runtime_authority", "promotion",
            "effect", "release_index", "source_ledger_sha256", "operation_id",
            "operation_index", "released_backup_bytes", "operation_sha256",
            "previous_release_sha256", "reason",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise CheckpointProofError("quota release record has unexpected fields")
        if (
            doc["schema_version"] != 1
            or doc["authority"] != "synthetic-compaction-quota-release-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or doc["effect"] != "quota-reservation-released-metadata-only"
        ):
            raise CheckpointProofError("quota release record boundary is invalid")
        return cls(
            doc["release_index"], doc["source_ledger_sha256"],
            doc["operation_id"], doc["operation_index"],
            doc["released_backup_bytes"], doc["operation_sha256"],
            doc["previous_release_sha256"], doc["reason"],
        )


@dataclass(frozen=True)
class CompactionQuotaLedgerCompactionRecord:
    source_ledger_sha256: str
    release_record_sha256s: tuple[str, ...]
    released_operation_ids: tuple[str, ...]
    released_backup_bytes: int
    remaining_operation_ids: tuple[str, ...]
    remaining_backup_bytes: int
    compacted_ledger_sha256: str

    def __post_init__(self) -> None:
        if any(
            not isinstance(value, str) or len(value) != 64
            for value in self.release_record_sha256s
            + (self.source_ledger_sha256, self.compacted_ledger_sha256)
        ):
            raise CheckpointProofError("quota compaction hash is invalid")
        if self.released_operation_ids != tuple(sorted(self.released_operation_ids)):
            raise CheckpointProofError("released operation IDs must be sorted")
        if self.remaining_operation_ids != tuple(sorted(self.remaining_operation_ids)):
            raise CheckpointProofError("remaining operation IDs must be sorted")
        if set(self.released_operation_ids) & set(self.remaining_operation_ids):
            raise CheckpointProofError("released and remaining operations overlap")

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-compaction-quota-ledger-compaction-only",
            "runtime_authority": "none",
            "promotion": "none",
            "effect": "ledger-compacted-provenance-retained",
            "source_ledger_sha256": self.source_ledger_sha256,
            "release_record_sha256s": list(self.release_record_sha256s),
            "released_operation_ids": list(self.released_operation_ids),
            "released_backup_bytes": self.released_backup_bytes,
            "remaining_operation_ids": list(self.remaining_operation_ids),
            "remaining_backup_bytes": self.remaining_backup_bytes,
            "compacted_ledger_sha256": self.compacted_ledger_sha256,
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return _sha(self.to_bytes())

    @classmethod
    def from_bytes(cls, data: bytes) -> "CompactionQuotaLedgerCompactionRecord":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CheckpointProofError("invalid quota ledger compaction record") from exc
        expected = {
            "schema_version", "authority", "runtime_authority", "promotion",
            "effect", "source_ledger_sha256", "release_record_sha256s",
            "released_operation_ids", "released_backup_bytes",
            "remaining_operation_ids", "remaining_backup_bytes",
            "compacted_ledger_sha256",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise CheckpointProofError("quota compaction record has unexpected fields")
        if (
            doc["schema_version"] != 1
            or doc["authority"] != "synthetic-compaction-quota-ledger-compaction-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or doc["effect"] != "ledger-compacted-provenance-retained"
        ):
            raise CheckpointProofError("quota compaction record boundary is invalid")
        return cls(
            doc["source_ledger_sha256"], tuple(doc["release_record_sha256s"]),
            tuple(doc["released_operation_ids"]), doc["released_backup_bytes"],
            tuple(doc["remaining_operation_ids"]), doc["remaining_backup_bytes"],
            doc["compacted_ledger_sha256"],
        )


def build_compaction_quota_release_record(
    ledger: CompactionQuotaLedger,
    operation_id: str,
    *,
    release_index: int,
    reason: str,
    previous_release: CompactionQuotaReleaseRecord | None = None,
) -> CompactionQuotaReleaseRecord:
    match = next(
        (item for item in ledger.reservations if item.operation_id == operation_id),
        None,
    )
    if match is None:
        raise CheckpointProofError("quota release operation is absent")
    expected_index = 1 if previous_release is None else previous_release.release_index + 1
    if release_index != expected_index:
        raise CheckpointProofError("quota release index is not contiguous")
    if previous_release and previous_release.source_ledger_sha256 != ledger.sha256:
        raise CheckpointProofError("quota releases reference different source ledgers")
    return CompactionQuotaReleaseRecord(
        release_index, ledger.sha256, match.operation_id, match.operation_index,
        match.operation_backup_bytes, match.operation_sha256,
        previous_release.sha256 if previous_release else _ZERO_HASH, reason,
    )


def compact_compaction_quota_ledger(
    ledger: CompactionQuotaLedger,
    releases: Iterable[CompactionQuotaReleaseRecord],
) -> tuple[CompactionQuotaLedger, CompactionQuotaLedgerCompactionRecord]:
    supplied = tuple(sorted(releases, key=lambda item: item.release_index))
    if not supplied:
        raise CheckpointProofError("quota ledger compaction requires releases")
    previous = _ZERO_HASH
    released: set[str] = set()
    by_id = {item.operation_id: item for item in ledger.reservations}
    for index, release in enumerate(supplied, start=1):
        if (
            release.release_index != index
            or release.previous_release_sha256 != previous
            or release.source_ledger_sha256 != ledger.sha256
        ):
            raise CheckpointProofError("quota release chain is invalid")
        item = by_id.get(release.operation_id)
        if (
            item is None
            or release.operation_index != item.operation_index
            or release.released_backup_bytes != item.operation_backup_bytes
            or release.operation_sha256 != item.operation_sha256
        ):
            raise CheckpointProofError("quota release does not match reservation")
        if release.operation_id in released:
            raise CheckpointProofError("quota operation released more than once")
        released.add(release.operation_id)
        previous = release.sha256

    cumulative = 0
    remaining: list[CompactionQuotaReservation] = []
    for index, item in enumerate(
        (item for item in ledger.reservations if item.operation_id not in released),
        start=1,
    ):
        cumulative += item.operation_backup_bytes
        remaining.append(
            CompactionQuotaReservation(
                item.operation_id, index, item.operation_backup_bytes,
                cumulative, item.operation_sha256,
            )
        )
    compacted = CompactionQuotaLedger(ledger.limits, tuple(remaining))
    record = CompactionQuotaLedgerCompactionRecord(
        ledger.sha256,
        tuple(item.sha256 for item in supplied),
        tuple(sorted(released)),
        sum(by_id[item].operation_backup_bytes for item in released),
        tuple(sorted(item.operation_id for item in remaining)),
        cumulative,
        compacted.sha256,
    )
    return compacted, record


def verify_compaction_quota_ledger_compaction(
    source: CompactionQuotaLedger,
    releases: Iterable[CompactionQuotaReleaseRecord],
    compacted: CompactionQuotaLedger,
    record: CompactionQuotaLedgerCompactionRecord,
) -> bool:
    expected_ledger, expected_record = compact_compaction_quota_ledger(source, releases)
    if expected_ledger != compacted or expected_record != record:
        raise CheckpointProofError("quota ledger compaction verification failed")
    return True
