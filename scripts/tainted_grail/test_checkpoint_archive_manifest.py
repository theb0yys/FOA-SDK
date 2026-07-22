import hashlib
import tempfile
import unittest
from pathlib import Path

from semantic_repair import (
    CheckpointArchiveManifest,
    CheckpointChainEntry,
    CheckpointProofError,
    build_checkpoint_archive_manifest,
    verify_checkpoint_archive_manifest,
)

FIXTURE = (
    Path(__file__).parent
    / "fixtures"
    / "checkpoint_archive"
    / "checkpoint-chain-archive-manifest.json"
)


class CheckpointArchiveManifestTests(unittest.TestCase):
    def _files(self, root: Path):
        first = CheckpointChainEntry.build(
            1,
            "checkpoint-proof",
            "0" * 64,
            {
                "authority": "synthetic-checkpoint-chain-only",
                "proof_sha256": "a" * 64,
            },
        )
        second = CheckpointChainEntry.build(
            2,
            "checkpoint-proof",
            first.entry_sha256,
            {
                "authority": "synthetic-checkpoint-chain-only",
                "proof_sha256": "b" * 64,
            },
        )
        archive = root / "checkpoint-chain-archive-001.jsonl"
        archive.write_bytes(first.to_bytes() + second.to_bytes())
        anchor = CheckpointChainEntry.build(
            1,
            "rotation-anchor",
            "0" * 64,
            {
                "authority": "synthetic-checkpoint-chain-rotation-only",
                "runtime_authority": "none",
                "promotion": "none",
                "rotated_chain_sha256": hashlib.sha256(
                    archive.read_bytes()
                ).hexdigest(),
                "rotated_entry_count": 2,
                "rotated_final_entry_sha256": second.entry_sha256,
            },
        )
        active = root / "checkpoint-chain.jsonl"
        active.write_bytes(anchor.to_bytes())
        return archive, active

    def test_manifest_matches_golden_and_verifies(self):
        with tempfile.TemporaryDirectory() as directory:
            archive, active = self._files(Path(directory))
            manifest = build_checkpoint_archive_manifest(
                archive,
                active,
                archive_id="tg.synthetic.batch011.checkpoint-archive-001",
                archive_index=1,
            )
            self.assertEqual(manifest.to_bytes(), FIXTURE.read_bytes())
            self.assertTrue(
                verify_checkpoint_archive_manifest(
                    archive,
                    active,
                    manifest,
                )
            )

    def test_manifest_rejects_archive_tamper(self):
        with tempfile.TemporaryDirectory() as directory:
            archive, active = self._files(Path(directory))
            manifest = CheckpointArchiveManifest.from_bytes(
                FIXTURE.read_bytes()
            )
            archive.write_bytes(archive.read_bytes() + b"tamper")
            with self.assertRaises(CheckpointProofError):
                verify_checkpoint_archive_manifest(
                    archive,
                    active,
                    manifest,
                )


if __name__ == "__main__":
    unittest.main()
