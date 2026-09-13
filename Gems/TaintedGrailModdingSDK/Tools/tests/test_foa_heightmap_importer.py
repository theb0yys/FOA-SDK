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
import os
import shutil
import math
import struct
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


    def test_matrix_preserves_parent_scale_and_child_rotation(self):
        def pointer(data, identity):
            return SimpleNamespace(path_id=identity, file_id=0, read=lambda: data)
        active = pointer(SimpleNamespace(m_IsActive=True), 10)
        parent_data = SimpleNamespace(m_LocalPosition=(10, 0, 0), m_LocalScale=(2, 1, 1),
            m_LocalRotation=SimpleNamespace(x=0, y=0, z=0, w=1), m_Father=None, m_GameObject=active)
        parent = pointer(parent_data, 1)
        child_data = SimpleNamespace(m_LocalPosition=(1, 0, 0), m_LocalScale=(1, 1, 1),
            m_LocalRotation=SimpleNamespace(x=0, y=0, z=math.sqrt(.5), w=math.sqrt(.5)),
            m_Father=parent, m_GameObject=active)
        child = pointer(child_data, 2)
        matrix, enabled = heightmaps.transform_matrix(child, {})
        self.assertTrue(enabled)
        for value, expected in zip(heightmaps.apply_matrix(matrix, (1, 0, 0)), (12, 1, 0)):
            self.assertAlmostEqual(value, expected)
        parent_data.m_GameObject = pointer(SimpleNamespace(m_IsActive=False), 11)
        self.assertFalse(heightmaps.transform_matrix(child, {})[1])
        parent_data.m_Father = child
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "cyclic"):
            heightmaps.transform_matrix(child, {})

    def test_game_object_resolves_transform_component_and_rejects_missing(self):
        component = SimpleNamespace(deref=lambda: SimpleNamespace(type=SimpleNamespace(name="Transform")))
        game_object = SimpleNamespace(m_Component=[SimpleNamespace(component=component)])
        self.assertIs(heightmaps.game_object_transform(game_object), component)
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "no resolvable Transform"):
            heightmaps.game_object_transform(SimpleNamespace(m_Component=[]))

    def test_bounded_bundle_directory_reader_uses_only_metadata(self):
        cab = "cab-" + "a" * 32
        directory = bytes(16) + struct.pack(">II", 0, 1) + bytes(20) + cab.encode() + b"\0"
        prefix = b"UnityFS\0" + struct.pack(">I", 7) + b"5.x.x\0" + b"0.0.0\0"
        start = (len(prefix) + 20 + 15) // 16 * 16
        payload = prefix + struct.pack(">QIII", start + len(directory), len(directory), len(directory), 0)
        payload += bytes(start - len(payload)) + directory
        path = self.temp_root / "synthetic.bundle"
        path.write_bytes(payload)
        self.assertEqual(heightmaps.read_bundle_cab_names(path), ([cab], len(directory)))
        path.write_bytes(payload[:-1])
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "sizes"):
            heightmaps.read_bundle_cab_names(path)

    def test_dependency_metadata_budget_is_cumulative_across_resolutions(self):
        root = self.temp_root / "bundles"
        root.mkdir()
        for name in ("a", "b"):
            (root / (name + ".bundle")).write_bytes(b"synthetic")
        cab_a, cab_b = "cab-" + "a" * 32, "cab-" + "b" * 32
        resolver = heightmaps.CabDependencyResolver(None, root)
        def metadata(path):
            return ([cab_a if path.stem == "a" else cab_b], 40 * 1024 * 1024)
        with mock.patch.object(heightmaps, "read_bundle_cab_names", side_effect=metadata):
            self.assertEqual(resolver.resolve(cab_a), root / "a.bundle")
            with self.assertRaisesRegex(heightmaps.HeightmapImportError, "byte budget"):
                resolver.resolve(cab_b)

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

    def test_rasterize_complete_mesh_and_normalize_u16(self) -> None:
        instance = heightmaps.MeshInstance(
            game_object_name="terrain",
            mesh_name="triangle",
            vertices=((0.0, 0.0, 0.0), (10.0, 10.0, 0.0), (0.0, 20.0, 10.0), (10.0, 30.0, 10.0)),
            triangles=((0, 1, 2), (1, 3, 2)),
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


    def output_fixture(self, op_id="terrain-import.transaction-test"):
        game_root = self.temp_root / "source"
        game_root.mkdir(exist_ok=True)
        scene_bundle = game_root / "synthetic.bundle"
        scene_bundle.write_bytes(b"synthetic scene identity; no game data")
        settings = heightmaps.ImportSettings(
            game_root=game_root, workspace_root=self.temp_root / "workspace",
            map_key="hos", resolution=2, mesh_source="collider",
            include_inactive=False, include_name_regex=None, exclude_name_regex=None,
            max_meshes=None, max_triangles=10, created_at_utc="2026-09-07T00:00:00Z",
            operation_id=op_id, profile_id="profile.synthetic", game_version="synthetic",
            branch="local", runtime_target="Mono",
        )
        return dict(
            settings=settings,
            install=heightmaps.GameInstall(game_root, game_root, game_root / "catalog.json"),
            campaign=heightmaps.CAMPAIGN_MAPS["hos"], scene_bundle=scene_bundle,
            dependency_count=0, component_stats={"accepted_components": 1},
            raster=heightmaps.RasterResult(
                2, 2, heightmaps.Bounds(0, 1, 0, 10, 0, 1), array("f", [0, 5, 7, 10]), 4, 2),
            samples=array("H", [0, 32768, 45875, 65535]),
            min_height=0, max_height=10, op_id=op_id,
        )

    def assert_no_published_output(self, workspace):
        self.assertFalse(list(workspace.glob("Derived/Terrain/**/terrain.tgheightmap.json")))
        self.assertFalse(list(workspace.glob("Derived/Terrain/**/*.terrain.u16le")))
        self.assertFalse(list(workspace.glob("SourceObservations/Terrain/**/source-observation.json")))

    def test_tile_write_failure_leaves_no_published_revision(self):
        arguments = self.output_fixture()
        original = heightmaps.write_tile
        def fail_after_tile(*args, **kwargs):
            original(*args, **kwargs)
            raise OSError("synthetic interrupted tile write")
        with mock.patch.object(heightmaps, "write_tile", side_effect=fail_after_tile):
            with self.assertRaisesRegex(OSError, "interrupted tile"):
                heightmaps.write_import_outputs(**arguments)
        self.assert_no_published_output(arguments["settings"].workspace_root)

    def test_observation_failure_leaves_no_published_revision(self):
        arguments = self.output_fixture()
        original = heightmaps.write_json_atomic
        def fail_observation(path, value):
            if path.name == "source-observation.json":
                raise OSError("synthetic observation failure")
            original(path, value)
        with mock.patch.object(heightmaps, "write_json_atomic", side_effect=fail_observation):
            with self.assertRaisesRegex(OSError, "observation failure"):
                heightmaps.write_import_outputs(**arguments)
        self.assert_no_published_output(arguments["settings"].workspace_root)

    def test_import_rejects_escaping_operation_before_writing(self):
        arguments = self.output_fixture("../../escaped")
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "operation-id"):
            heightmaps.write_import_outputs(**arguments)
        self.assert_no_published_output(arguments["settings"].workspace_root)

    def test_output_grid_and_sample_count_are_checked_before_writing(self):
        arguments = self.output_fixture()
        arguments["samples"] = array("H", [0])
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "sample"):
            heightmaps.write_import_outputs(**arguments)
        self.assert_no_published_output(arguments["settings"].workspace_root)

    def test_cancelled_publication_leaves_no_revision_and_can_retry(self):
        arguments = self.output_fixture()
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "cancelled"):
            heightmaps.write_import_outputs(**arguments, cancelled=lambda: True)
        self.assert_no_published_output(arguments["settings"].workspace_root)
        result = heightmaps.write_import_outputs(**arguments)
        self.assertTrue(Path(result["manifest_path"]).is_file())

    def test_repeat_operation_preserves_existing_manifest_and_observation(self):
        arguments = self.output_fixture()
        result = heightmaps.write_import_outputs(**arguments)
        paths = [Path(result["manifest_path"]), Path(result["source_observation_path"])]
        before = [p.read_bytes() for p in paths]
        arguments["samples"] = array("H", [0, 1, 2, 3])
        with self.assertRaises(heightmaps.HeightmapImportError):
            heightmaps.write_import_outputs(**arguments)
        self.assertEqual([p.read_bytes() for p in paths], before)



    def test_cancellation_after_observation_rolls_back_and_retries(self):
        arguments = self.output_fixture()
        state = {"cancelled": False}
        original = heightmaps.write_json_atomic
        def cancel_after_observation(path, value):
            original(path, value)
            if path.name == "source-observation.json":
                state["cancelled"] = True
        with mock.patch.object(heightmaps, "write_json_atomic", side_effect=cancel_after_observation):
            with self.assertRaisesRegex(heightmaps.HeightmapImportError, "cancelled"):
                heightmaps.write_import_outputs(**arguments, cancelled=lambda: state["cancelled"])
        self.assert_no_published_output(arguments["settings"].workspace_root)
        self.assertTrue(Path(heightmaps.write_import_outputs(**arguments)["manifest_path"]).is_file())

    def test_publish_rename_failure_rolls_back_observation_and_tiles(self):
        arguments = self.output_fixture()
        original = Path.rename
        def fail_publish(path, target):
            if target.name.startswith("terrain-revision."):
                raise OSError("synthetic publish failure")
            return original(path, target)
        with mock.patch.object(Path, "rename", new=fail_publish):
            with self.assertRaisesRegex(OSError, "publish failure"):
                heightmaps.write_import_outputs(**arguments)
        self.assert_no_published_output(arguments["settings"].workspace_root)
        self.assertTrue(Path(heightmaps.write_import_outputs(**arguments)["manifest_path"]).is_file())

    def test_existing_staging_state_is_preserved(self):
        arguments = self.output_fixture()
        staging = arguments["settings"].workspace_root / "Staging/Terrain" / arguments["op_id"]
        staging.mkdir(parents=True)
        sentinel = staging / "in-progress.txt"
        sentinel.write_bytes(b"existing operation")
        with self.assertRaisesRegex(heightmaps.HeightmapImportError, "already exists"):
            heightmaps.write_import_outputs(**arguments)
        self.assertEqual(sentinel.read_bytes(), b"existing operation")
        self.assert_no_published_output(arguments["settings"].workspace_root)

    def test_workspace_output_junction_or_symlink_is_rejected(self):
        arguments = self.output_fixture()
        workspace = arguments["settings"].workspace_root
        workspace.mkdir()
        external = self.temp_root / "unrelated"
        external.mkdir()
        sentinel = external / "keep.txt"
        sentinel.write_bytes(b"preserve unrelated data")
        link = workspace / "Derived"
        if os.name == "nt":
            import _winapi
            _winapi.CreateJunction(str(external), str(link))
        else:
            link.symlink_to(external, target_is_directory=True)
        try:
            with self.assertRaisesRegex(heightmaps.HeightmapImportError, "links or reparse"):
                heightmaps.write_import_outputs(**arguments)
            self.assertEqual(list(external.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_bytes(), b"preserve unrelated data")
        finally:
            if os.name == "nt":
                link.rmdir()
            else:
                link.unlink()

    def test_corrupted_staged_tile_is_rejected_before_publication(self):
        arguments = self.output_fixture()
        original = heightmaps.write_tile
        def corrupt_tile(samples, width, path, *dimensions):
            fingerprint = original(samples, width, path, *dimensions)
            path.write_bytes(b"corrupt")
            return fingerprint
        with mock.patch.object(heightmaps, "write_tile", side_effect=corrupt_tile):
            with self.assertRaisesRegex(heightmaps.HeightmapImportError, "tile validation"):
                heightmaps.write_import_outputs(**arguments)
        self.assert_no_published_output(arguments["settings"].workspace_root)



    def test_complete_candidate_is_staged_before_atomic_publish(self):
        arguments = self.output_fixture()
        width, height = 1025, 1025
        arguments["raster"] = heightmaps.replace(arguments["raster"], width=width, height=height)
        arguments["samples"] = array("H", [12345]) * (width * height)
        original_rename = Path.rename
        def inspect_commit(path, target):
            if target.name.startswith("terrain-revision."):
                self.assertFalse(target.exists())
                self.assertEqual(len(list(path.glob("*/terrain.tgheightmap.json"))), 1)
                self.assertEqual(len(list(path.glob("*/Tiles/*.terrain.u16le"))), 4)
                self.assertTrue(list(arguments["settings"].workspace_root.glob(
                    "SourceObservations/Terrain/**/source-observation.json")))
            return original_rename(path, target)
        with mock.patch.object(Path, "rename", new=inspect_commit):
            with mock.patch.object(heightmaps, "write_tile", wraps=heightmaps.write_tile) as tile_writer:
                result = heightmaps.write_import_outputs(**arguments)
        self.assertEqual(tile_writer.call_count, 4)
        document = json.loads(Path(result["manifest_path"]).read_text(encoding="utf-8"))
        self.assertEqual([(tile["width"], tile["height"]) for tile in document["tiles"]],
                         [(1024, 1024), (1, 1024), (1024, 1), (1, 1)])
        self.assertEqual(sum(tile["byte_size"] for tile in document["tiles"]), width * height * 2)

    def test_manifest_and_tiles_are_deterministic_across_three_workspaces(self):
        arguments = self.output_fixture()
        outputs = []
        for index in range(3):
            arguments["settings"] = heightmaps.replace(
                arguments["settings"], workspace_root=self.temp_root / f"workspace-{index}")
            result = heightmaps.write_import_outputs(**arguments)
            outputs.append((Path(result["manifest_path"]).read_bytes(),
                            [Path(path).read_bytes() for path in result["tile_paths"]]))
        self.assertEqual(outputs[0], outputs[1])
        self.assertEqual(outputs[1], outputs[2])
        self.assertNotIn(str(self.temp_root).encode(), outputs[0][0])

    def test_cleanup_error_after_commit_reports_published_result(self):
        arguments = self.output_fixture()
        error = io.StringIO()
        with mock.patch.object(heightmaps, "remove_owned_staging", side_effect=OSError("cleanup denied")):
            with contextlib.redirect_stderr(error):
                result = heightmaps.write_import_outputs(**arguments)
        self.assertTrue(Path(result["manifest_path"]).is_file())
        self.assertIn("revision published", error.getvalue())

    def test_cli_reports_io_failure_without_traceback(self):
        error = io.StringIO()
        with mock.patch.object(heightmaps, "import_campaign_map", side_effect=OSError("disk full")):
            with contextlib.redirect_stderr(error):
                code = heightmaps.main(["import", "--workspace-root", str(self.temp_root / "workspace"),
                                       "--map", "hos"])
        self.assertEqual(code, 1)
        self.assertIn("disk full", error.getvalue())
        self.assertNotIn("Traceback", error.getvalue())


if __name__ == "__main__":
    unittest.main()
