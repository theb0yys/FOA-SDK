#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_thumbnail_artifact_extractor.py"
SPEC = importlib.util.spec_from_file_location("foa_thumbnail_artifact_extractor", MODULE_PATH)
assert SPEC and SPEC.loader
thumbs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(thumbs)


class ThumbnailArtifactExtractorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="foa-thumbnail-tests-"))
        self.install = self.root / "game" / "FoA"
        self.icons = self.install / "Tainted Grail_Data" / "LooseIcons"
        self.extracted = self.root / "workspace" / "Extracted"
        self.icons.mkdir(parents=True)
        self.extracted.mkdir(parents=True)
        (self.icons / "iron.png").write_bytes(b"synthetic-png")
        (self.icons / "ore.tga").write_bytes(b"synthetic-tga")
        self.workspace_path = self.root / "workspace.tgworkspace.json"
        workspace = {
            "SchemaVersion": 1,
            "WorkspaceId": "test.workspace",
            "DisplayName": "Test Workspace",
            "RootPath": "./workspace",
            "OutputPath": "./workspace/Build",
            "StagingPath": "./workspace/Staging",
            "DeploymentPath": "./workspace/Deploy",
            "ActiveGameProfileId": "foa.mono.test",
            "GameProfiles": [
                {
                    "ProfileId": "foa.mono.test",
                    "DisplayName": "FoA Mono Test",
                    "InstallPath": "./game/FoA",
                    "GameVersion": "1.23.401",
                    "Branch": "mono",
                    "RuntimeTarget": "Mono",
                    "UnityVersion": "6000.0.64f1",
                    "BepInExVersion": "5.4.23.3",
                    "ManagedAssembliesPath": "",
                    "PluginPath": "",
                    "DiagnosticsPath": "./workspace/Diagnostics",
                    "ExtractedDataPath": "./workspace/Extracted",
                    "DlcScopes": ["base-game"],
                }
            ],
        }
        self.workspace_path.write_bytes(thumbs.pretty_json(workspace))
        self.index_path = self.extracted / thumbs.DEFAULT_INDEX_NAME
        self.index = self.make_index()
        self.index_path.write_bytes(thumbs.pretty_json(self.index))

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def make_index(self) -> dict:
        records = []
        for ordinal, relative in enumerate(("Tainted Grail_Data/LooseIcons/iron.png", "Tainted Grail_Data/LooseIcons/ore.tga")):
            source = self.install / relative
            payload = source.read_bytes()
            records.append(
                {
                    "AssetRecordId": f"visual.asset.foa.mono.test.test{ordinal}",
                    "NativeAssetRef": "$install/" + relative,
                    "ProfileId": "foa.mono.test",
                    "GameVersion": "1.23.401",
                    "Branch": "mono",
                    "RuntimeTarget": "Mono",
                    "Locator": "$install/" + relative,
                    "FileName": source.name,
                    "Extension": source.suffix.lower(),
                    "FileKind": "loose-texture",
                    "ByteSize": len(payload),
                    "Sha256": thumbs.sha256_bytes(payload),
                    "FingerprintStatus": "hashed",
                    "PreviewEligibility": {"ThumbnailCandidate": True, "StaticPreviewCandidate": False, "RequiresExtraction": False, "Reason": "thumbnail candidate"},
                    "EvidenceKind": "visual-asset-discovery",
                    "Confidence": "observed",
                    "DiscoveryOrdinal": ordinal,
                    "CatalogPromotionAllowed": False,
                    "RuntimePermissionGranted": False,
                    "PreviewProductGenerated": False,
                }
            )
        return {
            "SchemaVersion": 1,
            "DocumentKind": thumbs.INDEX_KIND,
            "IndexId": "visual.index.foa.mono.test.synthetic",
            "ProfileId": "foa.mono.test",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "ToolId": "foa.visual-asset-discovery-index",
            "ToolVersion": "0.1.0",
            "CapturedAt": "2026-07-28T00:00:00Z",
            "InstallRoot": "$install",
            "OutputRoot": "$extracted",
            "DiscoveryScope": {"ConfiguredInstallRootOnly": True, "FileContentCopyAllowed": False, "AssemblyLoadAllowed": False, "RuntimeInvocationAllowed": False},
            "PreviewGateStatus": {"VisualPreviewGateRequired": True, "FunctionCompleteAllowed": False, "Stage": "alpha.discovery-index"},
            "AssetRecords": records,
            "Issues": [],
            "OperationalAuthority": {"RuntimeInvocationAllowed": False, "GameMutationAllowed": False, "SaveAccessAllowed": False, "CatalogPromotionAllowed": False, "RuntimePermissionGranted": False, "PreviewProductGenerated": False, "O3deAssetProcessorInvoked": False, "UnityInvoked": False, "PayloadCopied": False},
        }

    def test_extracts_generated_and_unsupported_thumbnail_artifacts(self) -> None:
        preview_root = self.extracted / "PreviewArtifacts" / "Thumbnails"
        manifest = thumbs.build_artifacts(self.workspace_path, self.index_path, preview_root=preview_root, captured_at="2026-07-28T00:00:01Z")
        self.assertFalse(manifest["PreviewStageStatus"]["FunctionCompleteAllowed"])
        generated = [item for item in manifest["ThumbnailArtifacts"] if item["Status"] == "generated"]
        unsupported = [item for item in manifest["ThumbnailArtifacts"] if item["Status"] == "unsupported"]
        self.assertEqual(len(generated), 1)
        self.assertEqual(len(unsupported), 1)
        payload = preview_root / generated[0]["ArtifactPath"].removeprefix("$preview/")
        self.assertEqual(payload.read_bytes(), b"synthetic-png")
        self.assertFalse(generated[0]["RepositoryCommitAllowed"])
        self.assertFalse(generated[0]["RuntimePermissionGranted"])

    def test_write_and_verify_manifest(self) -> None:
        preview_root = self.extracted / "PreviewArtifacts" / "Thumbnails"
        manifest_path = preview_root / thumbs.DEFAULT_MANIFEST_NAME
        manifest = thumbs.build_artifacts(self.workspace_path, self.index_path, preview_root=preview_root, captured_at="2026-07-28T00:00:01Z")
        thumbs.write_manifest(manifest, manifest_path)
        verified = thumbs.verify_manifest(manifest_path, workspace_path=self.workspace_path, index_path=self.index_path, preview_root=preview_root)
        self.assertEqual(verified["SourceIndexId"], self.index["IndexId"])

    def test_profile_mismatch_is_rejected(self) -> None:
        self.index["GameVersion"] = "1.23.999"
        self.index_path.write_bytes(thumbs.pretty_json(self.index))
        with self.assertRaisesRegex(thumbs.ThumbnailError, "exact active workspace profile"):
            thumbs.build_artifacts(self.workspace_path, self.index_path, captured_at="2026-07-28T00:00:01Z")

    def test_authority_escalation_is_rejected(self) -> None:
        preview_root = self.extracted / "PreviewArtifacts" / "Thumbnails"
        manifest = thumbs.build_artifacts(self.workspace_path, self.index_path, preview_root=preview_root, captured_at="2026-07-28T00:00:01Z")
        manifest["OperationalAuthority"]["UnityInvoked"] = True
        path = preview_root / thumbs.DEFAULT_MANIFEST_NAME
        thumbs.write_manifest(manifest, path, replace=True)
        with self.assertRaisesRegex(thumbs.ThumbnailError, "authority escalation"):
            thumbs.verify_manifest(path, preview_root=preview_root)

    def test_private_path_leakage_is_rejected(self) -> None:
        self.index["AssetRecords"][0]["NativeAssetRef"] = "C:\\Games\\FoA\\icon.png"
        self.index_path.write_bytes(thumbs.pretty_json(self.index))
        with self.assertRaisesRegex(thumbs.ThumbnailError, "absolute or private path"):
            thumbs.build_artifacts(self.workspace_path, self.index_path, captured_at="2026-07-28T00:00:01Z")

    def test_preview_output_must_remain_inside_extracted_data(self) -> None:
        with self.assertRaisesRegex(thumbs.ThumbnailError, "Preview output root"):
            thumbs.build_artifacts(self.workspace_path, self.index_path, preview_root=self.root / "outside-preview", captured_at="2026-07-28T00:00:01Z")

    def test_whole_second_utc_required(self) -> None:
        with self.assertRaisesRegex(thumbs.ThumbnailError, "whole-second UTC"):
            thumbs.build_artifacts(self.workspace_path, self.index_path, captured_at="2026-07-28T00:00:01.123Z")

    def test_cli_fixture_and_verify(self) -> None:
        fixture_root = self.root / "fixture"
        self.assertEqual(thumbs.main(["fixture", "--output", str(fixture_root)]), 0)
        manifest_path = fixture_root / "workspace" / "Extracted" / "PreviewArtifacts" / "Thumbnails" / thumbs.DEFAULT_MANIFEST_NAME
        workspace_path = fixture_root / "workspace.tgworkspace.json"
        index_path = fixture_root / "workspace" / "Extracted" / thumbs.DEFAULT_INDEX_NAME
        preview_root = fixture_root / "workspace" / "Extracted" / "PreviewArtifacts" / "Thumbnails"
        self.assertEqual(thumbs.main(["verify", "--manifest", str(manifest_path), "--workspace", str(workspace_path), "--index", str(index_path), "--preview-root", str(preview_root)]), 0)


if __name__ == "__main__":
    unittest.main()
