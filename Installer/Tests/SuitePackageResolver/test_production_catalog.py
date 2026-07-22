# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import hashlib
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
RESOLVER_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Resolver" / "Source"
VIEW_MODEL_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "ViewModel" / "Source"
sys.path.insert(0, str(RESOLVER_SOURCE))
sys.path.insert(0, str(VIEW_MODEL_SOURCE))

from suite_package_resolver import (  # noqa: E402
    ResolutionContext,
    canonical_json_bytes,
    resolve_suite,
)
from wizard_view_model import build_view_model  # noqa: E402

PINNED_SOURCE_COMMIT = "e7cec6515c9e5ac4e64c8ee3eea5dd5b769121d1"
EXPECTED_FULL_ORDER = [
    "foa.foundation-descriptor",
    "foa.editor-descriptor",
    "foa.installer-documentation",
]


class ProductionCatalogTests(unittest.TestCase):
    def resolve(self, *, documentation: bool = False) -> dict[str, object]:
        features = ["foa.feature.documentation"] if documentation else []
        return resolve_suite(
            REPO_ROOT / "Installer",
            "DeveloperPreview",
            ResolutionContext("windows", "x86_64", "editor-only"),
            selected_feature_ids=features,
        )

    def test_default_and_optional_catalog_selection(self) -> None:
        default_plan = self.resolve()
        self.assertEqual(
            default_plan["package_order"],
            ["foa.foundation-descriptor", "foa.editor-descriptor"],
        )
        full_plan = self.resolve(documentation=True)
        self.assertEqual(full_plan["package_order"], EXPECTED_FULL_ORDER)
        rows = {row["package_id"]: row for row in full_plan["packages"]}
        self.assertEqual(
            rows["foa.foundation-descriptor"]["selection_reasons"],
            ["dependency:foa.editor-descriptor", "suite:required"],
        )
        self.assertEqual(
            rows["foa.installer-documentation"]["selection_reasons"],
            ["feature:foa.feature.documentation"],
        )

    def test_catalog_payload_fingerprints_match_pinned_source(self) -> None:
        plan = self.resolve(documentation=True)
        for package in plan["packages"]:
            source = package["source"]
            self.assertEqual(source["commit"], PINNED_SOURCE_COMMIT)
            source_root = REPO_ROOT / source["path"]
            for item in package["payload"]:
                payload = (source_root / item["source"]).read_bytes()
                self.assertEqual(len(payload), item["size_bytes"])
                self.assertEqual(hashlib.sha256(payload).hexdigest(), item["sha256"])

    def test_full_catalog_matches_golden_plan_and_builds_wizard_rows(self) -> None:
        plan = self.resolve(documentation=True)
        golden = (
            REPO_ROOT
            / "Installer"
            / "Tests"
            / "SuitePackageResolver"
            / "Fixtures"
            / "developer-preview.plan.json"
        ).read_bytes()
        self.assertEqual(canonical_json_bytes(plan), golden)

        view_model = build_view_model(plan)
        self.assertEqual(
            [row["package_id"] for row in view_model["packages"]],
            EXPECTED_FULL_ORDER,
        )
        self.assertEqual(
            [row["package_id"] for row in view_model["payload"]],
            EXPECTED_FULL_ORDER,
        )
        self.assertEqual(view_model["summary"], {
            "package_count": 3,
            "payload_file_count": 3,
            "payload_size_bytes": 2142,
        })
        self.assertTrue(all(value is False for value in view_model["authority"].values()))


if __name__ == "__main__":
    unittest.main()
