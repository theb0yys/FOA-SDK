#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations

import importlib.util
import shutil
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))
MODULE_PATH = TOOLS_ROOT / "foa_thumbnail_artifact_extractor.py"
SPEC = importlib.util.spec_from_file_location(
    "foa_thumbnail_artifact_extractor_extended_test",
    MODULE_PATH,
)
assert SPEC and SPEC.loader
thumbs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(thumbs)


def tga_2x2() -> bytes:
    header = struct.pack(
        "<BBBHHBHHHHBB",
        0,
        0,
        2,
        0,
        0,
        0,
        0,
        0,
        2,
        2,
        24,
        0x20,
    )
    return header + bytes(
        (
            0, 0, 255,
            0, 255, 0,
            255, 0, 0,
            255, 255, 255,
        )
    )


def dds_bc1(*, fourcc: bytes = b"DXT1", caps2: int = 0, depth: int = 0) -> bytes:
    block = struct.pack("<HHI", 0xF800, 0x07E0, 0)
    header = bytearray(124)
    struct.pack_into(
        "<7I",
        header,
        0,
        124,
        0x0002100F,
        4,
        4,
        len(block),
        depth,
        1,
    )
    struct.pack_into(
        "<II4s5I",
        header,
        72,
        32,
        0x4,
        fourcc,
        0,
        0,
        0,
        0,
        0,
    )
    struct.pack_into("<5I", header, 104, 0x1000, caps2, 0, 0, 0)
    return b"DDS " + bytes(header) + block


class ExtendedThumbnailArtifactExtractorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="foa-thumbnail-extended-"))
        self.install = self.root / "game" / "FoA"
        self.icons = self.install / "Tainted Grail_Data" / "LooseIcons"
        self.extracted = self.root / "workspace" / "Extracted"
        self.icons.mkdir(parents=True)
        self.extracted.mkdir(parents=True)
        self.files = {
            "iron.png": thumbs.encode_png_rgba(
                1,
                1,
                bytes((255, 255, 255, 255)),
            ),
            "ore.tga": tga_2x2(),
            "gem.dds": dds_bc1(),
        }
        for name, payload in self.files.items():
            (self.icons / name).write_bytes(payload)

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
        self.preview_root = self.extracted / "PreviewArtifacts" / "Thumbnails"

    def tearDown(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def make_index(self) -> dict:
        records = []
        for ordinal, (name, payload) in enumerate(self.files.items()):
            relative = f"Tainted Grail_Data/LooseIcons/{name}"
            records.append(
                {
                    "AssetRecordId": f"visual.asset.foa.mono.test.ext{ordinal}",
                    "NativeAssetRef": "$install/" + relative,
                    "ProfileId": "foa.mono.test",
                    "GameVersion": "1.23.401",
                    "Branch": "mono",
                    "RuntimeTarget": "Mono",
                    "Locator": "$install/" + relative,
                    "FileName": name,
                    "Extension": Path(name).suffix.lower(),
                    "FileKind": "loose-texture",
                    "ByteSize": len(payload),
                    "Sha256": thumbs.sha256_bytes(payload),
                    "FingerprintStatus": "hashed",
                    "PreviewEligibility": {
                        "ThumbnailCandidate": True,
                        "StaticPreviewCandidate": False,
                        "RequiresExtraction": Path(name).suffix.lower() in {".dds", ".tga"},
                        "Reason": "thumbnail candidate",
                    },
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
            "IndexId": "visual.index.foa.mono.test.extended",
            "ProfileId": "foa.mono.test",
            "GameVersion": "1.23.401",
            "Branch": "mono",
            "RuntimeTarget": "Mono",
            "ToolId": "foa.visual-asset-discovery-index",
            "ToolVersion": "0.1.0",
            "CapturedAt": "2026-07-28T00:00:00Z",
            "InstallRoot": "$install",
            "OutputRoot": "$extracted",
            "DiscoveryScope": {
                "ConfiguredInstallRootOnly": True,
                "FileContentCopyAllowed": False,
                "AssemblyLoadAllowed": False,
                "RuntimeInvocationAllowed": False,
            },
            "PreviewGateStatus": {
                "VisualPreviewGateRequired": True,
                "FunctionCompleteAllowed": False,
                "Stage": "alpha.discovery-index",
            },
            "AssetRecords": records,
            "Issues": [],
            "OperationalAuthority": {
                "RuntimeInvocationAllowed": False,
                "GameMutationAllowed": False,
                "SaveAccessAllowed": False,
                "CatalogPromotionAllowed": False,
                "RuntimePermissionGranted": False,
                "PreviewProductGenerated": False,
                "O3deAssetProcessorInvoked": False,
                "UnityInvoked": False,
                "PayloadCopied": False,
            },
        }

    def build_and_write(self) -> tuple[dict, Path]:
        manifest = thumbs.build_artifacts(
            self.workspace_path,
            self.index_path,
            preview_root=self.preview_root,
            captured_at="2026-07-28T00:00:01Z",
        )
        path = self.preview_root / thumbs.DEFAULT_MANIFEST_NAME
        thumbs.write_manifest(manifest, path, replace=True)
        return manifest, path

    def test_builds_copy_tga_and_dds_outputs(self) -> None:
        manifest, path = self.build_and_write()
        self.assertEqual(manifest["PreviewStageStatus"]["GeneratedArtifactCount"], 3)
        self.assertEqual(manifest["PreviewStageStatus"]["UnsupportedArtifactCount"], 0)
        methods = {item["GenerationMethod"] for item in manifest["ThumbnailArtifacts"]}
        self.assertEqual(
            methods,
            {
                "local-only-loose-icon-copy",
                "local-only-bounded-tga-decode",
                "local-only-bounded-dds-decode",
            },
        )
        decoded = [
            item
            for item in manifest["ThumbnailArtifacts"]
            if item["GenerationMethod"].startswith("local-only-bounded")
        ]
        self.assertTrue(all(item["ArtifactExtension"] == ".png" for item in decoded))
        self.assertTrue(all(item["OutputMediaType"] == "image/png" for item in decoded))
        self.assertTrue(all(item["DecodedWidth"] > 0 for item in decoded))
        verified = thumbs.verify_manifest(
            path,
            workspace_path=self.workspace_path,
            index_path=self.index_path,
            preview_root=self.preview_root,
        )
        self.assertEqual(verified["ToolVersion"], "0.2.0")

    def test_source_fingerprint_and_missing_size_fail_closed(self) -> None:
        (self.icons / "gem.dds").write_bytes(dds_bc1(fourcc=b"DXT3"))
        with self.assertRaisesRegex(thumbs.ThumbnailError, "drift"):
            thumbs.build_artifacts(
                self.workspace_path,
                self.index_path,
                preview_root=self.preview_root,
                captured_at="2026-07-28T00:00:01Z",
            )
        (self.icons / "gem.dds").write_bytes(self.files["gem.dds"])
        del self.index["AssetRecords"][2]["ByteSize"]
        self.index_path.write_bytes(thumbs.pretty_json(self.index))
        with self.assertRaisesRegex(thumbs.ThumbnailError, "ByteSize"):
            thumbs.build_artifacts(
                self.workspace_path,
                self.index_path,
                preview_root=self.preview_root,
                captured_at="2026-07-28T00:00:01Z",
            )

    def test_unsupported_dds_subformat_emits_receipt(self) -> None:
        payload = dds_bc1(fourcc=b"ZZZZ")
        (self.icons / "gem.dds").write_bytes(payload)
        record = self.index["AssetRecords"][2]
        record["ByteSize"] = len(payload)
        record["Sha256"] = thumbs.sha256_bytes(payload)
        self.index_path.write_bytes(thumbs.pretty_json(self.index))
        manifest = thumbs.build_artifacts(
            self.workspace_path,
            self.index_path,
            preview_root=self.preview_root,
            captured_at="2026-07-28T00:00:01Z",
        )
        unsupported = [item for item in manifest["ThumbnailArtifacts"] if item["Status"] == "unsupported"]
        self.assertEqual(len(unsupported), 1)
        self.assertEqual(unsupported[0]["GenerationMethod"], "unsupported-receipt")
        self.assertTrue(any(item["Code"] == "thumbnail-decode-unsupported" for item in manifest["Issues"]))

    def test_decoded_payload_tampering_is_rejected(self) -> None:
        manifest, path = self.build_and_write()
        decoded = next(
            item
            for item in manifest["ThumbnailArtifacts"]
            if item["GenerationMethod"] == "local-only-bounded-dds-decode"
        )
        payload_path = self.preview_root / decoded["ArtifactPath"].removeprefix("$preview/")
        payload_path.write_bytes(b"tampered")
        with self.assertRaisesRegex(thumbs.ThumbnailError, "size mismatch|SHA-256 mismatch"):
            thumbs.verify_manifest(
                path,
                workspace_path=self.workspace_path,
                index_path=self.index_path,
                preview_root=self.preview_root,
            )

    def test_cubemap_and_volume_dds_emit_unsupported_receipts(self) -> None:
        for payload in (dds_bc1(caps2=0x200), dds_bc1(caps2=0x200000, depth=2)):
            (self.icons / "gem.dds").write_bytes(payload)
            record = self.index["AssetRecords"][2]
            record["ByteSize"] = len(payload)
            record["Sha256"] = thumbs.sha256_bytes(payload)
            self.index_path.write_bytes(thumbs.pretty_json(self.index))
            manifest = thumbs.build_artifacts(
                self.workspace_path,
                self.index_path,
                preview_root=self.preview_root,
                captured_at="2026-07-28T00:00:01Z",
            )
            unsupported = [item for item in manifest["ThumbnailArtifacts"] if item["Status"] == "unsupported"]
            self.assertEqual(len(unsupported), 1)
            self.assertIn("cubemaps and volume", unsupported[0]["Reason"])


if __name__ == "__main__":
    unittest.main()
