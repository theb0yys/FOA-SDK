# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Qualified-parser tests with synthetic managed-reference records only."""
import copy
import importlib.metadata
from pathlib import Path
import struct
import sys
from types import SimpleNamespace as NS
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_scene_record_writer import null_reference_writer


class NullReferenceWriterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if importlib.metadata.version("UnityPy") != "1.24.2":
            raise RuntimeError("Run these parser contract tests in the qualified UnityPy 1.24.2 environment.")
        from UnityPy.helpers import TypeTreeHelper
        from UnityPy.helpers.TypeTreeNode import TypeTreeNode
        from UnityPy.streams import EndianBinaryReader, EndianBinaryWriter
        cls.helper = TypeTreeHelper
        cls.node = TypeTreeNode
        cls.reader = EndianBinaryReader
        cls.writer = EndianBinaryWriter
        cls.previous_boost = TypeTreeHelper.read_typetree_boost
        TypeTreeHelper.read_typetree_boost = None

    @classmethod
    def tearDownClass(cls):
        cls.helper.read_typetree_boost = cls.previous_boost

    def schema(self):
        def node(kind, name, children=()):
            return self.node(m_Level=0, m_Type=kind, m_Name=name, m_ByteSize=-1, m_Version=1,
                             m_Children=list(children), m_MetaFlag=0)
        type_node = node("Type", "type", [node("string", name) for name in ("class", "ns", "asm")])
        reference = node("ReferencedObject", "reference", [node("SInt64", "rid"), type_node,
                                                          node("ReferencedObjectData", "data")])
        asset = NS(ref_types=[NS(m_ClassName="Sample", m_NameSpace="Tests", m_AssemblyName="Synthetic",
                                node=node("int", "value"))])
        return reference, asset

    def encode(self, value, node, asset):
        writer = self.writer(endian="<")
        self.helper.write_typetree(value, node, writer, asset)
        return writer.bytes

    def test_null_reference_roundtrip_and_upstream_failure_are_both_proven(self):
        node, asset = self.schema()
        original = struct.pack("<qiii", -2, 0, 0, 0)
        value = self.helper.read_typetree(node, self.reader(original, endian="<"), as_dict=True,
                                        byte_size=len(original), check_read=True, assetsfile=asset)
        untouched = copy.deepcopy(value)
        self.assertNotIn("data", value)
        with self.assertRaises(KeyError):
            self.encode(value, node, asset)
        original_function = self.helper.write_value
        with null_reference_writer(self.helper, "1.24.2") as counts:
            self.assertEqual(self.encode(value, node, asset), original)
            self.assertEqual(counts["null_references"], 1)
        self.assertIs(self.helper.write_value, original_function)
        self.assertEqual(value, untouched)
        self.assertEqual(node.m_Children[-1].m_Type, "ReferencedObjectData")

    def test_nested_null_keeps_recursive_writer_behavior(self):
        node, asset = self.schema()
        root = self.node(m_Level=0, m_Type="Synthetic", m_Name="root", m_ByteSize=-1,
                         m_Version=1, m_Children=[node], m_MetaFlag=0)
        value = {"reference": {"rid": -2, "type": {"class": "", "ns": "", "asm": ""}}}
        with null_reference_writer(self.helper, "1.24.2") as counts:
            self.assertEqual(self.encode(value, root, asset), struct.pack("<qiii", -2, 0, 0, 0))
            self.assertEqual(counts["null_references"], 1)

    def test_nonnull_data_is_still_written_by_original_encoder(self):
        node, asset = self.schema()
        value = {"rid": 123, "type": {"class": "Sample", "ns": "Tests", "asm": "Synthetic"}, "data": 42}
        baseline = self.encode(value, node, asset)
        with null_reference_writer(self.helper, "1.24.2") as counts:
            self.assertEqual(self.encode(value, node, asset), baseline)
            self.assertEqual(counts["null_references"], 0)
        self.assertEqual(struct.unpack_from("<i", baseline, len(baseline)-4)[0], 42)

    def test_missing_nonnull_type_is_not_treated_as_null(self):
        node, asset = self.schema()
        value = {"rid": 123, "type": {"class": "Unknown", "ns": "Tests", "asm": "Synthetic"}, "data": 42}
        original = self.helper.write_value
        with self.assertRaisesRegex(ValueError, "Referenced type not found"):
            with null_reference_writer(self.helper, "1.24.2"):
                self.encode(value, node, asset)
        self.assertIs(self.helper.write_value, original)

    def test_unexpected_null_payload_is_rejected_and_hook_restored(self):
        node, asset = self.schema()
        value = {"rid": -2, "type": {"class": "", "ns": "", "asm": ""}, "data": 42}
        original = self.helper.write_value
        with self.assertRaisesRegex(ValueError, "Unexpected data"):
            with null_reference_writer(self.helper, "1.24.2"):
                self.encode(value, node, asset)
        self.assertIs(self.helper.write_value, original)

    def test_unqualified_parser_version_is_rejected(self):
        original = self.helper.write_value
        with self.assertRaisesRegex(ValueError, "1.24.2"):
            with null_reference_writer(self.helper, "1.25.0"):
                self.fail("unqualified parser accepted")
        self.assertIs(self.helper.write_value, original)


if __name__ == "__main__":
    unittest.main()
