"""Lease-owned multi-file publication with a recoverable intent journal."""
from __future__ import annotations

import base64
import hashlib
import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .errors import PublicationIntentError, SimulatedCrash
from .journal import CrashRecoveryJournal
from .ownership import ResourceLease

_SCHEMA_VERSION = 1
_PREPARED = "intent-prepared"
_FILE_PUBLISHED = "intent-file-published"
_COMMITTED = "intent-committed"
_ROLLED_BACK = "intent-rolled-back"


def _canonical(doc: dict[str, Any]) -> bytes:
    return (
        json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _restore(path: Path, previous: bytes | None) -> None:
    if previous is None:
        path.unlink(missing_ok=True)
    else:
        _atomic_write(path, previous)


def _relative_path(value: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise PublicationIntentError("publication target path must be non-empty text")
    path = Path(value)
    if (
        path.is_absolute()
        or value != path.as_posix()
        or any(part in {"", ".", ".."} for part in path.parts)
    ):
        raise PublicationIntentError("publication target must be a normalized relative path")
    return value


@dataclass(frozen=True)
class PublicationTarget:
    relative_path: str
    payload: bytes

    def __post_init__(self) -> None:
        _relative_path(self.relative_path)
        if not isinstance(self.payload, bytes):
            raise PublicationIntentError("publication payload must be bytes")


@dataclass(frozen=True)
class IntentTargetRecord:
    relative_path: str
    before_exists: bool
    before_base64: str | None
    before_sha256: str | None
    after_sha256: str
    after_size_bytes: int

    def to_dict(self) -> dict[str, Any]:
        return {
            "relative_path": self.relative_path,
            "before_exists": self.before_exists,
            "before_base64": self.before_base64,
            "before_sha256": self.before_sha256,
            "after_sha256": self.after_sha256,
            "after_size_bytes": self.after_size_bytes,
        }

    @classmethod
    def from_dict(cls, doc: Any) -> "IntentTargetRecord":
        expected = {
            "relative_path",
            "before_exists",
            "before_base64",
            "before_sha256",
            "after_sha256",
            "after_size_bytes",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise PublicationIntentError("intent target has unexpected fields")
        _relative_path(doc["relative_path"])
        if type(doc["before_exists"]) is not bool:
            raise PublicationIntentError("intent target before_exists must be boolean")
        if doc["before_exists"]:
            if not isinstance(doc["before_base64"], str) or not isinstance(
                doc["before_sha256"], str
            ):
                raise PublicationIntentError("intent target before image is missing")
            try:
                before = base64.b64decode(doc["before_base64"], validate=True)
            except Exception as exc:
                raise PublicationIntentError("intent target before image is invalid") from exc
            if _sha(before) != doc["before_sha256"]:
                raise PublicationIntentError("intent target before hash mismatch")
        elif doc["before_base64"] is not None or doc["before_sha256"] is not None:
            raise PublicationIntentError("absent target cannot contain before bytes")
        if (
            not isinstance(doc["after_sha256"], str)
            or len(doc["after_sha256"]) != 64
            or type(doc["after_size_bytes"]) is not int
            or doc["after_size_bytes"] < 0
        ):
            raise PublicationIntentError("intent target after image is invalid")
        return cls(
            doc["relative_path"],
            doc["before_exists"],
            doc["before_base64"],
            doc["before_sha256"],
            doc["after_sha256"],
            doc["after_size_bytes"],
        )

    def before_bytes(self) -> bytes | None:
        if not self.before_exists:
            return None
        assert self.before_base64 is not None
        return base64.b64decode(self.before_base64)


@dataclass(frozen=True)
class PublicationIntent:
    intent_id: str
    owner_id: str
    lock_generation: int
    targets: tuple[IntentTargetRecord, ...]

    def __post_init__(self) -> None:
        if not isinstance(self.intent_id, str) or not self.intent_id:
            raise PublicationIntentError("intent_id must be non-empty")
        if not isinstance(self.owner_id, str) or not self.owner_id:
            raise PublicationIntentError("owner_id must be non-empty")
        if type(self.lock_generation) is not int or self.lock_generation <= 0:
            raise PublicationIntentError("lock_generation must be positive")
        if not self.targets:
            raise PublicationIntentError("publication intent requires targets")
        paths = [target.relative_path for target in self.targets]
        if len(paths) != len(set(paths)):
            raise PublicationIntentError("publication intent target paths must be unique")
        if paths != sorted(paths):
            raise PublicationIntentError("publication intent targets must be sorted")

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": _SCHEMA_VERSION,
            "kind": "multi-file-publication-intent",
            "authority": "synthetic-publication-intent-only",
            "runtime_authority": "none",
            "promotion": "none",
            "intent_id": self.intent_id,
            "owner_id": self.owner_id,
            "lock_generation": self.lock_generation,
            "targets": [target.to_dict() for target in self.targets],
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return _sha(self.to_bytes())

    @classmethod
    def from_dict(cls, doc: Any) -> "PublicationIntent":
        expected = {
            "schema_version",
            "kind",
            "authority",
            "runtime_authority",
            "promotion",
            "intent_id",
            "owner_id",
            "lock_generation",
            "targets",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise PublicationIntentError("publication intent has unexpected fields")
        if (
            doc["schema_version"] != _SCHEMA_VERSION
            or doc["kind"] != "multi-file-publication-intent"
            or doc["authority"] != "synthetic-publication-intent-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or not isinstance(doc["targets"], list)
        ):
            raise PublicationIntentError("publication intent boundary is invalid")
        return cls(
            doc["intent_id"],
            doc["owner_id"],
            doc["lock_generation"],
            tuple(IntentTargetRecord.from_dict(item) for item in doc["targets"]),
        )


@dataclass(frozen=True)
class MultiFilePublicationReceipt:
    intent_id: str
    intent_sha256: str
    owner_id: str
    lock_generation: int
    target_count: int
    final_record_hash: str
    status: str

    def to_bytes(self) -> bytes:
        return _canonical(
            {
                "schema_version": _SCHEMA_VERSION,
                "authority": "synthetic-multi-file-publication-only",
                "runtime_authority": "none",
                "promotion": "none",
                "intent_id": self.intent_id,
                "intent_sha256": self.intent_sha256,
                "owner_id": self.owner_id,
                "lock_generation": self.lock_generation,
                "target_count": self.target_count,
                "final_record_hash": self.final_record_hash,
                "status": self.status,
            }
        )


class MultiFileIntentPublisher:
    """Publishes normalized relative targets under one owned intent journal."""

    def __init__(self, root: Path, journal: CrashRecoveryJournal):
        self.root = Path(root).resolve(strict=False)
        self.journal = journal

    def _target_path(self, relative_path: str) -> Path:
        relative_path = _relative_path(relative_path)
        candidate = (self.root / relative_path).resolve(strict=False)
        try:
            candidate.relative_to(self.root)
        except ValueError as exc:
            raise PublicationIntentError("publication target escapes root") from exc
        return candidate

    def _build_intent(
        self,
        lease: ResourceLease,
        intent_id: str,
        targets: Iterable[PublicationTarget],
    ) -> tuple[PublicationIntent, dict[str, bytes]]:
        supplied = sorted(tuple(targets), key=lambda target: target.relative_path)
        if not supplied:
            raise PublicationIntentError("publication requires at least one target")
        payloads: dict[str, bytes] = {}
        records: list[IntentTargetRecord] = []
        for target in supplied:
            path = self._target_path(target.relative_path)
            if target.relative_path in payloads:
                raise PublicationIntentError("duplicate publication target")
            before = path.read_bytes() if path.exists() else None
            payloads[target.relative_path] = target.payload
            records.append(
                IntentTargetRecord(
                    target.relative_path,
                    before is not None,
                    base64.b64encode(before).decode("ascii") if before is not None else None,
                    _sha(before) if before is not None else None,
                    _sha(target.payload),
                    len(target.payload),
                )
            )
        return (
            PublicationIntent(
                intent_id,
                lease.identity.owner_id,
                lease.identity.generation,
                tuple(records),
            ),
            payloads,
        )

    def publish_owned(
        self,
        lease: ResourceLease,
        intent_id: str,
        targets: Iterable[PublicationTarget],
        *,
        crash_after_target: int | None = None,
    ) -> MultiFilePublicationReceipt:
        self.journal.lock.assert_owned(lease)
        intent, payloads = self._build_intent(lease, intent_id, targets)
        self.journal.append_owned(
            lease,
            intent_id,
            _PREPARED,
            {"publication_intent": intent.to_dict(), "intent_sha256": intent.sha256},
        )
        try:
            for index, target in enumerate(intent.targets, start=1):
                path = self._target_path(target.relative_path)
                _atomic_write(path, payloads[target.relative_path])
                record = self.journal.append_owned(
                    lease,
                    intent_id,
                    _FILE_PUBLISHED,
                    {
                        "intent_sha256": intent.sha256,
                        "target_index": index,
                        "relative_path": target.relative_path,
                        "after_sha256": target.after_sha256,
                    },
                )
                if crash_after_target == index:
                    raise SimulatedCrash(
                        f"synthetic crash after publication target {index}"
                    )
            record = self.journal.append_owned(
                lease,
                intent_id,
                _COMMITTED,
                {"intent_sha256": intent.sha256, "target_count": len(intent.targets)},
            )
        except SimulatedCrash:
            raise
        except Exception as exc:
            self._restore_intent(intent)
            self.journal.append_owned(
                lease,
                intent_id,
                _ROLLED_BACK,
                {
                    "intent_sha256": intent.sha256,
                    "error_type": type(exc).__name__,
                    "error": str(exc),
                },
            )
            raise
        return MultiFilePublicationReceipt(
            intent_id,
            intent.sha256,
            lease.identity.owner_id,
            lease.identity.generation,
            len(intent.targets),
            record.record_hash,
            "committed",
        )

    def recover_owned(
        self,
        lease: ResourceLease,
        intent_id: str,
    ) -> MultiFilePublicationReceipt:
        self.journal.lock.assert_owned(lease)
        records, torn = self.journal.read_records(allow_torn_tail=True)
        if torn:
            raise PublicationIntentError(
                "intent journal must be recovered before publication recovery"
            )
        tx_records = [record for record in records if record.transaction_id == intent_id]
        if not tx_records:
            raise PublicationIntentError("publication intent is absent")
        latest = tx_records[-1]
        prepared = next((record for record in tx_records if record.phase == _PREPARED), None)
        if prepared is None:
            raise PublicationIntentError("publication intent has no prepared record")
        intent = PublicationIntent.from_dict(prepared.payload.get("publication_intent"))
        if intent.owner_id != lease.identity.owner_id:
            raise PublicationIntentError("publication recovery owner differs from intent owner")
        if latest.phase == _COMMITTED:
            return MultiFilePublicationReceipt(
                intent_id,
                intent.sha256,
                lease.identity.owner_id,
                lease.identity.generation,
                len(intent.targets),
                latest.record_hash,
                "already-committed",
            )
        if latest.phase == _ROLLED_BACK:
            return MultiFilePublicationReceipt(
                intent_id,
                intent.sha256,
                lease.identity.owner_id,
                lease.identity.generation,
                len(intent.targets),
                latest.record_hash,
                "already-rolled-back",
            )
        if latest.phase not in {_PREPARED, _FILE_PUBLISHED}:
            raise PublicationIntentError("publication intent phase requires manual review")

        for target in intent.targets:
            path = self._target_path(target.relative_path)
            current = path.read_bytes() if path.exists() else None
            current_sha = _sha(current) if current is not None else None
            if current_sha not in {target.before_sha256, target.after_sha256}:
                raise PublicationIntentError(
                    f"publication target {target.relative_path} has unknown bytes"
                )
        self._restore_intent(intent)
        record = self.journal.append_owned(
            lease,
            intent_id,
            _ROLLED_BACK,
            {"intent_sha256": intent.sha256, "reason": "deterministic crash recovery"},
        )
        return MultiFilePublicationReceipt(
            intent_id,
            intent.sha256,
            lease.identity.owner_id,
            lease.identity.generation,
            len(intent.targets),
            record.record_hash,
            "rolled-back",
        )

    def _restore_intent(self, intent: PublicationIntent) -> None:
        for target in reversed(intent.targets):
            _restore(self._target_path(target.relative_path), target.before_bytes())
