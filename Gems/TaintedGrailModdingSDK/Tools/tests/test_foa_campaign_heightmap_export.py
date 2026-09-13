#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic campaign export contract and failure tests; no game data is used."""
import json
from dataclasses import replace
import sys
import tempfile
import unittest
from array import array
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_campaign_heightmap_export as c
h = c.h


class CampaignExportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / "Game"
        self.workspace = self.root / "Workspace"
        self.game.mkdir()
        self.workspace.mkdir()
        self.request = {
            "schema": c.SCHEMA + ".request", "schema_version": 1,
            "game_root": str(self.game), "workspace_root": str(self.workspace),
            "map": "hos", "resolution": 33, "operation_id": "terrain-import.synthetic",
            "created_at_utc": "2026-09-08T00:00:00Z", "unity_version": h.UNITY_FALLBACK_VERSION,
            "profile_binding": {"profile_id": "profile.synthetic", "game_version": "synthetic",
                "branch": "mono", "runtime_target": "Mono", "profile_fingerprint": "sha256:" + "a"*64},
        }
        self.raster = h.RasterResult(33, 33, h.Bounds(-2400,-2300,0,10,-3200,-3100),
            array("f", [float(i % 11) for i in range(1089)]), 1089, 2, bytes([1])*1089)
        self.source = self.game / "source.bundle"
        self.source.write_bytes(b"synthetic-original")
        self.inventory = c.SourceInventory(self.game, lambda: False)
        self.inventory.add(self.source)
        self.candidate = self.workspace / "candidate"

    def publish(self, **kwargs):
        return c.publish_export(self.request, self.raster, self.inventory, {"layer_mask":8192},
            {"accepted_components":1}, self.candidate, kwargs.get("cancelled",lambda:False))

    def test_legacy_campaign_import_fails_before_reading_game_or_writing_output(self):
        with mock.patch.object(h, "resolve_game_install", side_effect=AssertionError("unexpected game read")):
            with self.assertRaisesRegex(h.HeightmapImportError, "Legacy campaign import is disabled"):
                h.import_campaign_map(None)
        self.assertEqual(list(self.workspace.iterdir()), [])

    def test_campaign_entry_point_rejects_before_game_read_or_output(self):
        with mock.patch.object(h, "resolve_game_install", side_effect=AssertionError("unexpected game read")):
            with self.assertRaisesRegex(h.HeightmapImportError, "game round-trip"):
                c.export_campaign(self.request, self.candidate)
        self.assertEqual(list(self.workspace.iterdir()), [])
        self.assertEqual(self.source.read_bytes(), b"synthetic-original")

    def test_missing_sample_is_rejected_even_when_count_claims_complete(self):
        self.raster = replace(self.raster, source_coverage=bytes([1])*1088 + bytes(1))
        with self.assertRaisesRegex(h.HeightmapImportError, "will not be estimated"):
            self.publish()
        self.assertEqual(list(self.workspace.iterdir()), [])

    def test_partial_triangle_is_rejected_instead_of_filling_gaps(self):
        mesh = h.MeshInstance("synthetic", "triangle", ((0,0,0),(10,10,0),(0,20,10)), ((0,1,2),))
        with self.assertRaisesRegex(h.HeightmapImportError, "will not be estimated"):
            h.rasterize_instances([mesh], resolution=33, max_triangles=2)

    def test_publish_binds_source_profile_raw_and_metadata_without_private_paths(self):
        result = self.publish()
        out = self.workspace / result["export_relative_path"]
        receipt = json.loads((out/"receipt.json").read_text())
        sidecar = json.loads((out/"heightmap.raw.json").read_text())
        self.assertEqual(receipt["source_inventory"][0]["sha256"], h.sha256_file(self.source)[0])
        self.assertEqual(receipt["profile_binding"], self.request["profile_binding"])
        self.assertEqual(result["raw_sha256"], h.sha256_file(out/"heightmap.raw")[0])
        self.assertEqual(sidecar["source_extraction"], receipt)
        self.assertEqual(sidecar["sample_position"], "grid-vertex")
        self.assertEqual(receipt["bounds_metres"]["min_x"], -2400)
        self.assertNotIn(str(self.root), json.dumps(sidecar))
        self.assertEqual(self.source.read_bytes(), b"synthetic-original")

    def test_duplicate_operation_preserves_export(self):
        result = self.publish()
        original = (self.workspace/result["export_relative_path"]/"heightmap.raw").read_bytes()
        with self.assertRaises((h.HeightmapImportError, FileExistsError)):
            self.publish()
        self.assertEqual((self.workspace/result["export_relative_path"]/"heightmap.raw").read_bytes(), original)

    def test_changed_input_does_not_publish(self):
        self.source.write_bytes(b"changed")
        with self.assertRaisesRegex(h.HeightmapImportError, "changed"):
            self.publish()
        self.assertFalse((self.workspace/"SourceExports").exists())

    def test_cancellation_does_not_publish(self):
        with self.assertRaisesRegex(h.HeightmapImportError, "cancel"):
            self.publish(cancelled=lambda:True)
        self.assertFalse(self.candidate.exists())

    def test_sparse_wrong_coordinate_frame_fails_before_writing(self):
        self.raster = h.RasterResult(33,33,self.raster.bounds,self.raster.heights,1,2)
        with self.assertRaisesRegex(h.HeightmapImportError, "uncovered"):
            self.publish()
        self.assertFalse(self.candidate.exists())

    def test_staging_outside_workspace_is_rejected(self):
        self.candidate = self.game / "candidate"
        with self.assertRaisesRegex(h.HeightmapImportError, "staging"):
            self.publish()
        self.assertFalse(self.candidate.exists())

    def test_workspace_in_game_or_checkout_is_rejected(self):
        self.request["workspace_root"] = str(self.game/"output")
        with self.assertRaises(h.HeightmapImportError):
            c.validate_request(self.request)
        self.request["workspace_root"] = str(self.workspace)
        (self.workspace/".git").mkdir()
        with self.assertRaisesRegex(h.HeightmapImportError, "source checkouts"):
            c.validate_request(self.request)

    def test_unknown_future_and_mistyped_requests_are_rejected(self):
        for key,value in [("map","unknown"),("schema_version",2),("resolution",True),
                          ("resolution",8193),("unity_version","unknown")]:
            with self.subTest(key=key):
                request=dict(self.request); request[key]=value
                with self.assertRaises(h.HeightmapImportError):
                    c.validate_request(request)

    def test_input_byte_budget_and_containment(self):
        with mock.patch.object(c,"MAX_FILE_BYTES",4):
            with self.assertRaisesRegex(h.HeightmapImportError,"budget"):
                c.SourceInventory(self.game,lambda:False).add(self.source)
        outside=self.root/"outside.bundle";outside.write_bytes(b"x")
        with self.assertRaisesRegex(h.HeightmapImportError,"registered install"):
            self.inventory.add(outside)

    def test_selection_must_be_unique_finite_and_qualified(self):
        tree={"RaycastLayerMask":{"m_Bits":8192},"RaycastTerrainBounds":{
            "m_Center":{"x":0,"y":0,"z":0},"m_Extent":{"x":100,"y":10,"z":100}}}
        reader=SimpleNamespace(type=SimpleNamespace(name="MonoBehaviour"),path_id=77,read_typetree=lambda:tree)
        self.assertEqual(c.selection_from_environment(SimpleNamespace(objects=[reader]))["layer_mask"],8192)
        for objects in ([],[reader,reader]):
            with self.assertRaises(h.HeightmapImportError):
                c.selection_from_environment(SimpleNamespace(objects=objects))
        tree["RaycastLayerMask"]["m_Bits"]=1
        with self.assertRaises(h.HeightmapImportError):
            c.selection_from_environment(SimpleNamespace(objects=[reader]))
        tree["RaycastLayerMask"]["m_Bits"]=8192
        tree["RaycastTerrainBounds"]["m_Center"]["x"]=float("nan")
        with self.assertRaises(h.HeightmapImportError):
            c.selection_from_environment(SimpleNamespace(objects=[reader]))

    def test_world_positioned_meshes_rasterize_in_their_own_bounds(self):
        mesh=h.MeshInstance("synthetic-ground","synthetic",
            ((-2400,0,-3200),(-2300,10,-3200),(-2400,0,-3100),(-2300,10,-3100)),
            ((0,1,2),(1,3,2)))
        raster=h.rasterize_instances([mesh],resolution=33,max_triangles=2)
        self.assertEqual(raster.filled_samples,1089)
        self.assertEqual(raster.bounds.min_x,-2400)
        self.assertGreater(max(raster.heights),9.9)

    def test_raster_cancellation_and_degenerate_triangle_budget(self):
        mesh=h.MeshInstance("synthetic","synthetic",((0,0,0),(1,0,0),(0,1,1)),((0,0,0),)*3)
        with self.assertRaisesRegex(h.HeightmapImportError,"cancel"):
            h.rasterize_instances([mesh],resolution=33,max_triangles=4,cancelled=lambda:True)
        with self.assertRaisesRegex(h.HeightmapImportError,"max-triangles"):
            h.rasterize_instances([mesh],resolution=33,max_triangles=2)


if __name__ == "__main__":
    unittest.main()
