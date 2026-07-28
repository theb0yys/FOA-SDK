#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Standalone boundary validator for Asset Browser pane UI rendering."""
from __future__ import annotations
import tempfile
from pathlib import Path
from foa_asset_browser_pane_ui_render import AssetBrowserUiRenderError, generate_fixture, verify_render


def main() -> int:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp) / 'fixture'
        result = generate_fixture(root)
        manifest = Path(result['RenderPath'])
        workspace = root / 'workspace.tgworkspace.json'
        model = next(root.rglob('foa-asset-browser-pane-model.json'))
        verified = verify_render(manifest, workspace_path=workspace, model_path=model)
        if verified['PreviewStageStatus']['FunctionCompleteAllowed'] is not False:
            raise AssetBrowserUiRenderError('FunctionCompleteAllowed must remain false')
        if verified['PreviewStageStatus']['O3deEditorPaneMutated'] is not False:
            raise AssetBrowserUiRenderError('O3DE editor pane mutation must remain false')
        for entry in verified['UiEntries']:
            if entry['CanCreateTypedAuthoringBinding'] is not False:
                raise AssetBrowserUiRenderError('UI entry cannot create typed binding')
        html = manifest.parent / 'ui' / 'asset-browser-pane.html'
        html.write_text('tampered', encoding='utf-8')
        try:
            verify_render(manifest, workspace_path=workspace, model_path=model)
        except AssetBrowserUiRenderError:
            print('FoA Asset Browser UI render boundary passed.')
            return 0
        raise AssetBrowserUiRenderError('tampered render artifact was accepted')

if __name__ == '__main__':
    raise SystemExit(main())
