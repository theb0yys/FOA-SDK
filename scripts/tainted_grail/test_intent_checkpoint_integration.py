import tempfile
import unittest
from pathlib import Path

from semantic_repair.intent_checkpoint import checkpoint_publication_intent_journal_owned
from semantic_repair.journal import CrashRecoveryJournal
from semantic_repair.publication_intent import MultiFileIntentPublisher, PublicationTarget

FIXTURE = Path(__file__).parent / "fixtures" / "publication" / "intent-checkpoint-receipt.json"


class IntentCheckpointIntegrationTests(unittest.TestCase):
    def test_terminal_intent_is_bound_into_checkpoint_and_chain(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            journal = CrashRecoveryJournal(root / "intent.jsonl")
            lease = journal.acquire("writer")
            publisher = MultiFileIntentPublisher(root, journal)
            intent, _ = publisher._build_intent(
                lease,
                "publish-1",
                (PublicationTarget("a.txt", b"after"),),
            )
            journal.append_owned(
                lease,
                "publish-1",
                "intent-prepared",
                {"publication_intent": intent.to_dict(), "intent_sha256": intent.sha256},
            )
            journal.append_owned(
                lease,
                "publish-1",
                "intent-committed",
                {"intent_sha256": intent.sha256, "target_count": 1},
            )
            receipt = checkpoint_publication_intent_journal_owned(
                journal,
                lease,
                root / "proof.json",
                root / "chain.jsonl",
            )
            self.assertEqual(receipt.intents[0].intent_id, "publish-1")
            self.assertEqual(receipt.intents[0].status, "intent-committed")
            self.assertEqual(receipt.to_bytes(), FIXTURE.read_bytes())
            records, torn = journal.read_records(allow_torn_tail=True)
            self.assertFalse(torn)
            self.assertEqual(len(records), 1)
            self.assertEqual(records[0].phase, "compacted-checkpoint")


if __name__ == "__main__":
    unittest.main()
