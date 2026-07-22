"""Typed mapping and synthetic mount transactions with cancellation semantics."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Generic, Mapping, MutableMapping, TypeVar

from .errors import CancellationRequested, RepairError, StateTransitionError
from .journal import CrashRecoveryJournal
from .ownership import ResourceLease
from .state_machine import CancellationToken, TransactionPhase, TransactionStateMachine

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
    journal_lease: ResourceLease | None = None
    _original: Any = field(init=False, default=_UNSET)
    _machine: TransactionStateMachine = field(init=False)

    def __post_init__(self) -> None:
        txid = self.transaction_id or (
            f"mapping:{self.profile_id}:{self.type_id}:{self.adapter.field_name}"
        )
        self._machine = TransactionStateMachine(txid, self.journal, self.journal_lease)

    @property
    def phase(self) -> TransactionPhase:
        return self._machine.phase

    @property
    def history(self) -> tuple[TransactionPhase, ...]:
        return tuple(self._machine.history)

    @property
    def committed(self) -> bool:
        return self._machine.committed

    @property
    def cancelled(self) -> bool:
        return self._machine.cancelled

    def prepare(self, cancellation: CancellationToken | None = None) -> None:
        try:
            if cancellation is not None:
                cancellation.checkpoint()
            self._machine.transition(TransactionPhase.PREPARING)
            self._original = self.adapter.read(
                self.record,
                profile_id=self.profile_id,
                type_id=self.type_id,
            )
            self.adapter.validate_value(self.proposed)
            if cancellation is not None:
                cancellation.checkpoint()
        except CancellationRequested as exc:
            self._cancel_and_restore(str(exc))
            raise
        except Exception as exc:
            self._machine.mark_failed(exc)
            raise
        self._machine.transition(
            TransactionPhase.PREPARED,
            payload={"field": self.adapter.field_name},
        )

    def commit(
        self,
        after_write: Callable[[], None] | None = None,
        *,
        cancellation: CancellationToken | None = None,
    ) -> None:
        try:
            if cancellation is not None:
                cancellation.checkpoint()
            self._machine.transition(TransactionPhase.COMMITTING)
            self.record[self.adapter.field_name] = self.proposed
            if after_write is not None:
                after_write()
            if cancellation is not None:
                cancellation.checkpoint()
        except CancellationRequested as exc:
            self._cancel_and_restore(str(exc))
            raise
        except Exception as exc:
            self._machine.mark_failed(exc)
            self.rollback()
            raise
        self._machine.transition(TransactionPhase.COMMITTED)

    def cancel(self, reason: str = "cancelled") -> None:
        if self._machine.phase in {
            TransactionPhase.COMMITTED,
            TransactionPhase.ROLLED_BACK,
            TransactionPhase.CANCELLED,
        }:
            raise StateTransitionError(f"cannot cancel transaction in {self._machine.phase.value}")
        self._cancel_and_restore(reason)

    def _cancel_and_restore(self, reason: str) -> None:
        self._machine.request_cancel(reason)
        self._machine.transition(TransactionPhase.CANCELLING)
        if self._original is not _UNSET:
            self.record[self.adapter.field_name] = self._original
        self._machine.transition(TransactionPhase.CANCELLED)

    def rollback(self) -> None:
        if self._machine.rolled_back or self._machine.cancelled:
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
    journal_lease: ResourceLease | None = None
    _snapshot: SyntheticMountState | None = field(init=False, default=None)
    _machine: TransactionStateMachine = field(init=False)

    def __post_init__(self) -> None:
        self._machine = TransactionStateMachine(
            self.transaction_id, self.journal, self.journal_lease
        )

    @property
    def phase(self) -> TransactionPhase:
        return self._machine.phase

    @property
    def history(self) -> tuple[TransactionPhase, ...]:
        return tuple(self._machine.history)

    @property
    def cancelled(self) -> bool:
        return self._machine.cancelled

    def prepare(self, cancellation: CancellationToken | None = None) -> None:
        try:
            if cancellation is not None:
                cancellation.checkpoint()
            self._machine.transition(TransactionPhase.PREPARING)
            self._snapshot = self.state.snapshot()
            if cancellation is not None:
                cancellation.checkpoint()
        except CancellationRequested as exc:
            self._cancel_and_restore(str(exc))
            raise
        self._machine.transition(
            TransactionPhase.PREPARED,
            payload={
                "owned_mount_is_null": self._snapshot.owned_mount is None,
                "native_action_count": len(self._snapshot.native_actions),
                "movement_component_count": len(self._snapshot.movement_enabled),
            },
        )

    def commit(
        self,
        *,
        wolf_id: str,
        fail_after: str | None = None,
        cancellation: CancellationToken | None = None,
        after_stage: Callable[[str], None] | None = None,
    ) -> None:
        if not wolf_id:
            raise RepairError("wolf_id must be non-empty")
        try:
            if cancellation is not None:
                cancellation.checkpoint()
            self._machine.transition(TransactionPhase.COMMITTING)
            self.state.created_objects.append(f"seat:{wolf_id}")
            self._stage("create", fail_after, cancellation, after_stage)
            self.state.fields["mount_kind"] = "synthetic-wolf"
            self._stage("field", fail_after, cancellation, after_stage)
            self.state.movement_enabled = {
                key: False for key in self.state.movement_enabled
            }
            self._stage("movement", fail_after, cancellation, after_stage)
            self.state.owned_mount = wolf_id
            self._stage("ownership", fail_after, cancellation, after_stage)
        except CancellationRequested as exc:
            self._cancel_and_restore(str(exc))
            raise
        except Exception as exc:
            self._machine.mark_failed(exc)
            self.rollback()
            raise
        self._machine.transition(TransactionPhase.COMMITTED)

    @staticmethod
    def _stage(
        stage: str,
        fail_after: str | None,
        cancellation: CancellationToken | None,
        after_stage: Callable[[str], None] | None,
    ) -> None:
        if after_stage is not None:
            after_stage(stage)
        if cancellation is not None:
            cancellation.checkpoint()
        if fail_after == stage:
            raise RuntimeError(f"injected {stage} failure")

    def cancel(self, reason: str = "cancelled") -> None:
        if self._machine.phase in {
            TransactionPhase.COMMITTED,
            TransactionPhase.ROLLED_BACK,
            TransactionPhase.CANCELLED,
        }:
            raise StateTransitionError(f"cannot cancel transaction in {self._machine.phase.value}")
        self._cancel_and_restore(reason)

    def _cancel_and_restore(self, reason: str) -> None:
        self._machine.request_cancel(reason)
        self._machine.transition(TransactionPhase.CANCELLING)
        self._restore_snapshot()
        self._machine.transition(TransactionPhase.CANCELLED)

    def _restore_snapshot(self) -> None:
        if self._snapshot is not None:
            restored = self._snapshot.snapshot()
            self.state.owned_mount = restored.owned_mount
            self.state.native_actions = restored.native_actions
            self.state.fields = restored.fields
            self.state.movement_enabled = restored.movement_enabled
            self.state.created_objects = restored.created_objects

    def rollback(self) -> None:
        if self._machine.rolled_back or self._machine.cancelled:
            return
        if self._machine.phase is TransactionPhase.NEW:
            return
        self._machine.transition(TransactionPhase.ROLLING_BACK)
        self._restore_snapshot()
        self._machine.transition(TransactionPhase.ROLLED_BACK)
