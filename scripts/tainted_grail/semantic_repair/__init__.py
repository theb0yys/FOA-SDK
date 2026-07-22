"""Reusable engine-neutral repair package for Semantic Hook synthetic work."""

from .abandonment import (
    LockAbandonmentReview,
    build_lock_abandonment_review,
    write_lock_abandonment_review,
)
from .ci_policy import (
    WorkflowPolicyReceipt,
    build_offline_workflow_receipt,
    evaluate_offline_workflow,
    validate_offline_workflow,
    validate_offline_workflow_file,
)
from .compaction import (
    CheckpointTransactionState,
    JournalCheckpointProof,
    compact_terminal_journal_owned,
    verify_checkpoint_proof,
)
from .diagnostics import (
    DiagnosticLimits,
    DiagnosticSession,
    PublicationReceipt,
    PublicationState,
    validate_session_segment,
)
from .dialogue_v2 import (
    API_VERSION,
    DEFAULT_MAX_GENERATION_GAP,
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
    CheckpointProofError,
    JournalCorruptionError,
    LockOwnershipError,
    PublicationIntentError,
    RepairError,
    SimulatedCrash,
    StaleGenerationError,
    StateTransitionError,
    WorkflowPolicyError,
)
from .failure_matrix import MatrixAxis, MatrixResult, matrix_cases, run_matrix
from .journal import CrashRecoveryJournal, JournalRecord
from .ownership import ExclusiveResourceLock, LeaseIdentity, ResourceLease
from .publication_intent import (
    IntentTargetRecord,
    MultiFileIntentPublisher,
    MultiFilePublicationReceipt,
    PublicationIntent,
    PublicationTarget,
)
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
    "DEFAULT_MAX_GENERATION_GAP",
    "ApiHello",
    "CancellationRequested",
    "CancellationToken",
    "CheckpointProofError",
    "CheckpointTransactionState",
    "CommandRegistration",
    "CrashRecoveryJournal",
    "DiagnosticLimits",
    "DiagnosticSession",
    "DialogueRegistryV2",
    "ExclusiveResourceLock",
    "IntentTargetRecord",
    "JournalCheckpointProof",
    "JournalCorruptionError",
    "JournalRecord",
    "LeaseIdentity",
    "LockAbandonmentReview",
    "LockOwnershipError",
    "MappingTransaction",
    "MatrixAxis",
    "MatrixResult",
    "MountConversionTransaction",
    "MultiFileIntentPublisher",
    "MultiFilePublicationReceipt",
    "PublicationIntent",
    "PublicationIntentError",
    "PublicationReceipt",
    "PublicationState",
    "PublicationTarget",
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
    "WorkflowPolicyReceipt",
    "build_lock_abandonment_review",
    "build_offline_workflow_receipt",
    "build_recovery_plan",
    "compact_terminal_journal_owned",
    "evaluate_offline_workflow",
    "matrix_cases",
    "negotiate_api_version",
    "run_matrix",
    "validate_offline_workflow",
    "validate_offline_workflow_file",
    "validate_session_segment",
    "verify_checkpoint_proof",
    "write_lock_abandonment_review",
]
