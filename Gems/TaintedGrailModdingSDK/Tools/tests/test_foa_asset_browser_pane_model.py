#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import foa_asset_browser_pane_model as pane


class AssetBrowserPaneModelTests(unittest.TestCase):
    def fixture(self):
        tmp = tempfile.TemporaryDirectory()
        root = Path(tmp.name)
        result = pane.generate_fixture(root / "fixture", replace=True)
        workspace = root / "fixture" / "workspace.tgworkspace.json"
        proof = root / "fixture" / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "ImportProofs" / "proof.fixture" / "foa-o3de-asset-processor-import-proof.json"
        model = Path(result["ModelPath"])
        return tmp, root, workspace, proof, model

    def test_fixture_and_verify_success(self):
        tmp, _, workspace, proof, model = self.fixture()
        with tmp:
            loaded = pane.verify_model(model, workspace_path=workspace, import_proof_path=proof)
            self.assertEqual(loaded["DocumentKind"], pane.DOCUMENT_KIND)
            self.assertEqual(len(loaded["PaneEntries"]), 2)

    def test_rejects_raw_conversion_document(self):
        tmp, _, workspace, proof, _ = self.fixture()
        with tmp:
            raw = json.loads(proof.read_text())
            raw["DocumentKind"] = "foa-o3de-preview-conversion"
            bad = proof.with_name("raw-conversion.json")
            bad.write_text(json.dumps(raw))
            with self.assertRaises(pane.AssetBrowserPaneError):
                pane.build_model(workspace, bad, captured_at="2026-07-28T00:00:00Z")

    def test_imported_product_entry_cannot_create_authoring_binding(self):
        tmp, _, workspace, proof, model = self.fixture()
        with tmp:
            loaded = pane.verify_model(model, workspace_path=workspace, import_proof_path=proof)
            product_entries = [e for e in loaded["PaneEntries"] if e["EntryKind"] == "o3de-preview-product"]
            self.assertEqual(len(product_entries), 1)
            self.assertFalse(product_entries[0]["SelectionPolicy"]["CanCreateTypedAuthoringBinding"])
            self.assertTrue(product_entries[0]["SelectionPolicy"]["RequiresExplicitBindingStep"])

    def test_product_cache_paths_are_tokenized(self):
        tmp, _, workspace, proof, model = self.fixture()
        with tmp:
            loaded = pane.verify_model(model, workspace_path=workspace, import_proof_path=proof)
            product_entries = [e for e in loaded["PaneEntries"] if e["EntryKind"] == "o3de-preview-product"]
            self.assertTrue(product_entries[0]["ProductCachePaths"][0].startswith("$assetcache/"))

    def test_failure_entry_preserved(self):
        tmp, _, workspace, proof, model = self.fixture()
        with tmp:
            loaded = pane.verify_model(model, workspace_path=workspace, import_proof_path=proof)
            failures = [e for e in loaded["PaneEntries"] if e["EntryKind"] == "o3de-import-failure"]
            self.assertEqual(len(failures), 1)
            self.assertEqual(failures[0]["IssueSeverity"], "error")
            self.assertEqual(failures[0]["PreviewAvailability"], "import-failed")

    def test_verify_rejects_binding_escalation(self):
        tmp, _, workspace, proof, model = self.fixture()
        with tmp:
            loaded = json.loads(model.read_text())
            loaded["PaneEntries"][0]["SelectionPolicy"]["CanCreateTypedAuthoringBinding"] = True
            model.write_text(json.dumps(loaded))
            with self.assertRaises(pane.AssetBrowserPaneError):
                pane.verify_model(model, workspace_path=workspace, import_proof_path=proof)

    def test_import_proof_profile_mismatch_rejected(self):
        tmp, _, workspace, proof, _ = self.fixture()
        with tmp:
            loaded = json.loads(proof.read_text())
            loaded["ProfileId"] = "foa.other.fixture"
            proof.write_text(json.dumps(loaded))
            with self.assertRaises(pane.AssetBrowserPaneError):
                pane.build_model(workspace, proof, captured_at="2026-07-28T00:00:00Z")

    def test_cli_fixture_and_verify_success(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "fixture"
            script = TOOLS / "foa_asset_browser_pane_model.py"
            result = subprocess.run([sys.executable, str(script), "fixture", "--output", str(root)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            model = next(root.glob("workspace/Extracted/PreviewArtifacts/AssetBrowser/*/foa-asset-browser-pane-model.json"))
            proof = root / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "ImportProofs" / "proof.fixture" / "foa-o3de-asset-processor-import-proof.json"
            verify = subprocess.run([sys.executable, str(script), "verify", "--input", str(model), "--workspace", str(root / "workspace.tgworkspace.json"), "--import-proof", str(proof)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(verify.returncode, 0, verify.stdout + verify.stderr)


if __name__ == "__main__":
    unittest.main()
