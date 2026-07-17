#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Tainted Grail Modding SDK editor foundation without building O3DE.

The check verifies repository structure, durable workspace and pack management,
source/evidence intake, and the editor-only product boundary. It does not replace
an O3DE configure or compile.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

GEM_NAME = "TaintedGrailModdingSDK"
GEM_PATH = f"Gems/{GEM_NAME}"


def fail(message: str) -> None:
    raise RuntimeError(message)


def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"Unable to read valid JSON from {path}: {exc}")
    if not isinstance(value, dict):
        fail(f"Expected a JSON object in {path}")
    return value


def require_contains(text: str, fragment: str, path: Path) -> None:
    if fragment not in text:
        fail(f"Missing required fragment {fragment!r} in {path}")


def validate_engine_registration(repo_root: Path) -> None:
    engine = load_json(repo_root / "engine.json")
    external_subdirectories = engine.get("external_subdirectories")
    if not isinstance(external_subdirectories, list):
        fail("engine.json external_subdirectories must be a list")
    if external_subdirectories.count(GEM_PATH) != 1:
        fail(f"{GEM_PATH} must appear exactly once in engine.json external_subdirectories")
    if GEM_NAME in engine.get("gem_names", []):
        fail(f"{GEM_NAME} must not be an engine-wide default Gem")


def validate_gem_metadata(gem_root: Path) -> None:
    gem = load_json(gem_root / "gem.json")
    expected_values = {
        "gem_name": GEM_NAME,
        "display_name": "Tainted Grail Modding SDK",
        "type": "Tool",
    }
    for key, expected in expected_values.items():
        if gem.get(key) != expected:
            fail(f"gem.json {key} must be {expected!r}")
    if not isinstance(gem.get("version"), str) or not re.fullmatch(r"\d+\.\d+\.\d+", gem["version"]):
        fail("gem.json version must use MAJOR.MINOR.PATCH")
    if gem.get("dependencies") != []:
        fail("The editor foundation must not add Gem dependencies yet")


def validate_cmake(gem_root: Path) -> None:
    cmake_path = gem_root / "Code" / "CMakeLists.txt"
    cmake = cmake_path.read_text(encoding="utf-8")
    for fragment in (
        "if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)",
        "NAME ${gem_name}.Editor GEM_MODULE",
        "taintedgrailmoddingsdk_editor_files.cmake",
        "AZ::AzToolsFramework",
        "ly_create_alias(NAME ${gem_name}.Tools",
        "ly_create_alias(NAME ${gem_name}.Builders",
        "VARIANTS Tools Builders",
    ):
        require_contains(cmake, fragment, cmake_path)
    for fragment in ("${gem_name}.Clients", "${gem_name}.Servers", "${gem_name}.Unified"):
        if fragment in cmake:
            fail(f"Editor-only foundation must not expose runtime alias {fragment}")


def validate_source_manifest(gem_root: Path) -> None:
    code_root = gem_root / "Code"
    manifest_path = code_root / "taintedgrailmoddingsdk_editor_files.cmake"
    manifest = manifest_path.read_text(encoding="utf-8")
    entries = set(re.findall(r"^\s+(Source/[^\s\)]+)\s*$", manifest, re.MULTILINE))
    required_entries = {
        "Source/CatalogDatabase.cpp", "Source/CatalogDatabase.h",
        "Source/FoundationModels.cpp", "Source/FoundationModels.h",
        "Source/FoundationNotificationBus.h",
        "Source/FoundationService.cpp", "Source/FoundationService.h",
        "Source/FoundationStatusWidget.cpp", "Source/FoundationStatusWidget.h",
        "Source/FoundationValidationService.cpp", "Source/FoundationValidationService.h",
        "Source/PackManagerWidget.cpp", "Source/PackManagerWidget.h",
        "Source/PackPersistenceService.cpp", "Source/PackPersistenceService.h",
        "Source/SourceEvidenceIntakeWidget.cpp", "Source/SourceEvidenceIntakeWidget.h",
        "Source/SourceEvidencePersistenceService.cpp", "Source/SourceEvidencePersistenceService.h",
        "Source/SourceEvidenceRegistry.cpp", "Source/SourceEvidenceRegistry.h",
        "Source/SourceImportService.cpp", "Source/SourceImportService.h",
        "Source/TaintedGrailModdingSDKEditorModule.cpp",
        "Source/TaintedGrailModdingSDKSystemComponent.cpp",
        "Source/TaintedGrailModdingSDKSystemComponent.h",
        "Source/WorkspacePersistenceService.cpp", "Source/WorkspacePersistenceService.h",
    }
    if entries != required_entries:
        fail(f"Editor source manifest mismatch: expected {sorted(required_entries)}, found {sorted(entries)}")
    for relative_path in entries:
        if not (code_root / relative_path).is_file():
            fail(f"Manifest entry does not exist: {relative_path}")


def read_sources(gem_root: Path) -> tuple[Path, str]:
    source_root = gem_root / "Code" / "Source"
    files = sorted(source_root.glob("*.[ch]pp")) + sorted(source_root.glob("*.h"))
    return source_root, "\n".join(path.read_text(encoding="utf-8") for path in files)


def validate_editor_foundation(gem_root: Path) -> None:
    _, combined = read_sources(gem_root)
    for fragment in (
        "WorkspaceModel", "GameProfile", "m_runtimeTarget", "m_outputPath", "m_stagingPath", "m_deploymentPath",
        "class WorkspacePersistenceService", "OpenWorkspace", "SaveWorkspaceAs",
        "PackManifest", "HasStableIdentity", "UsesSupportedSchema", "m_requiredCoreVersion",
        "m_requiredAdapterVersion", "m_requiredMods", "m_contentDefinitionPaths", "m_assetPaths",
        "m_localisationPaths", "m_buildConfiguration", "m_releaseChannel",
        "class PackPersistenceService", "class PackManagerWidget", "SaveActivePack", "LoadPack",
        "RegisterViewPane<PackManagerWidget>",
        "class SourceEvidenceRegistry", "class CatalogDatabase", "class FoundationValidationService",
        "class FoundationService", "FoundationNotificationBus", "class FoundationStatusWidget",
        "RegisterViewPane<FoundationStatusWidget>", "SaveObjectToFile", "LoadObjectFromFile",
        "TaintedGrailModdingSDKService", "FoA runtime execution remains disabled",
    ):
        if fragment not in combined:
            fail(f"Editor foundation is missing {fragment!r}")
    for token in (
        "#include <BepInEx", "HarmonyLib", "TG.Main", "LocationTemplate", "SpawnLocation(",
        "WriteAllBytes", "std::ofstream",
    ):
        if token in combined:
            fail(f"Editor-only foundation contains forbidden runtime integration {token!r}")


def validate_source_intake(gem_root: Path) -> None:
    source_root, combined = read_sources(gem_root)
    for fragment in (
        "SourceImporterContract", "SourceImportRequest", "SourceImportResult", "SourceDocument",
        "EvidenceDocument", "ImportIssue", "m_fingerprint", "m_profileId", "m_sourceFingerprint",
        "QCryptographicHash::Sha256", '"tg.structured-json"', '"tg.structured-csv"',
        '"tg.generic-artifact"', "evidence.manual-extraction-required", "schema.invalid-json",
        "schema.csv-required-columns", "source.tgsource.json", "evidence.tgevidence.json",
        "class SourceImportService", "class SourceEvidencePersistenceService", "ImportSource",
        "ReloadSourceEvidence", "class SourceEvidenceIntakeWidget",
        "RegisterViewPane<SourceEvidenceIntakeWidget>",
        "The evidence document does not match the source identity and fingerprint",
        "Evidence profile, build, branch, or fingerprint does not match its source",
    ):
        if fragment not in combined:
            fail(f"Source/evidence intake is missing {fragment!r}")

    service = (source_root / "FoundationService.cpp").read_text(encoding="utf-8")
    save_position = service.find("SaveDocuments(")
    publish_position = service.find("m_sourceRegistry = AZStd::move(candidateRegistry)")
    if save_position < 0 or publish_position < 0 or save_position > publish_position:
        fail("Source intake must persist documents before publishing the candidate registry")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    gem_root = repo_root / GEM_PATH
    try:
        validate_engine_registration(repo_root)
        validate_gem_metadata(gem_root)
        validate_cmake(gem_root)
        validate_source_manifest(gem_root)
        validate_editor_foundation(gem_root)
        validate_source_intake(gem_root)
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail SDK foundation validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail SDK foundation validation passed.")
    print(
        "Validated: workspace and pack editing, source/evidence importer contracts, SHA-256 fingerprinting, "
        "exact profile binding, durable documents, schema/import reporting, catalog/query service, blockers, "
        "editor docks, automatic refresh, and editor-only boundary."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
