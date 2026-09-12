# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic exact catalog/subobject tests. No game-derived fixtures."""
import base64
import copy
from pathlib import Path
import struct
import sys
from types import SimpleNamespace as NS
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_asset_catalog as c
import foa_scene_asset_binding as b
from foa_heightmap_importer import HeightmapImportError


def catalog_fixture():
    key_values = ["asset", "dependencies", "bundle"]
    keys, offsets = bytearray(struct.pack("<i", len(key_values))), []
    for key in key_values:
        offsets.append(len(keys)); raw = key.encode("ascii")
        keys += b'\0' + struct.pack("<i", len(raw)) + raw
    refs = [(0, 1), (2,), (2,)]
    buckets = bytearray(struct.pack("<i", len(refs)))
    for offset, rows in zip(offsets, refs):
        buckets += struct.pack("<ii", offset, len(rows)) + struct.pack("<" + str(len(rows)) + "i", *rows)
    entries = struct.pack("<i", 3) + b''.join(struct.pack("<7i", *row) for row in [
        (0, 0, 1, 0, -1, 0, 0), (0, 0, 1, 0, -1, 0, 1), (1, 1, -1, 0, -1, 2, 2)])
    return {"m_KeyDataString": base64.b64encode(keys).decode(),
            "m_BucketDataString": base64.b64encode(buckets).decode(),
            "m_EntryDataString": base64.b64encode(entries).decode(),
            "m_InternalIds": ["asset", c.RUNTIME_PREFIX + "/platform/source.bundle"],
            "m_ProviderIds": ["UnityEngine.ResourceManagement.ResourceProviders.BundledAssetProvider", "UnityEngine.ResourceManagement.ResourceProviders.AssetBundleProvider"],
            "m_resourceTypes": [{"m_ClassName": x, "m_AssemblyName": ("Unity.ResourceManager" if x.endswith("IAssetBundleResource") else "UnityEngine.CoreModule") + ", Version=0.0.0.0"} for x in ["UnityEngine.GameObject", "UnityEngine.Mesh",
                "UnityEngine.ResourceManagement.ResourceProviders.IAssetBundleResource"]]}


def file():
    return NS(name="CAB-" + "1" * 32, externals=[], objects={})


def obj(f, path_id, kind, tree):
    value = NS(assets_file=f, path_id=path_id, type=NS(name=kind), byte_size=4,
               serialized_type=NS(node=object()), read_typetree=mock.Mock(return_value=tree))
    f.objects[path_id] = value
    return value


def pointer(path_id):
    return {"m_FileID": 0, "m_PathID": path_id}


def bundle_fixture():
    f = file()
    mesh = obj(f, -(1 << 62) + 3, "Mesh", {"m_Name": "Submesh"})
    main = obj(f, 1, "GameObject", {"m_Name": "Parent"})
    container = obj(f, 2, "AssetBundle", {"m_Container": [
        ("asset", {"asset": pointer(main.path_id)}), ("asset", {"asset": pointer(mesh.path_id)})]})
    return f, mesh, main, container


class CatalogTests(unittest.TestCase):
    def test_typed_key_resolves_mesh_not_first_gameobject(self):
        catalog = c.Catalog(catalog_fixture())
        mesh = catalog.locate_typed("asset", "UnityEngine.Mesh")
        self.assertEqual(mesh["entry_id"], 1)
        self.assertEqual(catalog.main_bundle(mesh)["entry_id"], 2)
        self.assertEqual(mesh["dependency_entry_ids"], [2])

    def test_bundle_provider_identity_must_match(self):
        data = catalog_fixture(); data["m_ProviderIds"][1] = "unsupported"
        catalog = c.Catalog(data)
        with self.assertRaises(HeightmapImportError): catalog.main_bundle(catalog.locate_typed("asset", "UnityEngine.Mesh"))

    def test_missing_type_or_wrong_provider_rejected(self):
        data = catalog_fixture()
        with self.assertRaises(HeightmapImportError): c.Catalog(data).locate_typed("asset", "UnityEngine.Material")
        data["m_ProviderIds"][0] = "unqualified-provider"
        with self.assertRaises(HeightmapImportError): c.Catalog(data).locate_typed("asset", "UnityEngine.Mesh")

    def test_same_class_in_wrong_assembly_is_not_the_requested_type(self):
        data = catalog_fixture(); data["m_resourceTypes"][1]["m_AssemblyName"] = "Unrelated.Assembly"
        with self.assertRaises(HeightmapImportError): c.Catalog(data).locate_typed("asset", "UnityEngine.Mesh")

    def test_ambiguous_same_type_rejected(self):
        data = catalog_fixture(); data["m_resourceTypes"][0] = data["m_resourceTypes"][1]
        with self.assertRaises(HeightmapImportError): c.Catalog(data).locate_typed("asset", "UnityEngine.Mesh")

    def test_malformed_tables_rejected(self):
        for key in ["m_KeyDataString", "m_BucketDataString", "m_EntryDataString"]:
            for encoded in ["!!!", base64.b64encode(b'\xff' * 4).decode(), base64.b64encode(b'\0' * 4).decode()]:
                data = catalog_fixture(); data[key] = encoded
                with self.subTest(key=key, encoded=encoded), self.assertRaises(HeightmapImportError): c.Catalog(data)

    def test_bad_bucket_location_reference_rejected(self):
        data = catalog_fixture(); raw = bytearray(base64.b64decode(data["m_BucketDataString"]))
        struct.pack_into("<i", raw, 12, 99); data["m_BucketDataString"] = base64.b64encode(raw).decode()
        with self.assertRaises(HeightmapImportError): c.Catalog(data)

    def test_prefix_expansion_does_not_evaluate_runtime_expressions(self):
        data = catalog_fixture(); data["m_InternalIds"][0] = "0#mesh"
        data["m_InternalIdPrefixes"] = ["{Unknown.Property}/"]
        self.assertEqual(c.Catalog(data).locate("asset")[0]["internal_id"], "{Unknown.Property}/mesh")
        with self.assertRaises(HeightmapImportError): c.local_bundle_path(Path.cwd(), "{Unknown.Property}/mesh.bundle")

    def test_path_escape_rejected_before_filesystem_lookup(self):
        for suffix in ["../outside.bundle", "C:/outside.bundle", "platform//mesh.bundle", "platform/./mesh.bundle"]:
            with self.subTest(suffix=suffix), self.assertRaises(HeightmapImportError):
                c.local_bundle_path(Path.cwd(), c.RUNTIME_PREFIX + "/" + suffix)

    def test_cancellation_stops_catalog_work(self):
        with self.assertRaises(HeightmapImportError): c.Catalog(catalog_fixture(), lambda: True)


class BundleBindingTests(unittest.TestCase):
    def test_exact_pointer_and_full_signed_identity(self):
        f, mesh, main, container = bundle_fixture()
        result = b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "Submesh")
        self.assertIs(result.object_reader, mesh)
        self.assertEqual(result.identity()["path_id"], str(mesh.path_id))
        mesh.read_typetree.assert_called_once_with(nodes=mesh.serialized_type.node, check_read=True)
        main.read_typetree.assert_not_called()

    def test_preload_or_unlisted_similar_mesh_is_not_a_candidate(self):
        f, mesh, main, container = bundle_fixture()
        obj(f, 88, "Mesh", {"m_Name": "NotAnAsset"})
        container.read_typetree.return_value["m_PreloadTable"] = [pointer(88)]
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "NotAnAsset")

    def test_duplicate_pointer_is_one_asset_but_distinct_named_objects_are_ambiguous(self):
        f, mesh, main, container = bundle_fixture(); rows = container.read_typetree.return_value["m_Container"]
        rows.append(copy.deepcopy(rows[1]))
        self.assertIs(b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "Submesh").object_reader, mesh)
        other = obj(f, 18, "Mesh", {"m_Name": "Submesh"}); rows.append(("asset", {"asset": pointer(other.path_id)}))
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "Submesh")

    def test_missing_schema_and_missing_exact_container_key_rejected(self):
        f, mesh, main, container = bundle_fixture()
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("ASSET", "Mesh", "Submesh")
        mesh.serialized_type.node = None
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "Submesh")

    def test_missing_pointer_never_searches_ambient_files(self):
        f, mesh, main, container = bundle_fixture(); del f.objects[mesh.path_id]
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "Submesh")

    def test_main_asset_requires_unique_requested_type(self):
        f, mesh, main, container = bundle_fixture()
        self.assertEqual(b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", None).name, "Submesh")
        other = obj(f, 10, "Mesh", {"m_Name": "Another"})
        container.read_typetree.return_value["m_Container"].append(("asset", {"asset": pointer(other.path_id)}))
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", None)

    def test_record_size_guard_precedes_decoder(self):
        f, mesh, main, container = bundle_fixture(); mesh.byte_size = b.MAX_ASSET_BYTES + 1
        with self.assertRaises(HeightmapImportError): b.BundleAssets(f.objects.values()).resolve("asset", "Mesh", "Submesh")
        mesh.read_typetree.assert_not_called()


if __name__ == "__main__": unittest.main()
