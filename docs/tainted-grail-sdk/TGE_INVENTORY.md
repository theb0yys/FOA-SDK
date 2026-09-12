# Inventory and equipment through TGE

The extender owns `tge.foa.inventory@0.1`. The SDK reads observations through the
existing authenticated connection. The native game remains the authority for
item identities, quantities, equipment and localized display names.

Use `Tools/tge_inventory.py` from the SDK gem. Supply the existing session key
through `TGE_SDK_KEY`; do not put it in command arguments, logs or source files.

```powershell
python tge_inventory.py equipment --port <current-sdk-port>
python tge_inventory.py inventory --port <current-sdk-port>
python tge_inventory.py inventory --port <current-sdk-port> --offset 8 --revision <previous-revision>
```

The typed entry points are `read_equipment(client)` and
`read_inventory_page(client, offset=0, revision=None)`. Both return frozen
records and create a fresh invocation for each refresh. This slice provides a
Python/CLI consumer; it does not add an Editor inventory panel.

## Equipment

The equipment snapshot contains eight named slots: MainHand, OffHand, Helmet,
Cuirass, Gauntlets, Greaves, Boots and Back. Each has either an item observation
or an explicit empty value. An item contains the exact native instance ID,
template ID, localized name and positive stack quantity. Existing native fists
are items. An item occupying both hands can appear in both slots.

IDs are opaque observations. Preserve their spelling; they are not authoring
IDs, GUID guarantees, durable save handles or authority to modify an item.

## Inventory

Inventory pages contain up to eight visible hero-owned entries, including
equipped items, ordered by the native ID in UTF-16 ordinal order. Each entry
contains an item observation, native `equipped` flag and the matching slots
within the eight-slot scope. Jewellery or quivers can be equipped while having
an empty supported-slot list.

`total` counts visible owned items. `next_offset` is `None` (JSON `null`) on the last page.
For continuation, pass both the previous revision and next offset. The reader
does not fetch every page automatically. There is a limit of 2,048 raw owned
entries, including hidden items; larger collections return `inventory_limit`.

The revision fingerprints player/item identities, template identities,
quantities, equipped flags and supported slots. A change causes
`InventoryChanged("inventory_changed")`; explicitly refresh from page zero.
Names are evaluated per page and excluded from the revision. This is not a
retained or atomic snapshot. A change followed by an exact reversion can
produce the same revision.

This is the visible hero-owned inventory, not a storage browser. It does not
open chests, request stored items, enumerate the world, construct items, equip
items, change quantities or write saves.

## Availability and validation

A missing or unready player yields `player_unavailable`. Invalid native item
state yields `equipment_unavailable` or `inventory_unavailable`. Rejections
contain no partial/default rows. An older host without this service is an
explicit protocol failure. A new game session requires reconnecting.

Consumer tests validate shapes, IDs, quantities, slot consistency, UTF-16
ordering, pagination and stale revisions. The source-linked native fixture uses
synthetic items with the production service and TCP adapter. Run the socket lane
with `TGE_INVENTORY_FIXTURE_DLL` pointing to the built
`TGE.Inventory.Fixtures.dll`; a skipped socket test is not runtime proof.

Live acceptance requires the new host to discover this service, a loaded-player
equipment read matching the game, a manual weapon/armour switch followed by a
fresh SDK read, paged quantity checks and normal native shutdown. Build and
fixture results do not establish those live results. See the extender's
`docs/foa-sdk-inventory-v1.md` for the exact native boundary and limits.
