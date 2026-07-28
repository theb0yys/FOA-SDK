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
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_economy_candidate_promotion.py"
SPEC = importlib.util.spec_from_file_location("foa_economy_candidate_promotion", MODULE_PATH)
assert SPEC and SPEC.loader
promotion = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = promotion
SPEC.loader.exec_module(promotion)


class FoAEconomyCandidatePromotionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="foa-economy-promotion-tests-"))
        self.fixture_root = self.temp_root / "fixture"
        promotion.generate_fixture(self.fixture_root)
        self.workspace = self.fixture_root / "workspace.tgworkspace.json"
        self.evidence = self.fixture_root / "evidence.tgevidence.json"
        self.candidates = self.fixture_root / "candidates.tgcatalog-candidates.json"

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def read_candidates(self) -> dict:
        return json.loads(self.candidates.read_text(encoding="utf-8"))

    def write_candidates(self, document: dict) -> None:
        self.candidates.write_bytes(promotion.pretty_json_bytes(document))

    def stage(self) -> dict:
        return promotion.stage(
            workspace_path=self.workspace,
            candidates_path=self.candidates,
            evidence_path=self.evidence,
            staged_at="2026-07-28T00:00:02Z",
            reviewer="test",
        )

    def test_stage_generates_item_recipe_and_station_review_drafts(self) -> None:
        document = self.stage()
        promotion.verify_document(document, promotion.active_profile(self.workspace))
        self.assertEqual(len(document["RecordPromotions"]), 3)
        self.assertEqual(len(document["EconomyItemProfiles"]), 1)
        self.assertEqual(len(document["EconomyRecipeProfiles"]), 1)
        self.assertEqual(len(document["EconomyStationProfiles"]), 1)
        self.assertFalse(document["CatalogMutationAllowed"])
        self.assertFalse(document["RuntimePermissionGranted"])
        self.assertIn("economy.recipe-output-required", document["EconomyRecipeProfiles"][0]["CompletionBlockers"])

    def test_profile_mismatch_is_rejected(self) -> None:
        candidates = self.read_candidates()
        candidates["GameVersion"] = "1.23.999"
        self.write_candidates(candidates)
        with self.assertRaisesRegex(promotion.PromotionError, "exact active workspace profile"):
            self.stage()

    def test_missing_evidence_blocks_record(self) -> None:
        candidates = self.read_candidates()
        candidates["Records"][0]["EvidenceIds"] = ["evidence.missing"]
        self.write_candidates(candidates)
        document = self.stage()
        self.assertIn("economy.missing-evidence", {item["Code"] for item in document["Issues"]})
        blocked = next(item for item in document["RecordPromotions"] if item["RecordId"] == candidates["Records"][0]["RecordId"])
        self.assertEqual(blocked["ReviewState"], "blocked")
        self.assertFalse(blocked["PromotionRecommended"])

    def test_duplicate_native_ref_is_reported(self) -> None:
        candidates = self.read_candidates()
        candidates["Records"][1]["NativeRefExact"] = candidates["Records"][0]["NativeRefExact"]
        self.write_candidates(candidates)
        document = self.stage()
        self.assertIn("economy.duplicate-native-ref", {item["Code"] for item in document["Issues"]})

    def test_non_economy_record_is_rejected(self) -> None:
        candidates = self.read_candidates()
        candidates["Records"][0]["Domain"] = "population"
        self.write_candidates(candidates)
        document = self.stage()
        self.assertIn("economy.non-economy-record", {item["Code"] for item in document["Issues"]})

    def test_synthetic_item_requires_owner_and_no_native_ref(self) -> None:
        candidates = self.read_candidates()
        candidates["Records"][0]["IdentityKind"] = "synthetic"
        candidates["Records"][0]["OwnerPackId"] = "owner.pack"
        self.write_candidates(candidates)
        document = self.stage()
        self.assertIn("economy.synthetic-identity-invalid", {item["Code"] for item in document["Issues"]})

    def test_input_candidate_issues_are_preserved(self) -> None:
        candidates = self.read_candidates()
        candidates["Issues"].append({
            "IssueId": "issue.input.duplicate",
            "Severity": "error",
            "Code": "catalog-candidate.duplicate-native-ref",
            "Message": "input issue",
            "RecordId": candidates["Records"][0]["RecordId"],
            "Locator": "$.Records[0]",
            "RecordPath": "$.Issues[0]",
            "Line": 0,
        })
        self.write_candidates(candidates)
        document = self.stage()
        self.assertIn("catalog-candidate.duplicate-native-ref", {item["Code"] for item in document["Issues"]})

    def test_tampered_authority_is_rejected_by_verify(self) -> None:
        document = self.stage()
        document["CatalogMutationAllowed"] = True
        with self.assertRaisesRegex(promotion.PromotionError, "CatalogMutationAllowed must be false"):
            promotion.verify_document(document, promotion.active_profile(self.workspace))

    def test_cli_stage_and_verify_succeed(self) -> None:
        output = self.temp_root / "promotion.json"
        self.assertEqual(
            promotion.main([
                "stage",
                "--workspace", str(self.workspace),
                "--candidates", str(self.candidates),
                "--evidence", str(self.evidence),
                "--output", str(output),
                "--staged-at", "2026-07-28T00:00:02Z",
                "--reviewer", "test",
            ]),
            0,
        )
        self.assertEqual(promotion.main(["verify", "--input", str(output), "--workspace", str(self.workspace)]), 0)

    def test_fixture_generates_verified_output(self) -> None:
        output = self.temp_root / "second-fixture"
        manifest = promotion.generate_fixture(output)
        self.assertEqual(manifest["ToolId"], promotion.TOOL_ID)
        self.assertEqual(manifest["RecordPromotionCount"], 3)
        self.assertEqual(
            promotion.main([
                "verify",
                "--input", str(output / "economy-promotion.tgeconomy-promotion.json"),
                "--workspace", str(output / "workspace.tgworkspace.json"),
            ]),
            0,
        )


if __name__ == "__main__":
    unittest.main()
