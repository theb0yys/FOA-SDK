"""Typed mapping and synthetic mount transactions with explicit state machines."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Generic, Mapping, MutableMapping, TypeVar

from .errors import RepairError
from .journal import CrashRecoveryJournal
from .state_machine import TransactionPhase, TransactionStateMachine

T = TypeVar("T")
_UNSET = object()


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
    transaction_id: str | None = None
    journal: CrashRecoveryJournal | None = None
    _original: Any = field(init=False, default=_UNSET)
    _machine: TransactionStateMachine = field(init=False)

    def __post_init__(self) -> None:
        txid = self.transaction_id or (
            f"mapping:{self.profile_id}:{self.type_id}:{self.adapter.field_name}"
        )
        self._machine = TransactionStateMachine(txid, self.journal)

    @property
    def phase(self) -> TransactionPhase:
        return self._machine.phase

    @property
    def history(self) -> tuple[TransactionPhase, ...]:
        return tuple(self._machine.history)

    @property
    def committed(self) -> bool:
        return self._machine.committed

    def prepare(self) -> None:
        self._machine.transition(TransactionPhase.PREPARING)
        try:
            self._original = self.adapter.read(
                self.record,
                profile_id=self.profile_id,
                type_id=self.type_id,
            )
            self.adapter.validate_value(self.proposed)
        except Exception as exc:
            self._machine.mark_failed(exc)
            raise
        self._machine.transition(
            TransactionPhase.PREPARED,
            payload={"field": self.adapter.field_name},
        )

    def commit(self, after_write: Callable[[], None] | None = None) -> None:
        self._machine.transition(TransactionPhase.COMMITTING)
        try:
            self.record[self.adapter.field_name] = self.proposed
            if after_write is not None:
                after_write()
        except Exception as exc:
            self._machine.mark_failed(exc)
            self.rollback()
            raise
        self._machine.transition(TransactionPhase.COMMITTED)

    def rollback(self) -> None:
        if self._machine.rolled_back:
            return
        if self._machine.phase is TransactionPhase.NEW:
            return
        self._machine.transition(TransactionPhase.ROLLING_BACK)
        try:
            if self._original is not _UNSET:
                self.record[self.adapter.field_name] = self._original
        except Exception as exc:
            self._machine.mark_failed(exc)
            raise
        self._machine.transition(TransactionPhase.ROLLED_BACK)


@dataclass
class SyntheticMountState:
    owned_mount: str | None
    native_actions: tuple[str, ...]
    fields: dict[str, Any]
    movement_enabled: dict[str, bool]
    created_objects: list[str]

    def snapshot(self) -> "SyntheticMountState":
        return SyntheticMountState(
            self.owned_mount,
            tuple(self.native_actions),
            dict(self.fields),
            dict(self.movement_enabled),
            list(self.created_objects),
        )


@dataclass
class MountConversionTransaction:
    state: SyntheticMountState
    transaction_id: str = "synthetic-mount-conversion"
    journal: CrashRecoveryJournal | None = None
    _snapshot: SyntheticMountState | None = field(init=False, default=None)
    _machine: TransactionStateMachine = field(init=False)

    def __post_init__(self) -> None:
        self._machine = TransactionStateMachine(self.transaction_id, self.journal)

    @property
    def phase(self) -> TransactionPhase:
        return self._machine.phase

    @property
    def history(self) -> tuple[TransactionPhase, ...]:
        return tuple(self._machine.history)

    def prepare(self) -> None:
        self._machine.transition(TransactionPhase.PREPARING)
        self._snapshot = self.state.snapshot()
        self._machine.transition(
            TransactionPhase.PREPARED,
            payload={
                "owned_mount_is_null": self._snapshot.owned_mount is None,
                "native_action_count": len(self._snapshot.native_actions),
                "movement_component_count": len(self._snapshot.movement_enabled),
            },
        )

    def commit(self, *, wolf_id: str, fail_after: str | None = None) -> None:
        if not wolf_id:
            raise RepairError("wolf_id must be non-empty")
        self._machine.transition(TransactionPhase.COMMITTING)
        try:
            self.state.created_objects.append(f"seat:{wolf_id}")
            if fail_after == "create":
                raise RuntimeError("injected create failure")
            self.state.fields["mount_kind"] = "synthetic-wolf"
            if fail_after == "field":
                raise RuntimeError("injected field failure")
            self.state.movement_enabled = {
                key: False for key in self.state.movement_enabled
            }
            if fail_after == "movement":
                raise RuntimeError("injected movement failure")
            self.state.owned_mount = wolf_id
            if fail_after == "ownership":
                raise RuntimeError("injected ownership failure")
        except Exception as exc:
            self._machine.mark_failed(exc)
            self.rollback()
            raise
        self._machine.transition(TransactionPhase.COMMITTED)

    def rollback(self) -> None:
        if self._machine.rolled_back:
            return
        if self._machine.phase is TransactionPhase.NEW:
            return
        self._machine.transition(TransactionPhase.ROLLING_BACK)
        if self._snapshot is not None:
            restored = self._snapshot.snapshot()
            self.state.owned_mount = restored.owned_mount
            self.state.native_actions = restored.native_actions
            self.state.fields = restored.fields
            self.state.movement_enabled = restored.movement_enabled
            self.state.created_objects = restored.created_objects
        self._machine.transition(TransactionPhase.ROLLED_BACK)
