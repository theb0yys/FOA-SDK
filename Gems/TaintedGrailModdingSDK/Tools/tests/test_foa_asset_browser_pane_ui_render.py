#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import foa_asset_browser_pane_ui_render as ui

class AssetBrowserUiRenderTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.workspace = ui.write_workspace(self.root)
        self.model = ui.write_model(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def test_fixture_and_verify_success(self):
        result = ui.generate_fixture(self.root / 'fixture')
        manifest = Path(result['RenderPath'])
        workspace = self.root / 'fixture' / 'workspace.tgworkspace.json'
        model = next((self.root / 'fixture').rglob('foa-asset-browser-pane-model.json'))
        verified = ui.verify_render(manifest, workspace_path=workspace, model_path=model)
        self.assertEqual(len(verified['UiEntries']), 2)

    def test_rejects_import_proof_as_direct_input(self):
        proof = self.root / 'workspace' / 'Extracted' / 'proof.json'
        proof.write_text(json.dumps({'SchemaVersion': 1, 'DocumentKind': 'foa-o3de-asset-processor-import-proof'}), encoding='utf-8')
        with self.assertRaises(ui.AssetBrowserUiRenderError):
            ui.build_render(self.workspace, proof)

    def test_no_authoring_binding_created(self):
        doc, path = ui.build_render(self.workspace, self.model, captured_at='2026-07-28T00:00:00Z')
        self.assertFalse(doc['PreviewStageStatus']['TypedAuthoringBindingCreated'])
        self.assertFalse(doc['PreviewStageStatus']['TypedSelectorCreated'])
        for entry in doc['UiEntries']:
            self.assertFalse(entry['CanCreateTypedAuthoringBinding'])
            self.assertTrue(entry['RequiresExplicitBindingStep'])
        ui.verify_render(path, workspace_path=self.workspace, model_path=self.model)

    def test_tampered_render_artifact_rejected(self):
        _doc, path = ui.build_render(self.workspace, self.model, captured_at='2026-07-28T00:00:00Z')
        (path.parent / 'ui' / 'asset-browser-pane.html').write_text('tampered', encoding='utf-8')
        with self.assertRaises(ui.AssetBrowserUiRenderError):
            ui.verify_render(path, workspace_path=self.workspace, model_path=self.model)

    def test_model_binding_escalation_rejected(self):
        model = json.loads(self.model.read_text())
        model['PaneEntries'][0]['SelectionPolicy']['CanCreateTypedAuthoringBinding'] = True
        bad = self.model.parent / 'bad-model.json'
        bad.write_text(json.dumps(model), encoding='utf-8')
        with self.assertRaises(ui.AssetBrowserUiRenderError):
            ui.build_render(self.workspace, bad)

    def test_product_cache_tokens_preserved(self):
        doc, path = ui.build_render(self.workspace, self.model, captured_at='2026-07-28T00:00:00Z')
        product = [e for e in doc['UiEntries'] if e['EntryKind'] == 'o3de-preview-product'][0]
        self.assertTrue(product['ProductCachePaths'][0].startswith('$assetcache/'))
        ui.verify_render(path, workspace_path=self.workspace, model_path=self.model)

    def test_profile_mismatch_rejected(self):
        model = json.loads(self.model.read_text())
        model['GameVersion'] = 'different'
        bad = self.model.parent / 'bad-profile.json'
        bad.write_text(json.dumps(model), encoding='utf-8')
        with self.assertRaises(ui.AssetBrowserUiRenderError):
            ui.build_render(self.workspace, bad)

    def test_cli_fixture_and_verify(self):
        out = self.root / 'cli-fixture'
        script = TOOLS / 'foa_asset_browser_pane_ui_render.py'
        subprocess.run([sys.executable, str(script), 'fixture', '--output', str(out)], check=True)
        manifest = next(out.rglob('foa-asset-browser-pane-ui-render.json'))
        workspace = out / 'workspace.tgworkspace.json'
        model = next(out.rglob('foa-asset-browser-pane-model.json'))
        subprocess.run([sys.executable, str(script), 'verify', '--input', str(manifest), '--workspace', str(workspace), '--model', str(model)], check=True)

if __name__ == '__main__':
    unittest.main()
