# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Asset inventory must reject incomplete, changing or out-of-budget sources."""
from pathlib import Path
from types import SimpleNamespace
import json
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_asset_inventory as a
from foa_scene_asset_binding import SourceAssetReference
from foa_heightmap_importer import HeightmapImportError
from test_foa_scene_assets import bundle_fixture
from test_foa_scene_asset_reference import location


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'game'
        self.game.mkdir()
        self.bundle = self.game / 'asset.bundle'
        self.bundle.write_bytes(b'synthetic bundle input')
        self.catalog_file = self.game / 'catalog.json'
        self.catalog_file.write_text('{}')
        self.output = self.root / 'private' / 'manifest.json'
        self.ref = SourceAssetReference.serialized('Mesh', '1' * 32, '')
        self.locate = patch.object(SourceAssetReference, 'locate', return_value=({**location(), 'entry_id': 3}, self.bundle))
        self.locate.start()
        self.addCleanup(self.locate.stop)

    def test_typed_and_named_selectors_remain_distinct_but_exact_duplicates_share_work(self):
        material = SourceAssetReference.serialized('Material', '1' * 32, '')
        named = SourceAssetReference.serialized('Mesh', '1' * 32, 'Named')
        groups = a.plan_references([self.ref, self.ref, named, material], None, self.game)
        self.assertEqual([r for r, _ in groups[self.bundle]], [self.ref, named, material])

    def test_budget_applies_to_raw_input_even_if_all_refs_are_duplicates(self):
        with patch.object(a, 'MAX_REFERENCES', 2), self.assertRaises(HeightmapImportError):
            a.plan_references([self.ref] * 3, None, self.game)

    def test_empty_invalid_and_cancelled_plans_are_not_success(self):
        for refs in ([], [None]):
            with self.assertRaises(HeightmapImportError): a.plan_references(refs, None, self.game)
        with self.assertRaises(HeightmapImportError):
            a.plan_references([self.ref], None, self.game, lambda: True)

    def test_file_and_aggregate_byte_limits_apply_before_bundle_loading(self):
        for name in ('MAX_BUNDLE_BYTES', 'MAX_TOTAL_BYTES', 'MAX_BUNDLES'):
            with patch.object(a, name, 0), self.assertRaises(HeightmapImportError):
                a.plan_references([self.ref], None, self.game)

    def run_binding(self, progress=lambda _: None):
        file, mesh, _, _ = bundle_fixture()
        mesh.get_raw_data = Mock(return_value=b'synthetic mesh record')
        unity = SimpleNamespace(load=Mock(return_value=SimpleNamespace(objects=tuple(file.objects.values()))))
        with patch.object(a.importlib.metadata, 'version', return_value='1.24.2'), \
                patch.object(a.h, 'resolve_game_install', return_value=SimpleNamespace(catalog_path=self.catalog_file)), \
                patch.object(a, 'Catalog', return_value=None), \
                patch.object(a.h, 'import_unitypy', return_value=(unity, None)):
            return a.bind_references(self.game, [self.ref], self.output, progress=progress)

    def test_complete_synthetic_binding_publishes_exact_record_but_not_scene_or_visual_success(self):
        result = self.run_binding()
        self.assertEqual(result['source_binding'], 'PASSED')
        self.assertEqual(result['status'], 'PARTIAL')
        self.assertEqual(result['resolved'][0]['record_bytes'], len(b'synthetic mesh record'))
        for field in ('renderer_coverage', 'geometry_projection', 'native_scene_assembly', 'visual_parity', 'game_export'):
            self.assertEqual(result[field], 'NOT_RUN')
        self.assertEqual(json.loads(self.output.read_text()), result)

    def test_changed_released_bundle_or_catalog_never_publishes_a_valid_manifest(self):
        for path in (self.bundle, self.catalog_file):
            original = path.read_bytes()
            with self.assertRaises(HeightmapImportError):
                self.run_binding(lambda _: path.write_bytes(b'changed input'))
            self.assertFalse(self.output.exists())
            path.write_bytes(original)

    def test_existing_output_and_installation_targets_are_rejected_without_overwrite(self):
        self.output.parent.mkdir()
        self.output.write_bytes(b'preserve')
        with self.assertRaises(HeightmapImportError): self.run_binding()
        self.assertEqual(self.output.read_bytes(), b'preserve')
        self.output = self.game / 'forbidden.json'
        with self.assertRaises(HeightmapImportError): self.run_binding()
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
