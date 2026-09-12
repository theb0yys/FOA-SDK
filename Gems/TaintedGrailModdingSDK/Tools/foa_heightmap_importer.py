#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Shared mesh rasterization and synthetic publisher helpers.

The Editor campaign provider uses these geometry helpers, verifies source
selection, and hands a real RAW export to the SDK importer. The legacy direct
campaign import command is disabled because its provenance was unqualified.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import math
import os
import re
import shutil
import stat
import struct
import sys
from array import array
from dataclasses import dataclass, replace
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable, Mapping, Sequence

TOOL_VERSION = "0.2.0"
IMPORTER_ID = "importer.foa-heightmap-unity-mesh"
UNITY_FALLBACK_VERSION = "6000.0.64f1"
TILE_SIZE = 1024
MAX_GRID_DIMENSION = 32768
MAX_TOTAL_SAMPLES = 16385 * 16385

GAME_EXE_NAME = "Fall of Avalon.exe"
GAME_DATA_DIR = "Fall of Avalon_Data"
ADDRESSABLES_DIR = Path(GAME_DATA_DIR) / "StreamingAssets" / "aa"
WINDOWS_BUNDLE_DIR = ADDRESSABLES_DIR / "StandaloneWindows64"
CATALOG_PATH = ADDRESSABLES_DIR / "catalog.json"

UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
CAB_RE = re.compile(r"(cab-[0-9a-f]{32})", re.IGNORECASE)
ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$")
SHA_RE = re.compile(r"^sha256:[0-9a-f]{64}$")


class HeightmapImportError(RuntimeError):
    """Raised when the importer cannot produce a bounded local heightmap."""


@dataclass(frozen=True)
class CampaignMap:
    key: str
    map_id: str
    display_name: str
    public_alias: str
    scene_bundle_name: str
    addressable_keys: tuple[str, ...]


@dataclass(frozen=True)
class GameInstall:
    root: Path
    bundle_root: Path
    catalog_path: Path


@dataclass(frozen=True)
class Pose:
    position: tuple[float, float, float]
    rotation: tuple[float, float, float, float]
    scale: tuple[float, float, float]


@dataclass(frozen=True)
class MeshPayload:
    name: str
    vertices: tuple[tuple[float, float, float], ...]
    triangles: tuple[tuple[int, int, int], ...]


@dataclass(frozen=True)
class MeshInstance:
    game_object_name: str
    mesh_name: str
    vertices: tuple[tuple[float, float, float], ...]
    triangles: tuple[tuple[int, int, int], ...]


@dataclass(frozen=True)
class Bounds:
    min_x: float
    max_x: float
    min_y: float
    max_y: float
    min_z: float
    max_z: float


@dataclass(frozen=True)
class RasterResult:
    width: int
    height: int
    bounds: Bounds
    heights: array
    filled_samples: int
    rasterized_triangles: int
    source_coverage: bytes | None = None


@dataclass(frozen=True)
class ImportSettings:
    game_root: Path | None
    workspace_root: Path
    map_key: str
    resolution: int
    mesh_source: str
    include_inactive: bool
    include_name_regex: str | None
    exclude_name_regex: str | None
    max_meshes: int | None
    max_triangles: int
    created_at_utc: str
    operation_id: str | None
    profile_id: str
    game_version: str
    branch: str
    runtime_target: str
    terrain_layer_mask: int | None = None


CAMPAIGN_MAPS: dict[str, CampaignMap] = {
    "hos": CampaignMap(
        key="hos",
        map_id="terrain-map.foa.hos",
        display_name="Horns of the South Terrain",
        public_alias="Horns of the South",
        scene_bundle_name="scenes_scenes_campaignmap_hos_static.bundle",
        addressable_keys=(
            "CampaignMap_HOS",
            "CampaignMap_HOS_merged",
            "CampaignMap_HOS_merged_Static",
            "CampaignMap_HOS_Static",
        ),
    ),
    "cuanacht": CampaignMap(
        key="cuanacht",
        map_id="terrain-map.foa.cuanacht",
        display_name="Cuanacht Terrain",
        public_alias="Cuanacht / Cuanacht Village",
        scene_bundle_name="scenes_scenes_campaignmap_cuanacht_static.bundle",
        addressable_keys=(
            "CampaignMap_Cuanacht",
            "CampaignMap_Cuanacht_merged",
            "CampaignMap_Cuanacht_merged_Static",
            "CampaignMap_Cuanacht_Static",
        ),
    ),
    "forlorn": CampaignMap(
        key="forlorn",
        map_id="terrain-map.foa.forlorn",
        display_name="Forlorn Swords Terrain",
        public_alias="Forlorn Swords",
        scene_bundle_name="scenes_scenes_campaignmap_forlorn_static.bundle",
        addressable_keys=(
            "CampaignMap_Forlorn",
            "CampaignMap_Forlorn_merged",
            "CampaignMap_Forlorn_merged_Static",
            "CampaignMap_Forlorn_Static",
        ),
    ),
    "sarras": CampaignMap(
        key="sarras",
        map_id="terrain-map.foa.sarras",
        display_name="Sarras Terrain",
        public_alias="Sanctuary of Sarras / Sarras",
        scene_bundle_name="scenes_scenes_campaignmap_sarras_static.bundle",
        addressable_keys=(
            "CampaignMap_Sarras",
            "CampaignMap_Sarras_merged",
            "CampaignMap_Sarras_merged_Static",
            "CampaignMap_Sarras_Static",
        ),
    ),
}


def eprint(message: str) -> None:
    print(message, file=sys.stderr)


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def pretty_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=True, indent=2) + "\n").encode("utf-8")


def sha256_bytes(payload: bytes) -> str:
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def sha256_text(value: str) -> str:
    return sha256_bytes(value.encode("utf-8"))


def sha256_file(path: Path, cancelled=None, max_bytes=None) -> tuple[str, int]:
    expected = path.stat().st_size
    if max_bytes is not None and expected > max_bytes:
        raise HeightmapImportError("Source file exceeds its byte budget.")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as handle:
        while size < expected:
            check_cancelled(cancelled)
            chunk = handle.read(min(1024 * 1024, expected-size))
            if not chunk:
                raise HeightmapImportError("Source file changed while fingerprinting.")
            size += len(chunk)
            digest.update(chunk)
        if handle.read(1):
            raise HeightmapImportError("Source file changed while fingerprinting.")
    return "sha256:" + digest.hexdigest(), size


def strict_utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def require_utc(value: str) -> str:
    if not UTC_RE.match(value):
        raise HeightmapImportError("created-at must use whole-second UTC format YYYY-MM-DDTHH:MM:SSZ.")
    datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    return value


def require_id(value: str, label: str) -> str:
    if not ID_RE.match(value) or ".." in value or ":" in value or "/" in value or "\\" in value:
        raise HeightmapImportError(f"{label} must be a lower-case persistence-safe dotted identifier.")
    return value


def require_sha(value: str, label: str) -> str:
    if not SHA_RE.match(value):
        raise HeightmapImportError(f"{label} must be sha256:<64-lower-hex>.")
    return value


def require_name_regex(value: str | None, label: str) -> str | None:
    if value is None:
        return None
    try:
        re.compile(value, re.IGNORECASE)
    except re.error as exc:
        raise HeightmapImportError(f"{label} must be a valid regular expression: {exc}.") from exc
    return value


def compile_name_regex(value: str | None, label: str) -> re.Pattern[str] | None:
    validated = require_name_regex(value, label)
    return re.compile(validated, re.IGNORECASE) if validated else None


def require_import_limits(settings: ImportSettings) -> ImportSettings:
    if settings.resolution <= 1 or settings.resolution > MAX_GRID_DIMENSION:
        raise HeightmapImportError(f"resolution must be in the range 2..{MAX_GRID_DIMENSION}.")
    if settings.resolution * settings.resolution > MAX_TOTAL_SAMPLES:
        raise HeightmapImportError("resolution exceeds the terrain heightmap total-sample bound.")
    if settings.max_meshes is not None and settings.max_meshes <= 0:
        raise HeightmapImportError("max-meshes must be positive when provided.")
    if settings.max_triangles <= 0:
        raise HeightmapImportError("max-triangles must be positive.")
    require_name_regex(settings.include_name_regex, "include-name-regex")
    require_name_regex(settings.exclude_name_regex, "exclude-name-regex")
    return settings


def default_install_candidates() -> tuple[Path, ...]:
    candidates: list[Path] = []
    env_value = os.environ.get("FOA_GAME_ROOT")
    if env_value:
        candidates.append(Path(env_value))
    for drive in ("S", "D", "E", "C"):
        candidates.append(Path(f"{drive}:/SteamLibrary/steamapps/common/Tainted Grail FoA"))
    candidates.append(Path("C:/Program Files (x86)/Steam/steamapps/common/Tainted Grail FoA"))
    return tuple(dict.fromkeys(candidates))


def resolve_game_install(game_root: Path | None) -> GameInstall:
    candidates = (game_root,) if game_root is not None else default_install_candidates()
    checked: list[str] = []
    for candidate in candidates:
        root = candidate.expanduser().resolve(strict=False)
        checked.append(str(root))
        bundle_root = root / WINDOWS_BUNDLE_DIR
        catalog_path = root / CATALOG_PATH
        if (root / GAME_EXE_NAME).is_file() and bundle_root.is_dir() and catalog_path.is_file():
            return GameInstall(root=root, bundle_root=bundle_root, catalog_path=catalog_path)
    raise HeightmapImportError("Unable to find a Tainted Grail FoA install. Checked: " + "; ".join(checked))


def decode_addressable_key_data(encoded: str) -> list[str]:
    data = base64.b64decode(encoded)
    if len(data) < 4:
        raise HeightmapImportError("Addressables key table is too short.")
    count = int.from_bytes(data[0:4], "little", signed=True)
    if count < 0:
        raise HeightmapImportError("Addressables key table has a negative count.")
    offset = 4
    keys: list[str] = []
    for _ in range(count):
        if offset + 5 > len(data):
            raise HeightmapImportError("Addressables key table ended mid-entry.")
        value_type = data[offset]
        offset += 1
        byte_count = int.from_bytes(data[offset : offset + 4], "little", signed=True)
        offset += 4
        if byte_count < 0 or offset + byte_count > len(data):
            raise HeightmapImportError("Addressables key table has an invalid string length.")
        raw = data[offset : offset + byte_count]
        offset += byte_count
        if value_type == 0:
            keys.append(raw.decode("utf-8", errors="strict"))
    return keys


def read_catalog_keys(catalog_path: Path) -> set[str]:
    catalog = json.loads(catalog_path.read_text(encoding="utf-8", errors="strict"))
    encoded = catalog.get("m_KeyDataString")
    if not isinstance(encoded, str):
        return set()
    return set(decode_addressable_key_data(encoded))


def list_campaign_maps(install: GameInstall) -> list[dict[str, Any]]:
    try:
        catalog_keys = read_catalog_keys(install.catalog_path)
    except Exception:
        catalog_keys = set()
    result: list[dict[str, Any]] = []
    for campaign in CAMPAIGN_MAPS.values():
        bundle_path = install.bundle_root / campaign.scene_bundle_name
        result.append(
            {
                "key": campaign.key,
                "map_id": campaign.map_id,
                "display_name": campaign.display_name,
                "scene_bundle": campaign.scene_bundle_name,
                "scene_bundle_exists": bundle_path.is_file(),
                "scene_bundle_bytes": bundle_path.stat().st_size if bundle_path.is_file() else 0,
                "addressable_keys_present": sorted(key for key in campaign.addressable_keys if key in catalog_keys),
                "height_source": "enabled Unity MeshCollider geometry",
            }
        )
    return result


def import_unitypy(unity_version: str):
    try:
        import UnityPy  # type: ignore
        from UnityPy.helpers.MeshHelper import MeshHandler  # type: ignore
    except ImportError as exc:
        raise HeightmapImportError(
            "UnityPy is required for FoA heightmap extraction. Install the qualified UnityPy dependency in the tool environment."
        ) from exc
    UnityPy.config.FALLBACK_UNITY_VERSION = unity_version
    # The optional native typetree reader crashed on this scene cohort during qualification.
    from UnityPy.helpers import TypeTreeHelper
    TypeTreeHelper.read_typetree_boost = None
    return UnityPy, MeshHandler


def pptr_path_id(pointer: Any) -> int:
    return int(getattr(pointer, "path_id", getattr(pointer, "m_PathID", 0)))


def pptr_file_id(pointer: Any) -> int:
    return int(getattr(pointer, "file_id", getattr(pointer, "m_FileID", 0)))


def pptr_key(pointer: Any) -> tuple[str, int, int]:
    assets_file = getattr(pointer, "assetsfile", None)
    file_name = str(getattr(assets_file, "name", ""))
    return file_name.lower(), pptr_file_id(pointer), pptr_path_id(pointer)


def vector3(value: Any) -> tuple[float, float, float]:
    if isinstance(value, (tuple, list)) and len(value) >= 3:
        return float(value[0]), float(value[1]), float(value[2])
    return float(value.x), float(value.y), float(value.z)


def quaternion(value: Any) -> tuple[float, float, float, float]:
    return float(value.x), float(value.y), float(value.z), float(value.w)


def quat_mul(left: tuple[float, float, float, float], right: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


def quat_rotate(rotation: tuple[float, float, float, float], point: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z, w = rotation
    px, py, pz = point
    tx = 2.0 * (y * pz - z * py)
    ty = 2.0 * (z * px - x * pz)
    tz = 2.0 * (x * py - y * px)
    return (
        px + w * tx + (y * tz - z * ty),
        py + w * ty + (z * tx - x * tz),
        pz + w * tz + (x * ty - y * tx),
    )


def compose_pose(parent: Pose, local: Pose) -> Pose:
    scaled_local_position = (
        parent.scale[0] * local.position[0],
        parent.scale[1] * local.position[1],
        parent.scale[2] * local.position[2],
    )
    rotated_local_position = quat_rotate(parent.rotation, scaled_local_position)
    return Pose(
        position=(
            parent.position[0] + rotated_local_position[0],
            parent.position[1] + rotated_local_position[1],
            parent.position[2] + rotated_local_position[2],
        ),
        rotation=quat_mul(parent.rotation, local.rotation),
        scale=(
            parent.scale[0] * local.scale[0],
            parent.scale[1] * local.scale[1],
            parent.scale[2] * local.scale[2],
        ),
    )


IDENTITY_POSE = Pose((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0))


def transform_pose(pointer: Any, cache: dict[tuple[str, int, int], Pose], stack: set[tuple[str, int, int]] | None = None) -> Pose:
    if pointer is None or pptr_path_id(pointer) == 0:
        return IDENTITY_POSE
    key = pptr_key(pointer)
    if key in cache:
        return cache[key]
    if stack is None:
        stack = set()
    if key in stack:
        return IDENTITY_POSE
    stack.add(key)
    transform = pointer.read()
    local = Pose(
        position=vector3(transform.m_LocalPosition),
        rotation=quaternion(transform.m_LocalRotation),
        scale=vector3(transform.m_LocalScale),
    )
    parent_pointer = getattr(transform, "m_Father", None)
    parent = transform_pose(parent_pointer, cache, stack)
    world = compose_pose(parent, local)
    cache[key] = world
    stack.remove(key)
    return world


def apply_pose(pose: Pose, point: tuple[float, float, float]) -> tuple[float, float, float]:
    scaled = (
        pose.scale[0] * point[0],
        pose.scale[1] * point[1],
        pose.scale[2] * point[2],
    )
    rotated = quat_rotate(pose.rotation, scaled)
    return (
        pose.position[0] + rotated[0],
        pose.position[1] + rotated[1],
        pose.position[2] + rotated[2],
    )


IDENTITY_MATRIX = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def matrix_product(a, b):
    return tuple(sum(a[row * 4 + k] * b[k * 4 + column] for k in range(4))
                 for row in range(4) for column in range(4))


def apply_matrix(matrix, point):
    return tuple(sum(matrix[row * 4 + k] * point[k] for k in range(3)) + matrix[row * 4 + 3]
                 for row in range(3))


def game_object_transform(game_object):
    for entry in getattr(game_object, "m_Component", ()):
        pointer = getattr(entry, "component", entry)
        if pointer.deref().type.name == "Transform":
            return pointer
    pointer = getattr(game_object, "m_Transform", None)
    if pointer is None:
        raise HeightmapImportError("Mesh GameObject has no resolvable Transform component.")
    return pointer


def transform_matrix(pointer, cache, stack=None):
    if pointer is None or pptr_path_id(pointer) == 0:
        return IDENTITY_MATRIX, True
    key = pptr_key(pointer)
    if key in cache:
        return cache[key]
    stack = set() if stack is None else stack
    if key in stack or len(stack) >= 128:
        raise HeightmapImportError("Mesh Transform hierarchy is cyclic or too deep.")
    stack.add(key)
    try:
        data = pointer.read()
        position = vector3(data.m_LocalPosition)
        scale = vector3(data.m_LocalScale)
        rotation = quaternion(data.m_LocalRotation)
        if not all(math.isfinite(v) for v in (*position, *scale, *rotation)):
            raise HeightmapImportError("Mesh Transform contains a non-finite value.")
        basis = [quat_rotate(rotation, tuple(float(i == axis) for i in range(3))) for axis in range(3)]
        local = tuple(basis[col][row] * scale[col] if col < 3 else position[row]
                      for row in range(3) for col in range(4)) + (0., 0., 0., 1.)
        parent, parent_active = transform_matrix(data.m_Father, cache, stack)
        active = parent_active and bool(data.m_GameObject.read().m_IsActive)
        result = matrix_product(parent, local), active
        cache[key] = result
        return result
    finally:
        stack.remove(key)


def read_bundle_cab_names(path: Path) -> tuple[list[str], int]:
    """Read only the bounded UnityFS directory metadata, never the asset payload blocks."""
    def exact(handle, size):
        data = handle.read(size)
        if len(data) != size:
            raise HeightmapImportError("UnityFS directory metadata is truncated.")
        return data
    def cstring(handle):
        data = bytearray()
        for _ in range(512):
            char = exact(handle, 1)
            if char == b"\0":
                return data.decode("utf-8")
            data.extend(char)
        raise HeightmapImportError("UnityFS directory string exceeds its limit.")
    with path.open("rb") as handle:
        if cstring(handle) != "UnityFS":
            return [], 0
        version = struct.unpack(">I", exact(handle, 4))[0]
        if version not in (7, 8):
            raise HeightmapImportError("Unsupported UnityFS directory version.")
        cstring(handle)
        cstring(handle)
        size, compressed, uncompressed, flags = struct.unpack(">QIII", exact(handle, 20))
        if size != path.stat().st_size or not 0 < compressed <= 2 * 1024 * 1024 or not 0 < uncompressed <= 8 * 1024 * 1024:
            raise HeightmapImportError("UnityFS directory sizes exceed their bounds.")
        if flags & 0x400:
            raise HeightmapImportError("Encrypted UnityFS input is unsupported.")
        offset = size - compressed if flags & 0x80 else (handle.tell() + 15) // 16 * 16
        if offset < handle.tell() or offset + compressed > size:
            raise HeightmapImportError("UnityFS directory offset is invalid.")
        handle.seek(offset)
        block = exact(handle, compressed)
    codec = flags & 0x3f
    if codec in (2, 3):
        import lz4.block
        block = lz4.block.decompress(block, uncompressed_size=uncompressed)
    elif codec != 0:
        raise HeightmapImportError("Unsupported UnityFS directory compression.")
    if len(block) != uncompressed:
        raise HeightmapImportError("UnityFS directory decompressed size is invalid.")
    reader = io.BytesIO(block)
    exact(reader, 16)
    count = struct.unpack(">I", exact(reader, 4))[0]
    if count > 500000 or 20 + 10 * count + 4 > len(block):
        raise HeightmapImportError("UnityFS block table exceeds the directory bounds.")
    exact(reader, 10 * count)
    count = struct.unpack(">I", exact(reader, 4))[0]
    if not 0 < count <= 128:
        raise HeightmapImportError("UnityFS file count exceeds its limit.")
    names = []
    for _ in range(count):
        exact(reader, 20)
        name = cstring(reader)
        if CAB_RE.fullmatch(name):
            names.append(name.lower())
    return names, compressed


def cab_id_from_value(value: str) -> str | None:
    match = CAB_RE.search(value)
    return match.group(1).lower() if match else None


def cab_id_for_pointer(pointer: Any) -> str | None:
    file_id = pptr_file_id(pointer)
    if file_id <= 0:
        return None
    assets_file = getattr(pointer, "assetsfile", None)
    externals = getattr(assets_file, "externals", []) or []
    index = file_id - 1
    if index < 0 or index >= len(externals):
        return None
    return cab_id_from_value(str(getattr(externals[index], "path", "")))


def bundle_asset_file_name(UnityPy: Any, path: Path) -> str:
    env = UnityPy.load(str(path))
    if not env.objects:
        return ""
    return str(getattr(env.objects[0].assets_file, "name", "")).lower()


def direct_cab_candidates(bundle_root: Path, cab_id: str) -> list[Path]:
    stripped = cab_id.removeprefix("cab-")
    return [
        bundle_root / f"{cab_id}.bundle",
        bundle_root / f"{stripped}.bundle",
    ]


class CabDependencyResolver:
    def __init__(
        self,
        UnityPy: Any,
        bundle_root: Path,
        *,
        progress: Callable[[str], None] | None = None,
    ) -> None:
        self.UnityPy = UnityPy
        self.bundle_root = bundle_root
        self.progress = progress
        self.resolved: dict[str, Path] = {}
        self.loaded: set[str] = set()
        self.scanned_paths: set[Path] = set()
        self.scanned_count = 0
        self.metadata_bytes = 0

    @property
    def loaded_count(self) -> int:
        return len(self.loaded)

    def resolve(self, cab_id: str) -> Path:
        cab_id = cab_id.lower()
        cached = self.resolved.get(cab_id)
        if cached is not None:
            return cached
        for candidate in direct_cab_candidates(self.bundle_root, cab_id):
            if not candidate.is_file():
                continue
            if candidate.resolve().parent != self.bundle_root.resolve():
                raise HeightmapImportError("Terrain dependency escapes its bundle root.")
            try:
                names, size = read_bundle_cab_names(candidate)
                self.metadata_bytes += size
                if self.metadata_bytes > 64 * 1024 * 1024:
                    raise HeightmapImportError("Terrain dependency index exceeds its metadata byte budget.")
            except OSError:
                continue
            if cab_id in names:
                self.resolved[cab_id] = candidate
                return candidate
        if self.progress:
            self.progress(f"Resolving dependency {cab_id}")
        for candidate in sorted(self.bundle_root.glob("*.bundle")):
            if candidate in self.scanned_paths:
                continue
            self.scanned_paths.add(candidate)
            self.scanned_count += 1
            if self.scanned_count > 32768:
                raise HeightmapImportError("Terrain dependency index exceeds the 32768-file limit.")
            if candidate.resolve().parent != self.bundle_root.resolve():
                raise HeightmapImportError("Terrain dependency escapes its bundle root.")
            try:
                names, size = read_bundle_cab_names(candidate)
            except HeightmapImportError:
                continue
            self.metadata_bytes += size
            if self.metadata_bytes > 64 * 1024 * 1024:
                raise HeightmapImportError("Terrain dependency index exceeds its metadata byte budget.")
            for candidate_cab in names:
                self.resolved[candidate_cab] = candidate
            if cab_id in self.resolved:
                return self.resolved[cab_id]
        raise HeightmapImportError(f"Unable to resolve dependency bundle for {cab_id}.")

    def ensure_loaded(self, environment: Any, pointer: Any) -> None:
        cab_id = cab_id_for_pointer(pointer)
        if not cab_id or cab_id in self.loaded:
            return
        path = self.resolve(cab_id)
        environment.load_file(str(path), name=cab_id, is_dependency=True)
        self.loaded.add(cab_id)


def resolve_scene_dependencies(
    UnityPy: Any,
    scene_bundle: Path,
    bundle_root: Path,
    *,
    progress: Callable[[str], None] | None = None,
) -> list[Path]:
    scene_env = UnityPy.load(str(scene_bundle))
    dependencies: set[str] = set()
    for obj in scene_env.objects:
        if obj.type.name != "AssetBundle":
            continue
        asset_bundle = obj.read()
        for dependency in getattr(asset_bundle, "m_Dependencies", []) or []:
            cab_id = cab_id_from_value(str(dependency))
            if cab_id:
                dependencies.add(cab_id)
    if scene_env.objects:
        for external in getattr(scene_env.objects[0].assets_file, "externals", []) or []:
            cab_id = cab_id_from_value(str(getattr(external, "path", "")))
            if cab_id:
                dependencies.add(cab_id)

    resolved: dict[str, Path] = {}
    unresolved = set(dependencies)
    for cab_id in tuple(unresolved):
        for candidate in direct_cab_candidates(bundle_root, cab_id):
            if candidate.is_file():
                try:
                    asset_name = bundle_asset_file_name(UnityPy, candidate)
                except Exception:
                    continue
                if asset_name.startswith(cab_id):
                    resolved[cab_id] = candidate
                    unresolved.remove(cab_id)
                    break

    if unresolved and progress:
        progress(f"Indexing bundle CAB names for {len(unresolved)} unresolved dependencies")
    scanned = 0
    for candidate in sorted(bundle_root.glob("*.bundle")):
        if not unresolved:
            break
        if candidate == scene_bundle or candidate in resolved.values():
            continue
        scanned += 1
        try:
            asset_name = bundle_asset_file_name(UnityPy, candidate)
        except Exception:
            continue
        cab_id = cab_id_from_value(asset_name)
        if cab_id and cab_id in unresolved:
            resolved[cab_id] = candidate
            unresolved.remove(cab_id)
            if progress:
                progress(f"Resolved {cab_id} -> {candidate.name}")
    if unresolved:
        missing = ", ".join(sorted(unresolved)[:12])
        raise HeightmapImportError(f"Unable to resolve {len(unresolved)} scene dependencies. First missing: {missing}")
    if progress:
        progress(f"Resolved {len(resolved)} dependencies after scanning {scanned} bundles")
    return [scene_bundle, *[resolved[cab_id] for cab_id in sorted(resolved)]]


def load_bundle_set(UnityPy: Any, bundle_paths: Sequence[Path]) -> Any:
    environment = UnityPy.Environment()
    environment.load_files([str(path) for path in bundle_paths])
    return environment


def decode_mesh(MeshHandler: Any, mesh_pointer: Any, cache: dict[tuple[str, int, int], MeshPayload]) -> MeshPayload:
    key = pptr_key(mesh_pointer)
    cached = cache.get(key)
    if cached is not None:
        return cached
    mesh = mesh_pointer.read()
    handler = MeshHandler(mesh)
    handler.process()
    vertices = tuple(vector3(vertex) for vertex in getattr(handler, "m_Vertices", []) or [])
    triangles: list[tuple[int, int, int]] = []
    try:
        triangle_groups = handler.get_triangles()
    except Exception:
        triangle_groups = []
    for group in triangle_groups or []:
        for triangle in group:
            if len(triangle) != 3:
                continue
            a, b, c = (int(triangle[0]), int(triangle[1]), int(triangle[2]))
            if 0 <= a < len(vertices) and 0 <= b < len(vertices) and 0 <= c < len(vertices):
                triangles.append((a, b, c))
    payload = MeshPayload(str(getattr(mesh, "m_Name", "")) or "unnamed-mesh", vertices, tuple(triangles))
    cache[key] = payload
    return payload


def name_is_allowed(name: str, include_re: re.Pattern[str] | None, exclude_re: re.Pattern[str] | None) -> bool:
    if include_re is not None and include_re.search(name) is None:
        return False
    if exclude_re is not None and exclude_re.search(name) is not None:
        return False
    return True


def collect_mesh_instances(
    env: Any,
    MeshHandler: Any,
    settings: ImportSettings,
    *,
    dependency_resolver: CabDependencyResolver | None = None,
    progress: Callable[[str], None] | None = None,
    cancelled: Callable[[], bool] | None = None,
) -> tuple[list[MeshInstance], dict[str, int]]:
    if settings.max_meshes is not None and settings.max_meshes <= 0:
        raise HeightmapImportError("max-meshes must be positive when provided.")
    include_re = compile_name_regex(settings.include_name_regex, "include-name-regex")
    exclude_re = compile_name_regex(settings.exclude_name_regex, "exclude-name-regex")
    transform_cache = {}
    mesh_cache: dict[tuple[str, int, int], MeshPayload] = {}
    instances: list[MeshInstance] = []
    stats = {
        "candidate_components": 0,
        "accepted_components": 0,
        "inactive_components": 0,
        "filtered_components": 0,
        "unresolved_meshes": 0,
        "empty_meshes": 0,
    }

    component_types: set[str] = set()
    if settings.mesh_source in {"collider", "both"}:
        component_types.add("MeshCollider")
    if settings.mesh_source in {"render", "both"}:
        component_types.add("MeshFilter")

    for obj in tuple(env.objects):
        check_cancelled(cancelled)
        if obj.type.name not in component_types:
            continue
        if settings.max_meshes is not None and len(instances) >= settings.max_meshes:
            break
        stats["candidate_components"] += 1
        try:
            component = obj.read()
            enabled = bool(getattr(component, "m_Enabled", True))
            game_object = component.m_GameObject.read()
            if settings.terrain_layer_mask is not None and not (
                settings.terrain_layer_mask & (1 << int(game_object.m_Layer))
            ):
                stats["filtered_components"] += 1
                continue
            if not enabled or (not settings.include_inactive and not bool(getattr(game_object, "m_IsActive", True))):
                stats["inactive_components"] += 1
                continue
            mesh_pointer = getattr(component, "m_Mesh", None)
            if mesh_pointer is None or pptr_path_id(mesh_pointer) == 0:
                stats["unresolved_meshes"] += 1
                continue
            if dependency_resolver is not None:
                dependency_resolver.ensure_loaded(env, mesh_pointer)
            mesh = decode_mesh(MeshHandler, mesh_pointer, mesh_cache)
            combined_name = f"{getattr(game_object, 'm_Name', '')} {mesh.name}"
            if not name_is_allowed(combined_name, include_re, exclude_re):
                stats["filtered_components"] += 1
                continue
            if not mesh.vertices:
                stats["empty_meshes"] += 1
                continue
            matrix, hierarchy_active = transform_matrix(game_object_transform(game_object), transform_cache)
            if not hierarchy_active and not settings.include_inactive:
                stats["inactive_components"] += 1
                continue
            vertices = tuple(apply_matrix(matrix, vertex) for vertex in mesh.vertices)
            instances.append(
                MeshInstance(
                    game_object_name=str(getattr(game_object, "m_Name", "")) or "unnamed-object",
                    mesh_name=mesh.name,
                    vertices=vertices,
                    triangles=mesh.triangles,
                )
            )
            stats["accepted_components"] += 1
            if progress and len(instances) % 250 == 0:
                progress(f"Accepted {len(instances)} mesh instances")
        except HeightmapImportError:
            raise
        except Exception:
            stats["unresolved_meshes"] += 1
            continue
    return instances, stats


def bounds_for_instances(instances: Sequence[MeshInstance]) -> Bounds:
    if not instances:
        raise HeightmapImportError("No mesh geometry was accepted for heightmap extraction.")
    min_x = min_y = min_z = math.inf
    max_x = max_y = max_z = -math.inf
    for instance in instances:
        for x, y, z in instance.vertices:
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)
            min_z = min(min_z, z)
            max_z = max(max_z, z)
    if not all(math.isfinite(value) for value in (min_x, max_x, min_y, max_y, min_z, max_z)):
        raise HeightmapImportError("Mesh geometry produced non-finite bounds.")
    if max_x <= min_x or max_z <= min_z:
        raise HeightmapImportError("Mesh geometry does not span a usable X/Z area.")
    if max_y <= min_y:
        max_y = min_y + 1.0
    return Bounds(min_x, max_x, min_y, max_y, min_z, max_z)


def sample_coordinate(bounds: Bounds, width: int, height: int, point: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = point
    column = (x - bounds.min_x) / (bounds.max_x - bounds.min_x) * (width - 1)
    row = (bounds.max_z - z) / (bounds.max_z - bounds.min_z) * (height - 1)
    return column, row, y


def write_height_cell(heights: array, mask: bytearray, width: int, row: int, column: int, value: float) -> None:
    index = row * width + column
    if not mask[index] or value > heights[index]:
        heights[index] = value
        mask[index] = 1


def rasterize_point(
    heights: array,
    mask: bytearray,
    width: int,
    height: int,
    point: tuple[float, float, float],
) -> None:
    column = max(0, min(width - 1, int(round(point[0]))))
    row = max(0, min(height - 1, int(round(point[1]))))
    write_height_cell(heights, mask, width, row, column, point[2])


def rasterize_triangle(
    heights: array,
    mask: bytearray,
    width: int,
    height: int,
    a: tuple[float, float, float],
    b: tuple[float, float, float],
    c: tuple[float, float, float],
) -> bool:
    ax, ay, ah = a
    bx, by, bh = b
    cx, cy, ch = c
    min_col = max(0, math.floor(min(ax, bx, cx)))
    max_col = min(width - 1, math.ceil(max(ax, bx, cx)))
    min_row = max(0, math.floor(min(ay, by, cy)))
    max_row = min(height - 1, math.ceil(max(ay, by, cy)))
    denominator = ((by - cy) * (ax - cx)) + ((cx - bx) * (ay - cy))
    if abs(denominator) < 1e-9:
        rasterize_point(heights, mask, width, height, a)
        rasterize_point(heights, mask, width, height, b)
        rasterize_point(heights, mask, width, height, c)
        return False
    for row in range(min_row, max_row + 1):
        py = float(row)
        for column in range(min_col, max_col + 1):
            px = float(column)
            weight_a = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / denominator
            weight_b = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / denominator
            weight_c = 1.0 - weight_a - weight_b
            if weight_a >= -1e-6 and weight_b >= -1e-6 and weight_c >= -1e-6:
                value = (weight_a * ah) + (weight_b * bh) + (weight_c * ch)
                write_height_cell(heights, mask, width, row, column, value)
    return True


def rasterize_instances(
    instances: Sequence[MeshInstance],
    *,
    resolution: int,
    max_triangles: int,
    progress: Callable[[str], None] | None = None,
    bounds: Bounds | None = None,
    cancelled: Callable[[], bool] | None = None,
) -> RasterResult:
    if resolution <= 1 or resolution > MAX_GRID_DIMENSION:
        raise HeightmapImportError(f"resolution must be in the range 2..{MAX_GRID_DIMENSION}.")
    if resolution * resolution > MAX_TOTAL_SAMPLES:
        raise HeightmapImportError("resolution exceeds the terrain heightmap total-sample bound.")
    if max_triangles <= 0:
        raise HeightmapImportError("max-triangles must be positive.")
    bounds = bounds or bounds_for_instances(instances)
    width = resolution
    height = resolution
    heights = array("f", [-math.inf]) * (width * height)
    mask = bytearray(width * height)
    rasterized_triangles = 0
    checked_triangles = 0
    for instance_index, instance in enumerate(instances, start=1):
        check_cancelled(cancelled)
        projected = [sample_coordinate(bounds, width, height, vertex) for vertex in instance.vertices]
        for vertex in projected:
            if 0 <= vertex[0] <= width - 1 and 0 <= vertex[1] <= height - 1:
                rasterize_point(heights, mask, width, height, vertex)
        for ia, ib, ic in instance.triangles:
            checked_triangles += 1
            if checked_triangles % 256 == 0:
                check_cancelled(cancelled)
            if checked_triangles > max_triangles:
                raise HeightmapImportError(f"max-triangles limit reached at {max_triangles}.")
            if rasterize_triangle(heights, mask, width, height, projected[ia], projected[ib], projected[ic]):
                rasterized_triangles += 1
        if progress and instance_index % 100 == 0:
            progress(f"Rasterized {instance_index}/{len(instances)} mesh instances")
    source_coverage = bytes(mask)
    check_cancelled(cancelled)
    initial_filled = sum(mask)
    if initial_filled != width * height:
        raise HeightmapImportError(
            f"Ground geometry leaves {width * height - initial_filled} samples uncovered; "
            "missing heights will not be estimated.")
    return RasterResult(width, height, bounds, heights, initial_filled, rasterized_triangles, source_coverage)


def normalize_u16(heights: array) -> tuple[array, float, float]:
    min_height = min(heights)
    max_height = max(heights)
    if not math.isfinite(min_height) or not math.isfinite(max_height):
        raise HeightmapImportError("Height raster contains non-finite samples.")
    if max_height <= min_height:
        max_height = min_height + 1.0
    scale = 65535.0 / (max_height - min_height)
    samples = array("H")
    for height in heights:
        value = int(round((height - min_height) * scale))
        samples.append(max(0, min(65535, value)))
    return samples, float(min_height), float(max_height)


def tile_relative_path(origin_x: int, origin_y: int) -> str:
    return f"Tiles/{origin_y:08d}-{origin_x:08d}.terrain.u16le"


def write_tile(samples: array, width: int, tile_path: Path, origin_x: int, origin_y: int, tile_width: int, tile_height: int) -> str:
    digest = hashlib.sha256()
    tile_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = tile_path.with_suffix(tile_path.suffix + ".tmp")
    with temporary.open("wb") as handle:
        for row in range(origin_y, origin_y + tile_height):
            row_bytes = bytearray()
            base = row * width
            for column in range(origin_x, origin_x + tile_width):
                value = int(samples[base + column])
                row_bytes.append(value & 0xFF)
                row_bytes.append((value >> 8) & 0xFF)
            digest.update(row_bytes)
            handle.write(row_bytes)
    temporary.replace(tile_path)
    return "sha256:" + digest.hexdigest()


def safe_document_json(document: Mapping[str, Any]) -> str:
    return json.dumps(document, ensure_ascii=True, separators=(",", ":"))


def profile_fingerprint(settings: ImportSettings, install: GameInstall) -> str:
    value = {
        "schema": "foa.heightmap-import-profile-binding",
        "schema_version": 1,
        "game_root_token": "source-root.user-selected",
        "install_layout": {
            "game_exe": (install.root / GAME_EXE_NAME).is_file(),
            "catalog": install.catalog_path.name,
            "bundle_root": "Fall of Avalon_Data/StreamingAssets/aa/StandaloneWindows64",
        },
        "profile_id": settings.profile_id,
        "game_version": settings.game_version,
        "branch": settings.branch,
        "runtime_target": settings.runtime_target,
    }
    return sha256_bytes(canonical_json_bytes(value))


def configuration_fingerprint(settings: ImportSettings, campaign: CampaignMap, dependency_count: int) -> str:
    value = {
        "schema": "foa.heightmap-unity-mesh-import-configuration",
        "schema_version": 1,
        "campaign_map": campaign.key,
        "scene_bundle": campaign.scene_bundle_name,
        "resolution": settings.resolution,
        "mesh_source": settings.mesh_source,
        "terrain_layer_mask": settings.terrain_layer_mask,
        "include_inactive": settings.include_inactive,
        "include_name_regex": settings.include_name_regex or "",
        "exclude_name_regex": settings.exclude_name_regex or "",
        "max_meshes": settings.max_meshes or 0,
        "max_triangles": settings.max_triangles,
        "dependency_bundle_count": dependency_count,
    }
    return sha256_bytes(canonical_json_bytes(value))


def operation_id(settings: ImportSettings, campaign: CampaignMap) -> str:
    if settings.operation_id:
        return require_id(settings.operation_id, "operation-id")
    stamp = settings.created_at_utc.lower().replace("-", "").replace(":", "").replace("z", "z")
    return f"terrain-import.foa-heightmap.{campaign.key}.{stamp}"


def build_document(
    *,
    campaign: CampaignMap,
    settings: ImportSettings,
    install: GameInstall,
    scene_sha: str,
    scene_size: int,
    config_sha: str,
    samples: array,
    raster: RasterResult,
    min_height: float,
    max_height: float,
    op_id: str,
    published_root: Path,
    cancelled: Callable[[], bool] | None = None,
) -> tuple[dict[str, Any], list[Path]]:
    tiles: list[dict[str, Any]] = []
    tile_paths: list[Path] = []
    tile_root = published_root / "Tiles"
    for origin_y in range(0, raster.height, TILE_SIZE):
        tile_height = min(TILE_SIZE, raster.height - origin_y)
        for origin_x in range(0, raster.width, TILE_SIZE):
            check_cancelled(cancelled)
            tile_width = min(TILE_SIZE, raster.width - origin_x)
            relative_path = tile_relative_path(origin_x, origin_y)
            tile_path = published_root / relative_path
            tile_hash = write_tile(samples, raster.width, tile_path, origin_x, origin_y, tile_width, tile_height)
            tile_paths.append(tile_path)
            tiles.append(
                {
                    "tile_id": f"terrain-tile.{op_id}.{origin_y}-{origin_x}",
                    "origin_x": origin_x,
                    "origin_y": origin_y,
                    "width": tile_width,
                    "height": tile_height,
                    "relative_path": relative_path,
                    "byte_size": tile_width * tile_height * 2,
                    "sha256": tile_hash,
                }
            )
    spacing_x = (raster.bounds.max_x - raster.bounds.min_x) / max(1, raster.width - 1)
    spacing_y = (raster.bounds.max_z - raster.bounds.min_z) / max(1, raster.height - 1)
    document = {
        "schema": "foa.terrain-heightmap",
        "schema_version": 1,
        "document_id": f"terrain-document.{campaign.map_id}",
        "map_identity": {
            "map_id": campaign.map_id,
            "display_name": campaign.display_name,
            "public_aliases": [campaign.public_alias],
            "native_identity_evidence_id": "",
        },
        "profile_binding": {
            "profile_id": settings.profile_id,
            "game_version": settings.game_version,
            "branch": settings.branch,
            "runtime_target": settings.runtime_target,
            "profile_fingerprint": profile_fingerprint(settings, install),
        },
        "source_binding": {
            "source_kind": "user-exported-raw-u16-le",
            "source_container_sha256": scene_sha,
            "source_object_identifier": campaign.scene_bundle_name,
            "source_subresource_sha256": "",
            "exporter_id": IMPORTER_ID,
            "exporter_version": TOOL_VERSION,
            "configuration_fingerprint": config_sha,
            "redacted_root_token": "source-root.user-selected",
            "relative_locator": f"SourceObservations/Terrain/{op_id}/source-observation.json",
        },
        "grid": {
            "width": raster.width,
            "height": raster.height,
            "sample_spacing_x_metres": spacing_x,
            "sample_spacing_y_metres": spacing_y,
        },
        "sample_encoding": {
            "format": "u16",
            "byte_order": "little-endian",
            "storage_order": "row-major",
            "bits_per_sample": 16,
            "unsigned_integer": True,
        },
        "vertical_mapping": {
            "min_height_metres": min_height,
            "max_height_metres": max_height,
        },
        "coordinate_space": {
            "handedness": "left-handed",
            "up_axis": "y",
            "forward_axis": "z",
            "row_zero_orientation": "north",
            "sample_position": "cell-center",
            "source_to_canonical_transform": [
                1.0,
                0.0,
                0.0,
                0.0,
                0.0,
                1.0,
                0.0,
                0.0,
                0.0,
                0.0,
                1.0,
                0.0,
                0.0,
                0.0,
                0.0,
                1.0,
            ],
        },
        "tiles": tiles,
        "provenance": {
            "created_at_utc": settings.created_at_utc,
            "importer_id": IMPORTER_ID,
            "importer_version": TOOL_VERSION,
            "source_evidence_id": f"evidence.terrain.{op_id}",
            "limitations": "Local Unity MeshCollider extraction from user-selected FoA install; no native TerrainData, runtime, deployment, or identity authority claim.",
        },
        "legal_state": "user-exported-local-only",
        "revision": {
            "revision_id": f"terrain-revision.{op_id}",
            "parent_document_fingerprint": "",
            "operation_fingerprint": sha256_text(scene_sha + "\n" + config_sha + "\n" + op_id),
            "created_at_utc": settings.created_at_utc,
        },
        "local_payload_state": "workspace-local-derived",
        "authority": {
            "runtime_use_allowed": False,
            "deployment_allowed": False,
            "publication_allowed": False,
            "packaging_allowed": False,
            "game_write_allowed": False,
            "evidence_promotion_allowed": False,
        },
    }
    _ = scene_size
    _ = tile_root
    return document, tile_paths


def write_json_atomic(path: Path, value: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(pretty_json_bytes(value))
    temporary.replace(path)


def check_cancelled(cancelled: Callable[[], bool] | None) -> None:
    if cancelled is not None and cancelled():
        raise HeightmapImportError("Heightmap import cancelled; no revision was published.")


def require_direct_path(path: Path) -> Path:
    """Reject links and Windows reparse points before resolving or creating output."""
    path = Path(os.path.abspath(path.expanduser()))
    for component in (*reversed(path.parents), path):
        try:
            metadata = component.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(metadata.st_mode) or (
            getattr(metadata, "st_file_attributes", 0)
            & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
        ):
            raise HeightmapImportError("Heightmap output paths must not contain links or reparse points.")
    return path


def contained_directory(workspace: Path, relative: Path) -> Path:
    path = require_direct_path(workspace / relative)
    if path == workspace or not path.is_relative_to(workspace):
        raise HeightmapImportError("Heightmap output must remain inside its workspace.")
    path.mkdir(parents=True, exist_ok=True)
    require_direct_path(path)
    return path


def remove_owned_staging(workspace: Path, path: Path) -> None:
    """Only remove this operation's checked staging or source-observation directory."""
    path = require_direct_path(path)
    if not any(path.is_relative_to(workspace / root / "Terrain")
               and path != workspace / root / "Terrain"
               for root in ("Staging", "SourceObservations")):
        raise HeightmapImportError("Refusing to clean output outside the terrain staging boundary.")
    if path.exists():
        for parent, directories, files in os.walk(path, followlinks=False):
            for name in (*directories, *files):
                require_direct_path(Path(parent) / name)
        shutil.rmtree(path)


def validate_output_raster(raster: RasterResult, samples: array, min_height: float, max_height: float) -> None:
    if (type(raster.width) is not int or type(raster.height) is not int
            or not 2 <= raster.width <= MAX_GRID_DIMENSION
            or not 2 <= raster.height <= MAX_GRID_DIMENSION
            or raster.width * raster.height > MAX_TOTAL_SAMPLES):
        raise HeightmapImportError("Heightmap output dimensions exceed the bounded sample grid.")
    if samples.typecode != "H" or samples.itemsize != 2 or len(samples) != raster.width * raster.height:
        raise HeightmapImportError("Heightmap output sample count and U16 encoding must match the grid.")
    bounds = raster.bounds
    if (not all(math.isfinite(value) for value in (
            bounds.min_x, bounds.max_x, bounds.min_y, bounds.max_y, bounds.min_z, bounds.max_z,
            min_height, max_height))
            or bounds.max_x <= bounds.min_x or bounds.max_z <= bounds.min_z
            or max_height <= min_height):
        raise HeightmapImportError("Heightmap output requires finite bounds and a positive height range.")


def write_import_outputs(
    *,
    settings: ImportSettings,
    install: GameInstall,
    campaign: CampaignMap,
    scene_bundle: Path,
    dependency_count: int,
    component_stats: Mapping[str, int],
    raster: RasterResult,
    samples: array,
    min_height: float,
    max_height: float,
    op_id: str,
    cancelled: Callable[[], bool] | None = None,
) -> dict[str, Any]:
    require_id(op_id, "operation-id")
    require_id(campaign.map_id, "map-id")
    require_id(settings.profile_id, "profile-id")
    require_utc(settings.created_at_utc)
    validate_output_raster(raster, samples, min_height, max_height)
    check_cancelled(cancelled)
    workspace = require_direct_path(settings.workspace_root)
    install_root = install.root.resolve(strict=True)
    if workspace == install_root or workspace.is_relative_to(install_root):
        raise HeightmapImportError("workspace-root must not be inside the Tainted Grail FoA install.")
    workspace.mkdir(parents=True, exist_ok=True)
    require_direct_path(workspace)
    scene_sha, scene_size = sha256_file(scene_bundle)
    require_sha(scene_sha, "scene bundle fingerprint")
    config_sha = configuration_fingerprint(settings, campaign, dependency_count)
    revision_parent = contained_directory(workspace, Path("Derived/Terrain") / campaign.map_id / "Revisions")
    revision_root = revision_parent / f"terrain-revision.{op_id}"
    observation_parent = contained_directory(workspace, Path("SourceObservations/Terrain"))
    observation_root = observation_parent / op_id
    staging_parent = contained_directory(workspace, Path("Staging/Terrain"))
    staging_root = staging_parent / op_id
    for path in (revision_root, observation_root, staging_root):
        require_direct_path(path)
        if path.exists():
            raise HeightmapImportError("Terrain operation already exists; use a new operation-id to preserve it.")
    # Exclusive ownership also prevents two concurrent imports of the same operation.
    staging_root.mkdir(exist_ok=False)
    observation_owned = False
    published = False
    try:
        candidate_root = staging_root / "candidate"
        document, _ = build_document(
            campaign=campaign, settings=settings, install=install,
            scene_sha=scene_sha, scene_size=scene_size, config_sha=config_sha,
            samples=samples, raster=raster, min_height=min_height, max_height=max_height,
            op_id=op_id, published_root=candidate_root, cancelled=cancelled,
        )
        check_cancelled(cancelled)
        document_hash = sha256_text(safe_document_json(document))[7:23]
        write_json_atomic(candidate_root / "terrain.tgheightmap.json", document)
        # Validate the complete payload inventory while it is still unpublished.
        for tile in document["tiles"]:
            check_cancelled(cancelled)
            tile_path = require_direct_path(candidate_root / tile["relative_path"])
            fingerprint, byte_size = sha256_file(tile_path)
            if fingerprint != tile["sha256"] or byte_size != tile["byte_size"]:
                raise HeightmapImportError("Staged terrain tile validation failed; no revision was published.")
        staged_revision = staging_root / "revision"
        staged_revision.mkdir()
        candidate_root.rename(staged_revision / document_hash)
        published_root = revision_root / document_hash
        manifest_path = published_root / "terrain.tgheightmap.json"
        tile_paths = [published_root / tile["relative_path"] for tile in document["tiles"]]
        observation = {
            "schema": "foa.terrain-source-observation",
            "schema_version": 1,
            "source_kind": "user-exported-raw-u16-le",
            "source_sha256": scene_sha,
            "configuration_sha256": config_sha,
            "source_byte_size": scene_size,
            "source_object_identifier": campaign.scene_bundle_name,
            "captured_at_utc": settings.created_at_utc,
            "game_root_token": "source-root.user-selected",
            "scene_bundle": campaign.scene_bundle_name,
            "dependency_bundle_count": dependency_count,
            "mesh_source": settings.mesh_source,
            "component_stats": dict(component_stats),
            "grid": {
                "width": raster.width,
                "height": raster.height,
                "initial_filled_samples": raster.filled_samples,
                "rasterized_triangles": raster.rasterized_triangles,
            },
            "bounds_metres": {
                "min_x": raster.bounds.min_x,
                "max_x": raster.bounds.max_x,
                "min_y": raster.bounds.min_y,
                "max_y": raster.bounds.max_y,
                "min_z": raster.bounds.min_z,
                "max_z": raster.bounds.max_z,
            },
            "limitations": "Mesh-derived local height envelope; not a native TerrainData export and not runtime validation.",
        }
        check_cancelled(cancelled)
        require_direct_path(observation_root)
        observation_root.mkdir(exist_ok=False)
        observation_owned = True
        observation_path = observation_root / "source-observation.json"
        write_json_atomic(observation_path, observation)
        check_cancelled(cancelled)
        require_direct_path(revision_root)
        require_direct_path(staged_revision)
        if revision_root.exists():
            raise HeightmapImportError("Published terrain revision already exists; it will not be replaced.")
        # This rename is the commit point: readers see all tiles and the manifest together.
        staged_revision.rename(revision_root)
        published = True
        return {
            "schema": "foa.heightmap-import-result",
            "schema_version": 1,
            "tool_id": IMPORTER_ID,
            "tool_version": TOOL_VERSION,
            "map": campaign.key,
            "operation_id": op_id,
            "manifest_path": str(manifest_path),
            "source_observation_path": str(observation_path),
            "tile_count": len(tile_paths),
            "tile_paths": [str(path) for path in tile_paths],
            "component_stats": dict(component_stats),
            "dependency_bundle_count": dependency_count,
            "rasterized_triangles": raster.rasterized_triangles,
            "initial_filled_samples": raster.filled_samples,
        }
    finally:
        if published:
            try:
                remove_owned_staging(workspace, staging_root)
            except (OSError, HeightmapImportError):
                # Cleanup after the commit point must not report the completed import as failed.
                eprint("Heightmap revision published; its staging directory needs cleanup.")
        else:
            try:
                remove_owned_staging(workspace, staging_root)
            finally:
                if observation_owned:
                    remove_owned_staging(workspace, observation_root)


def import_campaign_map(settings: ImportSettings, *, verbose: bool = False) -> dict[str, Any]:
    # Preserve the legacy command boundary with an actionable failure. Its scene
    # selection and source labels did not qualify campaign terrain provenance.
    raise HeightmapImportError(
        "Legacy campaign import is disabled. Use FOA-SDK Home > Heightmap Importer > "
        "Edit Vanilla Map with the configured campaign extraction provider."
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", type=Path, help="Tainted Grail FoA install root. Defaults to FOA_GAME_ROOT and common Steam paths.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list-maps", help="List campaign map scene bundles found in the FoA install.")

    import_parser = subparsers.add_parser("import", help="Disabled legacy route; use the Editor campaign importer.")
    import_parser.add_argument("--workspace-root", type=Path, required=True, help="Workspace directory that will receive Derived/Terrain output.")
    import_parser.add_argument("--map", dest="map_key", choices=(*CAMPAIGN_MAPS.keys(), "all"), required=True)
    import_parser.add_argument("--resolution", type=int, default=2048, help="Square output resolution. Default: 2048.")
    import_parser.add_argument("--mesh-source", choices=("collider", "render", "both"), default="collider")
    import_parser.add_argument("--include-inactive", action="store_true")
    import_parser.add_argument("--include-name-regex")
    import_parser.add_argument("--exclude-name-regex")
    import_parser.add_argument("--max-meshes", type=int)
    import_parser.add_argument("--max-triangles", type=int, default=2_000_000)
    import_parser.add_argument("--created-at", default=strict_utc_now())
    import_parser.add_argument("--operation-id")
    import_parser.add_argument("--profile-id", default="profile.foa.local")
    import_parser.add_argument("--game-version", default="local-install")
    import_parser.add_argument("--branch", default="local")
    import_parser.add_argument("--runtime-target", choices=("Mono", "IL2CPP"), default="Mono")
    import_parser.add_argument("--verbose", action="store_true")
    return parser


def settings_from_args(args: argparse.Namespace) -> ImportSettings:
    return require_import_limits(
        ImportSettings(
            game_root=args.game_root,
            workspace_root=args.workspace_root,
            map_key=args.map_key,
            resolution=args.resolution,
            mesh_source=args.mesh_source,
            include_inactive=args.include_inactive,
            include_name_regex=require_name_regex(args.include_name_regex, "include-name-regex"),
            exclude_name_regex=require_name_regex(args.exclude_name_regex, "exclude-name-regex"),
            max_meshes=args.max_meshes,
            max_triangles=args.max_triangles,
            created_at_utc=require_utc(args.created_at),
            operation_id=args.operation_id,
            profile_id=require_id(args.profile_id, "profile-id"),
            game_version=args.game_version,
            branch=args.branch,
            runtime_target=args.runtime_target,
        )
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "list-maps":
            install = resolve_game_install(args.game_root)
            print(json.dumps(list_campaign_maps(install), ensure_ascii=True, indent=2))
            return 0
        if args.command == "import":
            settings = settings_from_args(args)
            if settings.map_key == "all":
                results = []
                for map_key in CAMPAIGN_MAPS:
                    map_operation_id = (
                        f"{settings.operation_id}.{map_key}"
                        if settings.operation_id
                        else None
                    )
                    results.append(
                        import_campaign_map(
                            replace(settings, map_key=map_key, operation_id=map_operation_id),
                            verbose=args.verbose,
                        )
                    )
                result = {
                    "schema": "foa.heightmap-import-result-set",
                    "schema_version": 1,
                    "tool_id": IMPORTER_ID,
                    "tool_version": TOOL_VERSION,
                    "results": results,
                }
            else:
                result = import_campaign_map(settings, verbose=args.verbose)
            print(json.dumps(result, ensure_ascii=True, indent=2))
            return 0
    except (HeightmapImportError, OSError) as exc:
        print(f"FoA heightmap import failed: {exc}", file=sys.stderr)
        return 1
    parser.error("unknown command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
