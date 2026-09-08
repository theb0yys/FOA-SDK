#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Bounded read-only NPC template observations, not runtime actor instances."""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time
import uuid

from foa_native_economy import component_rows
from foa_native_item_preview import (
    AUTHORITY, PROFILE_FIELDS, MAX_ITEMS, BundleReader, PreviewError,
    contained, digest, load_context, read_bounded, write_json,
)

NPC_BUNDLE = "Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64/templates.npc_assets_all.bundle"
REQUIRED_FIELDS = {"npcData", "npcType", "level", "maxHealth", "_isAbstract", "tags"}


def actor_fields(data):
    """Map observed fields only. Unity NPC enums have no authoring enum authority."""
    level = data["level"]
    if isinstance(level, bool) or not isinstance(level, int) or not 1 <= level <= 1000:
        raise PreviewError("Native level cannot be represented as a positive authoring level up to 1000.")
    tags = data["tags"]
    if (not isinstance(tags, list) or len(tags) > 126
            or any(not isinstance(tag, str) or not tag or len(tag.encode("utf-8")) > 256 for tag in tags)
            or len(set(tags)) != len(tags) or type(data["_isAbstract"]) not in (bool, int)
            or data["_isAbstract"] not in (0, 1)):
        raise PreviewError("Native tags or abstract flag have an unsupported shape.")
    tags = list(tags)
    if data["_isAbstract"] and "abstract-template" not in tags:
        tags.append("abstract-template")
    # This component supplies no portrait, model or localized name binding.
    # Its npcData reference contains perception/alert data, not appearance.
    return {"actor_kind": "other", "archetype": "native-npc-template",
        "minimum_level": level, "maximum_level": level, "tags": tags}


def extract(workspace: Path, forbidden_roots: list[Path], progress=print) -> Path:
    profile, install, preview_root = load_context(workspace, forbidden_roots)
    workspace_hash = digest(read_bounded(workspace, 1024 * 1024))
    reader = BundleReader(install, profile["UnityVersion"], progress)
    # UnityPy 1.24.2 supplies this fallback. The native type-tree decoder can
    # corrupt None refcounts on NPC managed references and fail at shutdown.
    # Select the pure-Python implementation in this isolated worker only.
    from UnityPy.helpers import TypeTreeHelper
    TypeTreeHelper.read_typetree_boost = None
    progress("Reading installed actor templates...")
    environment, source_hash, token = reader.load(contained(install / NPC_BUNDLE, install))
    records, unsupported, observed = [], [], set()
    generation = uuid.uuid4().hex
    for native_path, object_id, data in component_rows(environment, REQUIRED_FIELDS):
        reader.check_budget()
        observed.add(native_path)
        native_ref = token + "#/" + native_path
        try:
            actor = actor_fields(data)
        except PreviewError as exc:
            unsupported.append({"NativeRef": native_ref, "Reason": str(exc)})
            continue
        identity = json.dumps([*[profile[field] for field in PROFILE_FIELDS], token, source_hash, native_path, object_id])
        record_id = "native.actor." + hashlib.sha256(identity.encode()).hexdigest()[:32]
        records.append({"record_id": record_id, "subject_ref": native_ref, "native_ref": native_ref,
            "evidence_id": "evidence.population." + generation + "." + record_id,
            "kind": "actor", "confidence": "documented",
            "display_name": Path(native_path).stem.removeprefix("NPCTemplate_").replace("_", " "),
            "claim": "Serialized NPC template at " + native_ref + "; bundle " + source_hash
                + "; object " + str(object_id) + ". Level and tags are observed; actor kind is unclassified. "
                "This component supplies no portrait/model reference and proves no runtime behavior.",
            "actor": actor})
        if len(records) > MAX_ITEMS:
            raise PreviewError("Actor definitions exceed the 10,000 record limit.")
    unsupported.extend({"NativeRef": token + "#/" + path, "Reason": "No supported NPC component."}
        for path in sorted(environment.container) if path not in observed)
    if not records:
        raise PreviewError("The installation supplied no supported actor templates.")
    reader.verify_sources()
    if digest(read_bounded(workspace, 1024 * 1024)) != workspace_hash:
        raise PreviewError("The workspace changed during extraction.")
    output = contained(preview_root / ("population-" + generation), preview_root)
    output.mkdir(parents=True)
    document = {"SchemaVersion": 1, "DocumentKind": "foa-native-population-observations",
        **{field: profile[field] for field in PROFILE_FIELDS},
        "CapturedAt": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "SourceFiles": list(reader.sources.values()), "OperationalAuthority": AUTHORITY,
        "definitions": records, "evidence": records, "UnsupportedEntries": unsupported,
        "Measurements": {"ActorCount": len(records), "UnsupportedCount": len(unsupported),
            "ElapsedSeconds": round(time.monotonic() - reader.started, 3)}}
    path = output / "foa-native-population.json"
    write_json(path, document)
    return path
