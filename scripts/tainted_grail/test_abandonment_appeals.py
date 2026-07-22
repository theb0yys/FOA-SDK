import hashlib
import unittest
from pathlib import Path

from semantic_repair.abandonment_appeal import (
    AbandonmentDecisionAppeal,
    build_abandonment_decision_appeal,
    verify_abandonment_decision_appeal,
)
from semantic_repair.abandonment_lifecycle import AbandonmentDecisionRevocation, AbandonmentDecisionSupersession
from semantic_repair.abandonment_quorum import LockAbandonmentDecision, QuorumReviewReference
from semantic_repair.errors import RepairError

FIXTURE = Path(__file__).parent / "fixtures" / "abandonment" / "quorum-decision-appeal.json"


def hx(value):
    return hashlib.sha256(value.encode()).hexdigest()


def decision():
    return LockAbandonmentDecision(
        "resource", "stranded-owner", 2, hx("lock"), hx("generation"), 2,
        (
            QuorumReviewReference("reviewer-a", hx("review-a"), "owner unavailable"),
            QuorumReviewReference("reviewer-b", hx("review-b"), "manual review required"),
        ),
    )


class AbandonmentAppealTests(unittest.TestCase):
    def test_decision_appeal_matches_golden_and_retains_lock(self):
        target = decision()
        appeal = build_abandonment_decision_appeal(
            target,
            ("reviewer-b", "reviewer-a"),
            "new evidence requires reconsideration",
            appeal_id="appeal-001",
        )
        self.assertEqual(appeal.to_bytes(), FIXTURE.read_bytes())
        self.assertEqual(AbandonmentDecisionAppeal.from_bytes(appeal.to_bytes()), appeal)
        self.assertTrue(verify_abandonment_decision_appeal(target, appeal))
        self.assertEqual(appeal.to_dict()["effect"], "appeal-recorded-lock-retained")

    def test_revocation_and_supersession_are_supported_targets(self):
        target = decision()
        revocation = AbandonmentDecisionRevocation(
            target.sha256, target.resource_id, target.observed_owner_id,
            target.observed_generation, ("reviewer-a", "reviewer-b"), "revoke",
        )
        supersession = AbandonmentDecisionSupersession(
            target.sha256, revocation.sha256, hx("replacement"), target.resource_id,
            2, 3, ("reviewer-a", "reviewer-b"), "replace",
        )
        self.assertEqual(
            build_abandonment_decision_appeal(
                revocation, ("reviewer-a", "reviewer-b"), "appeal revocation",
                appeal_id="appeal-revocation",
            ).target_kind,
            "revocation",
        )
        self.assertEqual(
            build_abandonment_decision_appeal(
                supersession, ("reviewer-a", "reviewer-b"), "appeal supersession",
                appeal_id="appeal-supersession",
            ).observed_generation,
            3,
        )

    def test_duplicate_reviewers_fail_closed(self):
        with self.assertRaises(RepairError):
            build_abandonment_decision_appeal(
                decision(), ("same", "same"), "bad", appeal_id="appeal-bad"
            )


if __name__ == "__main__":
    unittest.main()
