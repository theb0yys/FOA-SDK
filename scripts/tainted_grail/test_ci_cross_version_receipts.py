import sys
import unittest
from pathlib import Path

from semantic_repair.ci_matrix import (
    CrossVersionPolicyFixture,
    build_cross_version_policy_fixture,
    verify_cross_version_policy_fixtures,
)
from semantic_repair.ci_policy import build_offline_workflow_receipt

ROOT = Path(__file__).parent
WORKFLOW = Path(__file__).parents[2] / ".github" / "workflows" / "semantic-hook-offline.yml"
FIXTURES = ROOT / "fixtures" / "ci"
VERSIONS = ("3.11", "3.12", "3.13")


class CrossVersionPolicyReceiptTests(unittest.TestCase):
    def test_all_matrix_fixtures_bind_identical_receipt(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        receipt = build_offline_workflow_receipt(text)
        fixtures = tuple(
            CrossVersionPolicyFixture.from_bytes(
                (FIXTURES / f"semantic-hook-offline-python-{version}.json").read_bytes()
            )
            for version in VERSIONS
        )
        self.assertTrue(verify_cross_version_policy_fixtures(fixtures, receipt))
        for fixture in fixtures:
            self.assertEqual(
                build_cross_version_policy_fixture(text, fixture.python_version).to_bytes(),
                fixture.to_bytes(),
            )

    def test_current_interpreter_has_fixture(self):
        version = f"{sys.version_info.major}.{sys.version_info.minor}"
        if version in VERSIONS:
            self.assertTrue((FIXTURES / f"semantic-hook-offline-python-{version}.json").exists())


if __name__ == "__main__":
    unittest.main()
