#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import json, subprocess, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_3d_preview_viewport import PreviewViewportError, build_viewport, generate_fixture, read_json, verify_viewport

class PreviewViewportTests(unittest.TestCase):
    def fixture(self):
        tmp = tempfile.TemporaryDirectory(); root = Path(tmp.name) / "fixture"; generate_fixture(root)
        workspace = root / "workspace.tgworkspace.json"
        viewport = next(root.glob("workspace/Extracted/PreviewArtifacts/Viewport3D/*/foa-3d-preview-viewport-render.json"))
        ui = root / "workspace/Extracted/PreviewArtifacts/AssetBrowserUI/assetbrowser.render.foa.mono.fixture.synthetic/foa-asset-browser-pane-ui-render.json"
        model = root / "workspace/Extracted/PreviewArtifacts/AssetBrowser/assetbrowser.model.foa.mono.fixture.synthetic/foa-asset-browser-pane-model.json"
        return tmp, root, workspace, ui, model, viewport
    def test_fixture_and_verify_success(self):
        tmp, *_rest, viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        self.assertEqual(verify_viewport(viewport)["DocumentKind"], "foa-3d-preview-viewport-render")
    def test_rejects_raw_model_as_ui_render(self):
        tmp, _root, workspace, _ui, model, _viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        with self.assertRaises(PreviewViewportError): build_viewport(workspace, model, model, captured_at="2026-07-28T00:00:01Z")
    def test_requires_matching_ui_and_model(self):
        tmp, _root, workspace, ui, model, _viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        data = read_json(ui); data["SourceAssetBrowserModelId"] = "assetbrowser.model.other.synthetic"; ui.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaises(PreviewViewportError): build_viewport(workspace, ui, model, captured_at="2026-07-28T00:00:01Z")
    def test_render_artifact_tamper_detection(self):
        tmp, root, _workspace, _ui, _model, viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        artifact = root / "workspace/Extracted/PreviewArtifacts/Viewport3D" / viewport.parent.name / "viewport" / "viewport.html"; artifact.write_text("tampered", encoding="utf-8")
        with self.assertRaises(PreviewViewportError): verify_viewport(viewport)
    def test_no_binding_authority(self):
        tmp, _root, _workspace, _ui, _model, viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        data = read_json(viewport); data["ViewportEntries"][0]["CanCreateTypedAuthoringBinding"] = True; viewport.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaises(PreviewViewportError): verify_viewport(viewport)
    def test_product_cache_token_preserved(self):
        tmp, _root, _workspace, _ui, _model, viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        products = [e for e in verify_viewport(viewport)["ViewportEntries"] if e["ViewportState"] == "product-reference-available"]
        self.assertTrue(products and products[0]["ProductCachePaths"][0].startswith("$assetcache/"))
    def test_profile_mismatch_rejected(self):
        tmp, _root, workspace, _ui, _model, viewport = self.fixture(); self.addCleanup(tmp.cleanup)
        data = read_json(viewport); data["ProfileId"] = "foa.other.fixture"; viewport.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaises(PreviewViewportError): verify_viewport(viewport, workspace_path=workspace)
    def test_cli_fixture_and_verify(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "fixture"; tool = Path(__file__).resolve().parents[1] / "foa_3d_preview_viewport.py"
            subprocess.run([sys.executable, str(tool), "fixture", "--output", str(root)], check=True)
            viewport = next(root.glob("workspace/Extracted/PreviewArtifacts/Viewport3D/*/foa-3d-preview-viewport-render.json"))
            subprocess.run([sys.executable, str(tool), "verify", "--input", str(viewport)], check=True)
if __name__ == "__main__": unittest.main()
