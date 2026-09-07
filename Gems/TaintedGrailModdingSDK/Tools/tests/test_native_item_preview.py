# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Synthetic contract and failure tests; actual game/UI proof is a separate lane."""
import base64
import json
from pathlib import Path
import struct
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from foa_addressables_catalog import AddressablesCatalog, CatalogError
import foa_native_item_preview as preview


def catalog_fixture():
    keys = ["icon-address", "icons.bundle"]
    raw = bytearray(struct.pack("<i", len(keys)))
    offsets = []
    for key in keys:
        offsets.append(len(raw))
        raw += b"\0" + struct.pack("<i", len(key)) + key.encode()
    buckets = struct.pack("<i", 2) + struct.pack("<iii", offsets[0], 1, 0) + struct.pack("<iii", offsets[1], 1, 1)
    entries = struct.pack("<i", 2) + struct.pack("<7i", 0, 1, 1, 0, -1, 0, 0) + struct.pack("<7i", 1, 0, -1, 0, -1, 1, 1)
    return {"m_InternalIds": ["icon-address", "{UnityEngine.AddressableAssets.Addressables.RuntimePath}/StandaloneWindows64/icons.bundle"],
            **{name: base64.b64encode(value).decode() for name, value in (
                ("m_KeyDataString", raw), ("m_BucketDataString", buckets), ("m_EntryDataString", entries))}}


class AddressablesCatalogTests(unittest.TestCase):
    def test_resolves_icon_address_to_its_explicit_bundle(self):
        catalog = AddressablesCatalog(catalog_fixture())
        self.assertEqual(catalog.icon_locations("icon-address"), [("icons.bundle", "icon-address")])
        self.assertEqual(catalog.icon_locations("missing"), [])

    def test_rejects_truncated_tables_and_excessive_counts(self):
        for field in ("m_KeyDataString", "m_BucketDataString", "m_EntryDataString"):
            for data in (b"\x00", struct.pack("<i", 200001)):
                with self.subTest(field=field, data=data):
                    document = catalog_fixture()
                    document[field] = base64.b64encode(data).decode()
                    with self.assertRaises(CatalogError):
                        AddressablesCatalog(document)

    def test_rejects_invalid_entry_reference_and_dependency(self):
        for field, offset in (("m_BucketDataString", 12), ("m_EntryDataString", 12)):
            document = catalog_fixture()
            value = bytearray(base64.b64decode(document[field]))
            struct.pack_into("<i", value, offset, 99999)
            document[field] = base64.b64encode(value).decode()
            with self.assertRaises(CatalogError):
                AddressablesCatalog(document)

    def test_rejects_bundle_path_traversal(self):
        document = catalog_fixture()
        document["m_InternalIds"][1] = document["m_InternalIds"][1].replace("icons.bundle", "../icons.bundle")
        with self.assertRaises(CatalogError):
            AddressablesCatalog(document).icon_locations("icon-address")

    def test_remote_locations_are_not_downloaded_or_used(self):
        document = catalog_fixture()
        document["m_InternalIds"][1] = "https://example.invalid/icons.bundle"
        self.assertEqual(AddressablesCatalog(document).icon_locations("icon-address"), [])


class NativeItemPreviewTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "game").mkdir()
        self.workspace = self.root / "workspace.json"
        self.profile = {"ProfileId": "test.mono", "GameVersion": "test-version", "Branch": "mono",
                        "RuntimeTarget": "Mono", "UnityVersion": "6000.0.64f1", "InstallPath": "game",
                        "ExtractedDataPath": "workspace/Extracted"}
        self.document = {"SchemaVersion": 1, "RootPath": "workspace", "ActiveGameProfileId": "test.mono",
                         "GameProfiles": [self.profile]}
        self.save()

    def save(self):
        self.workspace.write_text(json.dumps(self.document))

    def test_valid_context_keeps_generated_files_in_workspace(self):
        profile, install, output = preview.load_context(self.workspace, [])
        self.assertEqual(profile["ProfileId"], "test.mono")
        self.assertEqual(install, self.root / "game")
        self.assertEqual(output, self.root / "workspace/Extracted/PreviewArtifacts/NativeItems")

    def test_rejects_game_and_forbidden_output_roots(self):
        self.document["RootPath"] = "game"
        self.profile["ExtractedDataPath"] = "game/Extracted"
        self.save()
        with self.assertRaises(preview.PreviewError):
            preview.load_context(self.workspace, [])
        self.document["RootPath"] = "workspace"
        self.profile["ExtractedDataPath"] = "workspace/Extracted"
        self.save()
        with self.assertRaises(preview.PreviewError):
            preview.load_context(self.workspace, [self.root / "workspace"])

    def test_rejects_missing_duplicate_and_future_profiles(self):
        for change in ("missing", "duplicate", "future"):
            with self.subTest(change=change):
                original = json.loads(json.dumps(self.document))
                if change == "missing":
                    self.document["ActiveGameProfileId"] = "missing"
                elif change == "duplicate":
                    self.document["GameProfiles"].append(self.profile)
                else:
                    self.document["SchemaVersion"] = 2
                self.save()
                with self.assertRaises(preview.PreviewError):
                    preview.load_context(self.workspace, [])
                self.document = original

    def test_rejects_output_symlink_escape(self):
        target = self.root / "workspace"
        target.mkdir()
        try:
            (target / "Extracted").symlink_to(self.root / "game", target_is_directory=True)
        except OSError:
            self.skipTest("Directory symlinks require an unavailable Windows privilege.")
        with self.assertRaises(preview.PreviewError):
            preview.load_context(self.workspace, [])

    def test_bounded_read_and_deadline_fail(self):
        data = self.root / "source"
        data.write_bytes(b"12345")
        with self.assertRaises(preview.PreviewError):
            preview.read_bounded(data, 4)
        reader = preview.BundleReader.__new__(preview.BundleReader)
        reader.started = time.monotonic() - preview.MAX_SECONDS - 1
        with self.assertRaises(preview.PreviewError):
            reader.check_budget()

    def test_bundle_preflight_rejects_excessive_decompression(self):
        def bundle(expanded_bytes):
            blocks = b"\0" * 16 + struct.pack(">I", 1) + struct.pack(">IIH", expanded_bytes, 1, 0) + struct.pack(">I", 0)
            prefix = b"UnityFS\0" + struct.pack(">I", 7) + b"0.0.0\0" * 2
            header_size = (len(prefix) + 20 + 15) & ~15
            size = header_size + len(blocks) + 1
            header = prefix + struct.pack(">QIII", size, len(blocks), len(blocks), 0)
            return header.ljust(header_size, b"\0") + blocks + b"x"
        preview.preflight_bundle(bundle(1))
        with self.assertRaises(preview.PreviewError):
            preview.preflight_bundle(bundle(preview.MAX_EXPANDED_BYTES + 1))

    def test_source_drift_prevents_publication(self):
        data = self.root / "source"
        data.write_bytes(b"old")
        reader = preview.BundleReader.__new__(preview.BundleReader)
        reader.started = time.monotonic()
        reader.install = self.root
        reader.sources = {"source": {"Locator": "$install/source", "Sha256": preview.digest(b"old"),
                                     "ByteSize": 3, "ModifiedMs": data.stat().st_mtime_ns // 1000000}}
        data.write_bytes(b"new")
        with self.assertRaises(preview.PreviewError):
            reader.verify_sources()

    def test_item_identity_is_profile_source_and_object_bound(self):
        item_component = SimpleNamespace(type=SimpleNamespace(name="MonoBehaviour"),
            parse_as_dict=lambda: {"iconReference": {"arSpriteReference": {"address": "icon-address"}}})
        game_object = SimpleNamespace(type=SimpleNamespace(name="GameObject"), path_id=123,
            parse_as_object=lambda: SimpleNamespace(m_Component=[SimpleNamespace(
                component=SimpleNamespace(m_FileID=0, deref=lambda: item_component))]))
        environment = SimpleNamespace(container={"Assets/Items/Weapons/ItemTemplate_Test_Sword.prefab":
                                                  SimpleNamespace(deref=lambda: game_object)})
        first = preview.discover_items(environment, "sha256:first", "$install/items.bundle", self.profile)[0]
        again = preview.discover_items(environment, "sha256:first", "$install/items.bundle", self.profile)[0]
        changed = preview.discover_items(environment, "sha256:changed", "$install/items.bundle", self.profile)[0]
        self.assertEqual(first, again)
        self.assertNotEqual(first["AssetRecordId"], changed["AssetRecordId"])
        self.assertEqual(first["DisplayName"], "Test Sword")
        self.assertEqual(first["IconAddress"], "icon-address")
        self.assertEqual(first["Category"], "Weapons / Swords")

    def test_equipment_categories_ignore_tiers_sets_and_content_folders(self):
        cases = {
            "Armors/GenericArmors/1_Light/Arms/ItemTemplate_Armor_Light_T0_Arms.prefab": "Armor / Light",
            "Armors/SpecialArmors/NewSet/T5/ItemTemplate_Armor_Heavy_T5_Head.prefab": "Armor / Heavy",
            "BonusContentPacks/Pack4/ItemTemplate_Armor_Medium_T4_Body.prefab": "Armor / Medium",
            "Weapons/Melee/Special/Pack2/ItemTemplate_Weapon_1H_Sword_Tier4.prefab": "Weapons / Swords",
            "Weapons/Prefabs/ItemTemplate_Weapon_2H_Polearm_Tier4.prefab": "Weapons / Polearms",
            "Weapons/Ranged/Ammo/ItemTemplate_Weapon_Ammo_Tier1.prefab": "Weapons / Ammunition",
            "Weapons/Melee/Shields/ItemTemplate_Weapon_Shield_Light.prefab": "Weapons / Shields",
            "Jewelry/Rings/New/ItemTemplate_Jewelry_Static_Ring_Test.prefab": "Jewelry / Rings",
            "Armors/Story/Set/ItemTemplate_Jewelry_Static_Amulet_Test.prefab": "Jewelry / Amulets",
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(preview.item_category("Assets/ItemTemplates/" + path), expected)

    def test_ingredients_recipes_and_finished_consumables_remain_distinct(self):
        cases = {
            "CraftingIngredients/Alchemy/Herbs": "Ingredients / Alchemy",
            "CraftingIngredients/Cooking/Fruit": "Ingredients / Cooking",
            "CraftingIngredients/HandCrafting/Fabric": "Ingredients / Crafting materials",
            "CraftingResults/Alchemy/AdvancedPotions": "Consumables / Potions",
            "CraftingResults/Cooking/Custom/Dish": "Consumables / Food and drink",
            "Readables/Crafting/Handcrafting": "Crafting / Recipes",
            "CraftingRecipes/Cooking": "Crafting / Recipes",
            "Fishing/FishTemplates": "Ingredients / Fish",
            "CrazyConsumables/WeaponGrease": "Consumables / Weapon coatings",
        }
        for folder, expected in cases.items():
            with self.subTest(folder=folder):
                self.assertEqual(preview.item_category("Assets/" + folder + "/ItemTemplate_Test.prefab"), expected)

    def test_development_templates_are_retained_in_explicit_groups(self):
        cases = {
            "AbstractTypes/Weapon/Abstract_ItemTemplate_Weapon.prefab": "Abstract templates",
            "CraftingIngredients/Cooking/Abstract/Abstract_ItemTemplate_Fruit.prefab": "Abstract templates",
            "Debug/Templates/ItemTemplate_Weapon_1H_Sword.prefab": "Debug items",
            "Gems/Debug_TestGems/ItemTemplate_Test.prefab": "Debug items",
            "Weapons/Magic/Unused/ItemTemplate_Magic_Test.prefab": "Unused items",
            "CraftingResults/Alchemy/NotInUse/ItemTemplate_Test.prefab": "Unused items",
            "UnbalancedItems/ItemTemplate_Armor_Heavy.prefab": "Experimental items",
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(preview.item_category("Assets/" + path), "Developer templates / " + expected)

    def test_story_and_readable_categories_do_not_use_item_words_in_titles(self):
        self.assertEqual(preview.item_category("Assets/Readables/Quests/ItemTemplate_Readable_Armor.prefab"),
                         "Books and notes / Notes and letters")
        self.assertEqual(preview.item_category("Assets/StoryItems/Chapter2/ItemTemplate_Story_Weapon.prefab"),
                         "Quest items / Story items")
        self.assertEqual(preview.item_category("Assets/Keys/Housing Keys/ItemTemplate_Key_Test.prefab"),
                         "Keys / Housing keys")

    def test_category_handles_case_separators_and_unknown_sources(self):
        self.assertEqual(preview.item_category(r"ASSETS\ARMORS\1_LIGHT\ITEMTEMPLATE_ARMOR_LIGHT_T0.prefab"), "Armor / Light")
        self.assertEqual(preview.item_category("Assets/FutureFolder/ItemTemplate_Unknown.prefab"), "Miscellaneous / Other items")


if __name__ == "__main__":
    unittest.main()
