import json
import unittest
from pathlib import Path

from semantic_fixture_runner import build_receipt, validate_manifest_bytes

HERE = Path(__file__).resolve().parent
GOLDENS = HERE / "goldens"


class SemanticFixtureGoldenTests(unittest.TestCase):
    def assert_golden(self, manifest_name: str, receipt_name: str) -> None:
        manifest_path = GOLDENS / manifest_name
        expected = json.loads((GOLDENS / receipt_name).read_text(encoding="utf-8"))
        result = validate_manifest_bytes(manifest_path.read_bytes(), manifest_name)
        actual = build_receipt([result])
        self.assertEqual(actual, expected)
        self.assertEqual(actual["runtime_authority"], "none")
        self.assertEqual(actual["promotion"], "none")

    def test_valid_receipt_golden(self):
        self.assert_golden("synthetic-valid.json", "valid-receipt.json")

    def test_invalid_receipt_golden(self):
        self.assert_golden("synthetic-invalid.json", "invalid-receipt.json")


if __name__ == "__main__":
    unittest.main()
