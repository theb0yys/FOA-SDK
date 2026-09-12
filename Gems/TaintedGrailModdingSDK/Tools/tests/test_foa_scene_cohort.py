# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic scene-selection fixtures; no game-derived asset records."""
import copy
from pathlib import Path
import sys
from types import SimpleNamespace as NS
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_cohort as c
from foa_heightmap_importer import HeightmapImportError


def fixture():
    name = 'CAB-' + '1' * 32
    primary = NS(name=name)
    shared = NS(name=name + '.sharedAssets')
    tree = {'m_IsStreamedSceneAssetBundle': True,
            'm_Container': [('Assets/Unique.unity', {'asset': {'m_FileID': 0, 'm_PathID': 0}})],
            'm_SceneHashes': [('Assets/Unique.unity', name)]}
    bundle = NS(assets_file=shared, type=NS(name='AssetBundle'), byte_size=64,
                serialized_type=NS(node=object()), read_typetree=mock.Mock(side_effect=lambda **kwargs: copy.deepcopy(tree)))
    go = NS(assets_file=primary, type=NS(name='GameObject'))
    return tree, [bundle, go], name.lower()


def ref(digit):
    return {'reference': {'address': digit * 32, 'subObjectName': ''}}


def loader():
    return {'mapStaticScene': ref('1'), 'staticSubscenes': [ref('2')],
            'serializedSubscenesData': {'Scenes': [{'id': 7, 'stableUniqueId': 19, 'reference': ref('3')}],
                                       'Nodes': []}}


class SceneCohortTests(unittest.TestCase):
    def test_null_scene_container_uses_scene_hash_identity(self):
        _, objects, expected = fixture()
        self.assertEqual(c.primary_scene_file(objects, 'Assets/Unique.unity'), expected)
        self.assertEqual(c.primary_scene_file(reversed(objects), 'Assets/Unique.unity'), expected)

    def test_missing_scene_key_never_falls_back_to_only_nonshared_file(self):
        _, objects, _ = fixture()
        with self.assertRaises(HeightmapImportError): c.primary_scene_file(objects, 'Assets/Other.unity')

    def test_case_changed_internal_path_is_not_an_alias(self):
        _, objects, _ = fixture()
        with self.assertRaises(HeightmapImportError): c.primary_scene_file(objects, 'assets/unique.unity')

    def test_hash_target_must_exist_and_not_be_shared_prefab_file(self):
        for target in ('CAB-' + '2' * 32, 'CAB-' + '1' * 32 + '.sharedAssets'):
            tree, objects, _ = fixture()
            tree['m_SceneHashes'][0] = ('Assets/Unique.unity', target)
            with self.subTest(target=target), self.assertRaises(HeightmapImportError):
                c.primary_scene_file(objects, 'Assets/Unique.unity')

    def test_duplicate_tables_or_file_identity_are_rejected(self):
        for mode in ('hash', 'container', 'bundle', 'file'):
            tree, objects, _ = fixture()
            if mode == 'hash': tree['m_SceneHashes'] *= 2
            if mode == 'container': tree['m_Container'] *= 2
            if mode == 'bundle': objects.append(objects[0])
            if mode == 'file': objects.append(NS(assets_file=NS(name=objects[1].assets_file.name), type=NS(name='GameObject')))
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError):
                c.primary_scene_file(objects, 'Assets/Unique.unity')

    def test_non_scene_bundle_and_malformed_bindings_rejected(self):
        for field, value in [('m_IsStreamedSceneAssetBundle', 1), ('m_SceneHashes', []),
                             ('m_SceneHashes', [('Assets/Unique.unity', '../fake')]),
                             ('m_Container', []), ('m_Container', [None])]:
            tree, objects, _ = fixture(); tree[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(HeightmapImportError):
                c.primary_scene_file(objects, 'Assets/Unique.unity')

    def test_cancellation_and_object_budget(self):
        _, objects, _ = fixture()
        with self.assertRaises(HeightmapImportError): c.primary_scene_file(objects, 'Assets/Unique.unity', lambda: True)
        with mock.patch.object(c, 'MAX_OBJECTS', 1), self.assertRaises(HeightmapImportError):
            c.primary_scene_file(objects, 'Assets/Unique.unity')

    def test_loader_roles_ids_and_order_preserved(self):
        rows = c.loader_scene_references(loader())
        self.assertEqual([r['role'] for r in rows], ['map-static', 'static-subscene', 'dynamic-subscene'])
        self.assertEqual(rows[2], {'role': 'dynamic-subscene', 'ordinal': 0, 'id': 7,
                                    'stable_unique_id': 19, 'key': '3' * 32})

    def test_missing_guids_and_display_names_are_not_scene_keys(self):
        for address in ('', 'Assets/Unique.unity', 'Cuanacht', 'z' * 32, None):
            tree = loader(); tree['mapStaticScene']['reference']['address'] = address
            with self.subTest(address=address), self.assertRaises(HeightmapImportError): c.loader_scene_references(tree)

    def test_subobjects_duplicate_scene_loads_and_future_wrappers_reject(self):
        for mode in ('subobject', 'duplicate', 'wrapper', 'id'):
            tree = loader()
            if mode == 'subobject': tree['staticSubscenes'][0]['reference']['subObjectName'] = 'Other'
            if mode == 'duplicate': tree['staticSubscenes'][0] = ref('1')
            if mode == 'wrapper': tree['staticSubscenes'][0]['future'] = 1
            if mode == 'id': tree['serializedSubscenesData']['Scenes'][0]['id'] = True
            with self.subTest(mode=mode), self.assertRaises(HeightmapImportError): c.loader_scene_references(tree)

    def test_absent_dynamic_collection_and_oversize_never_silently_truncate(self):
        tree = loader(); del tree['serializedSubscenesData']['Scenes']
        with self.assertRaises(HeightmapImportError): c.loader_scene_references(tree)
        with mock.patch.object(c, 'MAX_SCENES', 3), self.assertRaises(HeightmapImportError): c.loader_scene_references(loader())

    def test_exact_scene_catalog_type_required(self):
        catalog = mock.Mock()
        catalog.locate_typed.return_value = {'entry_id': 4}
        catalog.main_bundle.return_value = {'internal_id': 'opaque'}
        with mock.patch.object(c, 'local_bundle_path', return_value=Path('source.bundle')):
            c.bind_scene(catalog, Path('root'), '1' * 32)
        catalog.locate_typed.assert_called_once_with('1' * 32, c.SCENE_CLASS, 'Unity.ResourceManager')


if __name__ == '__main__':
    unittest.main()
