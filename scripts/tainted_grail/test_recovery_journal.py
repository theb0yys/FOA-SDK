import json
import tempfile
import unittest
from pathlib import Path

from semantic_repair import (
    CrashRecoveryJournal,
    JournalCorruptionError,
    SimulatedCrash,
)


class CrashRecoveryJournalTests(unittest.TestCase):
    def test_hash_chain_and_latest_record(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = CrashRecoveryJournal(Path(tmp) / "recovery.jsonl")
            first = journal.append("tx-a", "prepared", {"value": 1})
            second = journal.append("tx-a", "committed", {"value": 2})
            journal.append("tx-b", "prepared", {})
            self.assertEqual(second.previous_hash, first.record_hash)
            self.assertEqual(journal.latest("tx-a"), second)
            self.assertEqual(len(journal.recover()), 3)

    def test_torn_final_record_is_removed_during_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "recovery.jsonl"
            journal = CrashRecoveryJournal(path)
            durable = journal.append("tx", "prepared", {})
            with self.assertRaises(SimulatedCrash):
                journal.append("tx", "committed", {}, crash_after_bytes=19)
            records, torn = journal.read_records(allow_torn_tail=True)
            self.assertEqual(records, [durable])
            self.assertTrue(torn)
            self.assertEqual(journal.recover(), (durable,))
            self.assertTrue(path.read_bytes().endswith(b"\n"))

    def test_append_after_torn_tail_recovers_before_writing(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "recovery.jsonl"
            journal = CrashRecoveryJournal(path)
            first = journal.append("tx", "prepared", {})
            with self.assertRaises(SimulatedCrash):
                journal.append("tx", "committing", {}, crash_after_bytes=11)
            second = journal.append("tx", "rolled-back", {})
            records = journal.recover()
            self.assertEqual(records, (first, second))
            self.assertEqual(second.previous_hash, first.record_hash)

    def test_interior_hash_corruption_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "recovery.jsonl"
            journal = CrashRecoveryJournal(path)
            journal.append("tx", "prepared", {"value": 1})
            doc = json.loads(path.read_text().strip())
            doc["payload"]["value"] = 9
            path.write_text(json.dumps(doc, sort_keys=True, separators=(",", ":")) + "\n")
            with self.assertRaises(JournalCorruptionError):
                journal.recover()


if __name__ == "__main__":
    unittest.main()
