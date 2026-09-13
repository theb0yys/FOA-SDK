# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Main-asset and named-subobject selection must retain distinct source identities."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_asset_binding as b
from foa_heightmap_importer import HeightmapImportError
from test_foa_scene_assets import bundle_fixture, obj, pointer

GUID = '1' * 32

def location():
    return {'internal_id': 'asset', 'provider': 'UnityEngine.ResourceManagement.ResourceProviders.BundledAssetProvider',
            'resource_type': {'m_ClassName': 'UnityEngine.Mesh', 'm_AssemblyName': 'UnityEngine.CoreModule'}}


class SourceReferenceTests(unittest.TestCase):
    def test_main_mesh_has_exact_container_pointer_without_name_guess(self):
        f, mesh, _, _ = bundle_fixture()
        ref = b.SourceAssetReference.serialized('Mesh', GUID, '')
        self.assertIsNone(ref.subobject_name)
        self.assertEqual(ref.key, GUID)
        self.assertIs(ref.bind(b.BundleAssets(f.objects.values()), location()).object_reader, mesh)

    def test_main_mesh_with_multiple_candidates_fails_instead_of_selecting_first(self):
        f, _, _, container = bundle_fixture()
        other = obj(f, 88, 'Mesh', {'m_Name': 'Second mesh'})
        container.read_typetree.return_value['m_Container'].append(('asset', {'asset': pointer(other.path_id)}))
        ref = b.SourceAssetReference.serialized('Mesh', GUID, '')
        with self.assertRaises(HeightmapImportError): ref.bind(b.BundleAssets(f.objects.values()), location())

    def test_runtime_and_serialized_selectors_agree_without_collapsing_named_selection(self):
        for subobject in ('', 'Submesh', 'Child [2]'):
            a = b.SourceAssetReference.serialized('Mesh', GUID, subobject)
            self.assertEqual(a, b.SourceAssetReference.runtime_key('Mesh', a.key))
        self.assertNotEqual(b.SourceAssetReference.serialized('Mesh', GUID, ''),
                            b.SourceAssetReference.serialized('Mesh', GUID, 'Submesh'))

    def test_typed_identity_distinguishes_mesh_from_material(self):
        mesh = b.SourceAssetReference.serialized('Mesh', GUID, '')
        material = b.SourceAssetReference.serialized('Material', GUID, '')
        self.assertNotEqual(mesh, material)
        self.assertEqual(len({mesh, material}), 2)

    def test_named_reference_does_not_fall_back_to_main_mesh(self):
        f, _, _, _ = bundle_fixture()
        ref = b.SourceAssetReference.serialized('Mesh', GUID, 'Wrong')
        with self.assertRaises(HeightmapImportError): ref.bind(b.BundleAssets(f.objects.values()), location())

    def test_malformed_or_future_reference_is_rejected(self):
        for kind, guid, sub in [('GameObject', GUID, ''), ('Mesh', '2'*31, ''), ('Mesh', 'z'*32, ''),
                                 ('Mesh', GUID, None), ('Mesh', GUID, '\0bad'), ('Mesh', GUID, 'x'*16385)]:
            with self.subTest(kind=kind, guid=guid), self.assertRaises(HeightmapImportError):
                b.SourceAssetReference.serialized(kind, guid, sub)
        for key in (GUID+'[]', GUID+'[unclosed', 'Assets/ByName', None, 'x'*16419):
            with self.subTest(key=key), self.assertRaises(HeightmapImportError):
                b.SourceAssetReference.runtime_key('Mesh', key)

    def test_wrong_catalog_type_assembly_or_provider_cannot_bind(self):
        f, _, _, _ = bundle_fixture(); assets = b.BundleAssets(f.objects.values())
        ref = b.SourceAssetReference.serialized('Mesh', GUID, '')
        for field, value in [('m_ClassName', 'UnityEngine.Material'), ('m_AssemblyName', 'Other')]:
            loc = location(); loc['resource_type'][field] = value
            with self.subTest(field=field), self.assertRaises(HeightmapImportError): ref.bind(assets, loc)
        loc = location(); loc['provider'] = 'Other'
        with self.assertRaises(HeightmapImportError): ref.bind(assets, loc)


if __name__ == '__main__':
    unittest.main()
