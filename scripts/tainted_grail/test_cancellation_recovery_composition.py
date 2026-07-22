import json
import tempfile
import unittest
from pathlib import Path

from semantic_repair.journal import CrashRecoveryJournal

GOLDEN = Path(__file__).parent / "fixtures" / "recovery" / "cancellation-recovery-matrix.json"


def build_receipt():
    cases = []
    for latest_phase in ("cancel-requested", "cancelling", "cancelled"):
        for torn_tail in (False, True):
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "journal.jsonl"
                journal = CrashRecoveryJournal(path)
                journal.append("tx", latest_phase, {"reason": "synthetic"})
                if torn_tail:
                    with path.open("ab") as handle:
                        handle.write(b'{"torn"')
                plan = journal.recovery_plan()
                actions = [step.action.value for step in plan.steps]
                expected = (
                    ["truncate-torn-tail", "none"]
                    if torn_tail and latest_phase == "cancelled"
                    else ["truncate-torn-tail", "rollback"]
                    if torn_tail
                    else ["none"]
                    if latest_phase == "cancelled"
                    else ["rollback"]
                )
                cases.append(
                    {
                        "case_id": f"{latest_phase}.torn-{str(torn_tail).lower()}",
                        "latest_phase": latest_phase,
                        "torn_tail": torn_tail,
                        "actions": actions,
                        "status": "passed" if actions == expected else "failed",
                    }
                )
    return {
        "schema_version": 1,
        "authority": "synthetic-cancellation-recovery-matrix-only",
        "runtime_authority": "none",
        "promotion": "none",
        "case_count": len(cases),
        "cases": cases,
    }


class CancellationRecoveryCompositionTests(unittest.TestCase):
    def test_matrix_matches_golden(self):
        receipt = build_receipt()
        encoded = (
            json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n"
        ).encode("utf-8")
        self.assertEqual(encoded, GOLDEN.read_bytes())
        self.assertTrue(all(case["status"] == "passed" for case in receipt["cases"]))


if __name__ == "__main__":
    unittest.main()
