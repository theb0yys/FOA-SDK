#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import base64
import contextlib
import io
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from array import array
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

TOOLS_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = TOOLS_ROOT / "foa_heightmap_importer.py"
SPEC = importlib.util.spec_from_file_location("foa_heightmap_importer", MODULE_PATH)
assert SPEC and SPEC.loader
heightmaps = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = heightmaps
SPEC.loader.exec_module(heightmaps)


def key_table(*keys: str) -> str:
    payload = bytearray()
    payload.extend(len(keys).to_bytes(4, "little", signed=True))
    for key in keys:
        raw = key.encode("utf-8")
        payload.append(0)
        payload.extend(len(raw).to_bytes(4, "little", signed=True))
        payload.extend(raw)
    return base64.b64encode(bytes(payload)).decode("ascii")


class FoAHeightmapImporterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_root = Path(tempfile.mkdtemp(prefix="foa-heightmap-importer-tests-"))

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_root, ignore_errors=True)

    def test_addressable_key_table_decodes_campaign_keys(self) -> None:
        encoded = key_table("CampaignMap_HOS", "CampaignMap_HOS_merged_Static", "Other")
        self.assertEqual(
            heightmaps.decode_addressable_key_data(encoded),
            ["CampaignMap_HOS", "CampaignMap_HOS_merged_Static", "Other"],
        )

    def test_list_campaign_maps_reports_scene_bundle_and_catalog_key_presence(self) -> None:
        install_root = self.temp_root / "Tainted Grail FoA"
        bundle_root = install_root / heightmaps.WINDOWS_BUNDLE_DIR
        bundle_root.mkdir(parents=True)
        (install_root / heightmaps.GAME_EXE_NAME).write_bytes(b"exe")
        hos = bundle_root / heightmaps.CAMPAIGN_MAPS["hos"].scene_bundle_name
        hos.write_bytes(b"bundle")
        catalog = install_root / heightmaps.CATALOG_PATH
        catalog.parent.mkdir(parents=True, exist_ok=True)
        catalog.write_text(json.dumps({"m_KeyDataString": key_table("CampaignMap_HOS")}), encoding="utf-8")

        install = heightmaps.resolve_game_install(install_root)
        maps = {entry["key"]: entry for entry in heightmaps.list_campaign_maps(install)}

        self.assertTrue(maps["hos"]["scene_bundle_exists"])
        self.assertEqual(maps["hos"]["scene_bundle_bytes"], len(b"bundle"))
        self.assertEqual(maps["hos"]["addressable_keys_present"], ["CampaignMap_HOS"])
        self.assertFalse(maps["sarras"]["scene_bundle_exists"])

    def test_transform_pose_composes_parent_scale_rotation_and_position(self) -> None:
        self.assertEqual(heightmaps.vector3((1, 2, 3)), (1.0, 2.0, 3.0))
        parent = heightmaps.Pose((10.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (2.0, 2.0, 2.0))
        child = heightmaps.Pose((1.0, 2.0, 3.0), (0.0, 0.0, 0.0, 1.0), (0.5, 1.0, 1.5))

        world = heightmaps.compose_pose(parent, child)

        self.assertEqual(world.position, (12.0, 4.0, 6.0))
        self.assertEqual(world.scale, (1.0, 2.0, 3.0))

    def test_rasterize_triangle_fills_grid_and_normalizes_u16(self) -> None:
        instance = heightmaps.MeshInstance(
            game_object_name="terrain",
            mesh_name="triangle",
            vertices=((0.0, 0.0, 0.0), (10.0, 10.0, 0.0), (0.0, 20.0, 10.0)),
            triangles=((0, 1, 2),),
        )

        raster = heightmaps.rasterize_instances([instance], resolution=8, max_triangles=10)
        samples, min_height, max_height = heightmaps.normalize_u16(raster.heights)

        self.assertEqual(raster.width, 8)
        self.assertEqual(raster.height, 8)
        self.assertGreater(raster.filled_samples, 0)
        self.assertEqual(len(samples), 64)
        self.assertEqual(min(samples), 0)
        self.assertEqual(max(samples), 65535)
        self.assertLessEqual(min_height, 0.0)
        self.assertGreaterEqual(max_height, 20.0)

    def test_write_import_outputs_publishes_manifest_tiles_and_observation(self) -> None:
        game_root = self.temp_root / "Game"
        bundle_root = game_root / heightmaps.WINDOWS_BUNDLE_DIR
        bundle_root.mkdir(parents=True)
        (game_root / heightmaps.GAME_EXE_NAME).write_bytes(b"exe")
        catalog = game_root / heightmaps.CATALOG_PATH
        catalog.parent.mkdir(parents=True, exist_ok=True)
        catalog.write_text(json.dumps({"m_KeyDataString": key_table("CampaignMap_HOS")}), encoding="utf-8")
        scene_bundle = bundle_root / heightmaps.CAMPAIGN_MAPS["hos"].scene_bundle_name
        scene_bundle.write_bytes(b"scene-bundle")
        workspace_root = self.temp_root / "workspace"
        install = heightmaps.resolve_game_install(game_root)
        settings = heightmaps.ImportSettings(
            game_root=game_root,
            workspace_root=workspace_root,
            map_key="hos",
            resolution=2,
            mesh_source="collider",
            include_inactive=False,
            include_name_regex=None,
            exclude_name_regex=None,
            max_meshes=None,
            max_triangles=10,
            created_at_utc="2026-08-25T00:00:00Z",
            operation_id="terrain-import.test-hos",
            profile_id="profile.foa.test",
            game_version="test-build",
            branch="local",
            runtime_target="Mono",
        )
        self.assertEqual(
            heightmaps.operation_id(settings, heightmaps.CAMPAIGN_MAPS["hos"]),
            "terrain-import.test-hos",
        )
        raster = heightmaps.RasterResult(
            width=2,
            height=2,
            bounds=heightmaps.Bounds(0.0, 1.0, 0.0, 10.0, 0.0, 1.0),
            heights=array("f", [0.0, 5.0, 7.0, 10.0]),
            filled_samples=4,
            rasterized_triangles=2,
        )
        samples, min_height, max_height = heightmaps.normalize_u16(raster.heights)

        result = heightmaps.write_import_outputs(
            settings=settings,
            install=install,
            campaign=heightmaps.CAMPAIGN_MAPS["hos"],
            scene_bundle=scene_bundle,
            dependency_count=3,
            component_stats={"accepted_components": 1},
            raster=raster,
            samples=samples,
            min_height=min_height,
            max_height=max_height,
            op_id="terrain-import.test-hos",
        )

        manifest = Path(result["manifest_path"])
        observation = Path(result["source_observation_path"])
        self.assertTrue(manifest.is_file())
        self.assertTrue(observation.is_file())
        document = json.loads(manifest.read_text(encoding="utf-8"))
        self.assertEqual(document["schema"], "foa.terrain-heightmap")
        self.assertEqual(document["source_binding"]["source_kind"], "user-exported-raw-u16-le")
        self.assertEqual(document["authority"]["runtime_use_allowed"], False)
        tile = manifest.parent / document["tiles"][0]["relative_path"]
        self.assertEqual(tile.read_bytes(), b"\x00\x00\x00\x802\xb3\xff\xff")
        self.assertEqual(Path(result["tile_paths"][0]), tile)

        blocked_settings = heightmaps.replace(settings, workspace_root=game_root / "Derived")
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "must not be inside"):
            heightmaps.write_import_outputs(
                settings=blocked_settings,
                install=install,
                campaign=heightmaps.CAMPAIGN_MAPS["hos"],
                scene_bundle=scene_bundle,
                dependency_count=3,
                component_stats={"accepted_components": 1},
                raster=raster,
                samples=samples,
                min_height=min_height,
                max_height=max_height,
                op_id="terrain-import.test-blocked",
            )

    def test_import_requires_safe_ids_and_existing_game_layout(self) -> None:
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "Unable to find"):
            heightmaps.resolve_game_install(self.temp_root / "missing")
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "profile-id"):
            heightmaps.require_id("Profile.FoA.Bad", "profile-id")

    def test_import_settings_rejects_bad_limits_before_loading_unity(self) -> None:
        parser = heightmaps.build_parser()

        args = parser.parse_args(
            [
                "import",
                "--workspace-root",
                str(self.temp_root / "workspace"),
                "--map",
                "hos",
                "--resolution",
                "1",
            ]
        )
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "resolution"):
            heightmaps.settings_from_args(args)

        args = parser.parse_args(
            [
                "import",
                "--workspace-root",
                str(self.temp_root / "workspace"),
                "--map",
                "hos",
                "--max-meshes",
                "0",
            ]
        )
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "max-meshes"):
            heightmaps.settings_from_args(args)

        args = parser.parse_args(
            [
                "import",
                "--workspace-root",
                str(self.temp_root / "workspace"),
                "--map",
                "hos",
                "--max-triangles",
                "0",
            ]
        )
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "max-triangles"):
            heightmaps.settings_from_args(args)

    def test_invalid_name_regex_fails_cleanly_before_import(self) -> None:
        error = io.StringIO()
        with mock.patch.object(heightmaps, "import_campaign_map", side_effect=AssertionError("should not import")):
            with contextlib.redirect_stderr(error):
                exit_code = heightmaps.main(
                    [
                        "import",
                        "--workspace-root",
                        str(self.temp_root / "workspace"),
                        "--map",
                        "hos",
                        "--include-name-regex",
                        "[",
                    ]
                )

        self.assertEqual(exit_code, 1)
        self.assertIn("include-name-regex", error.getvalue())
        self.assertIn("valid regular expression", error.getvalue())

    def test_programmatic_limits_match_cli_validation(self) -> None:
        instance = heightmaps.MeshInstance(
            game_object_name="terrain",
            mesh_name="triangle",
            vertices=((0.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 2.0, 1.0)),
            triangles=((0, 1, 2),),
        )
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "max-triangles"):
            heightmaps.rasterize_instances([instance], resolution=2, max_triangles=0)

        settings = heightmaps.ImportSettings(
            game_root=None,
            workspace_root=self.temp_root / "workspace",
            map_key="hos",
            resolution=2,
            mesh_source="collider",
            include_inactive=False,
            include_name_regex="[",
            exclude_name_regex=None,
            max_meshes=None,
            max_triangles=10,
            created_at_utc="2026-08-25T00:00:00Z",
            operation_id="terrain-import.test-hos",
            profile_id="profile.foa.test",
            game_version="test-build",
            branch="local",
            runtime_target="Mono",
        )
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "include-name-regex"):
            heightmaps.collect_mesh_instances(type("Env", (), {"objects": []})(), object, settings)

    def test_collect_mesh_instances_preserves_import_errors(self) -> None:
        settings = heightmaps.ImportSettings(
            game_root=None,
            workspace_root=self.temp_root / "workspace",
            map_key="hos",
            resolution=2,
            mesh_source="collider",
            include_inactive=False,
            include_name_regex=None,
            exclude_name_regex=None,
            max_meshes=None,
            max_triangles=10,
            created_at_utc="2026-08-25T00:00:00Z",
            operation_id="terrain-import.test-hos",
            profile_id="profile.foa.test",
            game_version="test-build",
            branch="local",
            runtime_target="Mono",
        )
        game_object = SimpleNamespace(m_IsActive=True, m_Name="terrain", m_Transform=None)
        component = SimpleNamespace(
            m_Enabled=True,
            m_GameObject=SimpleNamespace(read=lambda: game_object),
            m_Mesh=SimpleNamespace(path_id=1),
        )
        env = SimpleNamespace(
            objects=[
                SimpleNamespace(
                    type=SimpleNamespace(name="MeshCollider"),
                    read=lambda: component,
                )
            ]
        )
        resolver = SimpleNamespace(
            ensure_loaded=lambda _environment, _pointer: (_ for _ in ()).throw(
                heightmaps.HeightmapImportError("missing dependency")
            )
        )

        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "missing dependency"):
            heightmaps.collect_mesh_instances(env, object, settings, dependency_resolver=resolver)


if __name__ == "__main__":
    unittest.main()
