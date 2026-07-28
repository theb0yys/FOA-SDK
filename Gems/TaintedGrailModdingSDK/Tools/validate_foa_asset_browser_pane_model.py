#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import tempfile
from pathlib import Path

import foa_asset_browser_pane_model as pane


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td) / "fixture"
        result = pane.generate_fixture(root, replace=True)
        workspace = root / "workspace.tgworkspace.json"
        proof = root / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "ImportProofs" / "proof.fixture" / "foa-o3de-asset-processor-import-proof.json"
        model = Path(result["ModelPath"])
        loaded = pane.verify_model(model, workspace_path=workspace, import_proof_path=proof)
        assert loaded["InputContract"]["ImportProofEvidenceConsumed"] is True
        assert loaded["InputContract"]["RawConversionFileConsumed"] is False
        assert loaded["PreviewStageStatus"]["FunctionCompleteAllowed"] is False
        assert all(entry["SelectionPolicy"]["CanCreateTypedAuthoringBinding"] is False for entry in loaded["PaneEntries"])
    print("FoA Asset Browser pane model boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
