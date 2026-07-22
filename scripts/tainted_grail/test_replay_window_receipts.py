import unittest
from pathlib import Path

from semantic_repair.dialogue_v2 import DialogueRegistryV2, RegistrationToken, RegistrySnapshot
from semantic_repair.replay_window import evaluate_snapshot_replay_window

FIXTURES = Path(__file__).parent / "fixtures" / "api_v2"


class ReplayWindowReceiptTests(unittest.TestCase):
    def setUp(self):
        self.registry = DialogueRegistryV2()
        self.registry._generation = 2
        self.registry._registrations = {
            ("owner", "open"): RegistrationToken("v2:owner:open", "owner", "open", 2)
        }

    def test_receipts_match_goldens(self):
        cases = {
            "accepted": RegistrySnapshot(
                4,
                (RegistrationToken("v2:owner:open", "owner", "open", 2),),
            ),
            "stale": RegistrySnapshot(1, ()),
            "gap-exceeded": RegistrySnapshot(67, ()),
        }
        for name, snapshot in cases.items():
            with self.subTest(name=name):
                receipt = evaluate_snapshot_replay_window(self.registry, snapshot)
                self.assertEqual(
                    receipt.to_bytes(),
                    (FIXTURES / f"replay-window-{name}.json").read_bytes(),
                )
        self.assertEqual(self.registry.generation, 2)

    def test_same_generation_conflict_is_receipted_without_mutation(self):
        conflict = RegistrySnapshot(2, ())
        receipt = evaluate_snapshot_replay_window(self.registry, conflict)
        self.assertEqual(receipt.status, "same-generation-conflict")
        self.assertEqual(self.registry.generation, 2)


if __name__ == "__main__":
    unittest.main()
