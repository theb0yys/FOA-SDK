import tempfile
import unittest
from pathlib import Path

from semantic_repair.checkpoint_chain import (
    append_checkpoint_proof,
    read_checkpoint_chain,
    rotate_checkpoint_chain,
)
from semantic_repair.interrupted_compaction import _prepare_compaction
from semantic_repair.journal import CrashRecoveryJournal

FIXTURE = Path(__file__).parent / "fixtures" / "compaction" / "checkpoint-chain-rotation-anchor.json"


class CheckpointChainRotationTests(unittest.TestCase):
    def test_rotation_anchor_binds_prior_chain(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            chain = root / "chain.jsonl"
            for index in range(3):
                journal = CrashRecoveryJournal(root / f"journal-{index}.jsonl")
                lease = journal.acquire(f"writer-{index}")
                journal.append_owned(lease, f"tx-{index}", "committed", {})
                _, _, proof = _prepare_compaction(journal)
                append_checkpoint_proof(chain, proof)
            before = chain.read_bytes()
            anchor = rotate_checkpoint_chain(chain, max_entries=2)
            self.assertIsNotNone(anchor)
            entries = read_checkpoint_chain(chain)
            self.assertEqual(len(entries), 1)
            self.assertEqual(entries[0].kind, "rotation-anchor")
            self.assertEqual(entries[0].payload["rotated_entry_count"], 3)
            self.assertNotEqual(chain.read_bytes(), before)
            self.assertEqual(anchor.to_bytes(), FIXTURE.read_bytes())

    def test_invalid_rotation_limit_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                rotate_checkpoint_chain(Path(tmp) / "chain", max_entries=0)


if __name__ == "__main__":
    unittest.main()
