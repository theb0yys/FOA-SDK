import json
import tempfile
import unittest
from pathlib import Path

from semantic_repair.compaction import (
    JournalCheckpointProof,
    compact_terminal_journal_owned,
    verify_checkpoint_proof,
)
from semantic_repair.errors import CheckpointProofError
from semantic_repair.journal import CrashRecoveryJournal


class JournalCompactionTests(unittest.TestCase):
    def test_terminal_journal_compacts_and_proof_verifies(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            journal = CrashRecoveryJournal(root / "journal.jsonl")
            lease = journal.acquire("owner")
            journal.append_owned(lease, "tx-a", "prepared", {})
            journal.append_owned(lease, "tx-a", "committed", {})
            journal.append_owned(lease, "tx-b", "cancel-requested", {})
            journal.append_owned(lease, "tx-b", "cancelled", {})
            proof = compact_terminal_journal_owned(journal, lease, root / "proof.json")
            self.assertTrue(verify_checkpoint_proof(journal.path, proof))
            self.assertEqual(
                JournalCheckpointProof.from_bytes((root / "proof.json").read_bytes()),
                proof,
            )
            records, torn = journal.read_records()
            self.assertFalse(torn)
            self.assertEqual(len(records), 1)
            self.assertEqual(records[0].phase, "compacted-checkpoint")
            self.assertEqual(proof.source_record_count, 4)

    def test_nonterminal_journal_is_not_compacted(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            journal = CrashRecoveryJournal(root / "journal.jsonl")
            lease = journal.acquire("owner")
            journal.append_owned(lease, "tx", "prepared", {})
            before = journal.path.read_bytes()
            with self.assertRaises(CheckpointProofError):
                compact_terminal_journal_owned(journal, lease, root / "proof.json")
            self.assertEqual(journal.path.read_bytes(), before)

    def test_tampered_checkpoint_proof_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            journal = CrashRecoveryJournal(root / "journal.jsonl")
            lease = journal.acquire("owner")
            journal.append_owned(lease, "tx", "committed", {})
            proof = compact_terminal_journal_owned(journal, lease, root / "proof.json")
            doc = json.loads(proof.to_bytes())
            doc["compacted_journal_sha256"] = "0" * 64
            tampered = (
                json.dumps(doc, sort_keys=True, separators=(",", ":")) + "\n"
            ).encode("utf-8")
            with self.assertRaises(CheckpointProofError):
                verify_checkpoint_proof(journal.path, tampered)


if __name__ == "__main__":
    unittest.main()
