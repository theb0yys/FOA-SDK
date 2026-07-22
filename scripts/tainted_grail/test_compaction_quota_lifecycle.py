import hashlib
import unittest
from dataclasses import replace
from pathlib import Path

from semantic_repair.compaction_quota import (
    CompactionBackupLimits,
    CompactionQuotaLedger,
    CompactionQuotaReservation,
)
from semantic_repair.errors import CheckpointProofError
from semantic_repair.quota_lifecycle import (
    CompactionQuotaLedgerCompactionRecord,
    CompactionQuotaReleaseRecord,
    build_compaction_quota_release_record,
    compact_compaction_quota_ledger,
    verify_compaction_quota_ledger_compaction,
)

FIXTURES = Path(__file__).parent / "fixtures" / "compaction"


def hx(value):
    return hashlib.sha256(value.encode()).hexdigest()


def source_ledger():
    limits = CompactionBackupLimits(
        max_operations=5,
        max_total_backup_bytes=1000,
        max_single_operation_bytes=500,
    )
    return CompactionQuotaLedger(
        limits,
        (
            CompactionQuotaReservation("op-a", 1, 100, 100, hx("op-a")),
            CompactionQuotaReservation("op-b", 2, 200, 300, hx("op-b")),
            CompactionQuotaReservation("op-c", 3, 300, 600, hx("op-c")),
        ),
    )


class CompactionQuotaLifecycleTests(unittest.TestCase):
    def test_release_chain_and_compaction_match_goldens(self):
        ledger = source_ledger()
        first = build_compaction_quota_release_record(
            ledger,
            "op-a",
            release_index=1,
            reason="recovery material verified",
        )
        second = build_compaction_quota_release_record(
            ledger,
            "op-b",
            release_index=2,
            reason="retention window closed",
            previous_release=first,
        )
        compacted, record = compact_compaction_quota_ledger(
            ledger,
            (first, second),
        )
        self.assertEqual(first.to_bytes(), (FIXTURES / "quota-release-001.json").read_bytes())
        self.assertEqual(second.to_bytes(), (FIXTURES / "quota-release-002.json").read_bytes())
        self.assertEqual(compacted.to_bytes(), (FIXTURES / "quota-ledger-compacted.json").read_bytes())
        self.assertEqual(record.to_bytes(), (FIXTURES / "quota-ledger-compaction-record.json").read_bytes())
        self.assertEqual(CompactionQuotaReleaseRecord.from_bytes(first.to_bytes()), first)
        self.assertEqual(CompactionQuotaLedgerCompactionRecord.from_bytes(record.to_bytes()), record)
        self.assertTrue(verify_compaction_quota_ledger_compaction(ledger, (first, second), compacted, record))
        self.assertEqual(record.to_dict()["effect"], "ledger-compacted-provenance-retained")

    def test_duplicate_or_tampered_release_fails_closed(self):
        ledger = source_ledger()
        first = build_compaction_quota_release_record(
            ledger,
            "op-a",
            release_index=1,
            reason="done",
        )
        with self.assertRaises(CheckpointProofError):
            compact_compaction_quota_ledger(ledger, (first, first))
        tampered = replace(first, released_backup_bytes=101)
        with self.assertRaises(CheckpointProofError):
            compact_compaction_quota_ledger(ledger, (tampered,))


if __name__ == "__main__":
    unittest.main()
