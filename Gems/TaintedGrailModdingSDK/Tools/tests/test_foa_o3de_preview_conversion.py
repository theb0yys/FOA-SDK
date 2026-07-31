#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from foa_o3de_preview_conversion import (
    O3dePreviewConversionError,
    build_conversion,
    generate_fixture,
    read_json,
    synthetic_handoff,
    verify_conversion,
)


class O3dePreviewConversionTests(unittest.TestCase):
    def test_fixture_verify_success(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            manifest = generate_fixture(Path(tmp) / "fixture", replace=True)
            document = verify_conversion(Path(manifest["ManifestPath"]))
            self.assertEqual(1, len(document["O3dePreviewSources"]))
            self.assertFalse(document["PreviewStageStatus"]["GeneratedO3dePreviewProduct"])

    def test_conversion_keeps_product_evidence_not_invoked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "fixture"
            workspace, handoff = synthetic_handoff(root)
            document, manifest_path = build_conversion(workspace, handoff, captured_at="2026-07-28T00:00:02Z")
            verified = verify_conversion(manifest_path, workspace_path=workspace, handoff_path=handoff)
            self.assertEqual(document["ConversionId"], verified["ConversionId"])
            self.assertEqual("asset-processor-not-invoked", verified["O3dePreviewProductEvidence"][0]["EvidenceState"])
            self.assertFalse(verified["O3dePreviewProductEvidence"][0]["O3deAssetProcessorInvoked"])

    def test_payload_tamper_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "fixture"
            manifest = generate_fixture(root, replace=True)
            manifest_path = Path(manifest["ManifestPath"])
            document = read_json(manifest_path)
            source_path = manifest_path.parent / document["O3dePreviewSources"][0]["PreviewSourcePath"].removeprefix("$o3depreview/")
            source_path.write_bytes(b"tampered")
            with self.assertRaises(O3dePreviewConversionError):
                verify_conversion(manifest_path)

    def test_authority_escalation_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "fixture"
            manifest = generate_fixture(root, replace=True)
            manifest_path = Path(manifest["ManifestPath"])
            document = read_json(manifest_path)
            document["OperationalAuthority"]["O3deAssetProcessorInvoked"] = True
            bad_path = manifest_path.with_name("bad-authority.json")
            bad_path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaises(O3dePreviewConversionError):
                verify_conversion(bad_path)

    def test_top_level_transform_verified_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "fixture"
            manifest = generate_fixture(root, replace=True)
            manifest_path = Path(manifest["ManifestPath"])
            document = read_json(manifest_path)
            document["TransformVerified"] = False
            bad_path = manifest_path.with_name("bad-transform.json")
            bad_path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaises(O3dePreviewConversionError):
                verify_conversion(bad_path)

    def test_workspace_profile_mismatch_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "fixture"
            manifest = generate_fixture(root, replace=True)
            workspace = root / "workspace.tgworkspace.json"
            data = read_json(workspace)
            data["GameProfiles"][0]["GameVersion"] = "9.9.9"
            workspace.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaises(O3dePreviewConversionError):
                verify_conversion(Path(manifest["ManifestPath"]), workspace_path=workspace)


if __name__ == "__main__":
    unittest.main()
