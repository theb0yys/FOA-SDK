#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Alpha thumbnail extractor with bounded dependency-free DDS/TGA decoding.

The reviewed Alpha-0 extraction boundary is retained in the adjacent legacy
module. This module extends that boundary without invoking Unity, FoA, O3DE
Asset Processor, subprocesses, native decoder libraries, or runtime adapters.
"""
from __future__ import annotations

import hashlib
import os
import shutil
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

_TOOLS_ROOT = Path(__file__).resolve().parent
if str(_TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(_TOOLS_ROOT))

import foa_thumbnail_artifact_extractor_legacy as _legacy
from foa_thumbnail_artifact_extractor_legacy import *  # noqa: F401,F403
from foa_thumbnail_texture_codecs import (
    MAX_IMAGE_DIMENSION,
    MAX_IMAGE_PIXELS,
    PNG_SIGNATURE,
    TextureCodecError,
    UnsupportedTextureError,
    decode_dds,
    decode_tga,
    encode_png_rgba,
)

TOOL_VERSION = "0.2.0"
COPY_THUMBNAIL_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp"}
DECODED_TEXTURE_EXTENSIONS = {".dds", ".tga"}
SUPPORTED_THUMBNAIL_EXTENSIONS = (
    COPY_THUMBNAIL_EXTENSIONS | DECODED_TEXTURE_EXTENSIONS
)
# Compatibility name retained for callers. DDS/TGA are supported extensions;
# unsupported sub-formats still emit explicit unsupported receipts.
UNSUPPORTED_TEXTURE_EXTENSIONS: set[str] = set()

_legacy_verify_manifest = _legacy.verify_manifest
_legacy_synthetic_index = _legacy.synthetic_index


def _media_type(extension: str) -> str:
    return {
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".webp": "image/webp",
    }.get(extension.lower(), "application/octet-stream")


def _write_payload(destination: Path, payload: bytes) -> dict[str, Any]:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    try:
        temporary.write_bytes(payload)
        os.replace(temporary, destination)
    except OSError as exc:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass
        raise ThumbnailError(
            f"Unable to write thumbnail artefact {destination}: {exc}"
        ) from exc
    return {
        "ArtifactSha256": sha256_bytes(payload),
        "ArtifactByteSize": len(payload),
    }


def _read_source_payload(
    source: Path,
    record: Mapping[str, Any],
) -> bytes:
    try:
        stat = source.stat()
    except OSError as exc:
        raise ThumbnailError(
            f"Unable to stat native icon source: {source}: {exc}"
        ) from exc
    if stat.st_size <= 0:
        raise ThumbnailError(f"Native icon source is empty: {source}")
    if stat.st_size > MAX_THUMBNAIL_BYTES:
        raise ThumbnailError(
            f"Native icon source exceeds {MAX_THUMBNAIL_BYTES} bytes: {source}"
        )
    try:
        payload = source.read_bytes()
    except OSError as exc:
        raise ThumbnailError(
            f"Unable to read native icon source: {source}: {exc}"
        ) from exc

    declared_size = record.get("ByteSize")
    if isinstance(declared_size, int) and declared_size != len(payload):
        raise ThumbnailError(
            "Native icon source byte-size drift for "
            f"{record.get('AssetRecordId', '')}."
        )
    declared_sha = record.get("Sha256")
    if not isinstance(declared_sha, str):
        raise ThumbnailError("AssetRecord Sha256 must be present.")
    actual_sha = sha256_bytes(payload)
    if declared_sha != actual_sha:
        raise ThumbnailError(
            "Native icon source fingerprint drift for "
            f"{record.get('AssetRecordId', '')}."
        )
    return payload


def _generated_artifact(
    *,
    record: Mapping[str, Any],
    source_index_id: str,
    captured_at: str,
    artifact_id: str,
    destination: Path,
    preview_root: Path,
    payload: bytes,
    generation_method: str,
    fidelity: str,
    fidelity_detail: str,
    source_texture_format: str = "",
    decoded_width: int | None = None,
    decoded_height: int | None = None,
) -> dict[str, Any]:
    written = _write_payload(destination, payload)
    artifact = {
        "ThumbnailArtifactId": artifact_id,
        "AssetRecordId": require_id(
            record.get("AssetRecordId"),
            "AssetRecordId",
        ),
        "NativeAssetRef": record.get("NativeAssetRef", ""),
        "SourceIndexId": source_index_id,
        "SourceSha256": record.get("Sha256", ""),
        "ArtifactPath": preview_token(destination, preview_root),
        "ArtifactKind": "native-icon-thumbnail",
        "ArtifactExtension": destination.suffix.lower(),
        "OutputMediaType": _media_type(destination.suffix),
        "GenerationMethod": generation_method,
        "Fidelity": fidelity,
        "FidelityDetail": fidelity_detail,
        "Status": "generated",
        "LocalOnly": True,
        "RedistributionAllowed": False,
        "RepositoryCommitAllowed": False,
        "PreviewProductGenerated": False,
        "O3deAssetProcessorInvoked": False,
        "UnityInvoked": False,
        "RuntimePermissionGranted": False,
        "CapturedAt": captured_at,
        **written,
    }
    if source_texture_format:
        artifact["SourceTextureFormat"] = source_texture_format
    if decoded_width is not None and decoded_height is not None:
        artifact["DecodedWidth"] = decoded_width
        artifact["DecodedHeight"] = decoded_height
    return artifact


def _unsupported_artifact_v2(
    record: Mapping[str, Any],
    captured_at: str,
    source_index_id: str,
    reason: str,
) -> dict[str, Any]:
    artifact = _legacy.unsupported_artifact(
        record,
        captured_at,
        reason=reason,
    )
    artifact["SourceIndexId"] = source_index_id
    artifact["OutputMediaType"] = ""
    artifact["FidelityDetail"] = (
        "bounded-decoder-subformat-unsupported"
    )
    return artifact


def build_artifacts(
    workspace_path: Path,
    index_path: Path,
    *,
    preview_root: Path | None = None,
    captured_at: str | None = None,
) -> dict[str, Any]:
    profile = load_profile(workspace_path)
    index = read_json(index_path)
    validate_index(index, profile)
    if not is_relative_to(
        index_path.resolve(strict=False),
        profile["ExtractedDataPath"],
    ):
        raise ThumbnailError(
            "Index file must remain inside ExtractedDataPath."
        )
    captured_at = require_utc(
        captured_at
        or datetime.now(timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"
        ),
        "CapturedAt",
    )
    preview_root = (
        preview_root.resolve(strict=False)
        if preview_root is not None
        else (
            profile["ExtractedDataPath"]
            / "PreviewArtifacts"
            / "Thumbnails"
        ).resolve(strict=False)
    )
    if not is_relative_to(
        preview_root,
        profile["ExtractedDataPath"],
    ):
        raise ThumbnailError(
            "Preview output root must remain inside ExtractedDataPath."
        )
    preview_root.mkdir(parents=True, exist_ok=True)

    source_index_id = require_id(index.get("IndexId"), "IndexId")
    artifacts: list[dict[str, Any]] = []
    issues: list[dict[str, Any]] = []
    for record in index.get("AssetRecords", []):
        if not isinstance(record, dict):
            issues.append(
                issue(
                    "malformed-asset-record",
                    "error",
                    "AssetRecords entry is not an object.",
                    "",
                )
            )
            continue
        asset_record_id = require_id(
            record.get("AssetRecordId"),
            "AssetRecordId",
        )
        native_ref = record.get("NativeAssetRef")
        extension = str(record.get("Extension", "")).lower()
        eligibility = record.get("PreviewEligibility", {})
        if not (
            isinstance(eligibility, dict)
            and eligibility.get("ThumbnailCandidate") is True
        ):
            continue
        if extension not in SUPPORTED_THUMBNAIL_EXTENSIONS:
            issues.append(
                issue(
                    "unsupported-thumbnail-extension",
                    "warning",
                    f"{asset_record_id} uses unsupported "
                    f"extension {extension}.",
                    str(native_ref),
                )
            )
            continue

        source = token_to_install_path(
            str(native_ref),
            profile["InstallPath"],
        )
        if not source.is_file():
            issues.append(
                issue(
                    "missing-thumbnail-source",
                    "error",
                    f"{asset_record_id} source file is missing.",
                    str(native_ref),
                )
            )
            continue

        source_payload = _read_source_payload(source, record)
        artifact_id = (
            "thumbnail."
            + asset_record_id.removeprefix("visual.asset.")
        )
        if extension in COPY_THUMBNAIL_EXTENSIONS:
            suffix = ".jpg" if extension == ".jpeg" else extension
            destination = preview_root / f"{artifact_id}{suffix}"
            artifacts.append(
                _generated_artifact(
                    record=record,
                    source_index_id=source_index_id,
                    captured_at=captured_at,
                    artifact_id=artifact_id,
                    destination=destination,
                    preview_root=preview_root,
                    payload=source_payload,
                    generation_method=(
                        "local-only-loose-icon-copy"
                    ),
                    fidelity="exact",
                    fidelity_detail=(
                        "native-icon-byte-preserved"
                    ),
                )
            )
            continue

        try:
            if extension == ".tga":
                (
                    width,
                    height,
                    rgba,
                    source_texture_format,
                    fidelity,
                ) = decode_tga(source_payload)
                generation_method = (
                    "local-only-bounded-tga-decode"
                )
            else:
                (
                    width,
                    height,
                    rgba,
                    source_texture_format,
                    fidelity,
                ) = decode_dds(source_payload)
                generation_method = (
                    "local-only-bounded-dds-decode"
                )
            png_payload = encode_png_rgba(
                width,
                height,
                rgba,
            )
            destination = preview_root / f"{artifact_id}.png"
            artifacts.append(
                _generated_artifact(
                    record=record,
                    source_index_id=source_index_id,
                    captured_at=captured_at,
                    artifact_id=artifact_id,
                    destination=destination,
                    preview_root=preview_root,
                    payload=png_payload,
                    generation_method=generation_method,
                    fidelity=fidelity,
                    fidelity_detail=(
                        "decoded-first-mip-to-rgba8"
                        if fidelity == "exact"
                        else (
                            "decoded-first-mip-with-"
                            "partial-channel-interpretation"
                        )
                    ),
                    source_texture_format=(
                        source_texture_format
                    ),
                    decoded_width=width,
                    decoded_height=height,
                )
            )
        except (UnsupportedTextureError, TextureCodecError) as exc:
            reason = str(exc)
            artifacts.append(
                _unsupported_artifact_v2(
                    record,
                    captured_at,
                    source_index_id,
                    reason,
                )
            )
            issues.append(
                issue(
                    "thumbnail-decode-unsupported",
                    "warning",
                    f"{asset_record_id}: {reason}",
                    str(native_ref),
                )
            )

    if len(artifacts) > MAX_ARTIFACTS:
        raise ThumbnailError(
            f"Thumbnail artefact count exceeds {MAX_ARTIFACTS}."
        )
    artifacts.sort(key=lambda item: item["ThumbnailArtifactId"])
    generated_count = sum(
        item.get("Status") == "generated"
        for item in artifacts
    )
    unsupported_count = sum(
        item.get("Status") == "unsupported"
        for item in artifacts
    )
    manifest_id_seed = canonical_json(
        {
            "IndexId": source_index_id,
            "Artifacts": [
                (
                    item["ThumbnailArtifactId"],
                    item.get("ArtifactSha256", ""),
                )
                for item in artifacts
            ],
            "CapturedAt": captured_at,
        }
    )
    manifest = {
        "SchemaVersion": 1,
        "DocumentKind": DOCUMENT_KIND,
        "ManifestId": (
            "thumbnail.manifest."
            + hashlib.sha256(
                manifest_id_seed
            ).hexdigest()[:16]
        ),
        "SourceIndexId": source_index_id,
        "ProfileId": profile["ProfileId"],
        "GameVersion": profile["GameVersion"],
        "Branch": profile["Branch"],
        "RuntimeTarget": profile["RuntimeTarget"],
        "ToolId": TOOL_ID,
        "ToolVersion": TOOL_VERSION,
        "CapturedAt": captured_at,
        "PreviewRoot": "$preview",
        "PreviewStageStatus": {
            "DiscoveryIndexConsumed": True,
            "LocalPreviewArtifactsEmitted": (
                generated_count > 0
            ),
            "BoundedDdsTgaDecodeAvailable": True,
            "GeneratedArtifactCount": generated_count,
            "UnsupportedArtifactCount": unsupported_count,
            "GeneratedO3dePreviewProduct": False,
            "TypedAuthoringBindingCreated": False,
            "FunctionCompleteAllowed": False,
            "NextRequiredStages": [
                "unity-to-neutral-preview-handoff",
                "neutral-to-o3de-preview-conversion",
                "asset-browser-pane",
                "item-recipe-visual-selectors",
            ],
        },
        "ThumbnailArtifacts": artifacts,
        "Issues": issues,
        "OperationalAuthority": {
            key: False
            for key in AUTHORITY_FALSE_KEYS
        },
    }
    assert_no_private_paths(manifest)
    return manifest


def verify_manifest(
    manifest_path: Path,
    *,
    workspace_path: Path | None = None,
    index_path: Path | None = None,
    preview_root: Path | None = None,
) -> dict[str, Any]:
    manifest = _legacy_verify_manifest(
        manifest_path,
        workspace_path=workspace_path,
        index_path=index_path,
        preview_root=preview_root,
    )
    stage = manifest.get("PreviewStageStatus", {})
    legacy_manifest = (
        manifest.get("ToolVersion") == "0.1.0"
        and "BoundedDdsTgaDecodeAvailable" not in stage
    )
    if (
        not legacy_manifest
        and stage.get("BoundedDdsTgaDecodeAvailable") is not True
    ):
        raise ThumbnailError(
            "PreviewStageStatus must declare bounded "
            "DDS/TGA decode availability."
        )

    records: dict[str, Mapping[str, Any]] = {}
    if index_path is not None:
        index = read_json(index_path)
        for record in index.get("AssetRecords", []):
            if isinstance(record, dict):
                record_id = require_id(
                    record.get("AssetRecordId"),
                    "AssetRecordId",
                )
                records[record_id] = record

    root = (
        preview_root.resolve(strict=False)
        if preview_root is not None
        else manifest_path.parent.resolve(strict=False)
    )
    generated_count = 0
    unsupported_count = 0
    for artifact in manifest.get("ThumbnailArtifacts", []):
        record_id = require_id(
            artifact.get("AssetRecordId"),
            "AssetRecordId",
        )
        if records:
            record = records.get(record_id)
            if not record:
                raise ThumbnailError(
                    "Thumbnail artefact references missing "
                    f"index record: {record_id}"
                )
            if artifact.get("SourceSha256") != record.get(
                "Sha256"
            ):
                raise ThumbnailError(
                    "Thumbnail artefact source fingerprint "
                    f"mismatch: {record_id}"
                )
            if artifact.get("NativeAssetRef") != record.get(
                "NativeAssetRef"
            ):
                raise ThumbnailError(
                    "Thumbnail artefact native reference "
                    f"mismatch: {record_id}"
                )

        status = artifact.get("Status")
        if status == "generated":
            generated_count += 1
            generation = artifact.get("GenerationMethod")
            if generation in {
                "local-only-bounded-tga-decode",
                "local-only-bounded-dds-decode",
            }:
                token = str(artifact.get("ArtifactPath", ""))
                payload_path = root / token.removeprefix(
                    "$preview/"
                )
                payload = payload_path.read_bytes()
                if (
                    artifact.get("ArtifactExtension") != ".png"
                    or artifact.get("OutputMediaType")
                    != "image/png"
                    or not payload.startswith(PNG_SIGNATURE)
                ):
                    raise ThumbnailError(
                        "Decoded DDS/TGA artefacts must "
                        "be PNG payloads."
                    )
                width = artifact.get("DecodedWidth")
                height = artifact.get("DecodedHeight")
                if (
                    not isinstance(width, int)
                    or not isinstance(height, int)
                    or width <= 0
                    or height <= 0
                    or width > MAX_IMAGE_DIMENSION
                    or height > MAX_IMAGE_DIMENSION
                    or width * height > MAX_IMAGE_PIXELS
                ):
                    raise ThumbnailError(
                        "Decoded DDS/TGA artefact dimensions "
                        "are invalid."
                    )
                if not artifact.get("SourceTextureFormat"):
                    raise ThumbnailError(
                        "Decoded DDS/TGA artefacts require "
                        "SourceTextureFormat."
                    )
                if artifact.get("Fidelity") not in {
                    "exact",
                    "partial",
                }:
                    raise ThumbnailError(
                        "Decoded DDS/TGA fidelity must be "
                        "exact or partial."
                    )
        elif status == "unsupported":
            unsupported_count += 1

    if not legacy_manifest:
        if stage.get("GeneratedArtifactCount") != generated_count:
            raise ThumbnailError(
                "PreviewStageStatus GeneratedArtifactCount mismatch."
            )
        if (
            stage.get("UnsupportedArtifactCount")
            != unsupported_count
        ):
            raise ThumbnailError(
                "PreviewStageStatus UnsupportedArtifactCount mismatch."
            )
    return manifest


def _fixture_tga() -> bytes:
    header = struct.pack(
        "<BBBHHBHHHHBB",
        0,
        0,
        2,
        0,
        0,
        0,
        0,
        0,
        2,
        2,
        24,
        0x20,
    )
    return header + bytes(
        (
            0, 0, 255,
            0, 255, 0,
            255, 0, 0,
            255, 255, 255,
        )
    )


def _fixture_dds() -> bytes:
    header = bytearray(124)
    struct.pack_into(
        "<7I",
        header,
        0,
        124,
        0x0002100F,
        4,
        4,
        8,
        0,
        1,
    )
    struct.pack_into(
        "<II4s5I",
        header,
        72,
        32,
        0x4,
        b"DXT1",
        0,
        0,
        0,
        0,
        0,
    )
    struct.pack_into(
        "<5I",
        header,
        104,
        0x1000,
        0,
        0,
        0,
        0,
    )
    return (
        b"DDS "
        + bytes(header)
        + struct.pack("<HHI", 0xF800, 0x07E0, 0)
    )


def synthetic_index(
    workspace_path: Path,
) -> dict[str, Any]:
    index = _legacy_synthetic_index(workspace_path)
    profile = load_profile(workspace_path)
    relative = "Tainted Grail_Data/LooseIcons/gem.dds"
    source = profile["InstallPath"] / relative
    payload = source.read_bytes()
    record_id = (
        f"visual.asset.{profile['ProfileId']}."
        + hashlib.sha256(
            canonical_json(
                [relative, sha256_bytes(payload)]
            )
        ).hexdigest()[:16]
    )
    index["AssetRecords"].append(
        {
            "AssetRecordId": record_id,
            "NativeAssetRef": "$install/" + relative,
            "ProfileId": profile["ProfileId"],
            "GameVersion": profile["GameVersion"],
            "Branch": profile["Branch"],
            "RuntimeTarget": profile["RuntimeTarget"],
            "Locator": "$install/" + relative,
            "FileName": source.name,
            "Extension": ".dds",
            "FileKind": "loose-texture",
            "ByteSize": len(payload),
            "Sha256": sha256_bytes(payload),
            "FingerprintStatus": "hashed",
            "PreviewEligibility": {
                "ThumbnailCandidate": True,
                "StaticPreviewCandidate": False,
                "RequiresExtraction": True,
                "Reason": "thumbnail candidate",
            },
            "EvidenceKind": (
                "visual-asset-discovery"
            ),
            "Confidence": "observed",
            "DiscoveryOrdinal": len(
                index["AssetRecords"]
            ),
            "CatalogPromotionAllowed": False,
            "RuntimePermissionGranted": False,
            "PreviewProductGenerated": False,
        }
    )
    return index


def generate_fixture(
    output: Path,
    *,
    replace: bool = False,
) -> dict[str, Any]:
    if output.exists():
        if replace:
            if output.is_dir():
                shutil.rmtree(output)
            else:
                output.unlink()
        else:
            raise ThumbnailError(
                f"Fixture output is not empty: {output}"
            )
    install = output / "game" / "FoA"
    icons = (
        install
        / "Tainted Grail_Data"
        / "LooseIcons"
    )
    extracted = output / "workspace" / "Extracted"
    icons.mkdir(parents=True, exist_ok=True)
    extracted.mkdir(parents=True, exist_ok=True)
    (icons / "iron.png").write_bytes(
        encode_png_rgba(
            1,
            1,
            bytes((255, 255, 255, 255)),
        )
    )
    (icons / "ore.tga").write_bytes(_fixture_tga())
    (icons / "gem.dds").write_bytes(_fixture_dds())

    workspace = {
        "SchemaVersion": 1,
        "WorkspaceId": "fixture.workspace",
        "DisplayName": "Thumbnail Fixture",
        "RootPath": "./workspace",
        "OutputPath": "./workspace/Build",
        "StagingPath": "./workspace/Staging",
        "DeploymentPath": "./workspace/Deploy",
        "ActiveGameProfileId": "foa.mono.fixture",
        "GameProfiles": [
            {
                "ProfileId": "foa.mono.fixture",
                "DisplayName": "FoA Mono Fixture",
                "InstallPath": "./game/FoA",
                "GameVersion": "1.23.401",
                "Branch": "mono",
                "RuntimeTarget": "Mono",
                "UnityVersion": "6000.0.64f1",
                "BepInExVersion": "5.4.23.3",
                "ManagedAssembliesPath": "",
                "PluginPath": "",
                "DiagnosticsPath": (
                    "./workspace/Diagnostics"
                ),
                "ExtractedDataPath": (
                    "./workspace/Extracted"
                ),
                "DlcScopes": ["base-game"],
            }
        ],
    }
    workspace_path = (
        output / "workspace.tgworkspace.json"
    )
    workspace_path.write_bytes(pretty_json(workspace))
    index_path = extracted / DEFAULT_INDEX_NAME
    index_path.write_bytes(
        pretty_json(synthetic_index(workspace_path))
    )
    preview_root = (
        extracted
        / "PreviewArtifacts"
        / "Thumbnails"
    )
    manifest = build_artifacts(
        workspace_path,
        index_path,
        preview_root=preview_root,
        captured_at="2026-07-28T00:00:01Z",
    )
    manifest_path = (
        preview_root / DEFAULT_MANIFEST_NAME
    )
    write_manifest(
        manifest,
        manifest_path,
        replace=True,
    )
    verify_manifest(
        manifest_path,
        workspace_path=workspace_path,
        index_path=index_path,
        preview_root=preview_root,
    )
    return {
        "ManifestId": manifest["ManifestId"],
        "GeneratedCount": sum(
            item["Status"] == "generated"
            for item in manifest["ThumbnailArtifacts"]
        ),
        "UnsupportedCount": sum(
            item["Status"] == "unsupported"
            for item in manifest["ThumbnailArtifacts"]
        ),
        "ManifestPath": str(manifest_path),
    }


# Keep legacy CLI/global lookups on the extended implementations.
_legacy.TOOL_VERSION = TOOL_VERSION
_legacy.SUPPORTED_THUMBNAIL_EXTENSIONS = (
    SUPPORTED_THUMBNAIL_EXTENSIONS
)
_legacy.UNSUPPORTED_TEXTURE_EXTENSIONS = set()
_legacy.build_artifacts = build_artifacts
_legacy.verify_manifest = verify_manifest
_legacy.synthetic_index = synthetic_index
_legacy.generate_fixture = generate_fixture

main = _legacy.main


if __name__ == "__main__":
    raise SystemExit(main())
