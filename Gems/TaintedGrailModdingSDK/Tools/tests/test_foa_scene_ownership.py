# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Synthetic source ownership contracts; no game-derived source fixtures."""
import copy
from pathlib import Path
import sys
from types import SimpleNamespace as NS
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import foa_scene_ownership as o
from foa_heightmap_importer import HeightmapImportError


def pointer(ident, file_id=0):
    return {"m_FileID": file_id, "m_PathID": ident}


def file(digit="1", suffix=""):
    return NS(name="CAB-" + digit * 32 + suffix, externals=[], objects={})


def record(f, ident, kind, tree):
    children = [NS(m_Name=k, m_Type="PPtr<GameObject>" if k == "m_GameObject" else "synthetic") for k in tree]
    obj = NS(path_id=ident, assets_file=f, type=NS(name=kind), byte_size=64,
             serialized_type=NS(node=NS(m_Children=children)),
             read_typetree=mock.Mock(side_effect=lambda **kwargs: copy.deepcopy(tree)))
    obj.tree = tree
    f.objects[ident] = obj
    return obj


def entity(f, go, transform, parent=0, children=(), components=()):
    record(f, go, "GameObject", {"m_Name": "Same display name", "m_Component": [
        {"component": pointer(c)} for c in (transform, *components)]})
    record(f, transform, "Transform", {"m_GameObject": pointer(go), "m_Father": pointer(parent),
                                      "m_Children": [pointer(c) for c in children]})
    for c in components:
        record(f, c, "MeshCollider" if c % 2 else "MeshRenderer", {"m_GameObject": pointer(go)})


def fixture():
    f = file()
    entity(f, 1, 2, children=[4])
    entity(f, 3, 4, parent=2, components=[5, 6, 8])
    return f


def graph(f, extra=()):
    return o.SceneOwnership([*f.objects.values(), *extra], [f.name])


class OwnershipTests(unittest.TestCase):
    def test_source_owner_unifies_renderers_collider_and_descendants(self):
        f = fixture(); g = graph(f); key = (f.name.lower(), 4)
        child, = g.affected_entities(key)
        self.assertEqual([c.kind for c in child.components], ["Transform", "MeshCollider", "MeshRenderer", "MeshRenderer"])
        self.assertEqual(len(g.affected_entities((f.name.lower(), 2))), 2)
        self.assertEqual(g.report()["components"], 5)
        self.assertEqual(g.report()["game_export"], "NOT_RUN")

    def test_identical_names_do_not_create_ownership_links(self):
        f = fixture(); entity(f, 20, 21, components=[22])
        g = graph(f)
        self.assertEqual([x.key[1] for x in g.affected_entities((f.name.lower(), 2))], [1, 3])
        self.assertEqual(g.report()["roots"], 2)

    def test_shared_prefab_is_not_implicitly_a_placed_scene_entity(self):
        f = fixture(); shared = file(suffix=".sharedAssets"); entity(shared, 1, 2)
        g = graph(f, shared.objects.values())
        self.assertEqual(len(g.entities), 2)
        with self.assertRaises(HeightmapImportError): g.affected_entities((shared.name.lower(), 2))

    def test_signed_ids_and_external_file_scope_are_preserved(self):
        a, b = file(), file("2"); low = -(1 << 62) + 7
        entity(a, 1, low); entity(b, 1, low)
        a.externals = [NS(path=b.name)]; b.externals = [NS(path=a.name)]
        a.objects[low].tree["m_Children"] = [pointer(low, 1)]
        b.objects[low].tree["m_Father"] = pointer(low, 1)
        g = o.SceneOwnership([*a.objects.values(), *b.objects.values()], [a.name, b.name])
        self.assertEqual([e.key for e in g.affected_entities((a.name.lower(), low))], [(a.name.lower(), 1), (b.name.lower(), 1)])
        self.assertEqual(o.identity((b.name.lower(), low))["path_id"], str(low))

    def test_component_owner_disagreement_and_duplicate_claims_rejected(self):
        f = fixture(); f.objects[5].tree["m_GameObject"] = pointer(1)
        with self.assertRaisesRegex(HeightmapImportError, "owner disagrees"): graph(f)
        f = fixture(); f.objects[3].tree["m_Component"].append({"component": pointer(5)})
        with self.assertRaisesRegex(HeightmapImportError, "duplicated"): graph(f)

    def test_missing_component_and_null_component_rejected(self):
        for ident in [0, 999]:
            f = fixture(); f.objects[3].tree["m_Component"].append({"component": pointer(ident)})
            with self.subTest(ident=ident), self.assertRaises(HeightmapImportError): graph(f)

    def test_nonreciprocal_hierarchy_rejected(self):
        f = fixture(); f.objects[2].tree["m_Children"] = []
        with self.assertRaisesRegex(HeightmapImportError, "missing"): graph(f)
        f = fixture(); f.objects[4].tree["m_Father"] = pointer(0)
        with self.assertRaisesRegex(HeightmapImportError, "disagree"): graph(f)

    def test_reciprocal_cycle_and_duplicate_children_rejected(self):
        f = fixture(); f.objects[2].tree["m_Father"] = pointer(4); f.objects[4].tree["m_Children"] = [pointer(2)]
        with self.assertRaisesRegex(HeightmapImportError, "cycle"): graph(f)
        f = fixture(); f.objects[2].tree["m_Children"] *= 2
        with self.assertRaisesRegex(HeightmapImportError, "Duplicate"): graph(f)

    def test_multiple_or_unowned_transforms_rejected(self):
        f = fixture(); record(f, 10, "Transform", {"m_GameObject": pointer(3), "m_Father": pointer(0), "m_Children": []})
        with self.assertRaisesRegex(HeightmapImportError, "unowned Transform"): graph(f)
        f.objects[3].tree["m_Component"].append({"component": pointer(10)})
        with self.assertRaisesRegex(HeightmapImportError, "exactly one"): graph(f)

    def test_component_cannot_name_owner_without_being_listed(self):
        f = fixture(); record(f, 30, "MeshCollider", {"m_GameObject": pointer(3)})
        with self.assertRaisesRegex(HeightmapImportError, "absent from its component list"): graph(f)
        f.objects[30].tree["m_GameObject"] = pointer(0)
        self.assertEqual(graph(f).report()["ownerless_component_records"], {"MeshCollider": 1})

    def test_ambiguous_file_duplicate_record_and_absent_primary_rejected(self):
        f = fixture(); duplicate = fixture()
        with self.assertRaisesRegex(HeightmapImportError, "Ambiguous"): graph(f, duplicate.objects.values())
        with self.assertRaisesRegex(HeightmapImportError, "Duplicate"): graph(f, [f.objects[1]])
        with self.assertRaises(HeightmapImportError): o.SceneOwnership(f.objects.values(), [file("9").name])

    def test_limits_and_cancellation_fail_without_partial_graph(self):
        f = fixture()
        for name, value in [("MAX_RECORDS", 2), ("MAX_COMPONENTS_PER_OBJECT", 2),
                            ("MAX_EDGES", 2), ("MAX_DECODED_SOURCE_BYTES", 2), ("MAX_DEPTH", 0)]:
            with self.subTest(limit=name), mock.patch.object(o, name, value), self.assertRaises(HeightmapImportError): graph(f)
        with self.assertRaises(HeightmapImportError): o.SceneOwnership(f.objects.values(), [f.name], cancelled=lambda: True)

    def test_malformed_component_and_null_parent_schema_rejected(self):
        for pointer_value in [None, {"m_PathID": 0}, {"m_FileID": False, "m_PathID": 0}]:
            f = fixture(); f.objects[2].tree["m_Father"] = pointer_value
            with self.subTest(pointer=pointer_value), self.assertRaises(HeightmapImportError): graph(f)
        f = fixture(); f.objects[1].tree["m_Component"] = [{"guessed": pointer(2)}]
        with self.assertRaisesRegex(HeightmapImportError, "schema"): graph(f)

    def test_script_identity_retained_without_component_semantics_guess(self):
        f = fixture(); f.externals = [NS(path=file("9").name)]
        record(f, 9, "MonoBehaviour", {"m_GameObject": pointer(3), "m_Script": pointer(-(1 << 62), 1)})
        f.objects[3].tree["m_Component"].append({"component": pointer(9)})
        component = graph(f).entities[(f.name.lower(), 3)].components[-1]
        self.assertEqual(component.script, (file("9").name.lower(), -(1 << 62)))


if __name__ == "__main__":
    unittest.main()
