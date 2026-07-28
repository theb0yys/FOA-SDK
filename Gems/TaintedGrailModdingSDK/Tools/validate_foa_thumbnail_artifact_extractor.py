#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Validate the native icon/thumbnail artefact extraction boundary."""
from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOL = "Gems/TaintedGrailModdingSDK/Tools/foa_thumbnail_artifact_extractor.py"
TEST = "Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_thumbnail_artifact_extractor.py"
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
    test = read(TEST)
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
        require(tool, fragment, "thumbnail extractor boundary")

    for fragment in (
        "test_extracts_generated_and_unsupported_thumbnail_artifacts",
        "test_write_and_verify_manifest",
        "test_authority_escalation_is_rejected",
        "test_private_path_leakage_is_rejected",
        "test_preview_output_must_remain_inside_extracted_data",
        "test_whole_second_utc_required",
    ):
        require(test, fragment, "thumbnail extractor tests")

    for fragment in (
        "local preview artefact",
        "foa-visual-asset-index.json",
        "foa-thumbnail-artifacts.json",
        "does not invoke Unity",
        "does not invoke O3DE Asset Processor",
        "FunctionCompleteAllowed` remains `false`",
        "RepositoryCommitAllowed",
        "RedistributionAllowed",
    ):
        require(doc, fragment, "thumbnail extractor documentation")

    for forbidden in (
        "subprocess.run",
        "Unity.exe",
        "AssetProcessorBatch",
        "Harmony.Patch",
        "Catalog/catalog.tgcatalog.json",
    ):
        reject(tool, forbidden, "thumbnail extractor executable boundary")


def main() -> int:
    try:
        validate()
    except ValidationError as exc:
        print(f"FoA thumbnail artefact extractor validation failed: {exc}", file=sys.stderr)
        return 1
    print("FoA thumbnail artefact extractor boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
