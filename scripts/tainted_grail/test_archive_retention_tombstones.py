import hashlib
import unittest
from pathlib import Path

from semantic_repair.archive_retention import (
    ArchiveRetentionPolicy,
    CheckpointArchiveTombstoneManifest,
    build_checkpoint_archive_retention_plan,
    build_checkpoint_archive_tombstone,
    verify_checkpoint_archive_tombstone,
)
from semantic_repair.checkpoint_archive import CheckpointArchiveManifest
from semantic_repair.errors import CheckpointProofError

FIXTURES = Path(__file__).parent / "fixtures" / "checkpoint_archive"
ZERO = "0" * 64


def hx(value):
    return hashlib.sha256(value.encode()).hexdigest()


def manifests():
    result = []
    previous = ZERO
    for index in range(1, 6):
        item = CheckpointArchiveManifest(
            f"archive-{index:03d}",
            index,
            f"checkpoint-{index:03d}.jsonl",
            hx(f"archive-bytes-{index}"),
            index + 1,
            hx(f"first-{index}"),
            hx(f"final-{index}"),
            hx(f"anchor-{index}"),
            previous,
        )
        result.append(item)
        previous = item.sha256
    return tuple(result)


class ArchiveRetentionTests(unittest.TestCase):
    def test_retention_plan_and_tombstones_match_goldens(self):
        source = manifests()
        plan = build_checkpoint_archive_retention_plan(
            source,
            policy=ArchiveRetentionPolicy(2),
        )
        self.assertEqual(
            plan.to_bytes(),
            (FIXTURES / "checkpoint-archive-retention-plan.json").read_bytes(),
        )
        from semantic_repair.archive_retention import CheckpointArchiveRetentionPlan
        self.assertEqual(CheckpointArchiveRetentionPlan.from_bytes(plan.to_bytes()), plan)
        first = build_checkpoint_archive_tombstone(
            source[0],
            tombstone_index=1,
            reason="retention-window-exceeded",
        )
        second = build_checkpoint_archive_tombstone(
            source[1],
            tombstone_index=2,
            reason="retention-window-exceeded",
            previous_tombstone=first,
        )
        self.assertEqual(
            first.to_bytes(),
            (FIXTURES / "checkpoint-archive-tombstone-001.json").read_bytes(),
        )
        self.assertEqual(
            second.to_bytes(),
            (FIXTURES / "checkpoint-archive-tombstone-002.json").read_bytes(),
        )
        self.assertEqual(
            CheckpointArchiveTombstoneManifest.from_bytes(second.to_bytes()),
            second,
        )
        self.assertTrue(
            verify_checkpoint_archive_tombstone(
                source[1],
                second,
                previous_tombstone=first,
            )
        )
        self.assertEqual(
            second.to_dict()["effect"],
            "archive-metadata-tombstoned-no-delete-authority",
        )

    def test_broken_manifest_or_tombstone_chain_fails_closed(self):
        source = list(manifests())
        source[2] = CheckpointArchiveManifest(
            source[2].archive_id,
            source[2].archive_index,
            source[2].archive_name,
            source[2].archive_sha256,
            source[2].archived_entry_count,
            source[2].archived_first_entry_sha256,
            source[2].archived_final_entry_sha256,
            source[2].rotation_anchor_sha256,
            ZERO,
        )
        with self.assertRaises(CheckpointProofError):
            build_checkpoint_archive_retention_plan(source)
        with self.assertRaises(CheckpointProofError):
            build_checkpoint_archive_tombstone(
                manifests()[0],
                tombstone_index=2,
                reason="gap",
            )


if __name__ == "__main__":
    unittest.main()
