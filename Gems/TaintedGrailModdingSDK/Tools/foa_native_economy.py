#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Local item/recipe observations; no game execution or authoring authority."""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import time
import uuid

from foa_addressables_catalog import AddressablesCatalog
from foa_native_item_preview import (
    AUTHORITY, PROFILE_FIELDS, MAX_ITEMS, BundleReader, PreviewError,
    contained, digest, item_category, load_context, read_bounded, write_json,
)


def component_rows(environment, required):
    """Only local serialized MonoBehaviour data with an explicit supported shape."""
    for native_path, pointer in sorted(environment.container.items()):
        obj = pointer.deref()
        if obj.type.name != "GameObject":
            continue
        matches = []
        for component in obj.parse_as_object().m_Component:
            pointer = component.component
            if pointer.m_FileID != 0:
                continue
            component_reader = pointer.deref()
            if component_reader.type.name == "MonoBehaviour":
                data = component_reader.parse_as_dict()
                if required.issubset(data):
                    matches.append(data)
        if len(matches) > 1:
            raise PreviewError("Ambiguous item/recipe component identity.")
        if matches:
            yield native_path, obj.path_id, matches[0]


def positive_integer(value):
    if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= 1000000:
        raise PreviewError("Recipe quantity is outside the supported authoring range.")
    return value


def nonnegative_number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
        raise PreviewError("Item numeric field is invalid.")
    return value


def resolve_guid(guid, catalog, items_by_path):
    if not isinstance(guid, str) or not re.fullmatch(r"[0-9a-fA-F]{32}", guid):
        raise PreviewError("Recipe item reference is not a serialized GUID.")
    paths = {catalog.internal_id(index) for index in catalog.by_key.get(guid, [])}
    # Never join display names, folder suffixes or one arbitrarily chosen match.
    if len(paths) == 1 and next(iter(paths)) in items_by_path:
        return {"item_record_id": items_by_path[next(iter(paths))]}
    return {"item_subject_ref": "addressables.guid:" + guid}


def extract(workspace: Path, forbidden_roots: list[Path], progress=print) -> Path:
    profile, install, preview_root = load_context(workspace, forbidden_roots)
    workspace_hash = digest(read_bounded(workspace, 1024 * 1024))
    catalog_path = contained(install / "Fall of Avalon_Data/StreamingAssets/aa/catalog.json", install)
    catalog_payload = read_bounded(catalog_path, 32 * 1024 * 1024)
    catalog = AddressablesCatalog(json.loads(catalog_payload))
    bundle_root = contained(catalog_path.parent / "StandaloneWindows64", install)
    reader = BundleReader(install, profile["UnityVersion"], progress)
    records, items_by_path, unsupported = [], {}, []
    generation_id = uuid.uuid4().hex
    for record_kind, bundle, required in (
        ("item", "templates.items_assets_all.bundle", {"iconReference", "itemName", "weight", "basePrice"}),
        ("recipe", "templates.crafting_assets_all.bundle", {"ingredients", "outcome", "quantity"}),
    ):
        progress("Reading installed " + record_kind + " definitions...")
        environment, source_hash, token = reader.load(bundle_root / bundle)
        supported_paths = set()
        for native_path, object_id, data in component_rows(environment, required):
            supported_paths.add(native_path)
            reader.check_budget()
            identity = json.dumps([*[profile[f] for f in PROFILE_FIELDS], token, source_hash, native_path, object_id])
            record_id = "native." + record_kind + "." + hashlib.sha256(identity.encode()).hexdigest()[:32]
            native_ref = token + "#/" + native_path
            row = {
                "record_id": record_id, "subject_ref": native_ref, "native_ref": native_ref,
                "evidence_id": "evidence.economy." + generation_id + "." + record_id,
                "claim": "Serialized " + record_kind + " fields observed at " + native_ref
                    + "; bundle " + source_hash + "; object " + str(object_id)
                    + ". Values describe this artifact, not live runtime behavior.",
                "kind": record_kind, "confidence": "documented",
                "display_name": Path(native_path).stem.removeprefix("ItemTemplate_").removeprefix("Recipe_").replace("_", " "),
            }
            if record_kind == "item":
                category = item_category(native_path).split(" / ", 1)
                icon = data["iconReference"].get("arSpriteReference", {})
                row["item"] = {
                    "category": category[0], "subtype": category[-1],
                    "weight": nonnegative_number(data["weight"]),
                    "base_value": nonnegative_number(data["basePrice"]),
                    "quality": data.get("quality", {}).get("_enumRef", "").rsplit(":", 1)[-1],
                    "hidden": bool(data.get("hiddenOnUI", False)),
                    "name_ref": data["itemName"].get("ID", ""),
                    "description_ref": data.get("description", {}).get("locString", {}).get("ID", ""),
                    "icon_ref": icon.get("address", ""),
                    "tags": data.get("tags", []) + (["abstract-template"] if data.get("_isAbstract") else []),
                }
                items_by_path[native_path] = record_id
            else:
                ingredients = data["ingredients"]
                if not isinstance(ingredients, list) or len(ingredients) > 256:
                    raise PreviewError("Recipe ingredient count exceeds the supported limit.")
                row["recipe"] = {
                    "hidden": bool(data.get("isHidden", False)),
                    "ingredients": [{**resolve_guid(value["templateReference"]["_guid"], catalog, items_by_path),
                                     "quantity": positive_integer(value["count"])} for value in ingredients],
                    "outputs": [{**resolve_guid(data["outcome"]["_guid"], catalog, items_by_path),
                                 "quantity": positive_integer(data["quantity"])}],
                }
            records.append(row)
            if len(records) > MAX_ITEMS:
                raise PreviewError("Economy definitions exceed the 10,000 record limit.")
        unsupported.extend(token + "#/" + path for path in sorted(environment.container) if path not in supported_paths)
        del environment
    if not items_by_path or not any(row["kind"] == "recipe" for row in records):
        raise PreviewError("The installation did not supply both supported items and recipes.")
    reader.verify_sources()
    if digest(read_bounded(catalog_path, 32 * 1024 * 1024)) != digest(catalog_payload) or digest(read_bounded(workspace, 1024 * 1024)) != workspace_hash:
        raise PreviewError("The workspace or game changed during extraction.")
    output = contained(preview_root / ("economy-" + uuid.uuid4().hex), preview_root)
    output.mkdir(parents=True)
    sources = list(reader.sources.values()) + [{"Locator": "$install/" + catalog_path.relative_to(install).as_posix(),
        "Sha256": digest(catalog_payload), "ByteSize": len(catalog_payload), "ModifiedMs": catalog_path.stat().st_mtime_ns // 1000000}]
    evidence = list(records)
    for row in records:
        if "recipe" not in row:
            continue
        for collection, kind in (("ingredients", "recipe-ingredient"), ("outputs", "recipe-output")):
            for index, link in enumerate(row["recipe"][collection]):
                link["link_id"] = row["record_id"] + "." + collection + "." + str(index)
                link["evidence_id"] = "evidence.economy." + generation_id + "." + link["link_id"]
                evidence.append({"evidence_id": link["evidence_id"], "kind": kind, "confidence": "documented",
                    "subject_ref": "economy-" + kind + ":" + link["link_id"],
                    "claim": "Serialized recipe link in " + row["native_ref"] + ": " + json.dumps(link, sort_keys=True)
                        + ". Source context: " + row["claim"]})
    document = {
        "SchemaVersion": 1, "DocumentKind": "foa-native-economy-observations",
        **{field: profile[field] for field in PROFILE_FIELDS},
        "CapturedAt": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "SourceFiles": sources, "OperationalAuthority": AUTHORITY, "definitions": records, "evidence": evidence,
        "UnsupportedEntries": unsupported,
        "Measurements": {"ElapsedSeconds": round(time.monotonic() - reader.started, 3),
            "ItemCount": len(items_by_path), "RecipeCount": len(records) - len(items_by_path),
            "UnresolvedJoins": sum("item_subject_ref" in link for row in records if "recipe" in row
                for kind in ("ingredients", "outputs") for link in row["recipe"][kind])},
    }
    path = output / "foa-native-economy.json"
    write_json(path, document)
    return path
