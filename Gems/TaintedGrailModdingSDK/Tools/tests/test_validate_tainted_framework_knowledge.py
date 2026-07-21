#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
VALIDATOR_PATH = (
    REPO_ROOT
    / "Gems"
    / "TaintedGrailModdingSDK"
    / "Tools"
    / "validate_tainted_framework_knowledge.py"
)
SPEC = importlib.util.spec_from_file_location("tf_validator", VALIDATOR_PATH)
validator = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(validator)


class TaintedFrameworkKnowledgeValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "repo"
        shutil.copytree(REPO_ROOT / "Gems", self.root / "Gems")

    def tearDown(self) -> None:
        self.temp.cleanup()

    def document(self, relative: str) -> Path:
        return (
            self.root
            / "Gems"
            / "TaintedGrailModdingSDK"
            / "Knowledge"
            / "TaintedFramework"
            / relative
        )

    def rewrite(self, relative: str, transform) -> None:
        path = self.document(relative)
        value = json.loads(path.read_text(encoding="utf-8"))
        transform(value)
        path.write_bytes(validator.canonical_bytes(value))

    def assert_fails(self, fragment: str) -> None:
        with self.assertRaisesRegex(validator.TaintedFrameworkKnowledgeError, fragment):
            validator.validate(self.root)

    def test_repository_fixture_passes(self) -> None:
        validator.validate(self.root)

    def test_branch_drift_fails_closed(self) -> None:
        self.rewrite("upstream-intake.json", lambda value: value.update(branch="dev"))
        self.assert_fails("branch drifted")

    def test_missing_evidence_fails_closed(self) -> None:
        def remove_source(value):
            value["sources"] = value["sources"][:-1]

        self.rewrite("upstream-intake.json", remove_source)
        self.assert_fails("selected upstream source set")

    def test_capability_escalation_fails_closed(self) -> None:
        self.rewrite(
            "knowledge.json",
            lambda value: value["capabilities"].append("mutable_catalog"),
        )
        self.assert_fails("capability declaration escalated")

    def test_duplicate_component_identity_fails_closed(self) -> None:
        def duplicate(value):
            value["components"].append(dict(value["components"][0]))

        self.rewrite("component-inventory.json", duplicate)
        self.assert_fails("Component IDs must be unique")

    def test_il2cpp_promotion_fails_closed(self) -> None:
        def promote(value):
            for row in value["compatibility"]:
                if row["branch"] == "IL2CPP":
                    row["status"] = "supported"

        self.rewrite("knowledge.json", promote)
        self.assert_fails("IL2CPP must remain blocked")

    def test_manifest_tamper_fails_closed(self) -> None:
        path = self.document("golden/fixtures.json")
        path.write_text(path.read_text(encoding="utf-8") + " ", encoding="utf-8")
        self.assert_fails("not canonical JSON")


if __name__ == "__main__":
    unittest.main()
