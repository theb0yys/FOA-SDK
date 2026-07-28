#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT

from __future__ import annotations

import copy
import shutil
import tempfile
import unittest
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import foa_neutral_preview_handoff as handoff


class NeutralPreviewHandoffTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="foa-neutral-handoff-test-"))
        self.install = self.root / "game" / "FoA"
        self.icons = self.install / "Tainted Grail_Data" / "LooseIcons"
        self.extracted = self.root / "workspace" / "Extracted"
        self.preview = self.extracted / "PreviewArtifacts" / "Thumbnails"
        self.icons.mkdir(parents=True)
        self.preview.mkdir(parents=True)
        self.source_payload = b"synthetic-png"
        (self.icons / "iron.png").write_bytes(self.source_payload)
        self.workspace_path = self.root / "workspace.tgworkspace.json"
        self.workspace_path.write_bytes(handoff.pretty_json({
            "SchemaVersion": 1,
            "WorkspaceId": "fixture.workspace",
            "DisplayName": "Fixture",
            "RootPath": "./workspace",
            "OutputPath": "./workspace/Build",
            "StagingPath": "./workspace/Staging",
            "DeploymentPath": "./workspace/Deploy",
            "ActiveGameProfileId": "foa.mono.fixture",
            "GameProfiles": [{
                "ProfileId": "foa.mono.fixture",
                "DisplayName": "Fixture",
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
            }],
        }))
        self.profile = handoff.load_profile(self.workspace_path)
        self.source_sha = handoff.sha256_bytes(self.source_payload)
        self.asset_id = "visual.asset.foa.mono.fixture.aaaaaaaaaaaaaaaa"
        self.index = {
            "SchemaVersion": 1,
            "DocumentKind": handoff.INDEX_KIND,
            "IndexId": "visual.index.foa.mono.fixture.synthetic",
            "ProfileId": "foa.mono.fixture",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "ToolId": "foa.visual-asset-discovery-index",
            "ToolVersion": "0.1.0",
            "CapturedAt": "2026-07-28T00:00:00Z",
            "PreviewGateStatus": {"FunctionCompleteAllowed": False},
            "AssetRecords": [{
                "AssetRecordId": self.asset_id,
                "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png",
                "ProfileId": "foa.mono.fixture",
                "GameVersion": "1.23.401",
                "Branch": "mono",
                "RuntimeTarget": "Mono",
                "Sha256": self.source_sha,
            }],
            "OperationalAuthority": {"FunctionCompleteAllowed": False},
        }
        self.index_path = self.extracted / "foa-visual-asset-index.json"
        self.index_path.write_bytes(handoff.pretty_json(self.index))
        self.thumbnail_id = "thumbnail.foa.mono.fixture.aaaaaaaaaaaaaaaa"
        self.thumbnail_payload_path = self.preview / f"{self.thumbnail_id}.png"
        self.thumbnail_payload_path.write_bytes(self.source_payload)
        self.thumbnail_manifest = {
            "SchemaVersion": 1,
            "DocumentKind": handoff.THUMBNAIL_KIND,
            "ManifestId": "thumbnail.manifest.foa.mono.fixture.synthetic",
            "SourceIndexId": self.index["IndexId"],
            "ProfileId": "foa.mono.fixture",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "ToolId": "foa.thumbnail-artifact-extractor",
            "ToolVersion": "0.1.0",
            "CapturedAt": "2026-07-28T00:00:01Z",
            "PreviewStageStatus": {"FunctionCompleteAllowed": False},
            "ThumbnailArtifacts": [{
                "ThumbnailArtifactId": self.thumbnail_id,
                "AssetRecordId": self.asset_id,
                "NativeAssetRef": "$install/Tainted Grail_Data/LooseIcons/iron.png",
                "SourceIndexId": self.index["IndexId"],
                "SourceSha256": self.source_sha,
                "ArtifactPath": "$preview/" + self.thumbnail_payload_path.name,
                "ArtifactKind": "native-icon-thumbnail",
                "ArtifactExtension": ".png",
                "GenerationMethod": "local-only-loose-icon-copy",
                "Fidelity": "native-icon-byte-preserved",
                "Status": "generated",
                "LocalOnly": True,
                "RedistributionAllowed": False,
                "RepositoryCommitAllowed": False,
                "PreviewProductGenerated": False,
                "O3deAssetProcessorInvoked": False,
                "UnityInvoked": False,
                "RuntimePermissionGranted": False,
                "CapturedAt": "2026-07-28T00:00:01Z",
                "ArtifactSha256": self.source_sha,
                "ArtifactByteSize": len(self.source_payload),
            }],
            "Issues": [],
            "OperationalAuthority": {"FunctionCompleteAllowed": False},
        }
        self.thumbnail_manifest_path = self.preview / "foa-thumbnail-artifacts.json"
        self.thumbnail_manifest_path.write_bytes(handoff.pretty_json(self.thumbnail_manifest))

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def build(self) -> tuple[dict, Path]:
        return handoff.build_handoff(
            self.workspace_path,
            self.index_path,
            self.thumbnail_manifest_path,
            captured_at="2026-07-28T00:00:02Z",
        )

    def test_handoff_has_source_collection_and_primary_source(self) -> None:
        doc, path = self.build()
        verified = handoff.verify_handoff(path, workspace_path=self.workspace_path, index_path=self.index_path, thumbnail_manifest_path=self.thumbnail_manifest_path)
        self.assertEqual(verified["PrimarySourceAssetRecordId"], self.asset_id)
        self.assertEqual(verified["SourceAssetRecordIds"], [self.asset_id])
        self.assertEqual(verified["SourceDependencies"][0]["SourceAssetRecordId"], self.asset_id)
        self.assertEqual(verified["PreviewEntries"][0]["PrimarySourceAssetRecordId"], self.asset_id)

    def test_coordinate_declaration_is_separate_from_conversion_evidence(self) -> None:
        doc, path = self.build()
        self.assertIn("CoordinateDeclaration", doc)
        self.assertIn("CoordinateConversionEvidence", doc)
        self.assertFalse(doc["CoordinateConversionEvidence"]["ConversionOperationPerformed"])
        self.assertEqual(doc["CoordinateConversionEvidence"]["VerificationState"], "not-verified")
        self.assertNotIn("TransformVerified", doc)

    def test_payload_hash_is_verified(self) -> None:
        doc, path = self.build()
        payload = doc["Payloads"][0]
        payload_file = path.parent / payload["Path"].removeprefix("$handoff/")
        payload_file.write_bytes(b"synthetic-pnx")
        with self.assertRaisesRegex(handoff.HandoffError, "SHA-256"):
            handoff.verify_handoff(path)

    def test_authority_escalation_is_rejected(self) -> None:
        doc, path = self.build()
        doc["OperationalAuthority"]["UnityInvoked"] = True
        path.write_bytes(handoff.pretty_json(doc))
        with self.assertRaisesRegex(handoff.HandoffError, "UnityInvoked"):
            handoff.verify_handoff(path)

    def test_top_level_transform_verified_is_rejected(self) -> None:
        doc, path = self.build()
        doc["TransformVerified"] = False
        path.write_bytes(handoff.pretty_json(doc))
        with self.assertRaisesRegex(handoff.HandoffError, "TransformVerified"):
            handoff.verify_handoff(path)

    def test_unsupported_receipt_is_modeled_as_metadata_payload(self) -> None:
        unsupported = copy.deepcopy(self.thumbnail_manifest["ThumbnailArtifacts"][0])
        unsupported["ThumbnailArtifactId"] = "thumbnail.foa.mono.fixture.unsupported"
        unsupported["Status"] = "unsupported"
        unsupported["ArtifactPath"] = ""
        unsupported.pop("ArtifactSha256", None)
        unsupported.pop("ArtifactByteSize", None)
        unsupported["Reason"] = ".tga thumbnail decode is not implemented"
        self.thumbnail_manifest["ThumbnailArtifacts"].append(unsupported)
        self.thumbnail_manifest_path.write_bytes(handoff.pretty_json(self.thumbnail_manifest))
        doc, path = self.build()
        metadata_payloads = [payload for payload in doc["Payloads"] if payload["Role"] == "metadata"]
        self.assertEqual(len(metadata_payloads), 1)
        handoff.verify_handoff(path)

    def test_fixture_and_cli_verify_succeed(self) -> None:
        fixture_root = self.root / "fixture"
        manifest = handoff.generate_fixture(fixture_root)
        self.assertEqual(manifest["PreviewEntryCount"], 1)
        handoff_path = Path(manifest["HandoffPath"])
        self.assertEqual(handoff.main(["verify", "--input", str(handoff_path)]), 0)


if __name__ == "__main__":
    unittest.main()
