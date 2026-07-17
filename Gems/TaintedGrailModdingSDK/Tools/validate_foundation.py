#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the public Tainted Grail Modding SDK editor foundation without building O3DE.

The focused check covers repository structure, public governance, workspace and pack
management, source/evidence intake, canonical catalog and governance workflows, typed
item/recipe authoring, and the editor-only product boundary. It does not replace an
O3DE configure, compile, or test run.
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
    path = gem_root / "Code" / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")
    for fragment in (
        "if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)",
        "NAME ${gem_name}.Editor GEM_MODULE",
        "taintedgrailmoddingsdk_editor_files.cmake",
        "AZ::AzToolsFramework",
        "NAME ${gem_name}.Catalog.Tests",
        "taintedgrailmoddingsdk_catalog_tests_files.cmake",
        "ly_add_googletest",
        "ly_create_alias(NAME ${gem_name}.Tools",
        "ly_create_alias(NAME ${gem_name}.Builders",
        "VARIANTS Tools Builders",
    ):
        require_contains(text, fragment, path)
    for fragment in ("${gem_name}.Clients", "${gem_name}.Servers", "${gem_name}.Unified"):
        if fragment in text:
            fail(f"Editor-only foundation must not expose runtime alias {fragment}")


def validate_source_manifest(gem_root: Path) -> None:
    code_root = gem_root / "Code"
    path = code_root / "taintedgrailmoddingsdk_editor_files.cmake"
    text = path.read_text(encoding="utf-8")
    entries = set(re.findall(r"^\s+(Source/[^\s\)]+)\s*$", text, re.MULTILINE))
    required = {
        "Source/CatalogBrowserWidget.cpp", "Source/CatalogBrowserWidget.h",
        "Source/CatalogDatabase.cpp", "Source/CatalogDatabase.h",
        "Source/CatalogGovernanceBlockerService.cpp", "Source/CatalogGovernanceBlockerService.h",
        "Source/CatalogGovernanceService.cpp", "Source/CatalogGovernanceService.h",
        "Source/CatalogGovernanceWidget.cpp", "Source/CatalogGovernanceWidget.h",
        "Source/CatalogPersistenceService.cpp", "Source/CatalogPersistenceService.h",
        "Source/CatalogPromotionService.cpp", "Source/CatalogPromotionService.h",
        "Source/EconomyAuthoringService.cpp", "Source/EconomyAuthoringService.h",
        "Source/EconomyBlockerService.cpp", "Source/EconomyBlockerService.h",
        "Source/EconomyModels.cpp", "Source/EconomyModels.h",
        "Source/FoundationCatalogService.cpp", "Source/FoundationEconomyService.cpp",
        "Source/FoundationGovernanceService.cpp",
        "Source/FoundationModels.cpp", "Source/FoundationModels.h",
        "Source/FoundationNotificationBus.h",
        "Source/FoundationService.cpp", "Source/FoundationService.h",
        "Source/FoundationStatusWidget.cpp", "Source/FoundationStatusWidget.h",
        "Source/FoundationValidationService.cpp", "Source/FoundationValidationService.h",
        "Source/ItemRecipeEditorWidget.cpp", "Source/ItemRecipeEditorWidget.h",
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
    if entries != required:
        fail(f"Editor source manifest mismatch: expected {sorted(required)}, found {sorted(entries)}")
    for relative_path in entries:
        if not (code_root / relative_path).is_file():
            fail(f"Manifest entry does not exist: {relative_path}")


def read_sources(gem_root: Path) -> tuple[Path, str]:
    source_root = gem_root / "Code" / "Source"
    files = sorted(source_root.glob("*.[ch]pp")) + sorted(source_root.glob("*.h"))
    return source_root, "\n".join(path.read_text(encoding="utf-8") for path in files)


def validate_editor_foundation(gem_root: Path) -> None:
    _, combined = read_sources(gem_root)
    required = (
        "WorkspaceModel", "GameProfile", "m_runtimeTarget", "m_outputPath", "m_stagingPath", "m_deploymentPath",
        "class WorkspacePersistenceService", "OpenWorkspace", "SaveWorkspaceAs",
        "PackManifest", "HasStableIdentity", "UsesSupportedSchema", "m_requiredCoreVersion",
        "m_requiredAdapterVersion", "m_requiredMods", "m_contentDefinitionPaths", "m_assetPaths",
        "m_localisationPaths", "m_buildConfiguration", "m_releaseChannel",
        "class PackPersistenceService", "class PackManagerWidget", "SaveActivePack", "LoadPack",
        "RegisterViewPane<PackManagerWidget>", "class SourceEvidenceRegistry", "class CatalogDatabase",
        "class FoundationValidationService", "class FoundationService", "FoundationNotificationBus",
        "class FoundationStatusWidget", "RegisterViewPane<FoundationStatusWidget>",
        "SaveObjectToFile", "LoadObjectFromFile", "TaintedGrailModdingSDKService",
        "FoA runtime execution remains disabled",
    )
    for fragment in required:
        if fragment not in combined:
            fail(f"Editor foundation is missing {fragment!r}")
    for token in (
        "#include <BepInEx", "HarmonyLib", "TG.Main", "LocationTemplate", "SpawnLocation(",
        "HeroItems.Add", "HeroRecipes.LearnRecipe", "Stock.AddItem", "WriteAllBytes", "std::ofstream",
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


def validate_catalog(gem_root: Path) -> None:
    source_root, combined = read_sources(gem_root)
    for fragment in (
        "CatalogDocument", "CatalogRelationship", "CatalogValidationEvent", "CatalogPromotionRequest",
        "m_nativeRefExact", "m_ownerPackId", "m_aliases", "m_sourceScopedRefs", "m_conflictRefs",
        "m_supersededByRecordId", "catalog.tgcatalog.json", "class CatalogPersistenceService",
        "class CatalogPromotionService", "class CatalogBrowserWidget", "PromoteEvidenceToCatalog",
        "ReloadCatalog", "RegisterViewPane<CatalogBrowserWidget>",
        "Catalog record ID already exists; promotion never merges by display name",
        "Exact native reference is already owned by another canonical catalog record",
        "Claim promotion cannot grant usage permission", "no_unvalidated_runtime_use",
        "The canonical catalog document is bound to a different workspace or game profile",
        "permission-before-validation", "relationship-evidence", "validation-profile",
    ):
        if fragment not in combined:
            fail(f"Canonical catalog is missing {fragment!r}")

    promotion_path = source_root / "CatalogPromotionService.cpp"
    promotion = promotion_path.read_text(encoding="utf-8")
    if "record.m_allowedUsages" in promotion:
        fail("Evidence promotion must not assign allowed usages")
    require_contains(promotion, 'record.m_stalenessState = "unknown"', promotion_path)

    catalog_service_path = source_root / "FoundationCatalogService.cpp"
    catalog_service = catalog_service_path.read_text(encoding="utf-8")
    save_position = catalog_service.find("m_catalogPersistence.Save(")
    publish_position = catalog_service.find("m_catalog = candidate")
    if save_position < 0 or publish_position < 0 or save_position > publish_position:
        fail("Canonical catalog must persist the candidate document before publishing in-memory state")

    database_path = source_root / "CatalogDatabase.cpp"
    database = database_path.read_text(encoding="utf-8")
    if "record.m_displayName ==" in database or "record.m_displayName !=" in database:
        fail("Canonical identity must not merge or reject records based on display name")


def validate_governance_engine(gem_root: Path) -> None:
    source_root, combined = read_sources(gem_root)
    for fragment in (
        "CatalogGovernanceEvent", "CatalogGovernanceRequest", "CatalogValidationRequest",
        "m_stalenessState", "m_governanceHistory", "GetGovernanceHistory",
        "class CatalogGovernanceService", "class CatalogGovernanceBlockerService",
        "class CatalogGovernanceWidget", "ApplyCatalogGovernanceDecision",
        "ApplyCatalogValidationDecision", "RegisterViewPane<CatalogGovernanceWidget>",
        "GovernanceHistory", "stale_or_unverified", "validation_failed", "superseded",
        "Allowed usage requires at least one validated proof event",
        "Permission never follows automatically from evidence or validation",
        "Usage permission requires a validated, current, unresolved-free, non-superseded record",
    ):
        if fragment not in combined:
            fail(f"Catalog governance engine is missing {fragment!r}")

    path = source_root / "CatalogGovernanceService.cpp"
    text = path.read_text(encoding="utf-8")
    if "updated.m_allowedUsages.push_back" in text:
        fail("Governance permissions must use duplicate-safe reviewed transitions")
    for fragment in ("catalog.AddValidationEvent(validation", "updated.m_allowedUsages.clear();", "ValidatePermissionBasis"):
        require_contains(text, fragment, path)

    foundation_path = source_root / "FoundationGovernanceService.cpp"
    require_contains(
        foundation_path.read_text(encoding="utf-8"),
        "PersistCatalogCandidate(candidate, error)",
        foundation_path,
    )


def validate_economy_authoring(gem_root: Path) -> None:
    source_root, combined = read_sources(gem_root)
    required = (
        "EconomyItemProfile", "EconomyRecipeProfile", "EconomyRecipeIngredient", "EconomyRecipeOutput",
        "m_economyItems", "m_economyRecipes", "m_recipeIngredients", "m_recipeOutputs",
        "class EconomyAuthoringService", "class EconomyBlockerService", "class ItemRecipeEditorWidget",
        "UpsertEconomyItemProfile", "UpsertEconomyRecipeProfile", "UpsertEconomyRecipeIngredient",
        "UpsertEconomyRecipeOutput", "RegisterViewPane<ItemRecipeEditorWidget>",
        "Tainted Grail Item and Recipe Editor", "existing_item_grant", "existing_recipe_learn",
        "runtime_recipe_append", "custom_item_registration", "custom_recipe_registration",
        "asset_localisation_injection", "vendor_or_loot_injection",
        "quest_or_contract_reward_injection", "no_unvalidated_runtime_use",
        "Recipe ingredients require exactly one item record ID or unresolved item subject ref",
        "Recipe outputs require exactly one item record ID or unresolved item subject ref",
        "Recipe persistence mode must be unknown, native_template, runtime_append, or custom_template",
        "The canonical item record does not exist", "Economy evidence belongs to a different catalog subject",
    )
    for fragment in required:
        if fragment not in combined:
            fail(f"Item/recipe editor is missing {fragment!r}")

    database_path = source_root / "CatalogDatabase.cpp"
    database = database_path.read_text(encoding="utf-8")
    for fragment in (
        "document.m_economyItems = m_economyItems",
        "document.m_economyRecipes = m_economyRecipes",
        "document.m_recipeIngredients = m_recipeIngredients",
        "document.m_recipeOutputs = m_recipeOutputs",
        "candidate.UpsertEconomyItem",
        "candidate.UpsertEconomyRecipe",
        "candidate.UpsertRecipeIngredient",
        "candidate.UpsertRecipeOutput",
    ):
        require_contains(database, fragment, database_path)

    service_path = source_root / "FoundationEconomyService.cpp"
    service = service_path.read_text(encoding="utf-8")
    for fragment in (
        "CatalogDatabase candidate = m_catalog",
        "ValidateEvidenceForSubjects",
        "PersistCatalogCandidate(candidate, error)",
    ):
        require_contains(service, fragment, service_path)

    widget_path = source_root / "ItemRecipeEditorWidget.cpp"
    widget = widget_path.read_text(encoding="utf-8")
    for forbidden in ("ApplyCatalogGovernanceDecision", "ApplyCatalogValidationDecision", "m_allowedUsages.push_back"):
        if forbidden in widget:
            fail(f"Item/recipe editor must not author permission state directly: {forbidden}")


def validate_public_project(repo_root: Path) -> None:
    required_files = {
        "README.md", "CONTRIBUTING.md", "CODE_OF_CONDUCT.md", "SECURITY.md", "SUPPORT.md",
        "GOVERNANCE.md", "ROADMAP.md", "CHANGELOG.md", ".github/CODEOWNERS",
        ".github/PULL_REQUEST_TEMPLATE.md", ".github/ISSUE_TEMPLATE/config.yml",
        ".github/ISSUE_TEMPLATE/tg_sdk_bug.yml", ".github/ISSUE_TEMPLATE/tg_sdk_feature.yml",
        ".github/ISSUE_TEMPLATE/tg_sdk_research.yml", "docs/tainted-grail-sdk/README.md",
        "docs/tainted-grail-sdk/USER_GUIDE.md", "docs/tainted-grail-sdk/CATALOG_GUIDE.md",
        "docs/tainted-grail-sdk/GOVERNANCE_ENGINE_GUIDE.md",
        "docs/tainted-grail-sdk/ITEM_RECIPE_EDITOR_GUIDE.md",
        "docs/tainted-grail-sdk/ARCHITECTURE.md", "docs/tainted-grail-sdk/DEVELOPMENT_GUIDE.md",
        "docs/tainted-grail-sdk/CODE_QUALITY.md", "docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md",
        "docs/tainted-grail-sdk/DATA_FORMATS.md", "docs/tainted-grail-sdk/RELEASE_PROCESS.md",
        "docs/tainted-grail-sdk/MAINTAINER_CHECKLIST.md", "docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md",
        "docs/tainted-grail-sdk/PRIVACY.md", "docs/tainted-grail-sdk/ACCESSIBILITY.md",
        "docs/tainted-grail-sdk/GLOSSARY.md",
    }
    for relative_path in sorted(required_files):
        if not (repo_root / relative_path).is_file():
            fail(f"Required public-project document is missing: {relative_path}")

    required_fragments = {
        "README.md": "Direct development on `main` is prohibited",
        "CONTRIBUTING.md": "pre-commit self-review",
        "SECURITY.md": "Report a vulnerability",
        "GOVERNANCE.md": "Evidence, claims, reviewed records, validation, and permission remain separate",
        "docs/tainted-grail-sdk/CODE_QUALITY.md": "Never use a display name as a database key",
        "docs/tainted-grail-sdk/REVIEW_AND_MERGE_POLICY.md": "Pending is not passing",
        "docs/tainted-grail-sdk/DATA_FORMATS.md": "RecipeIngredients",
        "docs/tainted-grail-sdk/CATALOG_GUIDE.md": "Promotion cannot create allowed usages",
        "docs/tainted-grail-sdk/GOVERNANCE_ENGINE_GUIDE.md": "Validation does not grant permission",
        "docs/tainted-grail-sdk/ITEM_RECIPE_EDITOR_GUIDE.md": "An item record is not a recipe record",
        ".github/PULL_REQUEST_TEMPLATE.md": "Author self-review",
        ".github/ISSUE_TEMPLATE/tg_sdk_feature.yml": "design review before implementation",
        ".github/CODEOWNERS": "/Gems/TaintedGrailModdingSDK/ @theb0yys",
    }
    for relative_path, fragment in required_fragments.items():
        path = repo_root / relative_path
        require_contains(path.read_text(encoding="utf-8"), fragment, path)

    issue_config_path = repo_root / ".github/ISSUE_TEMPLATE/config.yml"
    require_contains(issue_config_path.read_text(encoding="utf-8"), "blank_issues_enabled: false", issue_config_path)


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
        validate_catalog(gem_root)
        validate_governance_engine(gem_root)
        validate_economy_authoring(gem_root)
        validate_public_project(repo_root)
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail SDK foundation validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail SDK foundation validation passed.")
    print(
        "Validated: public governance, workspace and pack editing, source/evidence intake, canonical catalog, "
        "independent governance and proof-backed permissions, typed item/recipe profiles and joins, economy "
        "blockers and lanes, transactional persistence, six editor panes, tests, and editor-only runtime separation."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
