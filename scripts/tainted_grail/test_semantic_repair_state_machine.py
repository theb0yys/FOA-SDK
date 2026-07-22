import tempfile
import unittest
from pathlib import Path

from semantic_repair import (
    CrashRecoveryJournal,
    MappingTransaction,
    StateTransitionError,
    TransactionPhase,
    TransactionStateMachine,
    TypedFieldAdapter,
)


class ExplicitStateMachineTests(unittest.TestCase):
    def test_illegal_transition_fails_closed(self):
        machine = TransactionStateMachine("tx")
        with self.assertRaises(StateTransitionError):
            machine.transition(TransactionPhase.COMMITTING)
        self.assertEqual(machine.phase, TransactionPhase.NEW)

    def test_mapping_transaction_records_complete_state_history(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = CrashRecoveryJournal(Path(tmp) / "journal.jsonl")
            record = {"quantity": 2}
            tx = MappingTransaction(
                TypedFieldAdapter("p", "item", "quantity", int),
                record,
                "p",
                "item",
                5,
                transaction_id="quantity-1",
                journal=journal,
            )
            tx.prepare()
            tx.commit()
            tx.rollback()
            self.assertEqual(record["quantity"], 2)
            self.assertEqual(
                tx.history,
                (
                    TransactionPhase.NEW,
                    TransactionPhase.PREPARING,
                    TransactionPhase.PREPARED,
                    TransactionPhase.COMMITTING,
                    TransactionPhase.COMMITTED,
                    TransactionPhase.ROLLING_BACK,
                    TransactionPhase.ROLLED_BACK,
                ),
            )
            phases = tuple(item.phase for item in journal.recover())
            self.assertEqual(
                phases,
                (
                    "preparing",
                    "prepared",
                    "committing",
                    "committed",
                    "rolling-back",
                    "rolled-back",
                ),
            )

    def test_original_none_is_restored(self):
        record = {"value": None}
        adapter = TypedFieldAdapter("p", "nullable", "value", type(None))
        tx = MappingTransaction(adapter, record, "p", "nullable", None)
        tx.prepare()
        tx.commit()
        tx.rollback()
        self.assertIsNone(record["value"])


if __name__ == "__main__":
    unittest.main()
