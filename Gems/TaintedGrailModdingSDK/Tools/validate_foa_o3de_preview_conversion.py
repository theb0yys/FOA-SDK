#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Boundary validator for the FoA O3DE preview conversion slice."""
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from foa_o3de_preview_conversion import (
    AUTHORITY_FALSE_KEYS,
    O3dePreviewConversionError,
    generate_fixture,
    read_json,
    verify_conversion,
)


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        fixture_root = Path(tmp) / "fixture"
        manifest = generate_fixture(fixture_root, replace=True)
        manifest_path = Path(manifest["ManifestPath"])
        document = verify_conversion(manifest_path)
        authority = document.get("OperationalAuthority", {})
        for key in AUTHORITY_FALSE_KEYS:
            if authority.get(key) is not False:
                raise O3dePreviewConversionError(f"authority flag is not false: {key}")
        for source in document.get("O3dePreviewSources", []):
            if source.get("GeneratedO3dePreviewProduct") is not False or source.get("O3deAssetProcessorInvoked") is not False:
                raise O3dePreviewConversionError("source incorrectly claims generated O3DE product")
        for evidence in document.get("O3dePreviewProductEvidence", []):
            if evidence.get("EvidenceState") != "asset-processor-not-invoked":
                raise O3dePreviewConversionError("product evidence must remain not-invoked")
        tampered = read_json(manifest_path)
        tampered["TransformVerified"] = True
        bad_path = manifest_path.with_name("tampered.json")
        bad_path.write_text(json.dumps(tampered), encoding="utf-8")
        try:
            verify_conversion(bad_path)
        except O3dePreviewConversionError:
            pass
        else:
            raise O3dePreviewConversionError("TransformVerified tamper was not rejected")
    print("FoA O3DE preview conversion boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
