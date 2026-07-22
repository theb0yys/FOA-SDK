import tempfile
import unittest
from pathlib import Path

from semantic_repair import (
    CrashRecoveryJournal,
    LockOwnershipError,
    MappingTransaction,
    RecoveryAction,
    SimulatedCrash,
    TypedFieldAdapter,
)


class JournalOwnershipTests(unittest.TestCase):
    def test_lock_contention_generations_and_stale_lease(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = CrashRecoveryJournal(Path(tmp) / "recovery.jsonl")
            lease_a = journal.acquire("writer-a", expected_generation=1)
            with self.assertRaises(LockOwnershipError):
                journal.acquire("writer-b", expected_generation=2)
            first = journal.append_owned(lease_a, "tx", "preparing")
            self.assertEqual(first.payload["journal_owner_id"], "writer-a")
            journal.lock.release(lease_a)

            lease_b = journal.acquire("writer-b", expected_generation=2)
            with self.assertRaises(LockOwnershipError):
                journal.append_owned(lease_a, "tx", "prepared")
            second = journal.append_owned(lease_b, "other", "committed")
            self.assertEqual(second.payload["journal_lock_generation"], 2)
            journal.lock.release(lease_b)

    def test_state_machine_writes_owned_records_without_partial_transition(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = CrashRecoveryJournal(Path(tmp) / "owned.jsonl")
            lease = journal.acquire("transaction-writer", expected_generation=1)
            record = {"quantity": 2}
            tx = MappingTransaction(
                TypedFieldAdapter("p", "item", "quantity", int),
                record,
                "p",
                "item",
                3,
                journal=journal,
                journal_lease=lease,
            )
            tx.prepare()
            tx.commit()
            records = journal.read_records()[0]
            self.assertTrue(records)
            self.assertTrue(
                all(
                    item.payload["journal_owner_id"] == "transaction-writer"
                    for item in records
                )
            )
            journal.lock.release(lease)

    def test_recovery_plan_is_deterministic_and_torn_tail_is_first(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = CrashRecoveryJournal(Path(tmp) / "recovery.jsonl")
            lease = journal.acquire("planner", expected_generation=1)
            journal.append_owned(lease, "tx-b", "preparing")
            journal.append_owned(lease, "tx-b", "prepared")
            journal.append_owned(lease, "tx-a", "preparing")
            journal.append_owned(lease, "tx-a", "prepared")
            journal.append_owned(lease, "tx-a", "committing")
            journal.append_owned(lease, "tx-a", "committed")
            with self.assertRaises(SimulatedCrash):
                journal.append_owned(
                    lease,
                    "tx-c",
                    "preparing",
                    crash_after_bytes=17,
                )

            plan_a = journal.recovery_plan()
            plan_b = journal.recovery_plan()
            self.assertEqual(plan_a.to_bytes(), plan_b.to_bytes())
            self.assertEqual(plan_a.steps[0].action, RecoveryAction.TRUNCATE_TORN_TAIL)
            by_tx = {step.transaction_id: step for step in plan_a.steps}
            self.assertEqual(by_tx["tx-a"].action, RecoveryAction.NONE)
            self.assertEqual(by_tx["tx-b"].action, RecoveryAction.ROLLBACK)
            self.assertGreater(plan_a.torn_tail_bytes, 0)

            journal.recover_owned(lease)
            self.assertEqual(journal.recovery_plan().torn_tail_bytes, 0)
            journal.lock.release(lease)

    def test_owner_change_within_transaction_requires_manual_review(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = CrashRecoveryJournal(Path(tmp) / "recovery.jsonl")
            lease_a = journal.acquire("writer-a", expected_generation=1)
            journal.append_owned(lease_a, "tx", "preparing")
            journal.lock.release(lease_a)
            lease_b = journal.acquire("writer-b", expected_generation=2)
            journal.append_owned(lease_b, "tx", "prepared")
            plan = journal.recovery_plan()
            step = next(step for step in plan.steps if step.transaction_id == "tx")
            self.assertEqual(step.action, RecoveryAction.MANUAL_REVIEW)
            journal.lock.release(lease_b)


if __name__ == "__main__":
    unittest.main()
