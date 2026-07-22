"""Shared fail-closed exceptions for engine-neutral repair models."""


class RepairError(ValueError):
    """A validation, state, serialization, or recovery contract failed closed."""


class StateTransitionError(RepairError):
    """A transaction attempted an illegal state transition."""


class JournalCorruptionError(RepairError):
    """A recovery journal failed structural or hash-chain validation."""


class LockOwnershipError(RepairError):
    """A caller does not own the active synthetic resource lease."""


class StaleGenerationError(RepairError):
    """A caller attempted to use an obsolete synthetic generation."""


class CancellationRequested(RepairError):
    """A synthetic transaction observed an explicit cancellation request."""


class WorkflowPolicyError(RepairError):
    """The offline CI workflow exceeded its read-only authority boundary."""


class CheckpointProofError(RepairError):
    """A journal checkpoint or compaction proof failed verification."""


class PublicationIntentError(RepairError):
    """A multi-file publication intent cannot be proven or safely recovered."""


class SimulatedCrash(RuntimeError):
    """Synthetic crash injected after a partial durable write."""
