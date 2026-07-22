import copy
import json
import tempfile
import unittest
from pathlib import Path

from semantic_fixture_runner import EXIT_INVALID, EXIT_USAGE_OR_IO, EXIT_VALID, build_receipt, main, validate_manifest_bytes


def valid_manifest():
    return {
        "schema_version": 1,
        "source_repository": "owner/repo",
        "source_commit": "a" * 40,
        "execution_state": "specification-only",
        "authority": "offline-fixture-specification-no-runtime-authority",
        "acceptance_rule": "fail-closed",
        "fixture_set_id": "tg.fixture.test",
        "profile": {
            "profile_id": "foa-test",
            "game_version": "1",
            "unity_version": "2",
            "runtime": "Mono",
            "loader": "BepInEx 5",
            "framework": "TF 1",
            "evidence_state": "HostLiveLoadValidated",
            "required_runtime_fingerprints": {"TG.Main.dll": "required-but-absent"},
        },
        "sources": [{"path": "mods/example.cs", "blob": "b" * 40}],
        "cases": [{"case_id": "case.one", "expected": "fail closed", "promotion_effect": "required-for-any-future-promotion"}],
        "decision": {"promotion": "none"},
    }


class ManifestValidationTests(unittest.TestCase):
    def validate(self, manifest):
        return validate_manifest_bytes(json.dumps(manifest, sort_keys=True).encode(), "fixture.json")

    def test_valid_specification(self):
        result = self.validate(valid_manifest())
        self.assertEqual("valid-specification", result.status)
        self.assertEqual(1, result.case_count)
        self.assertEqual((), result.errors)

    def test_duplicate_case_is_rejected(self):
        manifest = valid_manifest()
        manifest["cases"].append(copy.deepcopy(manifest["cases"][0]))
        self.assertTrue(any("duplicate case id" in error for error in self.validate(manifest).errors))

    def test_runtime_fingerprint_is_not_accepted_as_evidence(self):
        manifest = valid_manifest()
        manifest["profile"]["required_runtime_fingerprints"]["TG.Main.dll"] = "deadbeef"
        self.assertTrue(any("required-but-absent" in error for error in self.validate(manifest).errors))

    def test_promotion_is_rejected(self):
        manifest = valid_manifest()
        manifest["decision"]["promotion"] = "signature-verified"
        self.assertTrue(any("promotion must be none" in error for error in self.validate(manifest).errors))

    def test_receipt_is_deterministic(self):
        result = self.validate(valid_manifest())
        self.assertEqual(build_receipt([result]), build_receipt([result]))


class CliTests(unittest.TestCase):
    def test_directory_input_and_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "b.json").write_text(json.dumps(valid_manifest()), encoding="utf-8")
            other = valid_manifest()
            other["fixture_set_id"] = "tg.fixture.other"
            other["cases"][0]["case_id"] = "case.two"
            (root / "a.json").write_text(json.dumps(other), encoding="utf-8")
            output = root / "receipt.json"
            self.assertEqual(EXIT_VALID, main([str(root), "--output", str(output), "--pretty"]))
            receipt = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(2, receipt["manifest_count"])
            self.assertEqual(2, receipt["case_count"])
            paths = [item["path"] for item in receipt["manifests"]]
            self.assertEqual(sorted(paths), paths)

    def test_invalid_manifest_exit(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "invalid.json"
            manifest = valid_manifest()
            manifest["decision"]["promotion"] = "behavior-verified"
            path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(EXIT_INVALID, main([str(path)]))

    def test_non_json_input_is_usage_error(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "fixture.txt"
            path.write_text("{}", encoding="utf-8")
            self.assertEqual(EXIT_USAGE_OR_IO, main([str(path)]))


if __name__ == "__main__":
    unittest.main()
