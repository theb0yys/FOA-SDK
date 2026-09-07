#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Read installed item references and publish local-only 2D thumbnail evidence."""
from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import sys
import time
from types import ModuleType
import uuid
import warnings

from foa_addressables_catalog import AddressablesCatalog, CatalogError

MAX_ITEMS = 10000
MAX_OBJECTS = 100000
MAX_BUNDLES = 2048
MAX_BUNDLE_BYTES = 64 * 1024 * 1024
MAX_EXPANDED_BYTES = 256 * 1024 * 1024
MAX_TOTAL_BYTES = 2 * 1024 * 1024 * 1024
MAX_SECONDS = 180
MAX_PIXELS = 16 * 1024 * 1024
PROFILE_FIELDS = ("ProfileId", "GameVersion", "Branch", "RuntimeTarget")
AUTHORITY = {key: False for key in (
    "RuntimeInvocationAllowed", "GameMutationAllowed", "SaveAccessAllowed",
    "CatalogPromotionAllowed", "RuntimePermissionGranted", "GeneratedO3dePreviewProduct",
    "O3deAssetProcessorInvoked", "UnityInvoked", "RepositoryWriteAllowed", "FunctionCompleteAllowed",
)}


class PreviewError(ValueError):
    """A preview operation failed without publishing a replacement manifest."""


def digest(data: bytes) -> str:
    return "sha256:" + hashlib.sha256(data).hexdigest()


def inside(path: Path, root: Path) -> bool:
    return path.resolve().is_relative_to(root.resolve())


def contained(path: Path, root: Path) -> Path:
    if not inside(path, root):
        raise PreviewError("Preview path escapes its configured root.")
    return path.resolve()


def read_bounded(path: Path, limit: int) -> bytes:
    with path.open("rb") as stream:
        if os.fstat(stream.fileno()).st_size > limit:
            raise PreviewError("Preview source exceeds the supported size limit.")
        value = stream.read(limit + 1)
    if len(value) > limit:
        raise PreviewError("Preview source grew beyond the supported size limit.")
    return value


def load_context(workspace: Path, forbidden_roots: list[Path]) -> tuple[dict, Path, Path]:
    document = json.loads(read_bounded(workspace, 1024 * 1024))
    if document.get("SchemaVersion") != 1:
        raise PreviewError("Save the workspace with the supported schema before refreshing items.")
    profiles = [p for p in document.get("GameProfiles", []) if p.get("ProfileId") == document.get("ActiveGameProfileId")]
    if len(profiles) != 1 or any(not isinstance(profiles[0].get(f), str) or not profiles[0][f] for f in PROFILE_FIELDS):
        raise PreviewError("The saved workspace needs one complete active game profile.")
    profile = profiles[0]
    if profile["RuntimeTarget"] not in {"Mono", "IL2CPP"}:
        raise PreviewError("The active profile has an unsupported runtime target.")
    def resolve(field: str, values: dict) -> Path:
        raw = values.get(field)
        if not isinstance(raw, str) or not raw.strip():
            raise PreviewError(f"The workspace is missing {field}.")
        return (workspace.parent / raw).resolve()
    install = resolve("InstallPath", profile)
    extracted = resolve("ExtractedDataPath", profile)
    root = resolve("RootPath", document)
    if not install.is_dir() or not inside(extracted, root) or extracted == root:
        raise PreviewError("The game or generated-data location is invalid.")
    output = contained(extracted / "PreviewArtifacts" / "NativeItems", extracted)
    if any(inside(output, p) or inside(p, output) for p in [install, *forbidden_roots]):
        raise PreviewError("Item previews must be outside the game, engine, and source checkout.")
    if not re.fullmatch(r"\d+\.\d+\.\d+[abfp]\d+", profile.get("UnityVersion", "")):
        raise PreviewError("The active profile needs its observed Unity version before reading bundles.")
    return profile, install, output


def preflight_bundle(payload: bytes) -> None:
    """Bound UnityFS decompression before passing the container to UnityPy."""
    stream = io.BytesIO(payload)
    def string() -> bytes:
        value = bytearray()
        for _ in range(64):
            byte = stream.read(1)
            if byte == b"\0":
                return bytes(value)
            if not byte:
                break
            value.extend(byte)
        raise PreviewError("Unsupported Unity bundle header.")
    if string() != b"UnityFS":
        raise PreviewError("Only UnityFS item bundles are supported.")
    version, = struct.unpack(">I", stream.read(4))
    string()
    string()
    size, packed, expanded, flags = struct.unpack(">QIII", stream.read(20))
    if version not in (6, 7, 8) or size != len(payload) or not 0 < packed <= 1024 * 1024 or not 0 < expanded <= 4 * 1024 * 1024:
        raise PreviewError("Unity bundle header exceeds preview limits.")
    if version >= 7:
        stream.seek((stream.tell() + 15) & ~15)
    if flags & 0x80:
        stream.seek(size - packed)
    block_info = stream.read(packed)
    if len(block_info) != packed:
        raise PreviewError("Unity bundle block metadata is truncated.")
    compression = flags & 0x3f
    if compression in (2, 3):
        import lz4.block
        block_info = lz4.block.decompress(block_info, uncompressed_size=expanded)
    elif compression != 0:
        raise PreviewError("Unsupported Unity bundle metadata compression.")
    if len(block_info) != expanded or len(block_info) < 20:
        raise PreviewError("Unity bundle block metadata has an invalid size.")
    blocks, = struct.unpack_from(">I", block_info, 16)
    if blocks > 65536 or 20 + blocks * 10 > len(block_info):
        raise PreviewError("Unity bundle block count exceeds preview limits.")
    total = sum(struct.unpack_from(">I", block_info, 20 + n * 10)[0] for n in range(blocks))
    if total > MAX_EXPANDED_BYTES:
        raise PreviewError("Unity bundle expanded size exceeds the preview limit.")


class BundleReader:
    def __init__(self, install: Path, unity_version: str, progress):
        # UnityPy eagerly imports audio support. This isolated image-only worker
        # disables that converter and does not package or load native FMOD.
        audio_module = ModuleType("UnityPy.export.AudioClipConverter")
        def reject_audio(*_args, **_kwargs):
            raise PreviewError("Audio extraction is not supported by the item preview reader.")
        audio_module.extract_audioclip_samples = reject_audio
        sys.modules[audio_module.__name__] = audio_module
        import UnityPy
        from fsspec.implementations.memory import MemoryFileSystem
        if UnityPy.__version__ != "1.24.2":
            raise PreviewError("The packaged item reader has an unexpected UnityPy version.")
        UnityPy.config.FALLBACK_UNITY_VERSION = unity_version
        self.unity = UnityPy
        self.filesystem = MemoryFileSystem()
        self.install = install
        self.sources: dict[str, dict] = {}
        self.total_bytes = 0
        self.started = time.monotonic()
        self.progress = progress

    def check_budget(self):
        if time.monotonic() - self.started > MAX_SECONDS:
            raise PreviewError("Item preview extraction exceeded its 180 second budget.")

    def load(self, path: Path):
        self.check_budget()
        path = contained(path, self.install)
        token = "$install/" + path.relative_to(self.install).as_posix()
        if len(self.sources) >= MAX_BUNDLES:
            raise PreviewError("Too many bundles for one item preview refresh.")
        payload = read_bounded(path, MAX_BUNDLE_BYTES)
        self.total_bytes += len(payload)
        if self.total_bytes > MAX_TOTAL_BYTES:
            raise PreviewError("Item preview input exceeds the 2 GiB refresh limit.")
        preflight_bundle(payload)
        source_hash = digest(payload)
        self.sources[token] = {"Locator": token, "Sha256": source_hash, "ByteSize": len(payload),
                               "ModifiedMs": path.stat().st_mtime_ns // 1000000}
        # An empty in-memory filesystem prevents UnityPy from walking or opening the game directory.
        environment = self.unity.Environment(payload, fs=self.filesystem)
        if len(environment.objects) > MAX_OBJECTS:
            raise PreviewError("Unity bundle has too many objects for item previews.")
        return environment, source_hash, token

    def verify_sources(self):
        for source in self.sources.values():
            self.check_budget()
            path = contained(self.install / source["Locator"][len("$install/"):], self.install)
            payload = read_bounded(path, MAX_BUNDLE_BYTES)
            if len(payload) != source["ByteSize"] or digest(payload) != source["Sha256"] or path.stat().st_mtime_ns // 1000000 != source["ModifiedMs"]:
                raise PreviewError("The game changed during extraction. Refresh items again.")


def item_category(native_path: str) -> str:
    """Presentation groups from explicit source folders/type tokens, not runtime item classes."""
    path = Path(native_path.replace("\\", "/"))
    folders = {part.casefold() for part in path.parts[:-1]}
    name = path.stem.casefold()
    tokens = set(name.split("_"))
    type_match = re.search(r"(?:^|_)itemtemplate_(?:sos_)?(armor|jewelry|weapon|magic|currency)(?:_|$)", name)
    item_type = type_match[1] if type_match else ""
    if "abstracttypes" in folders or name.startswith("abstract_"):
        return "Developer templates / Abstract templates"
    if "debug" in folders or any(part.startswith("debug_") for part in folders):
        return "Developer templates / Debug items"
    if folders & {"unused", "notinuse", "_old", "forremoval", "obsolete", "useless"} or name.startswith(("useless_", "doesntwork_")):
        return "Developer templates / Unused items"
    if "unbalanceditems" in folders:
        return "Developer templates / Experimental items"
    if "jewelry" in folders or item_type == "jewelry":
        if "amulets" in folders or "amulet" in tokens:
            return "Jewelry / Amulets"
        if "rings" in folders or "ring" in tokens:
            return "Jewelry / Rings"
        return "Jewelry / Other jewelry"
    if "armors" in folders or item_type == "armor":
        for weight in ("light", "medium", "heavy"):
            if weight in tokens or any(part.endswith("_" + weight) for part in folders):
                return "Armor / " + weight.title()
        return "Armor / Other armor"
    if "weapons" in folders or item_type in {"weapon", "magic"}:
        if "magic" in folders or "magic" in tokens:
            return "Weapons / Magic"
        if "monsters" in folders or "monster" in tokens:
            return "Weapons / Monster weapons"
        for keys, label in (
            ({"ammo", "arrow"}, "Ammunition"), ({"shield", "shields"}, "Shields"),
            ({"bow", "greatbow", "crossbow"}, "Bows"), ({"throwable", "throwables"}, "Thrown weapons"),
            ({"sword"}, "Swords"), ({"axe"}, "Axes"), ({"mace", "hammer", "club"}, "Maces and hammers"),
            ({"dagger", "daggers"}, "Daggers"), ({"polearm", "spear"}, "Polearms"),
            ({"wand", "wands"}, "Wands"), ({"staff", "staffs"}, "Staves"),
            ({"fist", "fists"}, "Unarmed"), ({"tools", "improvised"}, "Tools and improvised weapons"),
        ):
            if keys & (tokens | folders):
                return "Weapons / " + label
        return "Weapons / Other weapons"
    if "gems" in folders:
        if "armorgems" in folders:
            return "Gems / Armor gems"
        if "weapongems" in folders:
            return "Gems / Weapon gems"
        return "Gems / Other gems"
    if "fishtemplates" in folders:
        return "Ingredients / Fish"
    if "craftingingredients" in folders:
        if "alchemy" in folders:
            return "Ingredients / Alchemy"
        if "cooking" in folders:
            return "Ingredients / Cooking"
        return "Ingredients / Crafting materials"
    if "craftingresults" in folders:
        if "alchemy" in folders:
            return "Consumables / Potions"
        if "cooking" in folders:
            return "Consumables / Food and drink"
        return "Crafting / Crafted items"
    if "crazyconsumables" in folders:
        if "weapongrease" in folders:
            return "Consumables / Weapon coatings"
        if "questitems" in folders:
            return "Quest items / Consumables"
        return "Consumables / Other consumables"
    if "craftingrecipes" in folders or ("readables" in folders and "crafting" in folders):
        return "Crafting / Recipes"
    if "questcrafting" in folders:
        return "Crafting / Quest crafting"
    if "readables" in folders:
        if "books" in folders:
            return "Books and notes / Books"
        return "Books and notes / Notes and letters"
    if "keys" in folders:
        return "Keys / " + ("Housing keys" if "housing keys" in folders else "Other keys")
    if "storyitems" in folders or "stories" in folders:
        return "Quest items / Story items"
    if "housing" in folders:
        if folders & {"beds", "chairs", "tables", "closets", "dressers", "cabinets", "racks"}:
            return "Housing / Furniture"
        return "Housing / Decorations"
    if "seeds" in folders:
        return "Ingredients / Seeds"
    if item_type == "currency":
        return "Miscellaneous / Currency"
    if "garbageitems" in folders:
        return "Miscellaneous / " + ("Valuables" if "valuable" in folders or "valuable" in tokens else "Junk and tools")
    if "relics" in folders:
        return "Miscellaneous / Relics"
    return "Miscellaneous / Other items"


def discover_items(environment, source_hash: str, source_token: str, profile: dict) -> list[dict]:
    records = []
    for native_path, pointer in sorted(environment.container.items()):
        obj = pointer.deref()
        if obj.type.name != "GameObject":
            continue
        game_object = obj.parse_as_object()
        for component in game_object.m_Component:
            pointer = component.component
            if pointer.m_FileID != 0:
                continue
            reader = pointer.deref()
            if reader.type.name != "MonoBehaviour":
                continue
            data = reader.parse_as_dict()
            if "iconReference" not in data:
                continue
            reference = data["iconReference"].get("arSpriteReference", {})
            name = Path(native_path).stem.removeprefix("ItemTemplate_").replace("_", " ")
            identity = json.dumps([*[profile[f] for f in PROFILE_FIELDS], source_token, source_hash, native_path, obj.path_id])
            records.append({
                "AssetRecordId": "native.item." + hashlib.sha256(identity.encode()).hexdigest()[:32],
                "NativeAssetRef": source_token + "#/" + native_path,
                "DisplayName": name,
                "Category": item_category(native_path),
                "SourceSha256": source_hash,
                "SourceObjectPathId": str(obj.path_id),
                "IconAddress": reference.get("address", ""),
                "IconSubObject": reference.get("subObjectName", ""),
            })
            if len(records) > MAX_ITEMS:
                raise PreviewError("Item count exceeds the 10,000 row preview limit.")
            break
    if not records:
        raise PreviewError("No supported item icon references were found in the installed item bundle.")
    return records


def decode_icon(environment, address: str, sub_object: str):
    if address not in environment.container:
        raise PreviewError("Icon address is absent from its referenced bundle.")
    pointer = environment.container[address]
    obj = pointer.deref()
    data = obj.parse_as_object()
    if obj.type.name == "Texture2D":
        width, height = data.m_Width, data.m_Height
    elif obj.type.name == "Sprite":
        width, height = data.m_Rect.width, data.m_Rect.height
    else:
        raise PreviewError("Icon address does not resolve to a Sprite or Texture2D.")
    if sub_object and data.m_Name != sub_object:
        raise PreviewError("The requested icon sub-object is unsupported.")
    if width <= 0 or height <= 0 or width > 8192 or height > 8192 or width * height > MAX_PIXELS:
        raise PreviewError("Icon dimensions exceed the preview limit.")
    image = data.image
    if image.width * image.height > MAX_PIXELS:
        raise PreviewError("Decoded icon exceeds the preview pixel limit.")
    image.thumbnail((512, 512))
    return image, str(obj.path_id), data.m_Name


def write_json(path: Path, document: dict):
    payload = json.dumps(document, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(payload) > 16 * 1024 * 1024:
        raise PreviewError("Item preview evidence exceeds the document limit.")
    temporary = path.with_suffix(".tmp")
    temporary.write_bytes(payload)
    temporary.replace(path)


def extract(workspace: Path, forbidden_roots: list[Path], progress=print) -> Path:
    profile, install, output = load_context(workspace, forbidden_roots)
    workspace_hash = digest(read_bounded(workspace, 1024 * 1024))
    data_root = contained(install / "Fall of Avalon_Data", install)
    catalog_path = contained(data_root / "StreamingAssets" / "aa" / "catalog.json", install)
    catalog_payload = read_bounded(catalog_path, 32 * 1024 * 1024)
    catalog = AddressablesCatalog(json.loads(catalog_payload))
    bundle_root = contained(catalog_path.parent / "StandaloneWindows64", install)
    reader = BundleReader(install, profile["UnityVersion"], progress)
    progress("Reading installed item references...")
    environment, source_hash, source_token = reader.load(bundle_root / "templates.items_assets_all.bundle")
    records = discover_items(environment, source_hash, source_token, profile)
    del environment
    output.mkdir(parents=True, exist_ok=True)
    output = contained(output, (workspace.parent / profile["ExtractedDataPath"]).resolve())
    generation = contained(output / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + "-" + uuid.uuid4().hex), output)
    generation.mkdir()
    binding = {f: profile[f] for f in PROFILE_FIELDS}
    index_id = "native.items.index." + hashlib.sha256(json.dumps([binding, source_hash, digest(catalog_payload)], sort_keys=True).encode()).hexdigest()[:32]
    artifacts = {r["AssetRecordId"]: {
        **r, "ThumbnailArtifactId": "thumbnail." + r["AssetRecordId"], "SourceIndexId": index_id,
        "Status": "unsupported", "Fidelity": "unsupported", "Issue": "Item has no supported icon reference.",
        "RepositoryCommitAllowed": False, "RedistributionAllowed": False,
        "RuntimePermissionGranted": False, "GeneratedO3dePreviewProduct": False,
    } for r in records}
    requests = defaultdict(list)
    for record in records:
        locations = catalog.icon_locations(record["IconAddress"])
        if len(locations) == 1:
            bundle, address = locations[0]
            requests[bundle].append((record, address))
        elif locations:
            artifacts[record["AssetRecordId"]]["Issue"] = "Icon has multiple bundle dependencies; extraction is unsupported."
    completed = 0
    for number, (bundle, items) in enumerate(sorted(requests.items()), 1):
        progress(f"Reading item icons: bundle {number}/{len(requests)}, {completed} previews ready...")
        environment, icon_hash, icon_token = reader.load(bundle_root / bundle)
        decoded = {}
        for record, address in items:
            reader.check_budget()
            artifact = artifacts[record["AssetRecordId"]]
            try:
                key = (address, record["IconSubObject"])
                if key not in decoded:
                    image, object_id, object_name = decode_icon(environment, *key)
                    image_id = hashlib.sha256(json.dumps([icon_hash, address, key[1]]).encode()).hexdigest() + ".png"
                    image_path = contained(generation / image_id, generation)
                    image.save(image_path)
                    payload = read_bounded(image_path, 4 * 1024 * 1024)
                    decoded[key] = {
                        "GeneratedArtifactPath": "$preview/" + image_id,
                        "OutputSha256": digest(payload), "OutputByteSize": len(payload),
                        "OutputMediaType": "image/png", "Width": image.width, "Height": image.height,
                        "IconObjectPathId": object_id, "IconObjectName": object_name,
                    }
                artifact.update(decoded[key])
                artifact["ThumbnailArtifactId"] = "thumbnail." + hashlib.sha256(json.dumps([
                    record["AssetRecordId"], icon_hash, address, artifact["OutputSha256"]
                ]).encode()).hexdigest()[:32]
                artifact.update({"Status": "generated", "Fidelity": "native-icon-decoded", "Issue": "",
                                 "GenerationMethod": "unitypy-1.24.2-sprite-or-texture-to-png",
                                 "IconBundle": icon_token, "IconSourceSha256": icon_hash})
                completed += 1
            except (ValueError, KeyError, FileNotFoundError, NotImplementedError) as exc:
                artifact["Issue"] = str(exc)[:240]
        del environment
    if completed == 0:
        raise PreviewError("No item icons could be decoded from this installation.")
    progress("Verifying source files before publishing item previews...")
    reader.verify_sources()
    if digest(read_bounded(catalog_path, 32 * 1024 * 1024)) != digest(catalog_payload) or digest(read_bounded(workspace, 1024 * 1024)) != workspace_hash:
        raise PreviewError("The workspace or game catalog changed. Refresh items again.")
    reader.check_budget()
    sources = list(reader.sources.values())
    sources.append({"Locator": "$install/" + catalog_path.relative_to(install).as_posix(), "Sha256": digest(catalog_payload),
                    "ByteSize": len(catalog_payload), "ModifiedMs": catalog_path.stat().st_mtime_ns // 1000000})
    index = {"SchemaVersion": 1, "DocumentKind": "foa-native-item-discovery-index", "IndexId": index_id,
             **binding, "AssetRecords": records, "SourceFiles": sources, "OperationalAuthority": AUTHORITY}
    write_json(generation / "foa-native-item-index.json", index)
    manifest = {"SchemaVersion": 1, "DocumentKind": "foa-thumbnail-artifact-evidence", **binding,
                "SourceIndexId": index_id, "ManifestId": "native.item.thumbnails." + generation.name.lower(),
                "ToolId": "foa.native-item-preview", "ToolVersion": "1.0.0", "UnityPyVersion": "1.24.2",
                "InstallRootFingerprint": digest((install.as_posix().casefold() if os.name == "nt" else install.as_posix()).encode()),
                "SourceFiles": sources,
                "CapturedAt": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "ThumbnailArtifacts": list(artifacts.values()), "OperationalAuthority": AUTHORITY,
                "PreviewStageStatus": {"FunctionCompleteAllowed": False, "TypedAuthoringBindingCreated": False,
                                       "O3deAssetBrowserEntryCreated": False},
                "Measurements": {"ElapsedSeconds": round(time.monotonic() - reader.started, 3),
                                 "ItemCount": len(records), "GeneratedCount": completed,
                                 "BundleCount": len(reader.sources), "InputBytes": reader.total_bytes,
                                 "TimeBudgetSeconds": MAX_SECONDS}}
    manifest_path = generation / "foa-thumbnail-artifacts.json"
    write_json(manifest_path, manifest)
    return manifest_path


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--vendor", type=Path)
    parser.add_argument("--forbid-root", action="append", default=[], type=Path)
    args = parser.parse_args(argv)
    if args.vendor:
        sys.path.insert(0, str(args.vendor.resolve()))
    # Source checkout is always excluded even for standalone command-line use.
    source_root = Path(__file__).resolve().parents[3]
    forbidden = args.forbid_root + ([source_root] if (source_root / "AGENTS.md").is_file() else [])
    def progress(message):
        print(json.dumps({"progress": message}), flush=True)
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            path = extract(args.workspace.resolve(), forbidden, progress)
        print(json.dumps({"manifest": str(path)}), flush=True)
        return 0
    except (PreviewError, CatalogError, OSError, ValueError, KeyError, ImportError, struct.error) as exc:
        print(json.dumps({"error": str(exc)[:500]}), flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
