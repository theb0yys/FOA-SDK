#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the Developer Preview 0 service-level persistence smoke contract."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_file(path: Path) -> str:
    if not path.is_file():
        fail(f"Required Developer Preview smoke file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    text = require_file(path)
    for fragment in fragments:
        if fragment not in text:
            fail(f"Missing required fragment {fragment!r} in {path}")
    return text


def manifest_entries(path: Path) -> set[str]:
    return set(re.findall(r"^\s+((?:Source|Tests)/[^\s\)]+)\s*$", require_file(path), re.MULTILINE))


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    smoke_test = code_root / "Tests/DeveloperPreviewSmokeTests.cpp"
    test_manifest = code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    framework_manifest = code_root / "taintedgrailmoddingsdk_framework_files.cmake"
    cmake = code_root / "CMakeLists.txt"
    persistence = source_root / "CatalogPersistenceService.cpp"
    compatibility = source_root / "PersistenceJsonUtils.h"
    workspace_persistence = source_root / "WorkspacePersistenceService.cpp"
    pack_persistence = source_root / "PackPersistenceService.cpp"
    evidence_persistence = source_root / "SourceEvidencePersistenceService.cpp"
    guide = repo_root / "docs/tainted-grail-sdk/DEVELOPER_PREVIEW_0.md"
    fixture_readme = gem_root / "Preview/README.md"
    workflow = repo_root / ".github/workflows/tainted-grail-sdk-foundation.yml"

    try:
        smoke = require_fragments(
            smoke_test,
            (
                "DeveloperPreviewFixtureLoadSaveReopenPreservesCanonicalState",
                "ProofBackedAllowedUsageSurvivesCatalogLoad",
                "LegacyUnprovenAllowanceStillFailsClosed",
                "WorkspacePersistenceService",
                "PackPersistenceService",
                "SourceEvidencePersistenceService",
                "CatalogPersistenceService",
                "LoadPreviewState",
                "SavePreviewState",
                "BuildCanonicalSnapshot",
                "BuildRecipeStationEvidence",
                "loaded = {}",
                "EXPECT_EQ(snapshotBefore, snapshotAfterResult.GetValue())",
                "preview.source.synthetic",
                "preview.recipe.synthesis",
            ),
        )
        for forbidden in (
            "FoA.exe", "BepInEx", "HarmonyLib", "WriteProcessMemory",
            "CreateProcess", "QProcess", "system(", "Deployment/",
        ):
            if forbidden in smoke:
                fail(f"Developer Preview smoke test contains prohibited runtime behavior: {forbidden}")

        test_entries = manifest_entries(test_manifest)
        if "Tests/DeveloperPreviewSmokeTests.cpp" not in test_entries:
            fail("Developer Preview smoke test is not owned by the catalog test manifest")
        production_in_tests = sorted(entry for entry in test_entries if entry.startswith("Source/"))
        if production_in_tests:
            fail(
                "Developer Preview test manifest must link production targets instead of recompiling: "
                + ", ".join(production_in_tests)
            )

        framework_entries = manifest_entries(framework_manifest)
        required_framework = {
            "Source/WorkspacePersistenceService.cpp",
            "Source/PackPersistenceService.cpp",
            "Source/SourceEvidencePersistenceService.cpp",
            "Source/CatalogPersistenceService.cpp",
        }
        if not required_framework.issubset(framework_entries):
            fail(
                "Framework target is missing Developer Preview persistence services: "
                + ", ".join(sorted(required_framework - framework_entries))
            )

        require_fragments(
            cmake,
            (
                "NAME ${gem_name}.Framework.Static STATIC",
                "Gem::${gem_name}.Framework.Static",
                "Tests/DeveloperPreviewSmokeTests.cpp",
                "TG_SDK_PREVIEW_TEMPLATE_ROOT",
                "../Preview/Template",
            ),
        )
        require_fragments(
            compatibility,
            (
                "namespace TaintedGrailModdingSDK::PersistenceJsonUtils",
                "ReadJsonFile",
                'document.HasMember("Type")',
                "AZ::JsonSerialization::Load",
                "Processing::Completed",
                "m_clearContainers = true",
            ),
        )
        for service in (
            workspace_persistence,
            pack_persistence,
            evidence_persistence,
            persistence,
        ):
            require_fragments(
                service,
                ('#include "PersistenceJsonUtils.h"', "PersistenceJsonUtils::LoadObjectFromFile"),
            )

        persistence_text = require_fragments(
            persistence,
            (
                "FindLatestPermissionEvent",
                "HasValidatedPermissionBasis",
                "HasProofBackedAllowance",
                'event->m_newValue == "allow"',
                "NormalizeAllowedUsages",
                "legacy_permission_review_required",
                "NormalizeLegacyValidationHistory(document);",
                "NormalizeLegacyGovernanceState(document);",
            ),
        )
        if persistence_text.find("NormalizeLegacyValidationHistory(document);") > persistence_text.find(
            "NormalizeLegacyGovernanceState(document);"
        ):
            fail("Legacy validation proof must normalize before permission compatibility decisions")

        template_root = gem_root / "Preview/Template"
        for relative_path in (
            "preview.tgworkspace.json",
            "Packs/preview.developer-preview-0.tgpack.json",
            "Sources/preview.source.synthetic/source.tgsource.json",
            "Sources/preview.source.synthetic/evidence.tgevidence.json",
            "Catalog/catalog.tgcatalog.json",
        ):
            document = json.loads((template_root / relative_path).read_text(encoding="utf-8"))
            if "Type" in document or "ClassData" in document:
                fail(f"Public fixture must remain a plain schema document: {relative_path}")

        require_fragments(
            fixture_readme,
            ("plain schema-1 JSON", "service-level", "load, save, close-equivalent, and reopen"),
        )
        require_fragments(
            guide,
            (
                "service-level persistence smoke",
                "load, save, close-equivalent, and reopen",
                "canonical state",
                "proof-backed allowed usages",
                "legacy unproven allowances",
                "TaintedGrailModdingSDK.Catalog.Tests",
            ),
        )
        require_fragments(
            workflow,
            ("Validate Developer Preview 0 persistence smoke contract", "validate_developer_preview_smoke.py"),
        )
    except (OSError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"Developer Preview 0 persistence smoke validation failed: {exc}", file=sys.stderr)
        return 1

    print("Developer Preview 0 linked-target load/save/reopen persistence smoke contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
