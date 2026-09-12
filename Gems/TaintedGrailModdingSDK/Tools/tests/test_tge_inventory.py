#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Strict inventory consumer and production TCP tests with synthetic native items."""

from dataclasses import FrozenInstanceError
import json
import os
from pathlib import Path
import queue
import secrets
import subprocess
import sys
import threading
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tge_inventory import SLOTS, InventoryChanged, InventoryUnavailable, read_equipment, read_inventory_page
from tge_sdk_client import ProtocolError, SdkRequest, SdkResponse
from tge_sdk_transport import TgeSdkClient

REVISION = "a" * 64


def equipment():
    values = {"count": "8"}
    for i, slot in enumerate(SLOTS):
        values.update({f"{i}.slot": slot, f"{i}.occupied": "false", f"{i}.itemId": "",
                       f"{i}.templateId": "", f"{i}.name": "", f"{i}.quantity": "0"})
    values.update({"0.occupied": "true", "0.itemId": "opaque:item", "0.templateId": "opaque:template",
                   "0.name": "Fixture sword", "0.quantity": "1"})
    return values


def page(total=10, offset=0):
    count = min(8, total - offset)
    values = {"offset": str(offset), "count": str(count), "total": str(total),
              "nextOffset": str(offset + count) if offset + count < total else "",
              "revision": REVISION, "slotScope": "hands-and-armour"}
    for i in range(count):
        values.update({f"{i}.itemId": f"item:{offset+i:04}", f"{i}.templateId": "template:fixture",
                       f"{i}.name": f"Item {offset+i}", f"{i}.quantity": str(offset+i+1),
                       f"{i}.equipped": "true" if offset+i == 0 else "false",
                       f"{i}.slots": "MainHand" if offset+i == 0 else ""})
    return values


class InventoryTests(unittest.TestCase):
    def client(self, values, code="player_inventory", status="succeeded", session="session"):
        client = Mock()
        client.create_request.side_effect = lambda service, version, operation, args: SdkRequest(
            service, version, operation, "session", args)
        client.invoke.return_value = SdkResponse(session, status, code, "", values)
        return client

    def test_equipment_frozen_and_exact_request(self):
        client = self.client(equipment(), "player_equipment")
        result = read_equipment(client)
        client.create_request.assert_called_once_with("tge.foa.inventory", "0.1", "equipment", {})
        self.assertEqual(tuple(row.slot for row in result.slots), SLOTS)
        self.assertEqual(result.slots[0].item.name, "Fixture sword")
        self.assertIsNone(result.slots[1].item)
        with self.assertRaises(FrozenInstanceError):
            result.slots[0].item.quantity = 2

    def test_inventory_pages_and_fresh_requests(self):
        client = self.client(page())
        first = read_inventory_page(client)
        client.create_request.assert_called_once_with("tge.foa.inventory", "0.1", "inventory", {"offset": "0"})
        self.assertEqual((len(first.entries), first.total, first.next_offset, first.entries[0].slots),
                         (8, 10, 8, ("MainHand",)))
        client.invoke.return_value = SdkResponse("session", "succeeded", "player_inventory", "", page(10, 8))
        last = read_inventory_page(client, 8, first.revision)
        self.assertEqual((len(last.entries), last.next_offset, last.entries[-1].item.quantity), (2, None, 10))
        sent = [call.args[0] for call in client.invoke.call_args_list]
        self.assertNotEqual(sent[0].request_id, sent[1].request_id)
        self.assertEqual(dict(sent[1].arguments), {"offset": "8", "revision": REVISION})
        with self.assertRaises(FrozenInstanceError):
            last.total = 0
        self.assertEqual(read_inventory_page(self.client(page(0))).entries, ())

    def test_invalid_arguments_never_invoke(self):
        for offset, revision in ((True, None), (-8, None), (1, None), (2049, REVISION),
                                 ("8", REVISION), (8, None), (0, ""), (0, "A"*64),
                                 (0, "a"*63), (0, 123)):
            client = self.client(page())
            with self.subTest(offset=offset, revision=revision), self.assertRaises(ValueError):
                read_inventory_page(client, offset, revision)
            client.create_request.assert_not_called()
            client.invoke.assert_not_called()

    def test_unavailable_change_and_unsupported_service(self):
        for reader, codes in ((read_equipment, ("player_unavailable", "equipment_unavailable")),
                              (read_inventory_page, ("player_unavailable", "inventory_unavailable",
                                                     "inventory_limit", "inventory_changed"))):
            for code in codes:
                client = self.client({}, code, "rejected")
                with self.assertRaises(InventoryUnavailable) as error:
                    reader(client)
                self.assertEqual(error.exception.code, code)
                self.assertEqual(isinstance(error.exception, InventoryChanged), code == "inventory_changed")
                self.assertEqual(client.invoke.call_count, 1)
                with self.assertRaises(ProtocolError):
                    reader(self.client({"partial": "data"}, code, "rejected"))
                with self.assertRaises(ProtocolError):
                    reader(self.client({}, code, "rejected", session="old-session"))
            for status, code in (("rejected", "service_not_found"), ("rejected", "invalid_arguments"),
                                 ("failed", "service_failed"), ("failed", "player_unavailable")):
                with self.assertRaises(ProtocolError):
                    reader(self.client({}, code, status))
        with self.assertRaises(ProtocolError):
            read_inventory_page(self.client(page(), session="old-session"))

    def test_exact_response_shapes(self):
        for reader, good, code in ((read_equipment, equipment(), "player_equipment"),
                                   (read_inventory_page, page(), "player_inventory")):
            for missing in good:
                with self.subTest(reader=reader.__name__, missing=missing), self.assertRaises(ProtocolError):
                    reader(self.client({k: v for k, v in good.items() if k != missing}, code))
            for bad in (good | {"extra": "value"}, None, [], {}):
                with self.assertRaises(ProtocolError):
                    reader(self.client(bad, code))
            with self.assertRaises(ProtocolError):
                reader(self.client(good, "wrong_code"))

    def test_invalid_item_metadata(self):
        for reader, good, code in ((read_equipment, equipment(), "player_equipment"),
                                   (read_inventory_page, page(), "player_inventory")):
            for field, maximum in (("itemId", 256), ("templateId", 256), ("name", 512)):
                for bad in ("", " ", "\u00a0", "\ud800", "\udc00", "x"*(maximum+1),
                            "\U0001F600"*(maximum//2+1), None, 1, True):
                    with self.subTest(field=field, bad=repr(bad)), self.assertRaises(ProtocolError):
                        reader(self.client(good | {"0."+field: bad}, code))
            for bad in ("0", "-1", "+1", "01", "1 ", " 1", "１", "1.0", "2147483648", "9"*100, None, True):
                with self.subTest(bad=bad), self.assertRaises(ProtocolError):
                    reader(self.client(good | {"0.quantity": bad}, code))

    def test_opaque_unicode_and_utf16_order(self):
        values = page(2)
        values.update({"0.itemId": "\U00010000", "1.itemId": "\uE000",
                       "0.templateId": "\u001c", "0.name": "\U0001F600"*256, "0.quantity": "2147483647"})
        result = read_inventory_page(self.client(values))
        self.assertEqual(result.entries[0].item.item_id, "\U00010000")
        self.assertEqual(result.entries[0].item.template_id, "\u001c")
        self.assertEqual(result.entries[0].item.quantity, 2147483647)
        for first, second in (("\uE000", "\U00010000"), ("same", "same")):
            with self.assertRaises(ProtocolError):
                read_inventory_page(self.client(values | {"0.itemId": first, "1.itemId": second}))
        observed = read_equipment(self.client(equipment() | {"0.itemId": "  exact ID  "}, "player_equipment"))
        self.assertEqual(observed.slots[0].item.item_id, "  exact ID  ")

    def test_equipment_slot_and_duplicate_semantics(self):
        for key, bad in (("count", "7"), ("0.slot", "OffHand"), ("0.occupied", "True"),
                         ("1.occupied", True), ("1.name", "fabricated"), ("1.quantity", "1")):
            with self.assertRaises(ProtocolError):
                read_equipment(self.client(equipment() | {key: bad}, "player_equipment"))
        values = equipment()
        for field in ("occupied", "itemId", "templateId", "name", "quantity"):
            values["1."+field] = values["0."+field]
        result = read_equipment(self.client(values, "player_equipment"))
        self.assertEqual(result.slots[0].item, result.slots[1].item)
        for field, bad in (("templateId", "different"), ("quantity", "2")):
            with self.assertRaises(ProtocolError):
                read_equipment(self.client(values | {"1."+field: bad}, "player_equipment"))

    def test_inventory_metadata_and_pagination_consistency(self):
        for key, bad in (("count", "7"), ("count", "9"), ("count", "08"), ("total", "7"),
                         ("total", "2049"), ("offset", "8"), ("nextOffset", ""), ("nextOffset", "16"),
                         ("revision", "A"*64), ("revision", None), ("slotScope", "all"),
                         ("0.equipped", "True"), ("0.equipped", "false"),
                         ("0.slots", "OffHand,MainHand"), ("0.slots", "MainHand,MainHand"),
                         ("0.slots", "Ring"), ("0.slots", None)):
            with self.subTest(key=key, bad=bad), self.assertRaises(ProtocolError):
                read_inventory_page(self.client(page() | {key: bad}))
        with self.assertRaises(ProtocolError):
            read_inventory_page(self.client(page(10, 8) | {"revision": "b"*64}), 8, REVISION)
        with self.assertRaises(ProtocolError):
            read_inventory_page(self.client(page(8, 8)), 8, REVISION)
        with self.assertRaises(ProtocolError):
            read_inventory_page(self.client(page() | {"1.equipped": "true", "1.slots": "MainHand"}))
        outside = read_inventory_page(self.client(page(1) | {"0.slots": ""}))
        self.assertTrue(outside.entries[0].equipped)
        self.assertEqual(outside.entries[0].slots, ())

    def test_transport_failure_does_not_retry(self):
        client = self.client(page())
        client.invoke.side_effect = TimeoutError("synthetic timeout")
        with self.assertRaises(TimeoutError):
            read_inventory_page(client)
        self.assertEqual(client.invoke.call_count, 1)


@unittest.skipUnless(os.environ.get("TGE_INVENTORY_FIXTURE_DLL"), "Set TGE_INVENTORY_FIXTURE_DLL for synthetic native socket proof")
class InventorySocketTests(unittest.TestCase):
    def setUp(self):
        key = secrets.token_bytes(32)
        self.process = subprocess.Popen(["dotnet", os.environ["TGE_INVENTORY_FIXTURE_DLL"], "--tcp"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            encoding="utf-8", env=os.environ | {"TGE_SDK_KEY": key.hex()})
        self.lines = queue.Queue(maxsize=8)

        def read():
            for line in self.process.stdout:
                self.lines.put(line, timeout=5)

        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.addCleanup(self.close)
        port = json.loads(self.lines.get(timeout=10))["port"]
        self.client = TgeSdkClient(port, key, expected_host_id="tge.inventory.fixture", expected_host_version="1.0.0")
        self.client.connect()

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
            self.fail("Synthetic inventory fixture failed to close")
        finally:
            self.reader.join(timeout=3)
            self.process.stdout.close()
            errors = self.process.stderr.read()
            self.process.stderr.close()
        self.assertEqual(self.process.returncode, 0, errors)

    def command(self, value):
        self.process.stdin.write(value + "\n")
        self.process.stdin.flush()
        self.assertEqual(json.loads(self.lines.get(timeout=5)), {})

    def test_discovery_equipment_swap_pages_stack_change_and_menu(self):
        discovery = self.client.invoke(self.client.create_request("tge.core.identity", "0.1", "services"))
        self.assertTrue(discovery.succeeded)
        self.assertEqual(discovery.values["nextOffset"], "")
        self.assertIn(("tge.foa.inventory", "0.1", "tge.foa.inventory"), [
            (discovery.values[f"{i}.id"], discovery.values[f"{i}.version"], discovery.values[f"{i}.owner"])
            for i in range(int(discovery.values["total"]))])
        for reader in (read_equipment, read_inventory_page):
            with self.assertRaises(InventoryUnavailable):
                reader(self.client)
        self.command("player")
        first = read_inventory_page(self.client)
        initial = read_equipment(self.client)
        self.assertEqual((first.total, first.next_offset, initial.slots[0].item.item_id), (10, 8, "item:0000"))
        last = read_inventory_page(self.client, first.next_offset, first.revision)
        self.assertEqual(last.entries[-1].item.quantity, 10)
        self.command("equip")
        updated = read_equipment(self.client)
        self.assertEqual((updated.slots[0].item.item_id, updated.slots[2].item.item_id), ("item:0001", "item:0002"))
        self.assertEqual(updated.session_id, initial.session_id)
        with self.assertRaises(InventoryChanged):
            read_inventory_page(self.client, first.next_offset, first.revision)
        first = read_inventory_page(self.client)
        self.command("quantity")
        with self.assertRaises(InventoryChanged):
            read_inventory_page(self.client, first.next_offset, first.revision)
        first = read_inventory_page(self.client)
        self.assertEqual(read_inventory_page(self.client, 8, first.revision).entries[-1].item.quantity, 99)
        self.command("menu")
        with self.assertRaises(InventoryUnavailable):
            read_equipment(self.client)


if __name__ == "__main__":
    unittest.main()
