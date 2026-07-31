#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations

import shutil
import tempfile
from pathlib import Path

import foa_o3de_asset_processor_import_proof as proof


def main() -> int:
    root = Path(tempfile.mkdtemp(prefix="foa-ap-proof-validator-"))
    try:
        result = proof.generate_fixture(root, replace=True)
        manifest = next((root / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "ImportProofs").glob("*/foa-o3de-asset-processor-import-proof.json"))
        conversion = root / "workspace" / "Extracted" / "PreviewArtifacts" / "O3DE" / "o3de.preview.foa.mono.fixture.synthetic" / "foa-o3de-preview-conversion.json"
        proof.verify_proof(manifest, workspace_path=root / "workspace.tgworkspace.json", conversion_path=conversion)
        if result["ImportedProducts"] != 1:
            raise RuntimeError("fixture did not record exactly one imported product")
        print("FoA O3DE Asset Processor import proof boundary passed.")
        return 0
    finally:
        shutil.rmtree(root)


if __name__ == "__main__":
    raise SystemExit(main())
