#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Campaign import is disabled pending verified source preservation and round trip.

The retained collider helpers are research utilities, not an editable game map
importer. Missing terrain samples are rejected, never estimated.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from array import array
from pathlib import Path

# -I keeps user/site and working-directory modules out of the worker. The sibling
# helper is product code, resolved only beside this installed/source entry point.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import foa_heightmap_importer as h

SCHEMA = "foa.campaign-heightmap-export"
VERSION = "1.0.0"
MAX_SOURCE_BYTES = 768 * 1024 * 1024
MAX_FILE_BYTES = 256 * 1024 * 1024


def selection_from_environment(environment):
    selections = []
    for obj in tuple(environment.objects):
        if obj.type.name != "MonoBehaviour":
            continue
        tree = obj.read_typetree()
        if "RaycastLayerMask" not in tree or "RaycastTerrainBounds" not in tree:
            continue
        bounds = tree["RaycastTerrainBounds"]
        center, extent = bounds["m_Center"], bounds["m_Extent"]
        values = [float(center[k]) for k in "xyz"] + [float(extent[k]) for k in "xyz"]
        if not all(math.isfinite(v) for v in values) or not all(v > 0 for v in values[3:]):
            raise h.HeightmapImportError("Campaign terrain bounds are invalid.")
        mask = tree["RaycastLayerMask"]["m_Bits"]
        if type(mask) is not int or mask != 8192:
            raise h.HeightmapImportError("Campaign terrain layer selection is unsupported.")
        selections.append({
            "layer_mask": mask, "center": dict(center), "extent": dict(extent),
            "source_component_id": int(obj.path_id),
        })
    if len(selections) != 1:
        raise h.HeightmapImportError("Exactly one campaign terrain selection is required.")
    return selections[0]


def source_bounds(selection):
    c, e = selection["center"], selection["extent"]
    return h.Bounds(c["x"]-e["x"], c["x"]+e["x"], c["y"]-e["y"],
                    c["y"]+e["y"], c["z"]-e["z"], c["z"]+e["z"])


class SourceInventory:
    def __init__(self, root, cancelled):
        self.root = h.require_direct_path(root)
        self.cancelled = cancelled
        self.rows = {}
        self.total = 0

    def add(self, path):
        h.check_cancelled(self.cancelled)
        path = h.require_direct_path(path)
        if not path.is_relative_to(self.root) or not path.is_file():
            raise h.HeightmapImportError("Campaign input must remain inside its registered install.")
        if path in self.rows:
            return
        size = path.stat().st_size
        self.total += size
        if size > MAX_FILE_BYTES or self.total > MAX_SOURCE_BYTES or len(self.rows) >= 256:
            raise h.HeightmapImportError("Campaign source inventory exceeds its resource budget.")
        digest, measured = h.sha256_file(path, self.cancelled, MAX_FILE_BYTES)
        if measured != size:
            raise h.HeightmapImportError("Campaign source changed while fingerprinting.")
        self.rows[path] = {"relative_path": path.relative_to(self.root).as_posix(),
                           "sha256": digest, "byte_size": size}

    def verify(self):
        for path, row in self.rows.items():
            h.check_cancelled(self.cancelled)
            h.require_direct_path(path)
            if h.sha256_file(path, self.cancelled, MAX_FILE_BYTES) != (row["sha256"], row["byte_size"]):
                raise h.HeightmapImportError("Campaign source changed during extraction.")


class CampaignResolver(h.CabDependencyResolver):
    def __init__(self, unity, bundle_root, inventory):
        super().__init__(unity, bundle_root, progress=h.eprint)
        self.inventory = inventory

    def ensure_loaded(self, environment, pointer):
        cab = h.cab_id_for_pointer(pointer)
        if cab and cab not in self.loaded:
            self.inventory.add(self.resolve(cab))
        super().ensure_loaded(environment, pointer)


def validate_request(request):
    fields = {"schema", "schema_version", "workspace_root", "game_root", "map",
              "resolution", "operation_id", "created_at_utc", "unity_version", "profile_binding"}
    if set(request) != fields or request["schema"] != SCHEMA + ".request" or request["schema_version"] != 1:
        raise h.HeightmapImportError("Unsupported campaign export request.")
    if request["map"] not in h.CAMPAIGN_MAPS:
        raise h.HeightmapImportError("Unknown campaign map.")
    if type(request["resolution"]) is not int or not 33 <= request["resolution"] <= 4097:
        raise h.HeightmapImportError("Campaign export resolution must be 33..4097.")
    h.require_id(request["operation_id"], "operation-id")
    h.require_utc(request["created_at_utc"])
    if request["unity_version"] != h.UNITY_FALLBACK_VERSION:
        raise h.HeightmapImportError("This campaign provider requires the qualified Unity profile.")
    profile = request["profile_binding"]
    if not isinstance(profile, dict) or set(profile) != {"profile_id", "game_version", "branch", "runtime_target", "profile_fingerprint"}:
        raise h.HeightmapImportError("Exact profile binding is required.")
    h.require_id(profile["profile_id"], "profile-id")
    h.require_sha(profile["profile_fingerprint"], "profile fingerprint")
    if not all(isinstance(value, str) and value for value in profile.values()):
        raise h.HeightmapImportError("Incomplete profile binding.")
    workspace = h.require_direct_path(Path(request["workspace_root"]))
    game = h.require_direct_path(Path(request["game_root"]))
    if workspace == game or workspace.is_relative_to(game) or game.is_relative_to(workspace):
        raise h.HeightmapImportError("Campaign output and game installation must be separate.")
    if any((parent / ".git").exists() for parent in (workspace, *workspace.parents)):
        raise h.HeightmapImportError("Campaign exports must remain outside source checkouts.")
    return workspace, game


def publish_export(request, raster, inventory, selection, stats, candidate, cancelled):
    workspace, _ = validate_request(request)
    h.check_cancelled(cancelled)
    if raster.width != request["resolution"] or raster.height != request["resolution"]:
        raise h.HeightmapImportError("Campaign grid does not match its request.")
    total = raster.width * raster.height
    if (raster.filled_samples != total or raster.source_coverage is None
            or len(raster.source_coverage) != total or any(value != 1 for value in raster.source_coverage)):
        raise h.HeightmapImportError("Campaign ground has uncovered samples; missing heights will not be estimated.")
    samples, minimum, maximum = h.normalize_u16(raster.heights)
    h.validate_output_raster(raster, samples, minimum, maximum)
    candidate = h.require_direct_path(candidate)
    if not candidate.is_relative_to(workspace):
        raise h.HeightmapImportError("Campaign staging must remain inside its workspace.")
    candidate.mkdir(exist_ok=False)
    raw = array("H", samples)
    if sys.byteorder != "little":
        raw.byteswap()
    raw_path = candidate / "heightmap.raw"
    raw_path.write_bytes(raw.tobytes())
    raw_hash, raw_size = h.sha256_file(raw_path)
    if raster.source_coverage is None or len(raster.source_coverage) != raster.width * raster.height:
        raise h.HeightmapImportError("Campaign coverage mask is missing.")
    (candidate / "coverage.raw").write_bytes(raster.source_coverage)
    bounds = raster.bounds
    receipt = {
        "schema": SCHEMA + ".receipt", "schema_version": 1, "provider_version": VERSION,
        "unitypy_version": "1.24.2", "unity_version": request["unity_version"],
        "profile_binding": request["profile_binding"], "map": request["map"],
        "operation_id": request["operation_id"], "created_at_utc": request["created_at_utc"],
        "source_root_token": "source-root.registered-install",
        "source_inventory": sorted(inventory.rows.values(), key=lambda x: x["relative_path"]),
        "selection": selection, "component_stats": stats,
        "surface": "mesh-collider-upper-envelope", "missing_sample_policy": "reject-uncovered-samples",
        "coverage_fraction": raster.filled_samples / (raster.width * raster.height),
        "covered_samples": raster.filled_samples, "total_samples": raster.width * raster.height,
        "rasterized_triangles": raster.rasterized_triangles,
        "coverage_mask": {"relative_path": "coverage.raw", "format": "u8-row-major-covered-is-1",
            "sha256": h.sha256_file(candidate / "coverage.raw")[0]},
        "bounds_metres": vars(bounds), "bounds_basis": "transformed-ground-collider-union", "raw_sha256": raw_hash, "raw_bytes": raw_size,
        "limitations": "Research-only single-valued ground surface; caves and overhangs are not reconstructed. No native TerrainData, runtime, deployment or publication claim.",
        "authority": {"game_write_allowed": False, "publication_allowed": False,
                      "deployment_allowed": False, "runtime_use_allowed": False},
    }
    # The existing RAW contract hashes the entire sidecar, including this receipt.
    metadata = {
        "schema": "foa.raw-u16-heightmap-sidecar", "schema_version": 1,
        "width": raster.width, "height": raster.height, "byte_order": "little-endian",
        "sample_spacing_x_metres": (bounds.max_x-bounds.min_x)/(raster.width-1),
        "sample_spacing_y_metres": (bounds.max_z-bounds.min_z)/(raster.height-1),
        "min_height_metres": minimum, "max_height_metres": maximum,
        "handedness": "left-handed", "up_axis": "y", "forward_axis": "z",
        "row_zero_orientation": "north", "sample_position": "grid-vertex",
        "source_to_canonical_transform": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],
        "source_extraction": receipt,
    }
    (candidate / "heightmap.raw.json").write_bytes(h.pretty_json_bytes(metadata))
    (candidate / "receipt.json").write_bytes(h.pretty_json_bytes(receipt))
    from PIL import Image
    Image.frombytes("L", (raster.width, raster.height), bytes(value >> 8 for value in samples)).resize((256,256)).save(candidate / "preview.png")
    h.check_cancelled(cancelled)
    inventory.verify()
    parent = h.contained_directory(workspace, Path("SourceExports/Terrain"))
    target = h.require_direct_path(parent / request["operation_id"])
    if target.exists():
        raise h.HeightmapImportError("Campaign export operation already exists; it will not be replaced.")
    candidate.rename(target)
    return {"schema": SCHEMA + ".result", "schema_version": 1, "status": "PASSED",
            "operation_id": request["operation_id"], "map": request["map"],
            "export_relative_path": target.relative_to(workspace).as_posix(),
            "raw_sha256": raw_hash, "raw_bytes": raw_size,
            "sidecar_sha256": h.sha256_file(target / "heightmap.raw.json")[0],
            "coverage_fraction": receipt["coverage_fraction"]}


def export_campaign(request, candidate, cancelled=lambda: False):
    # Even complete collider raster coverage cannot prove preservation of the
    # game's map representation or a return path. Do not read game content or
    # create an editable campaign reconstruction until that contract is proven.
    h.check_cancelled(cancelled)
    raise h.HeightmapImportError(
        "Campaign map import is unavailable: faithful source preservation and "
        "game round-trip support have not been verified. No export was created.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", type=Path, required=True)
    args = parser.parse_args()
    path = h.require_direct_path(args.request)
    if path.stat().st_size > 16384:
        raise h.HeightmapImportError("Campaign request exceeds its size limit.")
    request = json.loads(path.read_text(encoding="utf-8-sig"))
    workspace, _ = validate_request(request)
    if not path.is_relative_to(workspace):
        raise h.HeightmapImportError("Campaign work orders must remain inside their workspace.")
    # Candidate is owned by the caller's disposable operation directory. No broad
    # cleanup or changes to any input/game/save directory are performed here.
    result = export_campaign(request, path.parent / "candidate", lambda: (path.parent / "cancel.flag").exists())
    (path.parent / "result.json").write_bytes(h.pretty_json_bytes(result))
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
