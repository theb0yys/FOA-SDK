# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic component identity and opaque navigation-container tests."""
import hashlib
from pathlib import Path
import struct
import sys
from types import SimpleNamespace as NS
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_component_audit as c
import foa_scene_navigation_audit as n
import foa_scene_source_audit as s

A, B = "CAB-" + "1" * 32, "CAB-" + "2" * 32


def asset(name=A, external=()):
    return NS(name=name, externals=[NS(path=x) for x in external], objects={})


def obj(file, path_id, kind="MonoBehaviour", tree=None):
    value = NS(assets_file=file, path_id=path_id, type=NS(name=kind),
               serialized_type=NS(node=object()), byte_size=4,
               read_typetree=mock.Mock(return_value=tree or {}),
               get_raw_data=mock.Mock(return_value=b"same"))
    file.objects[path_id] = value
    return value


def script(file, path_id, name="Sample"):
    return obj(file, path_id, "MonoScript", {"m_AssemblyName": "Synthetic", "m_Namespace": "Tests", "m_ClassName": name})


def ref(file_id=0, path_id=1):
    return {"m_FileID": file_id, "m_PathID": path_id}


def container(entries):
    result = bytearray(struct.pack("<i", len(entries)))
    for name, payload in entries:
        encoded = name.encode("utf-8")
        length = len(encoded)
        while length >= 128:
            result.append((length & 127) | 128)
            length >>= 7
        result.append(length)
        result.extend(encoded)
        result.extend(struct.pack("<i", len(payload)))
        result.extend(payload)
    return bytes(result)


class ComponentBindingsTests(unittest.TestCase):
    def test_path_ids_keep_64_bit_precision_and_file_scope(self):
        first, second = asset(external=["archive:/" + B + "/" + B]), asset(B)
        path_id = (1 << 62) + 17
        source = obj(first, 4)
        local, other = script(first, path_id, "Local"), script(second, path_id, "External")
        load = mock.Mock(return_value=[other])
        audit = c.ComponentScriptAudit([source, local], load)
        audit.observe(source, {"m_Script": ref(0, path_id)})
        audit.observe(source, {"m_Script": ref(1, path_id)})
        rows = audit.report()["types"]
        self.assertEqual({row["class"] for row in rows}, {"Local", "External"})
        self.assertTrue(all(row["path_id"] == str(path_id) for row in rows))
        self.assertEqual(audit.report()["resolved_components"], 2)
        load.assert_called_once_with(B.lower())
        other.read_typetree.assert_called_once_with(nodes=other.serialized_type.node, check_read=True)

    def test_repeated_components_resolve_shared_script_once(self):
        file = asset()
        target = script(file, 1)
        source = obj(file, 2)
        load = mock.Mock(side_effect=AssertionError("ambient lookup"))
        audit = c.ComponentScriptAudit([source, target], load)
        for _ in range(5):
            audit.observe(source, {"m_Script": ref()})
        self.assertEqual(audit.report()["resolved_components"], 5)
        target.read_typetree.assert_called_once()
        load.assert_not_called()
        self.assertEqual(audit.report()["editable_mapping"], "NOT_RUN")

    def test_bad_shared_dependency_is_not_retried_per_object(self):
        source = obj(asset(external=[B]), 2)
        load = mock.Mock(side_effect=OSError("unavailable"))
        audit = c.ComponentScriptAudit([source], load)
        for _ in range(5):
            audit.observe(source, {"m_Script": ref(1)})
        load.assert_called_once()
        self.assertEqual(audit.report()["unresolved_components"], 5)
        self.assertEqual(audit.report()["resolved_components"], 0)

    def test_bad_pointer_never_reaches_loader(self):
        file = asset(external=[B])
        invalid = [ref(-1), ref(2), ref(0, 0), ref(0, True), ref(0, 1 << 63), {},
                   {"m_FileID": 0, "m_PathID": 1, "label": "Sample"}]
        for pointer in invalid:
            with self.subTest(pointer=pointer):
                with self.assertRaises(c.h.HeightmapImportError):
                    c.reference_key(file, pointer)
        with self.assertRaises(c.h.HeightmapImportError):
            c.reference_key(asset(external=["../../fake.dll"]), ref(1))

    def test_wrong_target_type_and_missing_schema_cannot_resolve(self):
        for kind, node in [("GameObject", object()), ("MonoScript", None)]:
            file = asset()
            source = obj(file, 2)
            target = obj(file, 1, kind)
            target.serialized_type.node = node
            audit = c.ComponentScriptAudit([source, target], mock.Mock())
            audit.observe(source, {"m_Script": ref()})
            target.read_typetree.assert_not_called()
            self.assertEqual(audit.report()["unresolved_components"], 1)

    def test_ambiguous_file_identity_is_rejected(self):
        with self.assertRaisesRegex(c.h.HeightmapImportError, "Ambiguous"):
            c.ComponentScriptAudit([obj(asset(), 1), obj(asset(), 2)], mock.Mock())

    def test_component_cancellation_propagates(self):
        source = obj(asset(), 2)
        audit = c.ComponentScriptAudit([source], mock.Mock(), lambda: True)
        with self.assertRaisesRegex(c.h.HeightmapImportError, "cancel"):
            audit.observe(source, {"m_Script": ref()})

    def test_unresolved_mapping_does_not_erase_preserved_component_record(self):
        source = obj(asset(), 2, tree={"m_Script": ref()})
        bindings = c.ComponentScriptAudit([source], mock.Mock())
        report = s.inspect_objects([source], lambda *_: b"same", components=bindings)
        self.assertEqual(report["byte_identical_records"], {"MonoBehaviour": 1})
        self.assertEqual(bindings.report()["unresolved_components"], 1)
        self.assertEqual(source.get_raw_data(), b"same")


class NavigationContainerTests(unittest.TestCase):
    def test_preserves_opaque_payload_offsets_names_and_hashes(self):
        values = [("graph0.json", b'{"synthetic":true}'), ("graph0_extra.binary", b"\0\xff\x01\x7f"),
                  ("name_" + "x" * 130, b"")]
        data = container(values)
        entries = n.inspect_cache(data)
        self.assertEqual([row.name for row in entries], [name for name, _ in values])
        for row, (_, payload) in zip(entries, values):
            self.assertEqual(data[row.offset:row.offset + row.size], payload)
            self.assertEqual(row.sha256, hashlib.sha256(payload).hexdigest())

    def test_truncation_is_rejected_at_every_byte(self):
        data = container([("one", b"data"), ("two", b"more")])
        for length in range(len(data)):
            with self.subTest(length=length):
                with self.assertRaises(n.h.HeightmapImportError):
                    n.inspect_cache(data[:length])

    def test_duplicate_entries_and_trailing_bytes_are_rejected(self):
        with self.assertRaisesRegex(n.h.HeightmapImportError, "Duplicate"):
            n.inspect_cache(container([("same", b"a"), ("same", b"b")]))
        with self.assertRaisesRegex(n.h.HeightmapImportError, "trailing"):
            n.inspect_cache(container([("one", b"a")]) + b"unknown")

    def test_integer_and_allocation_limits(self):
        for count in (-1, 0, n.MAX_ENTRIES + 1):
            with self.assertRaisesRegex(n.h.HeightmapImportError, "count"):
                n.inspect_cache(struct.pack("<i", count))
        for length in (-1, n.MAX_ENTRY_BYTES + 1):
            with self.assertRaisesRegex(n.h.HeightmapImportError, "byte limit"):
                n.inspect_cache(struct.pack("<i", 1) + b"\x01x" + struct.pack("<i", length))
        with self.assertRaisesRegex(n.h.HeightmapImportError, "size bounds"):
            with mock.patch.object(n, "MAX_CACHE_BYTES", 5):
                n.inspect_cache(container([("one", b"a")]))

    def test_invalid_utf8_and_overflowing_prefix(self):
        for raw in (b"\x01\xff", b"\xff\xff\xff\xff\xff", b"\x00"):
            with self.assertRaises(n.h.HeightmapImportError):
                n.inspect_cache(struct.pack("<i", 1) + raw + struct.pack("<i", 0))

    def test_entry_names_are_data_not_filesystem_paths(self):
        data = container([("../../opaque", b"payload")])
        self.assertEqual(n.inspect_cache(data)[0].name, "../../opaque")

    def test_navigation_cancellation(self):
        with self.assertRaisesRegex(n.h.HeightmapImportError, "cancel"):
            n.inspect_cache(container([("one", b"data")]), lambda: True)


if __name__ == "__main__":
    unittest.main()
