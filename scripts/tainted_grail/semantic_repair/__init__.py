"""Reusable engine-neutral repair package for Semantic Hook synthetic work."""

from .ci_policy import validate_offline_workflow, validate_offline_workflow_file
from .diagnostics import (
    DiagnosticLimits,
    DiagnosticSession,
    PublicationReceipt,
    PublicationState,
    validate_session_segment,
)
from .dialogue_v2 import (
    API_VERSION,
    ApiHello,
    CommandRegistration,
    DialogueRegistryV2,
    RegistrationToken,
    RegistrySnapshot,
    RetryableCleanup,
    negotiate_api_version,
)
from .errors import (
    CancellationRequested,
    JournalCorruptionError,
    LockOwnershipError,
    RepairError,
    SimulatedCrash,
    StaleGenerationError,
    StateTransitionError,
    WorkflowPolicyError,
)
from .failure_matrix import MatrixAxis, MatrixResult, matrix_cases, run_matrix
from .journal import CrashRecoveryJournal, JournalRecord
from .ownership import ExclusiveResourceLock, LeaseIdentity, ResourceLease
from .recovery import RecoveryAction, RecoveryPlan, RecoveryStep, build_recovery_plan
from .state_machine import CancellationToken, TransactionPhase, TransactionStateMachine
from .transactions import (
    MappingTransaction,
    MountConversionTransaction,
    SyntheticMountState,
    TypedFieldAdapter,
)

__all__ = [
    "API_VERSION",
    "ApiHello",
    "CancellationRequested",
    "CancellationToken",
    "CommandRegistration",
    "CrashRecoveryJournal",
    "DiagnosticLimits",
    "DiagnosticSession",
    "DialogueRegistryV2",
    "ExclusiveResourceLock",
    "JournalCorruptionError",
    "JournalRecord",
    "LeaseIdentity",
    "LockOwnershipError",
    "MappingTransaction",
    "MatrixAxis",
    "MatrixResult",
    "MountConversionTransaction",
    "PublicationReceipt",
    "PublicationState",
    "RecoveryAction",
    "RecoveryPlan",
    "RecoveryStep",
    "RegistrationToken",
    "RegistrySnapshot",
    "RepairError",
    "ResourceLease",
    "RetryableCleanup",
    "SimulatedCrash",
    "StaleGenerationError",
    "StateTransitionError",
    "SyntheticMountState",
    "TransactionPhase",
    "TransactionStateMachine",
    "TypedFieldAdapter",
    "WorkflowPolicyError",
    "build_recovery_plan",
    "matrix_cases",
    "negotiate_api_version",
    "run_matrix",
    "validate_offline_workflow",
    "validate_offline_workflow_file",
    "validate_session_segment",
]
