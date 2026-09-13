# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic source-record and protected-output diagnostics tests."""
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_source_audit as audit


def record(kind="Transform", file="CAB-synthetic", path_id=1, node=True):
    return SimpleNamespace(assets_file=SimpleNamespace(name=file), path_id=path_id,
                           type=SimpleNamespace(name=kind), serialized_type=SimpleNamespace(node=object() if node else None),
                           byte_size=4, read_typetree=mock.Mock(return_value={"m_LocalPosition": {"x": 0, "y": 0, "z": 0}}),
                           get_raw_data=mock.Mock(return_value=b"same"))


class SceneSourceAuditTests(unittest.TestCase):
    def test_object_ids_are_scoped_to_serialized_file(self):
        result = audit.inspect_objects([record(file="CAB-a"), record(file="CAB-b")], lambda *_: b"same")
        self.assertEqual(result["object_count"], 2)
        self.assertEqual(result["byte_identical_records"], {"Transform": 2})
        with self.assertRaisesRegex(audit.h.HeightmapImportError, "Duplicate source object"):
            audit.inspect_objects([record(), record()], lambda *_: b"same")

    def test_missing_embedded_schema_is_never_inferred(self):
        source = record(node=False)
        encode = mock.Mock(side_effect=AssertionError("schema inferred"))
        result = audit.inspect_objects([source], encode)
        source.read_typetree.assert_not_called()
        encode.assert_not_called()
        self.assertEqual(result["uninterpreted_records"], {"Transform": 1})
        self.assertEqual(result["byte_identical_records"], {})

    def test_roundtrip_difference_is_reported_without_modifying_source(self):
        source = record()
        result = audit.inspect_objects([source], lambda *_: b"changed")
        self.assertEqual(result["different_records"], {"Transform": 1})
        self.assertEqual(result["byte_identical_records"], {})
        self.assertEqual(source.get_raw_data(), b"same")
        source.read_typetree.assert_called_once_with(nodes=source.serialized_type.node, check_read=True)

    def test_unmapped_components_remain_counted(self):
        source = record(kind="UnknownComponent")
        result = audit.inspect_objects([source], lambda *_: b"same")
        self.assertEqual(result["uninterpreted_records"], {"UnknownComponent": 1})
        source.read_typetree.assert_not_called()

    def test_all_records_mode_checks_unknown_embedded_records_without_schema_fallback(self):
        source = record(kind="UnknownComponent")
        result = audit.inspect_objects([source], lambda *_: b"same", all_records=True)
        self.assertEqual(result["byte_identical_records"], {"UnknownComponent": 1})
        source.serialized_type.node = None
        source.read_typetree.reset_mock()
        result = audit.inspect_objects([source], lambda *_: b"same", all_records=True)
        self.assertEqual(result["uninterpreted_records"], {"UnknownComponent": 1})
        source.read_typetree.assert_not_called()

    def test_record_limit_and_cancellation(self):
        source = record()
        source.byte_size = audit.MAX_RECORD_BYTES + 1
        result = audit.inspect_objects([source], lambda *_: b"same")
        self.assertEqual(result["byte_identical_records"], {})
        source.read_typetree.assert_not_called()
        with self.assertRaisesRegex(audit.h.HeightmapImportError, "cancel"):
            audit.inspect_objects([record()], lambda *_: b"same", lambda: True)

    def test_read_failure_does_not_become_roundtrip_success(self):
        source = record()
        source.read_typetree.side_effect = ValueError("bad record")
        result = audit.inspect_objects([source], lambda *_: b"same")
        self.assertEqual(result["byte_identical_records"], {})
        self.assertEqual(result["uninterpreted_records"], {"Transform": 1})

    def test_encode_failure_is_distinct_from_decode_failure(self):
        source = record()
        result = audit.inspect_objects([source], mock.Mock(side_effect=KeyError("synthetic-field")))
        self.assertEqual(result["decode_failures"], {})
        self.assertEqual(result["encode_failures"], {"Transform": 1})
        self.assertEqual(result["failures"][0]["stage"], "encode")
        self.assertEqual(result["failures"][0]["path_id"], "1")
        self.assertEqual(result["byte_identical_records"], {})
        self.assertEqual(source.get_raw_data(), b"same")

    def test_scene_identity_selection_rejects_absent_and_duplicate_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve(); game = root / "Game"; game.mkdir()
            source = game / "synthetic.bundle"; source.write_bytes(b"synthetic")
            output = root / "Evidence" / "audit.json"
            name = "CAB-" + "1" * 32
            for selection in [[name, name.lower()], ["guessed filename"], [True]]:
                with self.subTest(selection=selection), mock.patch.object(audit.h, "import_unitypy") as load:
                    with self.assertRaises(audit.h.HeightmapImportError):
                        audit.audit(game, [source], output, primary_scene_files=selection)
                    load.assert_not_called()
            # No selected scene is present: an empty parser result must never pass ownership.
            empty_unity = SimpleNamespace(load=mock.Mock(return_value=SimpleNamespace(objects=[])))
            with mock.patch.object(audit.h, "import_unitypy", return_value=(empty_unity, None)):
                with self.assertRaisesRegex(audit.h.HeightmapImportError, "absent"):
                    audit.audit(game, [source], output, primary_scene_files=[name])
            self.assertFalse(output.exists())
            self.assertEqual(source.read_bytes(), b"synthetic")

    def test_output_boundaries_and_existing_evidence_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            game = root / "Game"
            game.mkdir()
            source = game / "synthetic.bundle"
            source.write_bytes(b"synthetic")
            output = root / "Evidence" / "audit.json"
            self.assertEqual(audit.validate_paths(game, [source], output)[1], [source])
            with self.assertRaisesRegex(audit.h.HeightmapImportError, "separate"):
                audit.validate_paths(game, [source], game / "audit.json")
            with self.assertRaisesRegex(audit.h.HeightmapImportError, "Duplicate"):
                audit.validate_paths(game, [source, source], output)
            output.parent.mkdir()
            output.write_bytes(b"prior-evidence")
            with self.assertRaisesRegex(audit.h.HeightmapImportError, "already exists"):
                audit.validate_paths(game, [source], output)
            self.assertEqual(output.read_bytes(), b"prior-evidence")
            checkout = root / "Checkout"
            checkout.mkdir()
            (checkout / ".git").mkdir()
            with self.assertRaisesRegex(audit.h.HeightmapImportError, "outside source control"):
                audit.validate_paths(game, [source], checkout / "audit.json")
            self.assertEqual(source.read_bytes(), b"synthetic")

if __name__ == "__main__":
    unittest.main()
