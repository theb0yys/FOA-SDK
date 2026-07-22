# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import hashlib
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Catalog" / "Source"
sys.path.insert(0, str(SOURCE))

from wizard_catalog import (  # noqa: E402
    CatalogInterfaceError,
    canonical_json_bytes,
    create_selection,
    discover_catalog,
    resolve_selection,
    validate_catalog,
    validate_selection,
)

INSTALLER_ROOT = REPO_ROOT / "Installer"
EXPECTED_ORDER = [
    "foa.foundation-descriptor",
    "foa.editor-descriptor",
    "foa.installer-documentation",
]


class SuiteWizardCatalogTests(unittest.TestCase):
    def selection(self, catalog: dict[str, object], *, documentation: bool = True) -> dict[str, object]:
        return create_selection(
            catalog,
            suite_id="foa.developer-preview",
            platform="windows",
            architecture="x86_64",
            runtime_target="editor-only",
            selected_feature_ids=["foa.feature.documentation"] if documentation else [],
        )

    def test_production_catalog_discovery_is_canonical_and_displayable(self) -> None:
        first = discover_catalog(INSTALLER_ROOT)
        second = discover_catalog(INSTALLER_ROOT)
        self.assertEqual(canonical_json_bytes(first), canonical_json_bytes(second))
        self.assertEqual(validate_catalog(first), first)
        self.assertEqual(first["summary"], {
            "suite_count": 1,
            "package_count": 3,
            "feature_count": 2,
        })
        self.assertTrue(all(value is False for value in first["authority"].values()))

        suite = first["suites"][0]
        self.assertEqual(suite["suite_id"], "foa.developer-preview")
        self.assertEqual(
            [row["package_id"] for row in suite["packages"]],
            EXPECTED_ORDER,
        )
        self.assertEqual(
            [row["feature_id"] for row in suite["features"]],
            ["foa.feature.documentation", "foa.feature.editor"],
        )
        self.assertEqual(suite["required_package_ids"], ["foa.foundation-descriptor"])
        self.assertEqual(
            suite["default_package_ids"],
            ["foa.editor-descriptor", "foa.foundation-descriptor"],
        )
        self.assertEqual(suite["optional_package_ids"], ["foa.installer-documentation"])

        packages = {row["package_id"]: row for row in first["packages"]}
        self.assertEqual(set(packages), set(EXPECTED_ORDER))
        self.assertEqual(packages["foa.editor-descriptor"]["payload_summary"], {
            "file_count": 1,
            "size_bytes": 1032,
        })

    def test_review_only_selection_resolves_to_plan_and_view_model(self) -> None:
        catalog = discover_catalog(INSTALLER_ROOT)
        selection = self.selection(catalog)
        self.assertEqual(validate_selection(selection), selection)
        result = resolve_selection(INSTALLER_ROOT, selection)

        self.assertEqual(result["catalog_sha256"], catalog["catalog_sha256"])
        self.assertEqual(result["selection_sha256"], selection["selection_sha256"])
        self.assertEqual(result["plan"]["package_order"], EXPECTED_ORDER)
        self.assertEqual(
            [row["package_id"] for row in result["view_model"]["packages"]],
            EXPECTED_ORDER,
        )
        self.assertEqual(
            result["view_model"]["summary"],
            {"package_count": 3, "payload_file_count": 3, "payload_size_bytes": 2292},
        )
        self.assertTrue(all(value is False for value in result["authority"].values()))
        unsigned = {key: value for key, value in result.items() if key != "result_sha256"}
        self.assertEqual(
            result["result_sha256"],
            hashlib.sha256(canonical_json_bytes(unsigned)).hexdigest(),
        )

    def test_default_selection_and_explicit_exclusion_remain_resolver_owned(self) -> None:
        catalog = discover_catalog(INSTALLER_ROOT)
        default_result = resolve_selection(INSTALLER_ROOT, self.selection(catalog, documentation=False))
        self.assertEqual(
            default_result["plan"]["package_order"],
            ["foa.foundation-descriptor", "foa.editor-descriptor"],
        )
        excluded = create_selection(
            catalog,
            suite_id="foa.developer-preview",
            platform="windows",
            architecture="x86_64",
            runtime_target="editor-only",
            excluded_package_ids=["foa.editor-descriptor"],
        )
        excluded_result = resolve_selection(INSTALLER_ROOT, excluded)
        self.assertEqual(excluded_result["plan"]["package_order"], ["foa.foundation-descriptor"])

    def test_stale_catalog_selection_fails_closed(self) -> None:
        catalog = discover_catalog(INSTALLER_ROOT)
        selection = self.selection(catalog)
        with tempfile.TemporaryDirectory() as temporary:
            copied_installer = Path(temporary) / "Installer"
            shutil.copytree(INSTALLER_ROOT, copied_installer)
            suite_path = copied_installer / "Suites" / "DeveloperPreview" / "suite.json"
            document = json.loads(suite_path.read_text(encoding="utf-8"))
            document["display_name"] = "Changed after review"
            suite_path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(CatalogInterfaceError, "catalogue changed"):
                resolve_selection(copied_installer, selection)

    def test_unknown_and_tampered_selections_fail_closed(self) -> None:
        catalog = discover_catalog(INSTALLER_ROOT)
        with self.assertRaisesRegex(CatalogInterfaceError, "Unknown suite ID"):
            create_selection(
                catalog,
                suite_id="foa.unknown",
                platform="windows",
                architecture="x86_64",
                runtime_target="editor-only",
            )
        with self.assertRaisesRegex(CatalogInterfaceError, "unknown features"):
            create_selection(
                catalog,
                suite_id="foa.developer-preview",
                platform="windows",
                architecture="x86_64",
                runtime_target="editor-only",
                selected_feature_ids=["foa.feature.unknown"],
            )

        selection = self.selection(catalog)
        tampered = copy.deepcopy(selection)
        tampered["selected_feature_ids"] = []
        with self.assertRaisesRegex(CatalogInterfaceError, "fingerprint"):
            validate_selection(tampered)

    def test_interface_source_exposes_no_executor_surface(self) -> None:
        source = (SOURCE / "wizard_catalog.py").read_text(encoding="utf-8")
        for forbidden in (
            "subprocess",
            "urllib",
            "requests",
            "os.system",
            "atomic_write",
            "os.replace",
            "shutil.copy",
            "write_bytes",
            "write_text",
        ):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
