# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Synthetic NPC mapping and component identity regressions; no game assets."""
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_native_population import actor_fields, REQUIRED_FIELDS
from foa_native_economy import component_rows
from foa_native_item_preview import PreviewError


def npc(**changes):
    return {"npcData": {}, "npcType": 99, "level": 12, "maxHealth": 100,
        "_isAbstract": 0, "tags": ["fixture:test"], **changes}


def pointer(data, file_id=0):
    obj = SimpleNamespace(type=SimpleNamespace(name="MonoBehaviour"), parse_as_dict=lambda: data)
    return SimpleNamespace(m_FileID=file_id, deref=lambda: obj)


def environment(components):
    go = SimpleNamespace(type=SimpleNamespace(name="GameObject"), path_id=123,
        parse_as_object=lambda: SimpleNamespace(m_Component=[SimpleNamespace(component=p) for p in components]))
    return SimpleNamespace(container={"Assets/Synthetic/Actor.prefab": SimpleNamespace(deref=lambda: go)})


class NativePopulationTests(unittest.TestCase):
    def test_unknown_native_enum_does_not_invent_actor_kind_or_visuals(self):
        fields = actor_fields(npc(npcType=999))
        self.assertEqual(fields["actor_kind"], "other")
        self.assertEqual((fields["minimum_level"], fields["maximum_level"]), (12, 12))
        self.assertNotIn("portrait_ref", fields)
        self.assertNotIn("model_ref", fields)

    def test_abstract_native_byte_and_boolean_keep_explicit_template_tag(self):
        for flag in (True, 1):
            data = npc(_isAbstract=flag)
            self.assertIn("abstract-template", actor_fields(data)["tags"])
            self.assertEqual(data["tags"], ["fixture:test"])

    def test_zero_or_invalid_level_is_reported_unsupported_without_coercion(self):
        for level in (0, -1, True, 1.5, "12", 1001, None):
            with self.subTest(level=level), self.assertRaises(PreviewError):
                actor_fields(npc(level=level))

    def test_malformed_or_duplicate_tags_and_abstract_flags_are_rejected(self):
        for changes in ({"tags": ["same", "same"]}, {"tags": [None]}, {"tags": "npc"},
                {"tags": ["x" * 257]}, {"tags": [str(i) for i in range(127)]},
                {"_isAbstract": "false"}, {"_isAbstract": 2}, {"_isAbstract": None}):
            with self.subTest(changes=changes), self.assertRaises(PreviewError):
                actor_fields(npc(**changes))

    def test_component_shape_excludes_shops_and_audio(self):
        rows = list(component_rows(environment([pointer({"maxWealth": 100}), pointer(npc()),
            pointer({"aliveAudioContainerWrapper": {}})]), REQUIRED_FIELDS))
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][:2], ("Assets/Synthetic/Actor.prefab", 123))

    def test_ambiguous_npc_component_is_never_selected_arbitrarily(self):
        with self.assertRaises(PreviewError):
            list(component_rows(environment([pointer(npc()), pointer(npc(level=20))]), REQUIRED_FIELDS))

    def test_external_component_pointer_is_not_followed(self):
        external = SimpleNamespace(m_FileID=1, deref=lambda: self.fail("External component was dereferenced"))
        self.assertEqual(list(component_rows(environment([external]), REQUIRED_FIELDS)), [])


if __name__ == "__main__":
    unittest.main()
