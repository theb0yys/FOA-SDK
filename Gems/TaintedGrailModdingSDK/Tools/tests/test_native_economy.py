# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Synthetic exact-identity and numeric guards; no installed game required."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_native_economy import nonnegative_number, positive_integer, resolve_guid
from foa_native_item_preview import PreviewError


class Catalog:
    def __init__(self, paths):
        self.paths = paths
        self.by_key = {"a" * 32: list(range(len(paths)))}

    def internal_id(self, index):
        return self.paths[index]


class NativeEconomyTests(unittest.TestCase):
    def test_exact_guid_keeps_abstract_identity(self):
        path = "Assets/Items/Abstract/Ingredient.prefab"
        self.assertEqual(resolve_guid("a" * 32, Catalog([path]), {path: "item.abstract"}),
                         {"item_record_id": "item.abstract"})

    def test_ambiguous_guid_does_not_choose_a_matching_candidate(self):
        self.assertEqual(resolve_guid("a" * 32, Catalog(["one", "two"]), {"one": "item.one"}),
                         {"item_subject_ref": "addressables.guid:" + "a" * 32})

    def test_absent_guid_is_explicitly_unresolved(self):
        self.assertIn("item_subject_ref", resolve_guid("a" * 32, Catalog([]), {}))

    def test_display_name_and_case_folding_are_not_identity_joins(self):
        self.assertIn("item_subject_ref", resolve_guid("a" * 32, Catalog(["Assets/Item.prefab"]),
                                                       {"assets/item.prefab": "item.fake"}))

    def test_invalid_guid_is_rejected(self):
        for value in ("", "../elsewhere", "Sword", None, "a" * 33):
            with self.subTest(value=value), self.assertRaises(PreviewError):
                resolve_guid(value, Catalog([]), {})

    def test_quantity_requires_bounded_positive_integer(self):
        self.assertEqual(positive_integer(1), 1)
        self.assertEqual(positive_integer(1000000), 1000000)
        for value in (True, 0, -1, 1.5, "2", 1000001):
            with self.subTest(value=value), self.assertRaises(PreviewError):
                positive_integer(value)

    def test_item_numbers_reject_nonfinite_or_coerced_values(self):
        for value in (float("inf"), float("nan"), -1, True, "5"):
            with self.subTest(value=value), self.assertRaises(PreviewError):
                nonnegative_number(value)
        self.assertEqual(nonnegative_number(0), 0)
        self.assertEqual(nonnegative_number(0.125), 0.125)


if __name__ == "__main__":
    unittest.main()
