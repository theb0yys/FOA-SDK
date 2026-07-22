"""Reusable engine-neutral repair package for Semantic Hook synthetic work."""

from .abandonment import (
    LockAbandonmentReview,
    build_lock_abandonment_review,
    write_lock_abandonment_review,
)
from .abandonment_lifecycle import (
    AbandonmentDecisionRevocation,
    AbandonmentDecisionSupersession,
    revoke_abandonment_decision,
    supersede_abandonment_decision,
)
from .abandonment_quorum import (
    LockAbandonmentDecision,
    QuorumReviewReference,
    build_lock_abandonment_decision,
    verify_abandonment_decision_against_lock,
)
from .checkpoint_archive import (
    CheckpointArchiveManifest,
    build_checkpoint_archive_manifest,
    verify_checkpoint_archive_manifest,
)
from .checkpoint_chain import (
    CheckpointChainEntry,
    append_checkpoint_proof,
    chain_sha256,
    read_checkpoint_chain,
    rotate_checkpoint_chain,
    verify_checkpoint_chain_contains_proof,
)
from .ci_matrix import (
    CrossVersionPolicyFixture,
    build_cross_version_policy_fixture,
    verify_cross_version_policy_fixtures,
)
from .ci_migration import (
    WorkflowPolicyReceiptV2,
    WorkflowReceiptMigrationFixture,
    build_workflow_receipt_migration_fixture,
    migrate_workflow_policy_receipt_v1_to_v2,
    verify_workflow_receipt_migration,
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
from .compaction_quota import (
    CompactionBackupLimits,
    CompactionQuotaLedger,
    CompactionQuotaReservation,
    reserve_compaction_backup_quota,
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
from .intent_checkpoint import (
    IntentCheckpointState,
    IntentJournalCheckpointReceipt,
    checkpoint_publication_intent_journal_owned,
)
from .intent_dependencies import (
    IntentDependencyGraph,
    IntentDependencyNode,
    build_intent_dependency_graph,
)
from .interrupted_compaction import (
    InterruptedCompactionReceipt,
    InterruptedCompactionState,
    compact_terminal_journal_resilient_owned,
    recover_interrupted_compaction_owned,
)
from .journal import CrashRecoveryJournal, JournalRecord
from .ownership import ExclusiveResourceLock, LeaseIdentity, ResourceLease
from .publication_intent import (
    IntentTargetRecord,
    MultiFileIntentPublisher,
    MultiFilePublicationReceipt,
    PublicationIntent,
    PublicationTarget,
)
from .publication_limits import (
    BoundedMultiFileIntentPublisher,
    PublicationBackupLimits,
)
from .recovery import RecoveryAction, RecoveryPlan, RecoveryStep, build_recovery_plan
from .replay_sequence import (
    ReplaySequenceEntry,
    ReplayWindowSequenceProof,
    build_replay_window_sequence_proof,
    verify_replay_window_sequence_proof,
)
from .replay_window import (
    SnapshotReplayWindowReceipt,
    apply_snapshot_with_replay_receipt,
    evaluate_snapshot_replay_window,
)
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
    "AbandonmentDecisionRevocation",
    "AbandonmentDecisionSupersession",
    "ApiHello",
    "BoundedMultiFileIntentPublisher",
    "CancellationRequested",
    "CancellationToken",
    "CheckpointArchiveManifest",
    "CheckpointChainEntry",
    "CheckpointProofError",
    "CheckpointTransactionState",
    "CommandRegistration",
    "CompactionBackupLimits",
    "CompactionQuotaLedger",
    "CompactionQuotaReservation",
    "CrashRecoveryJournal",
    "CrossVersionPolicyFixture",
    "DiagnosticLimits",
    "DiagnosticSession",
    "DialogueRegistryV2",
    "ExclusiveResourceLock",
    "IntentCheckpointState",
    "IntentDependencyGraph",
    "IntentDependencyNode",
    "IntentJournalCheckpointReceipt",
    "IntentTargetRecord",
    "InterruptedCompactionReceipt",
    "InterruptedCompactionState",
    "JournalCheckpointProof",
    "JournalCorruptionError",
    "JournalRecord",
    "LeaseIdentity",
    "LockAbandonmentDecision",
    "LockAbandonmentReview",
    "LockOwnershipError",
    "MappingTransaction",
    "MatrixAxis",
    "MatrixResult",
    "MountConversionTransaction",
    "MultiFileIntentPublisher",
    "MultiFilePublicationReceipt",
    "PublicationBackupLimits",
    "PublicationIntent",
    "PublicationIntentError",
    "PublicationReceipt",
    "PublicationState",
    "PublicationTarget",
    "QuorumReviewReference",
    "RecoveryAction",
    "RecoveryPlan",
    "RecoveryStep",
    "RegistrationToken",
    "RegistrySnapshot",
    "RepairError",
    "ReplaySequenceEntry",
    "ReplayWindowSequenceProof",
    "ResourceLease",
    "RetryableCleanup",
    "SimulatedCrash",
    "SnapshotReplayWindowReceipt",
    "StaleGenerationError",
    "StateTransitionError",
    "SyntheticMountState",
    "TransactionPhase",
    "TransactionStateMachine",
    "TypedFieldAdapter",
    "WorkflowPolicyError",
    "WorkflowPolicyReceipt",
    "WorkflowPolicyReceiptV2",
    "WorkflowReceiptMigrationFixture",
    "append_checkpoint_proof",
    "apply_snapshot_with_replay_receipt",
    "build_checkpoint_archive_manifest",
    "build_cross_version_policy_fixture",
    "build_intent_dependency_graph",
    "build_lock_abandonment_decision",
    "build_lock_abandonment_review",
    "build_offline_workflow_receipt",
    "build_recovery_plan",
    "build_replay_window_sequence_proof",
    "build_workflow_receipt_migration_fixture",
    "chain_sha256",
    "checkpoint_publication_intent_journal_owned",
    "compact_terminal_journal_owned",
    "compact_terminal_journal_resilient_owned",
    "evaluate_offline_workflow",
    "evaluate_snapshot_replay_window",
    "matrix_cases",
    "migrate_workflow_policy_receipt_v1_to_v2",
    "negotiate_api_version",
    "read_checkpoint_chain",
    "recover_interrupted_compaction_owned",
    "reserve_compaction_backup_quota",
    "revoke_abandonment_decision",
    "rotate_checkpoint_chain",
    "run_matrix",
    "supersede_abandonment_decision",
    "validate_offline_workflow",
    "validate_offline_workflow_file",
    "validate_session_segment",
    "verify_abandonment_decision_against_lock",
    "verify_checkpoint_archive_manifest",
    "verify_checkpoint_chain_contains_proof",
    "verify_checkpoint_proof",
    "verify_cross_version_policy_fixtures",
    "verify_replay_window_sequence_proof",
    "verify_workflow_receipt_migration",
    "write_lock_abandonment_review",
]
