#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Equipment and paged visible-owned inventory observations through TGE."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from collections.abc import Mapping
import re

from tge_sdk_client import ProtocolError
from tge_sdk_transport import TgeSdkClient, TransportError, key_from_environment

SERVICE = "tge.foa.inventory"
VERSION = "0.1"
SLOTS = ("MainHand", "OffHand", "Helmet", "Cuirass", "Gauntlets", "Greaves", "Boots", "Back")
SLOT_SCOPE = "hands-and-armour"
PAGE_SIZE = 8
MAX_ITEMS = 2048
_DECIMAL = re.compile(r"(?:0|[1-9][0-9]*)")
_REVISION = re.compile(r"[0-9a-f]{64}")
# Unicode White_Space used by .NET char.IsWhiteSpace, without Python's extra
# U+001C..U+001F separators. Text is checked, never normalized or trimmed.
_WHITE_SPACE = "\t\n\v\f\r \u0085\u00a0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u2028\u2029\u202f\u205f\u3000"


@dataclass(frozen=True)
class ItemObservation:
    item_id: str
    template_id: str
    name: str
    quantity: int


@dataclass(frozen=True)
class EquipmentSlot:
    slot: str
    item: ItemObservation | None


@dataclass(frozen=True)
class EquipmentSnapshot:
    slots: tuple[EquipmentSlot, ...]
    session_id: str


@dataclass(frozen=True)
class InventoryEntry:
    item: ItemObservation
    equipped: bool
    slots: tuple[str, ...]


@dataclass(frozen=True)
class InventoryPage:
    entries: tuple[InventoryEntry, ...]
    offset: int
    total: int
    next_offset: int | None
    revision: str
    slot_scope: str
    session_id: str


class InventoryUnavailable(RuntimeError):
    """Explicit unavailable/change/limit result; no fabricated empty snapshot."""

    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


class InventoryChanged(InventoryUnavailable):
    """Restart from page zero explicitly; no continuation is returned."""


def _text(value: str, maximum: int) -> str:
    if not isinstance(value, str) or not value.strip(_WHITE_SPACE):
        raise ProtocolError("Item observation contains invalid text")
    try:
        if len(value.encode("utf-16-le")) // 2 > maximum:
            raise ProtocolError("Item observation contains oversized text")
    except UnicodeError:
        raise ProtocolError("Item observation contains invalid Unicode") from None
    return value


def _integer(value: str, maximum: int, *, positive: bool = False) -> int:
    if not isinstance(value, str) or len(value) > 10 or not _DECIMAL.fullmatch(value):
        raise ProtocolError("Item observation contains an invalid integer")
    parsed = int(value)
    if parsed > maximum or (positive and parsed == 0):
        raise ProtocolError("Item observation integer is out of range")
    return parsed


def _boolean(value: str) -> bool:
    if value not in ("true", "false"):
        raise ProtocolError("Item observation contains an invalid boolean")
    return value == "true"


def _item(values: Mapping[str, str], prefix: str) -> ItemObservation:
    return ItemObservation(
        _text(values[prefix + "itemId"], 256),
        _text(values[prefix + "templateId"], 256),
        _text(values[prefix + "name"], 512),
        _integer(values[prefix + "quantity"], 2_147_483_647, positive=True),
    )


def _invoke(client: TgeSdkClient, operation: str, arguments: dict[str, str]):
    request = client.create_request(SERVICE, VERSION, operation, arguments)
    result = client.invoke(request)
    if not isinstance(result.values, Mapping):
        raise ProtocolError("Invalid item response map")
    allowed = {"player_unavailable", "equipment_unavailable"} if operation == "equipment" else {
        "player_unavailable", "inventory_unavailable", "inventory_limit", "inventory_changed"
    }
    if result.status == "rejected" and result.code in allowed:
        if result.session_id != request.session_id:
            raise ProtocolError("Item rejection belongs to another session")
        if result.values:
            raise ProtocolError("Rejected item response must not contain values")
        if result.code == "inventory_changed":
            raise InventoryChanged(result.code)
        raise InventoryUnavailable(result.code)
    if not result.succeeded or result.session_id != request.session_id:
        raise ProtocolError("Item observation failed or belongs to another session")
    return result


def read_equipment(client: TgeSdkClient) -> EquipmentSnapshot:
    """Read eight existing native slots; fists and multi-slot items are preserved."""
    result = _invoke(client, "equipment", {})
    fields = {"count"} | {
        f"{i}.{field}" for i in range(len(SLOTS))
        for field in ("slot", "occupied", "itemId", "templateId", "name", "quantity")
    }
    values = result.values
    if result.code != "player_equipment" or set(values) != fields or values["count"] != "8":
        raise ProtocolError("Unexpected equipment response shape")
    rows = []
    identities = {}
    for i, slot in enumerate(SLOTS):
        prefix = f"{i}."
        if values[prefix + "slot"] != slot:
            raise ProtocolError("Unexpected equipment slot order")
        if _boolean(values[prefix + "occupied"]):
            item = _item(values, prefix)
            prior = identities.get(item.item_id)
            if prior is not None and (prior.template_id, prior.quantity) != (item.template_id, item.quantity):
                raise ProtocolError("Repeated equipment identity has inconsistent metadata")
            identities[item.item_id] = item
        else:
            if any(values[prefix + field] != "" for field in ("itemId", "templateId", "name")) or values[prefix + "quantity"] != "0":
                raise ProtocolError("Empty equipment slot contains item values")
            item = None
        rows.append(EquipmentSlot(slot, item))
    return EquipmentSnapshot(tuple(rows), result.session_id)


def read_inventory_page(client: TgeSdkClient, offset: int = 0,
                        revision: str | None = None) -> InventoryPage:
    """Read one page. No polling/retries or retained snapshots; names are per-page."""
    if type(offset) is not int or not 0 <= offset <= MAX_ITEMS or offset % PAGE_SIZE:
        raise ValueError("Offset must be a page-aligned integer from zero through 2048")
    if revision is not None and (not isinstance(revision, str) or not _REVISION.fullmatch(revision)):
        raise ValueError("Revision must be 64 lowercase hexadecimal characters")
    if offset and revision is None:
        raise ValueError("Continuation requires the previous revision")
    arguments = {"offset": str(offset)}
    if revision is not None:
        arguments["revision"] = revision
    result = _invoke(client, "inventory", arguments)
    values = result.values
    base = {"offset", "count", "total", "nextOffset", "revision", "slotScope"}
    if result.code != "player_inventory" or not base <= set(values):
        raise ProtocolError("Unexpected inventory response shape")
    count = _integer(values["count"], PAGE_SIZE)
    total = _integer(values["total"], MAX_ITEMS)
    returned_offset = _integer(values["offset"], MAX_ITEMS)
    expected_fields = base | {
        f"{i}.{field}" for i in range(count)
        for field in ("itemId", "templateId", "name", "quantity", "equipped", "slots")
    }
    if set(values) != expected_fields or returned_offset != offset or values["slotScope"] != SLOT_SCOPE:
        raise ProtocolError("Unexpected inventory page fields or scope")
    if offset >= total and not (offset == 0 and total == 0):
        raise ProtocolError("Inventory page is outside its total")
    if count != min(PAGE_SIZE, total - offset):
        raise ProtocolError("Inventory page count is inconsistent")
    actual_revision = values["revision"]
    if not isinstance(actual_revision, str) or not _REVISION.fullmatch(actual_revision):
        raise ProtocolError("Invalid inventory revision")
    if revision is not None and actual_revision != revision:
        raise ProtocolError("Inventory revision changed in a successful response")
    next_offset = offset + count if offset + count < total else None
    if values["nextOffset"] != (str(next_offset) if next_offset is not None else ""):
        raise ProtocolError("Inventory next offset is inconsistent")
    rows = []
    prior_id = None
    occupied_slots = set()
    for i in range(count):
        prefix = f"{i}."
        item = _item(values, prefix)
        # .NET StringComparer.Ordinal orders UTF-16 units, not Unicode scalars.
        ordered_id = item.item_id.encode("utf-16-be")
        if prior_id is not None and ordered_id <= prior_id:
            raise ProtocolError("Inventory identities are duplicated or out of order")
        prior_id = ordered_id
        equipped = _boolean(values[prefix + "equipped"])
        encoded_slots = values[prefix + "slots"]
        if not isinstance(encoded_slots, str):
            raise ProtocolError("Invalid inventory equipment slots")
        slots = tuple(encoded_slots.split(",")) if encoded_slots else ()
        if any(slot not in SLOTS for slot in slots) or slots != tuple(slot for slot in SLOTS if slot in slots):
            raise ProtocolError("Inventory equipment slots are duplicated or out of order")
        if slots and not equipped:
            raise ProtocolError("Occupied slots contradict the equipped flag")
        if occupied_slots.intersection(slots):
            raise ProtocolError("Different inventory entries occupy the same equipment slot")
        occupied_slots.update(slots)
        rows.append(InventoryEntry(item, equipped, slots))
    return InventoryPage(tuple(rows), offset, total, next_offset, actual_revision, SLOT_SCOPE, result.session_id)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("equipment", "inventory"))
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--expected-host-version", default="0.1.0")
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--revision")
    args = parser.parse_args()
    try:
        if args.operation == "equipment" and (args.offset or args.revision is not None):
            raise ValueError("Equipment does not accept inventory pagination")
        client = TgeSdkClient(args.port, key_from_environment(),
                              expected_host_id="kane.tgfoa.tainted-grail-extender",
                              expected_host_version=args.expected_host_version, timeout=5)
        client.connect()
        snapshot = read_equipment(client) if args.operation == "equipment" else read_inventory_page(client, args.offset, args.revision)
        print(json.dumps(asdict(snapshot), ensure_ascii=False, allow_nan=False))
        return 0
    except InventoryUnavailable as exc:
        print(json.dumps({"available": False, "code": exc.code}))
        return 1
    except (ProtocolError, TransportError, ValueError) as exc:
        parser.exit(2, str(exc) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
