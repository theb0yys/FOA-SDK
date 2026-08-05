#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Validate the native icon/thumbnail artefact extraction boundary."""
from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOL = "Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_artifact_extractor.py"
LEGACY = "Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_artifact_extractor_legacy.py"
CODECS = "Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_texture_codecs.py"
TEST = "Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_thumbnail_artifact_extractor.py"
CODEC_TEST = "Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_thumbnail_texture_codecs.py"
DOC = "docs/tainted-grail-sdk/FOA_THUMBNAIL_ARTIFACT_EXTRACTOR.md"


class ValidationError(RuntimeError):
    pass


def read(relative: str) -> str:
    path = REPO_ROOT / relative
    if not path.is_file():
        raise ValidationError(f"Missing required file: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise ValidationError(f"{label} missing required fragment: {fragment}")


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise ValidationError(f"{label} contains forbidden fragment: {fragment}")


def validate() -> None:
    tool = read(TOOL)
    legacy = read(LEGACY)
    codecs = read(CODECS)
    test = read(TEST)
    codec_test = read(CODEC_TEST)
    doc = read(DOC)

    for fragment in (
        "foa-thumbnail-artifact-evidence",
        "foa-visual-asset-discovery-index",
        "FoA native asset reference -> version-bound discovery record -> local preview artefact",
        "FunctionCompleteAllowed",
        "GeneratedO3dePreviewProduct",
        "O3deAssetProcessorInvoked",
        "UnityInvoked",
        "RepositoryWriteAllowed",
        "RuntimePermissionGranted",
        "SUPPORTED_THUMBNAIL_EXTENSIONS",
        "UNSUPPORTED_TEXTURE_EXTENSIONS",
        "local-only-loose-icon-copy",
        "unsupported-receipt",
        "RedistributionAllowed",
        "RepositoryCommitAllowed",
        "Preview output root must remain inside ExtractedDataPath",
    ):
        require(legacy, fragment, "legacy thumbnail boundary")

    for fragment in (
        "foa_thumbnail_artifact_extractor_legacy",
        "foa_thumbnail_texture_codecs",
        "DECODED_TEXTURE_EXTENSIONS",
        "local-only-bounded-tga-decode",
        "local-only-bounded-dds-decode",
        "Native icon source fingerprint drift",
        "BoundedDdsTgaDecodeAvailable",
        "DecodedWidth",
        "DecodedHeight",
        "SourceTextureFormat",
        "GeneratedArtifactCount",
        "UnsupportedArtifactCount",
        "legacy_manifest",
    ):
        require(tool, fragment, "extended thumbnail boundary")

    for fragment in (
        "def encode_png_rgba",
        "def decode_tga",
        "def decode_dds",
        '"BC1"',
        '"BC2"',
        '"BC3"',
        '"BC4"',
        '"BC5"',
        "MAX_IMAGE_DIMENSION",
        "MAX_IMAGE_PIXELS",
        "Color-mapped TGA images are outside the Alpha cohort",
        "DDS arrays, cubemaps, and non-2D resources",
    ):
        require(codecs, fragment, "bounded thumbnail codecs")

    for fragment in (
        "test_extracts_generated_and_unsupported_thumbnail_artifacts",
        "test_write_and_verify_manifest",
        "test_authority_escalation_is_rejected",
        "test_private_path_leakage_is_rejected",
        "test_preview_output_must_remain_inside_extracted_data",
        "test_whole_second_utc_required",
    ):
        require(test, fragment, "legacy thumbnail tests")

    for fragment in (
        "test_png_encoder_is_deterministic",
        "test_tga_raw_and_rle_origins",
        "test_dds_bc1_and_bc3",
        "test_dds_unknown_fourcc_is_explicitly_unsupported",
        "test_bounds_and_truncation_fail_closed",
    ):
        require(codec_test, fragment, "thumbnail codec tests")

    for fragment in (
        "local preview artefact",
        "foa-visual-asset-index.json",
        "foa-thumbnail-artifacts.json",
        "bounded DDS and TGA decoder",
        "BC1",
        "BC3",
        "RLE true-colour",
        "does not invoke Unity",
        "does not invoke O3DE Asset Processor",
        "FunctionCompleteAllowed` remains `false`",
        "source fingerprint drift",
        "RepositoryCommitAllowed",
        "RedistributionAllowed",
    ):
        require(doc, fragment, "thumbnail extractor documentation")

    combined_executable = tool + "\n" + codecs
    for forbidden in (
        "subprocess.run",
        "Unity.exe",
        "AssetProcessorBatch",
        "Harmony.Patch",
        "Catalog/catalog.tgcatalog.json",
        "from PIL",
        "import PIL",
        "ctypes",
    ):
        reject(
            combined_executable,
            forbidden,
            "thumbnail extractor executable boundary",
        )


def main() -> int:
    try:
        validate()
    except ValidationError as exc:
        print(
            f"FoA thumbnail artefact extractor validation failed: {exc}",
            file=sys.stderr,
        )
        return 1
    print("FoA thumbnail artefact extractor boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
