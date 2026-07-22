import unittest
from pathlib import Path

from semantic_repair.ci_policy import (
    WorkflowPolicyReceipt,
    build_offline_workflow_receipt,
)

ROOT = Path(__file__).parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "semantic-hook-offline.yml"
GOLDEN = Path(__file__).parent / "fixtures" / "ci" / "semantic-hook-offline-policy-receipt.json"


class CiPolicyReceiptTests(unittest.TestCase):
    def test_receipt_matches_exact_workflow_bytes(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        receipt = build_offline_workflow_receipt(text)
        self.assertEqual(receipt.status, "valid")
        self.assertEqual(receipt.violations, ())
        self.assertEqual(receipt.to_bytes(), GOLDEN.read_bytes())
        self.assertEqual(
            WorkflowPolicyReceipt.from_bytes(GOLDEN.read_bytes()),
            receipt,
        )

    def test_repeated_receipt_is_byte_identical(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertEqual(
            build_offline_workflow_receipt(text).to_bytes(),
            build_offline_workflow_receipt(text).to_bytes(),
        )


if __name__ == "__main__":
    unittest.main()
