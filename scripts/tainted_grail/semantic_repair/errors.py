"""Shared fail-closed exceptions for engine-neutral repair models."""


class RepairError(ValueError):
    """A validation, state, serialization, or recovery contract failed closed."""


class StateTransitionError(RepairError):
    """A transaction attempted an illegal state transition."""


class JournalCorruptionError(RepairError):
    """A recovery journal failed structural or hash-chain validation."""


class SimulatedCrash(RuntimeError):
    """Synthetic crash injected after a partial durable write."""
