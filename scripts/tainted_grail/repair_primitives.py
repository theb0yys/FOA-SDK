#!/usr/bin/env python3
"""Engine-neutral repair primitives for Semantic Hook Batch 006.

This module intentionally has no game, loader, assembly, reflection, subprocess,
network, deployment, or evidence-promotion capability.
"""
from __future__ import annotations

import csv
import io
import json
import os
import re
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Generic, Mapping, MutableMapping, TypeVar

T = TypeVar("T")
_RESERVED_WINDOWS_NAMES = {
    "con", "prn", "aux", "nul",
    *(f"com{i}" for i in range(1, 10)),
    *(f"lpt{i}" for i in range(1, 10)),
}
_FORMULA_PREFIXES = ("=", "+", "-", "@")


class RepairError(ValueError):
    """Fail-closed validation or transaction error."""


@dataclass(frozen=True)
class TypedFieldAdapter(Generic[T]):
    profile_id: str
    type_id: str
    field_name: str
    value_type: type
    minimum: int | None = None
    maximum: int | None = None

    def validate_value(self, value: Any) -> T:
        if type(value) is not self.value_type:
            raise RepairError(f"{self.field_name}: expected exact {self.value_type.__name__}")
        if isinstance(value, int) and not isinstance(value, bool):
            if self.minimum is not None and value < self.minimum:
                raise RepairError(f"{self.field_name}: below minimum")
            if self.maximum is not None and value > self.maximum:
                raise RepairError(f"{self.field_name}: above maximum")
        return value

    def read(self, record: Mapping[str, Any], *, profile_id: str, type_id: str) -> T:
        self._validate_identity(profile_id, type_id)
        if self.field_name not in record:
            raise RepairError(f"{self.field_name}: missing")
        return self.validate_value(record[self.field_name])

    def _validate_identity(self, profile_id: str, type_id: str) -> None:
        if profile_id != self.profile_id or type_id != self.type_id:
            raise RepairError("adapter identity mismatch")


@dataclass
class MappingTransaction(Generic[T]):
    adapter: TypedFieldAdapter[T]
    record: MutableMapping[str, Any]
    profile_id: str
    type_id: str
    proposed: T
    _original: T | None = field(init=False, default=None)
    _prepared: bool = field(init=False, default=False)
    _committed: bool = field(init=False, default=False)

    def prepare(self) -> None:
        self._original = self.adapter.read(self.record, profile_id=self.profile_id, type_id=self.type_id)
        self.adapter.validate_value(self.proposed)
        self._prepared = True

    def commit(self, after_write: Callable[[], None] | None = None) -> None:
        if not self._prepared:
            raise RepairError("transaction not prepared")
        if self._committed:
            raise RepairError("transaction already committed")
        self.record[self.adapter.field_name] = self.proposed
        try:
            if after_write is not None:
                after_write()
        except Exception:
            self.rollback()
            raise
        self._committed = True

    def rollback(self) -> None:
        if self._prepared and self._original is not None:
            self.record[self.adapter.field_name] = self._original
        self._committed = False

    @property
    def committed(self) -> bool:
        return self._committed


@dataclass(frozen=True)
class DiagnosticLimits:
    field_bytes: int = 256
    row_bytes: int = 4096
    session_bytes: int = 65536
    session_segment_chars: int = 64


@dataclass
class DiagnosticSession:
    root: Path
    session_name: str
    limits: DiagnosticLimits = field(default_factory=DiagnosticLimits)
    _published_bytes: int = 0

    def __post_init__(self) -> None:
        self.root = self.root.resolve(strict=False)
        self.session_name = validate_session_segment(self.session_name, self.limits.session_segment_chars)

    @property
    def session_dir(self) -> Path:
        candidate = (self.root / self.session_name).resolve(strict=False)
        _require_descendant(self.root, candidate)
        return candidate

    def encode_support_csv(self, values: Mapping[str, Any], *, sensitive_fields: set[str]) -> bytes:
        sanitized: list[str] = []
        for key in sorted(values):
            raw = "[REDACTED]" if key in sensitive_fields else str(values[key])
            neutralized = "'" + raw if raw.startswith(_FORMULA_PREFIXES) else raw
            data = neutralized.encode("utf-8")
            if len(data) > self.limits.field_bytes:
                marker = "…[TRUNCATED]"
                budget = max(0, self.limits.field_bytes - len(marker.encode("utf-8")))
                neutralized = data[:budget].decode("utf-8", errors="ignore") + marker
            sanitized.append(neutralized)
        stream = io.StringIO(newline="")
        csv.writer(stream, lineterminator="\n").writerow(sanitized)
        encoded = stream.getvalue().encode("utf-8")
        if len(encoded) > self.limits.row_bytes:
            raise RepairError("diagnostic row exceeds byte limit")
        if self._published_bytes + len(encoded) > self.limits.session_bytes:
            raise RepairError("diagnostic session exceeds byte limit")
        return encoded

    def publish(self, filename: str, payload: bytes, summary: Mapping[str, Any]) -> tuple[Path, Path]:
        if not filename or Path(filename).name != filename:
            raise RepairError("unsafe diagnostic filename")
        target_dir = self.session_dir
        target_dir.mkdir(parents=True, exist_ok=True)
        row_path = target_dir / filename
        summary_path = target_dir / "diagnostic-summary.json"
        new_total = self._published_bytes + len(payload)
        if new_total > self.limits.session_bytes:
            raise RepairError("diagnostic session exceeds byte limit")

        row_before = row_path.read_bytes() if row_path.exists() else None
        summary_before = summary_path.read_bytes() if summary_path.exists() else None
        summary_bytes = (json.dumps(summary, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
        try:
            _atomic_write(row_path, payload)
            _atomic_write(summary_path, summary_bytes)
        except Exception:
            _restore(row_path, row_before)
            _restore(summary_path, summary_before)
            raise
        self._published_bytes = new_total
        return row_path, summary_path


def validate_session_segment(value: str, max_chars: int = 64) -> str:
    if not isinstance(value, str):
        raise RepairError("session segment must be text")
    segment = value.strip()
    if not segment or segment in {".", ".."}:
        raise RepairError("unsafe session segment")
    if Path(segment).is_absolute() or "/" in segment or "\\" in segment:
        raise RepairError("session segment must be one relative component")
    if len(segment) > max_chars:
        raise RepairError("session segment too long")
    if segment.rstrip(" .").lower() in _RESERVED_WINDOWS_NAMES:
        raise RepairError("reserved session segment")
    if re.search(r"[\x00-\x1f<>:\"|?*]", segment):
        raise RepairError("invalid session segment characters")
    return segment


def _require_descendant(root: Path, candidate: Path) -> None:
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise RepairError("path escapes configured root") from exc
    if candidate == root:
        raise RepairError("session path must be a strict descendant")


def _atomic_write(path: Path, data: bytes) -> None:
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


@dataclass
class SyntheticMountState:
    owned_mount: str | None
    native_actions: tuple[str, ...]
    fields: dict[str, Any]
    movement_enabled: dict[str, bool]
    created_objects: list[str]


@dataclass
class MountConversionTransaction:
    state: SyntheticMountState
    _snapshot: SyntheticMountState | None = field(init=False, default=None)
    _prepared: bool = field(init=False, default=False)
    _committed: bool = field(init=False, default=False)

    def prepare(self) -> None:
        self._snapshot = SyntheticMountState(
            self.state.owned_mount,
            tuple(self.state.native_actions),
            dict(self.state.fields),
            dict(self.state.movement_enabled),
            list(self.state.created_objects),
        )
        self._prepared = True

    def commit(self, *, wolf_id: str, fail_after: str | None = None) -> None:
        if not self._prepared:
            raise RepairError("mount transaction not prepared")
        try:
            self.state.created_objects.append(f"seat:{wolf_id}")
            if fail_after == "create":
                raise RuntimeError("injected create failure")
            self.state.fields["mount_kind"] = "synthetic-wolf"
            if fail_after == "field":
                raise RuntimeError("injected field failure")
            self.state.movement_enabled = {key: False for key in self.state.movement_enabled}
            if fail_after == "movement":
                raise RuntimeError("injected movement failure")
            self.state.owned_mount = wolf_id
            if fail_after == "ownership":
                raise RuntimeError("injected ownership failure")
            self._committed = True
        except Exception:
            self.rollback()
            raise

    def rollback(self) -> None:
        if self._snapshot is None:
            return
        self.state.owned_mount = self._snapshot.owned_mount
        self.state.native_actions = tuple(self._snapshot.native_actions)
        self.state.fields = dict(self._snapshot.fields)
        self.state.movement_enabled = dict(self._snapshot.movement_enabled)
        self.state.created_objects = list(self._snapshot.created_objects)
        self._committed = False


@dataclass(frozen=True)
class CommandRegistration:
    owner_id: str
    command_id: str
    label: str


@dataclass(frozen=True)
class RegistrationToken:
    value: str
    owner_id: str
    command_id: str


@dataclass
class DialogueRegistryV2:
    api_version: int = 2
    _registrations: dict[tuple[str, str], RegistrationToken] = field(default_factory=dict)

    def register(self, registration: CommandRegistration) -> RegistrationToken:
        key = (registration.owner_id, registration.command_id)
        if not all((registration.owner_id, registration.command_id, registration.label)):
            raise RepairError("registration fields must be non-empty")
        if key in self._registrations:
            raise RepairError("duplicate owner/command registration")
        token = RegistrationToken(
            value=f"v2:{registration.owner_id}:{registration.command_id}",
            owner_id=registration.owner_id,
            command_id=registration.command_id,
        )
        self._registrations[key] = token
        return token

    def query(self, owner_id: str) -> tuple[RegistrationToken, ...]:
        return tuple(sorted(
            (token for (owner, _), token in self._registrations.items() if owner == owner_id),
            key=lambda token: token.command_id,
        ))

    def unregister(self, owner_id: str, token: RegistrationToken, *, fail: bool = False) -> bool:
        key = (owner_id, token.command_id)
        current = self._registrations.get(key)
        if current != token or token.owner_id != owner_id:
            raise RepairError("stale or cross-owner registration token")
        if fail:
            return False
        del self._registrations[key]
        return True


@dataclass
class RetryableCleanup:
    registry: DialogueRegistryV2
    owner_id: str
    token: RegistrationToken | None

    def attempt(self, *, fail: bool = False) -> bool:
        if self.token is None:
            return True
        removed = self.registry.unregister(self.owner_id, self.token, fail=fail)
        if removed:
            self.token = None
        return removed
