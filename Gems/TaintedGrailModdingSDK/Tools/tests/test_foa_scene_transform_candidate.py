# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic source-translation candidates with the qualified binary serializer."""
import copy
import hashlib
from pathlib import Path
import sys
from types import SimpleNamespace as NS
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_transform_candidate as c
from foa_heightmap_importer import HeightmapImportError


class TranslationCandidateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from UnityPy.helpers import TypeTreeHelper
        from UnityPy.helpers.TypeTreeNode import TypeTreeNode
        from UnityPy.streams import EndianBinaryWriter
        cls.helper, cls.node, cls.writer = TypeTreeHelper, TypeTreeNode, EndianBinaryWriter
        cls.boost = TypeTreeHelper.read_typetree_boost
        TypeTreeHelper.read_typetree_boost = None

    @classmethod
    def tearDownClass(cls):
        cls.helper.read_typetree_boost = cls.boost

    def fixture(self, endian="<"):
        def node(kind, name, children=()):
            return self.node(m_Level=0, m_Type=kind, m_Name=name, m_ByteSize=-1, m_Version=1,
                             m_Children=list(children), m_MetaFlag=0)
        schema = node("Transform", "Base", [
            node("SInt64", "m_FatherIdentity"),
            node("Vector3f", "m_LocalPosition", [node("float", axis) for axis in "xyz"]),
            node("Quaternionf", "m_LocalRotation", [node("float", axis) for axis in "xyzw"]),
            node("UInt32", "UnmappedSentinel")])
        value = {"m_FatherIdentity": -(1 << 61) + 13, "m_LocalPosition": dict(zip("xyz", (1., 2., 3.))),
                 "m_LocalRotation": dict(zip("xyzw", (0., 0., 0., 1.))), "UnmappedSentinel": 0xFEDCBA98}
        asset = NS(name="CAB-" + "1" * 32, ref_types=[])
        writer = self.writer(endian=endian); self.helper.write_typetree(value, schema, writer, asset)
        raw = writer.bytes
        obj = NS(type=NS(name="Transform"), assets_file=asset, path_id=(1 << 62) + 5,
                 reader=NS(endian=endian), serialized_type=NS(node=schema), byte_size=len(raw),
                 get_raw_data=mock.Mock(return_value=raw), read_typetree=mock.Mock(side_effect=lambda **kw: copy.deepcopy(value)))
        binding = {"serialized_file": asset.name, "path_id": str(obj.path_id), "record_sha256": hashlib.sha256(raw).hexdigest()}
        return obj, binding, value, raw

    def test_unchanged_candidate_is_byte_exact(self):
        obj, binding, _, raw = self.fixture()
        candidate, report = c.translation_candidate(obj, binding, (1., 3., 2.), (1., 3., 2.))
        self.assertEqual(candidate, raw); self.assertEqual(report["changed_fields"], [])
        self.assertEqual(report["game_export"], "NOT_RUN")

    def test_only_position_changes_in_each_endianness(self):
        for endian in ("<", ">"):
            obj, binding, tree, raw = self.fixture(endian)
            candidate, report = c.translation_candidate(obj, binding, (1., 3., 2.), (4., 5., 6.))
            self.assertEqual(report["candidate_local_position"], [4., 6., 5.])
            self.assertEqual(report["changed_fields"], ["m_LocalPosition"])
            self.assertEqual(candidate[:8], raw[:8]); self.assertEqual(candidate[20:], raw[20:])
            self.assertEqual(obj.get_raw_data(), raw)
            self.assertEqual(tree["m_LocalPosition"], {"x": 1., "y": 2., "z": 3.})

    def test_stale_source_hash_rejected_before_decode(self):
        obj, binding, _, _ = self.fixture(); binding["record_sha256"] = "0" * 64
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (2,3,2))
        obj.read_typetree.assert_not_called()

    def test_file_and_full_path_identity_must_match(self):
        for field, value in [("serialized_file", "CAB-" + "2" * 32), ("path_id", "5")]:
            obj, binding, _, _ = self.fixture(); binding[field] = value
            with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (2,3,2))

    def test_non_transform_or_missing_schema_rejected(self):
        obj, binding, _, _ = self.fixture(); obj.type.name = "MonoBehaviour"
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (2,3,2))
        obj.type.name = "Transform"; obj.serialized_type.node = None
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (2,3,2))

    def test_changed_editor_baseline_rejected(self):
        obj, binding, _, _ = self.fixture()
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,2,3), (2,3,2))

    def test_invalid_or_unrepresentable_translation_rejected(self):
        for vector in [(True,3,2), (1,float('nan'),2), (1,float('inf'),2), (1,2), ('1',3,2), (1e100,3,2), (10**400,3,2)]:
            obj, binding, _, _ = self.fixture()
            with self.subTest(vector=vector), self.assertRaises(HeightmapImportError):
                c.translation_candidate(obj, binding, (1,3,2), vector)

    def test_unreproducible_source_record_rejected(self):
        obj, binding, tree, raw = self.fixture()
        bad = raw + b'\0'; obj.byte_size = len(bad); obj.get_raw_data.return_value = bad
        binding["record_sha256"] = hashlib.sha256(bad).hexdigest()
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (2,3,2))

    def test_sub_float_precision_edit_is_not_silently_dropped(self):
        obj, binding, _, _ = self.fixture()
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (1+1e-10,3,2))

    def test_unqualified_vector_schema_rejected(self):
        obj, binding, _, _ = self.fixture()
        obj.serialized_type.node.m_Children[1].m_Children[0].m_Type = "double"
        with self.assertRaises(HeightmapImportError): c.translation_candidate(obj, binding, (1,3,2), (2,3,2))


if __name__ == "__main__": unittest.main()
