import unittest
from pathlib import Path

from semantic_repair import (
    LockAbandonmentDecision,
    QuorumReviewReference,
    RepairError,
    revoke_abandonment_decision,
    supersede_abandonment_decision,
)

ROOT = Path(__file__).parent / "fixtures" / "abandonment"


class AbandonmentDecisionLifecycleTests(unittest.TestCase):
    def _decision(
        self,
        owner: str,
        generation: int,
        lock_hash: str,
        state_hash: str,
    ):
        reviews = (
            QuorumReviewReference(
                "reviewer-a",
                "1" * 64,
                "reviewed",
            ),
            QuorumReviewReference(
                "reviewer-b",
                "2" * 64,
                "reviewed",
            ),
        )
        return LockAbandonmentDecision(
            "journal:synthetic",
            owner,
            generation,
            lock_hash,
            state_hash,
            2,
            reviews,
        )

    def test_revocation_and_supersession_match_goldens(self):
        previous = self._decision(
            "writer-a",
            1,
            "a" * 64,
            "b" * 64,
        )
        replacement = self._decision(
            "writer-b",
            2,
            "c" * 64,
            "d" * 64,
        )
        revocation = revoke_abandonment_decision(
            previous,
            ("reviewer-b", "reviewer-a"),
            "new lock observation requires supersession",
        )
        supersession = supersede_abandonment_decision(
            previous,
            replacement,
            revocation,
            ("reviewer-a", "reviewer-b"),
            "replace revoked decision with newer observation",
        )
        self.assertEqual(
            revocation.to_bytes(),
            (ROOT / "quorum-decision-revocation.json").read_bytes(),
        )
        self.assertEqual(
            supersession.to_bytes(),
            (ROOT / "quorum-decision-supersession.json").read_bytes(),
        )
        self.assertEqual(
            revocation.to_dict()["effect"],
            "decision-revoked-lock-retained",
        )
        self.assertEqual(
            supersession.to_dict()["effect"],
            "decision-superseded-lock-retained",
        )

    def test_duplicate_reviewers_do_not_meet_lifecycle_quorum(self):
        previous = self._decision(
            "writer-a",
            1,
            "a" * 64,
            "b" * 64,
        )
        with self.assertRaises(RepairError):
            revoke_abandonment_decision(
                previous,
                ("reviewer-a", "reviewer-a"),
                "duplicate",
            )

    def test_supersession_requires_matching_revocation_and_resource(self):
        previous = self._decision(
            "writer-a",
            1,
            "a" * 64,
            "b" * 64,
        )
        other = LockAbandonmentDecision(
            "other-resource",
            "writer-b",
            2,
            "c" * 64,
            "d" * 64,
            2,
            previous.reviews,
        )
        revocation = revoke_abandonment_decision(
            previous,
            ("reviewer-a", "reviewer-b"),
            "replace",
        )
        with self.assertRaises(RepairError):
            supersede_abandonment_decision(
                previous,
                other,
                revocation,
                ("reviewer-a", "reviewer-b"),
                "bad resource",
            )


if __name__ == "__main__":
    unittest.main()
