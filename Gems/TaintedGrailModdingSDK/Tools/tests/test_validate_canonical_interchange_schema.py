# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
VALIDATOR_PATH = REPOSITORY_ROOT / "Gems/TaintedGrailModdingSDK/Tools/validate_canonical_interchange_schema.py"
SPEC = importlib.util.spec_from_file_location("foa_validate_canonical_interchange_schema", VALIDATOR_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load validator: {VALIDATOR_PATH}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class CanonicalInterchangeSchemaValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.schema = json.loads((REPOSITORY_ROOT / MODULE.SCHEMA_RELATIVE).read_text(encoding="utf-8"))
        cls.documents_example = json.loads(
            (REPOSITORY_ROOT / MODULE.DOCUMENTS_EXAMPLE_RELATIVE).read_text(encoding="utf-8")
        )
        cls.asset_example = json.loads(
            (REPOSITORY_ROOT / MODULE.ASSET_EXAMPLE_RELATIVE).read_text(encoding="utf-8")
        )

    def test_repository_contract_is_consistent(self) -> None:
        self.assertEqual([], MODULE.validate_repository(REPOSITORY_ROOT))

    def test_examples_validate_against_closed_schema(self) -> None:
        self.assertEqual([], MODULE.validate_instance(self.documents_example, self.schema))
        self.assertEqual([], MODULE.validate_instance(self.asset_example, self.schema))

    def test_unknown_top_level_field_is_rejected(self) -> None:
        candidate = copy.deepcopy(self.documents_example)
        candidate["runtime_authority"] = True
        errors = MODULE.validate_instance(candidate, self.schema)
        self.assertTrue(any("unknown property runtime_authority" in error for error in errors))

    def test_missing_required_collection_is_rejected(self) -> None:
        candidate = copy.deepcopy(self.documents_example)
        del candidate["payloads"]
        errors = MODULE.validate_instance(candidate, self.schema)
        self.assertTrue(any("missing required property payloads" in error for error in errors))

    def test_enum_drift_is_rejected(self) -> None:
        candidate = copy.deepcopy(self.documents_example)
        candidate["package_kind"] = "runtime-package"
        errors = MODULE.validate_instance(candidate, self.schema)
        self.assertTrue(any("closed enum" in error for error in errors))

    def test_identifier_grammar_is_rejected(self) -> None:
        candidate = copy.deepcopy(self.documents_example)
        candidate["package_id"] = "NoNamespace"
        errors = MODULE.validate_instance(candidate, self.schema)
        self.assertTrue(any("semantic-id-v1" in error or "pattern" in error for error in errors))

    def test_document_revision_fixture_is_bound(self) -> None:
        candidate = copy.deepcopy(self.documents_example)
        candidate["documents"][0]["revision_fingerprint"] = "0" * 64
        errors = MODULE.validate_example_semantics(candidate, "mutated")
        self.assertTrue(any("revision fingerprint mismatch" in error for error in errors))

    def test_asset_revision_fixture_is_bound(self) -> None:
        candidate = copy.deepcopy(self.asset_example)
        candidate["assets"][0]["revision_fingerprint"] = "0" * 64
        errors = MODULE.validate_example_semantics(candidate, "mutated")
        self.assertTrue(any("revision fingerprint mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
