"""Explicit transaction states shared by all synthetic repair primitives."""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any

from .errors import StateTransitionError
from .journal import CrashRecoveryJournal


class TransactionPhase(str, Enum):
    NEW = "new"
    PREPARING = "preparing"
    PREPARED = "prepared"
    COMMITTING = "committing"
    COMMITTED = "committed"
    FAILED = "failed"
    ROLLING_BACK = "rolling-back"
    ROLLED_BACK = "rolled-back"


_ALLOWED: dict[TransactionPhase, frozenset[TransactionPhase]] = {
    TransactionPhase.NEW: frozenset({TransactionPhase.PREPARING}),
    TransactionPhase.PREPARING: frozenset({TransactionPhase.PREPARED, TransactionPhase.FAILED}),
    TransactionPhase.PREPARED: frozenset({TransactionPhase.COMMITTING, TransactionPhase.ROLLING_BACK}),
    TransactionPhase.COMMITTING: frozenset({TransactionPhase.COMMITTED, TransactionPhase.FAILED}),
    TransactionPhase.COMMITTED: frozenset({TransactionPhase.ROLLING_BACK}),
    TransactionPhase.FAILED: frozenset({TransactionPhase.ROLLING_BACK}),
    TransactionPhase.ROLLING_BACK: frozenset({TransactionPhase.ROLLED_BACK, TransactionPhase.FAILED}),
    TransactionPhase.ROLLED_BACK: frozenset(),
}


@dataclass
class TransactionStateMachine:
    transaction_id: str
    journal: CrashRecoveryJournal | None = None
    phase: TransactionPhase = field(init=False, default=TransactionPhase.NEW)
    history: list[TransactionPhase] = field(init=False, default_factory=lambda: [TransactionPhase.NEW])

    def transition(
        self,
        target: TransactionPhase,
        *,
        payload: dict[str, Any] | None = None,
    ) -> None:
        if target not in _ALLOWED[self.phase]:
            raise StateTransitionError(f"illegal transaction transition: {self.phase.value} -> {target.value}")
        self.phase = target
        self.history.append(target)
        if self.journal is not None:
            self.journal.append(self.transaction_id, target.value, payload)

    def mark_failed(self, exc: BaseException) -> None:
        if TransactionPhase.FAILED in _ALLOWED[self.phase]:
            self.transition(
                TransactionPhase.FAILED,
                payload={"error_type": type(exc).__name__, "error": str(exc)},
            )

    @property
    def committed(self) -> bool:
        return self.phase is TransactionPhase.COMMITTED

    @property
    def rolled_back(self) -> bool:
        return self.phase is TransactionPhase.ROLLED_BACK
