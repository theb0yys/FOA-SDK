"""Retention plans and metadata-only tombstones for checkpoint archives."""
from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from typing import Any, Iterable

from .checkpoint_archive import CheckpointArchiveManifest
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
class ArchiveRetentionPolicy:
    max_retained_archives: int = 4

    def __post_init__(self) -> None:
        if type(self.max_retained_archives) is not int or self.max_retained_archives <= 0:
            raise CheckpointProofError("archive retention limit must be positive")


@dataclass(frozen=True)
class ArchiveRetentionDisposition:
    archive_id: str
    archive_index: int
    manifest_sha256: str
    disposition: str

    def __post_init__(self) -> None:
        if not isinstance(self.archive_id, str) or not self.archive_id.strip():
            raise CheckpointProofError("archive retention archive_id must be non-empty")
        if type(self.archive_index) is not int or self.archive_index <= 0:
            raise CheckpointProofError("archive retention index must be positive")
        if not isinstance(self.manifest_sha256, str) or len(self.manifest_sha256) != 64:
            raise CheckpointProofError("archive retention manifest hash is invalid")
        if self.disposition not in {"retain", "tombstone"}:
            raise CheckpointProofError("archive retention disposition is unsupported")

    def to_dict(self) -> dict[str, Any]:
        return {
            "archive_id": self.archive_id,
            "archive_index": self.archive_index,
            "manifest_sha256": self.manifest_sha256,
            "disposition": self.disposition,
        }

    @classmethod
    def from_dict(cls, doc: Any) -> "ArchiveRetentionDisposition":
        expected = {"archive_id", "archive_index", "manifest_sha256", "disposition"}
        if not isinstance(doc, dict) or set(doc) != expected:
            raise CheckpointProofError("archive retention disposition has unexpected fields")
        return cls(doc["archive_id"], doc["archive_index"], doc["manifest_sha256"], doc["disposition"])


@dataclass(frozen=True)
class CheckpointArchiveRetentionPlan:
    policy: ArchiveRetentionPolicy
    source_manifest_count: int
    source_final_manifest_sha256: str
    dispositions: tuple[ArchiveRetentionDisposition, ...]

    def __post_init__(self) -> None:
        if type(self.source_manifest_count) is not int or self.source_manifest_count <= 0:
            raise CheckpointProofError("archive retention source count must be positive")
        if len(self.source_final_manifest_sha256) != 64:
            raise CheckpointProofError("archive retention final manifest hash is invalid")
        if len(self.dispositions) != self.source_manifest_count:
            raise CheckpointProofError("archive retention disposition count mismatch")
        indexes = tuple(item.archive_index for item in self.dispositions)
        if indexes != tuple(range(1, self.source_manifest_count + 1)):
            raise CheckpointProofError("archive retention indexes must be contiguous")
        if sum(item.disposition == "retain" for item in self.dispositions) > self.policy.max_retained_archives:
            raise CheckpointProofError("archive retention plan exceeds policy")

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-checkpoint-archive-retention-only",
            "runtime_authority": "none",
            "promotion": "none",
            "policy": {"max_retained_archives": self.policy.max_retained_archives},
            "source_manifest_count": self.source_manifest_count,
            "source_final_manifest_sha256": self.source_final_manifest_sha256,
            "dispositions": [item.to_dict() for item in self.dispositions],
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return _sha(self.to_bytes())

    @classmethod
    def from_bytes(cls, data: bytes) -> "CheckpointArchiveRetentionPlan":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CheckpointProofError("invalid archive retention plan") from exc
        expected = {"schema_version", "authority", "runtime_authority", "promotion", "policy", "source_manifest_count", "source_final_manifest_sha256", "dispositions"}
        if not isinstance(doc, dict) or set(doc) != expected:
            raise CheckpointProofError("archive retention plan has unexpected fields")
        if doc["schema_version"] != 1 or doc["authority"] != "synthetic-checkpoint-archive-retention-only" or doc["runtime_authority"] != "none" or doc["promotion"] != "none" or not isinstance(doc["policy"], dict) or set(doc["policy"]) != {"max_retained_archives"} or not isinstance(doc["dispositions"], list):
            raise CheckpointProofError("archive retention plan boundary is invalid")
        return cls(ArchiveRetentionPolicy(doc["policy"]["max_retained_archives"]), doc["source_manifest_count"], doc["source_final_manifest_sha256"], tuple(ArchiveRetentionDisposition.from_dict(item) for item in doc["dispositions"]))


@dataclass(frozen=True)
class CheckpointArchiveTombstoneManifest:
    tombstone_index: int
    archive_id: str
    archive_index: int
    archive_name: str
    archive_sha256: str
    archive_manifest_sha256: str
    previous_tombstone_sha256: str
    reason: str

    def __post_init__(self) -> None:
        if type(self.tombstone_index) is not int or self.tombstone_index <= 0:
            raise CheckpointProofError("archive tombstone index must be positive")
        if not isinstance(self.archive_id, str) or not self.archive_id.strip():
            raise CheckpointProofError("archive tombstone archive_id must be non-empty")
        if type(self.archive_index) is not int or self.archive_index <= 0:
            raise CheckpointProofError("archive tombstone archive index must be positive")
        if not isinstance(self.archive_name, str) or not self.archive_name.strip():
            raise CheckpointProofError("archive tombstone archive name must be non-empty")
        for value in (self.archive_sha256, self.archive_manifest_sha256, self.previous_tombstone_sha256):
            if not isinstance(value, str) or len(value) != 64:
                raise CheckpointProofError("archive tombstone hash is invalid")
        if not isinstance(self.reason, str) or not self.reason.strip():
            raise CheckpointProofError("archive tombstone reason must be non-empty")

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": 1,
            "authority": "synthetic-checkpoint-archive-tombstone-only",
            "runtime_authority": "none",
            "promotion": "none",
            "effect": "archive-metadata-tombstoned-no-delete-authority",
            "tombstone_index": self.tombstone_index,
            "archive_id": self.archive_id,
            "archive_index": self.archive_index,
            "archive_name": self.archive_name,
            "archive_sha256": self.archive_sha256,
            "archive_manifest_sha256": self.archive_manifest_sha256,
            "previous_tombstone_sha256": self.previous_tombstone_sha256,
            "reason": self.reason,
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @property
    def sha256(self) -> str:
        return _sha(self.to_bytes())

    @classmethod
    def from_bytes(cls, data: bytes) -> "CheckpointArchiveTombstoneManifest":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CheckpointProofError("invalid archive tombstone manifest") from exc
        expected = {"schema_version", "authority", "runtime_authority", "promotion", "effect", "tombstone_index", "archive_id", "archive_index", "archive_name", "archive_sha256", "archive_manifest_sha256", "previous_tombstone_sha256", "reason"}
        if not isinstance(doc, dict) or set(doc) != expected:
            raise CheckpointProofError("archive tombstone manifest has unexpected fields")
        if doc["schema_version"] != 1 or doc["authority"] != "synthetic-checkpoint-archive-tombstone-only" or doc["runtime_authority"] != "none" or doc["promotion"] != "none" or doc["effect"] != "archive-metadata-tombstoned-no-delete-authority":
            raise CheckpointProofError("archive tombstone boundary is invalid")
        return cls(doc["tombstone_index"], doc["archive_id"], doc["archive_index"], doc["archive_name"], doc["archive_sha256"], doc["archive_manifest_sha256"], doc["previous_tombstone_sha256"], doc["reason"])


def _validate_manifest_chain(manifests: tuple[CheckpointArchiveManifest, ...]) -> None:
    if not manifests:
        raise CheckpointProofError("archive retention requires manifests")
    expected_previous = _ZERO_HASH
    for index, manifest in enumerate(manifests, start=1):
        if manifest.archive_index != index:
            raise CheckpointProofError("archive retention manifests are not contiguous")
        if manifest.previous_manifest_sha256 != expected_previous:
            raise CheckpointProofError("archive retention manifest chain is broken")
        expected_previous = manifest.sha256


def build_checkpoint_archive_retention_plan(manifests: Iterable[CheckpointArchiveManifest], *, policy: ArchiveRetentionPolicy = ArchiveRetentionPolicy()) -> CheckpointArchiveRetentionPlan:
    supplied = tuple(sorted(manifests, key=lambda item: item.archive_index))
    _validate_manifest_chain(supplied)
    retain_from = max(1, len(supplied) - policy.max_retained_archives + 1)
    dispositions = tuple(ArchiveRetentionDisposition(manifest.archive_id, manifest.archive_index, manifest.sha256, "retain" if manifest.archive_index >= retain_from else "tombstone") for manifest in supplied)
    return CheckpointArchiveRetentionPlan(policy, len(supplied), supplied[-1].sha256, dispositions)


def build_checkpoint_archive_tombstone(manifest: CheckpointArchiveManifest, *, tombstone_index: int, reason: str, previous_tombstone: CheckpointArchiveTombstoneManifest | None = None) -> CheckpointArchiveTombstoneManifest:
    expected_index = 1 if previous_tombstone is None else previous_tombstone.tombstone_index + 1
    if tombstone_index != expected_index:
        raise CheckpointProofError("archive tombstone index is not contiguous")
    return CheckpointArchiveTombstoneManifest(tombstone_index, manifest.archive_id, manifest.archive_index, manifest.archive_name, manifest.archive_sha256, manifest.sha256, previous_tombstone.sha256 if previous_tombstone else _ZERO_HASH, reason)


def verify_checkpoint_archive_tombstone(manifest: CheckpointArchiveManifest, tombstone: CheckpointArchiveTombstoneManifest, *, previous_tombstone: CheckpointArchiveTombstoneManifest | None = None) -> bool:
    rebuilt = build_checkpoint_archive_tombstone(manifest, tombstone_index=tombstone.tombstone_index, reason=tombstone.reason, previous_tombstone=previous_tombstone)
    if rebuilt != tombstone:
        raise CheckpointProofError("archive tombstone does not match manifest")
    return True
