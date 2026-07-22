"""Reusable engine-neutral repair package for Semantic Hook synthetic work."""

from .diagnostics import DiagnosticLimits, DiagnosticSession, validate_session_segment
from .dialogue_v2 import (
    API_VERSION,
    ApiHello,
    CommandRegistration,
    DialogueRegistryV2,
    RegistrationToken,
    RetryableCleanup,
    negotiate_api_version,
)
from .errors import (
    JournalCorruptionError,
    RepairError,
    SimulatedCrash,
    StateTransitionError,
)
from .failure_matrix import MatrixAxis, MatrixResult, matrix_cases, run_matrix
from .journal import CrashRecoveryJournal, JournalRecord
from .state_machine import TransactionPhase, TransactionStateMachine
from .transactions import (
    MappingTransaction,
    MountConversionTransaction,
    SyntheticMountState,
    TypedFieldAdapter,
)

__all__ = [
    "API_VERSION",
    "ApiHello",
    "CommandRegistration",
    "CrashRecoveryJournal",
    "DiagnosticLimits",
    "DiagnosticSession",
    "DialogueRegistryV2",
    "JournalCorruptionError",
    "JournalRecord",
    "MappingTransaction",
    "MatrixAxis",
    "MatrixResult",
    "MountConversionTransaction",
    "RegistrationToken",
    "RepairError",
    "RetryableCleanup",
    "SimulatedCrash",
    "StateTransitionError",
    "SyntheticMountState",
    "TransactionPhase",
    "TransactionStateMachine",
    "TypedFieldAdapter",
    "matrix_cases",
    "negotiate_api_version",
    "run_matrix",
    "validate_session_segment",
]
