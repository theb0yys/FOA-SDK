import tempfile
import unittest
from pathlib import Path

from semantic_repair.errors import PublicationIntentError
from semantic_repair.journal import CrashRecoveryJournal
from semantic_repair.publication_intent import PublicationTarget
from semantic_repair.publication_limits import (
    BoundedMultiFileIntentPublisher,
    PublicationBackupLimits,
)


class PublicationBackupLimitTests(unittest.TestCase):
    def test_per_target_total_and_count_limits(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            journal = CrashRecoveryJournal(root / "intent.jsonl")
            lease = journal.acquire("writer")
            (root / "a.txt").write_bytes(b"1234")
            (root / "b.txt").write_bytes(b"5678")
            publisher = BoundedMultiFileIntentPublisher(
                root,
                journal,
                PublicationBackupLimits(per_target_bytes=4, total_bytes=8, max_targets=2),
            )
            intent, _ = publisher._build_intent(
                lease,
                "ok",
                (PublicationTarget("a.txt", b"a"), PublicationTarget("b.txt", b"b")),
            )
            self.assertEqual(len(intent.targets), 2)
            (root / "c.txt").write_bytes(b"12345")
            with self.assertRaises(PublicationIntentError):
                publisher._build_intent(lease, "large", (PublicationTarget("c.txt", b"c"),))
            with self.assertRaises(PublicationIntentError):
                publisher._build_intent(
                    lease,
                    "many",
                    (
                        PublicationTarget("a.txt", b"a"),
                        PublicationTarget("b.txt", b"b"),
                        PublicationTarget("new.txt", b"n"),
                    ),
                )

    def test_invalid_limits_fail_closed(self):
        with self.assertRaises(PublicationIntentError):
            PublicationBackupLimits(per_target_bytes=5, total_bytes=4)


if __name__ == "__main__":
    unittest.main()
