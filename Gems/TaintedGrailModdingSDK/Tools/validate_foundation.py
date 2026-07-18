#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the public TG SDK editor foundation and its decomposed build graph."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

from validate_core_framework_build_graph import validate_build_graph


GEM_NAME = "TaintedGrailModdingSDK"
GEM_PATH = f"Gems/{GEM_NAME}"


def fail(message: str) -> None:
    raise RuntimeError(message)


def load_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"Unable to read valid JSON from {path}: {exc}")
    if not isinstance(value, dict):
        fail(f"Expected a JSON object in {path}")
    return value


def require_contains(text: str, fragment: str, path: Path) -> None:
    if fragment not in text:
        fail(f"Missing required fragment {fragment!r} in {path}")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    if not path.is_file():
        fail(f"Required foundation file is missing: {path}")
    text = path.read_text(encoding="utf-8")
    for fragment in fragments:
        require_contains(text, fragment, path)
    return text


def validate_engine_registration(repo_root: Path) -> None:
    engine = load_json(repo_root / "engine.json")
    external = engine.get("external_subdirectories")
    if not isinstance(external, list) or external.count(GEM_PATH) != 1:
        fail(f"{GEM_PATH} must appear exactly once in engine.json external_subdirectories")
    if GEM_NAME in engine.get("gem_names", []):
        fail(f"{GEM_NAME} must not be an engine-wide default Gem")


def validate_gem_metadata(gem_root: Path) -> None:
    gem = load_json(gem_root / "gem.json")
    for key, expected in {
        "gem_name": GEM_NAME,
        "display_name": "Tainted Grail Modding SDK",
        "type": "Tool",
    }.items():
        if gem.get(key) != expected:
            fail(f"gem.json {key} must be {expected!r}")
    if not isinstance(gem.get("version"), str) or not re.fullmatch(r"\d+\.\d+\.\d+", gem["version"]):
        fail("gem.json version must use MAJOR.MINOR.PATCH")
    if gem.get("dependencies") != []:
        fail("The editor foundation must not add Gem dependencies yet")


def validate_cmake(gem_root: Path) -> None:
    path = gem_root / "Code/CMakeLists.txt"
    text = require_fragments(
        path,
        (
            "if(NOT PAL_TRAIT_BUILD_HOST_TOOLS)",
            "NAME ${gem_name}.Core.Static STATIC",
            "NAME ${gem_name}.Framework.Static STATIC",
            "NAME ${gem_name}.Editor GEM_MODULE",
            "taintedgrailmoddingsdk_core_files.cmake",
            "taintedgrailmoddingsdk_framework_files.cmake",
            "taintedgrailmoddingsdk_editor_files.cmake",
            "Gem::${gem_name}.Core.Static",
            "Gem::${gem_name}.Framework.Static",
            "AZ::AzToolsFramework",
            "NAME ${gem_name}.Catalog.Tests",
            "ly_add_googletest",
            "ly_create_alias(NAME ${gem_name}.Tools",
            "ly_create_alias(NAME ${gem_name}.Builders",
            "VARIANTS Tools Builders",
        ),
    )
    for fragment in ("${gem_name}.Clients", "${gem_name}.Servers", "${gem_name}.Unified"):
        if fragment in text:
            fail(f"Editor-only foundation must not expose runtime alias {fragment}")


def read_sources(gem_root: Path) -> tuple[Path, str]:
    source_root = gem_root / "Code/Source"
    files = sorted(source_root.glob("*.[ch]pp")) + sorted(source_root.glob("*.h"))
    return source_root, "\n".join(path.read_text(encoding="utf-8") for path in files)


def validate_editor_foundation(gem_root: Path) -> None:
    _, combined = read_sources(gem_root)
    required = (
        "WorkspaceModel", "GameProfile", "m_runtimeTarget", "m_outputPath", "m_stagingPath",
        "m_deploymentPath", "class WorkspacePersistenceService", "OpenWorkspace", "SaveWorkspaceAs",
        "PackManifest", "HasStableIdentity", "UsesSupportedSchema", "m_requiredCoreVersion",
        "m_requiredAdapterVersion", "m_requiredMods", "m_contentDefinitionPaths", "m_assetPaths",
        "m_localisationPaths", "m_buildConfiguration", "m_releaseChannel", "class PackPersistenceService",
        "class PackManagerWidget", "SaveActivePack", "LoadPack", "RegisterViewPane<PackManagerWidget>",
        "class SourceEvidenceRegistry", "class CatalogDatabase", "class FoundationValidationService",
        "class FoundationService", "FoundationNotificationBus", "class FoundationStatusWidget",
        "RegisterViewPane<FoundationStatusWidget>", "LoadObjectFromFile", "TaintedGrailModdingSDKService",
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


def validate_catalog_and_governance(gem_root: Path) -> None:
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
        "CatalogGovernanceEvent", "CatalogGovernanceRequest", "CatalogValidationRequest",
        "m_stalenessState", "m_governanceHistory", "GetGovernanceHistory",
        "class CatalogGovernanceService", "class CatalogGovernanceBlockerService",
        "class CatalogGovernanceWidget", "ApplyCatalogGovernanceDecision",
        "ApplyCatalogValidationDecision", "RegisterViewPane<CatalogGovernanceWidget>",
        "Allowed usage requires at least one validated proof event",
        "Permission never follows automatically from evidence or validation",
        "Usage permission requires a validated, current, unresolved-free, non-superseded subject",
        "enum class CatalogSubjectKind", "enum class GovernanceAxis", "enum class ResearchStage",
        "enum class ConfidenceLevel", "enum class OperationalRisk", "enum class ValidationState",
        "enum class StalenessState", "enum class PermissionDecision", "GovernedSubjectState",
        "class CatalogTransactionService", "CatalogGovernanceApplyResult", "CatalogValidationApplyResult",
    ):
        if fragment not in combined:
            fail(f"Catalog/governance foundation is missing {fragment!r}")

    promotion_path = source_root / "CatalogPromotionService.cpp"
    promotion = promotion_path.read_text(encoding="utf-8")
    if "record.m_allowedUsages" in promotion:
        fail("Evidence promotion must not assign allowed usages")
    require_contains(promotion, 'record.m_stalenessState = "unknown"', promotion_path)

    transaction_path = source_root / "CatalogTransactionService.cpp"
    transaction = transaction_path.read_text(encoding="utf-8")
    save_position = transaction.find("save(document, workspace.m_rootPath)")
    result_position = transaction.find("result.m_catalog = candidate")
    if save_position < 0 or result_position < 0 or save_position > result_position:
        fail("Catalog transaction service must save before returning a publishable candidate")

    foundation_path = source_root / "FoundationCatalogService.cpp"
    foundation = foundation_path.read_text(encoding="utf-8")
    commit_position = foundation.find("m_catalogTransaction.Commit(")
    publish_position = foundation.find("m_catalog = AZStd::move(committed.m_catalog)")
    if commit_position < 0 or publish_position < 0 or commit_position > publish_position:
        fail("Foundation catalog service must complete the transaction before publishing catalog state")

    database = (source_root / "CatalogDatabase.cpp").read_text(encoding="utf-8")
    if "record.m_displayName ==" in database or "record.m_displayName !=" in database:
        fail("Canonical identity must not merge or reject records based on display name")


def validate_economy_authoring(gem_root: Path) -> None:
    source_root, combined = read_sources(gem_root)
    for fragment in (
        "EconomyItemProfile", "EconomyRecipeProfile", "EconomyRecipeIngredient", "EconomyRecipeOutput",
        "m_economyItems", "m_economyRecipes", "m_recipeIngredients", "m_recipeOutputs",
        "class EconomyAuthoringService", "class EconomyBlockerService", "class ItemRecipeEditorWidget",
        "UpsertEconomyItemProfile", "UpsertEconomyRecipeProfile", "UpsertEconomyRecipeIngredient",
        "UpsertEconomyRecipeOutput", "RegisterViewPane<ItemRecipeEditorWidget>",
        "Tainted Grail Item and Recipe Editor", "existing_item_grant", "existing_recipe_learn",
        "runtime_recipe_append", "custom_item_registration", "custom_recipe_registration",
        "asset_localisation_injection", "vendor_or_loot_injection", "quest_or_contract_reward_injection",
        "no_unvalidated_runtime_use", "Recipe ingredients require exactly one item record ID or unresolved item subject ref",
        "Recipe outputs require exactly one item record ID or unresolved item subject ref",
        "Recipe persistence mode must be unknown, native_template, runtime_append, or custom_template",
        "The canonical item record does not exist", "Economy evidence belongs to a different catalog subject",
    ):
        if fragment not in combined:
            fail(f"Item/recipe editor is missing {fragment!r}")

    widget = (source_root / "ItemRecipeEditorWidget.cpp").read_text(encoding="utf-8")
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
        "docs/tainted-grail-sdk/GLOSSARY.md", "docs/tainted-grail-sdk/CORE_FRAMEWORK_BUILD_GRAPH.md",
    }
    for relative_path in sorted(required_files):
        if not (repo_root / relative_path).is_file():
            fail(f"Required public-project document is missing: {relative_path}")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    gem_root = repo_root / GEM_PATH
    try:
        validate_engine_registration(repo_root)
        validate_gem_metadata(gem_root)
        validate_cmake(gem_root)
        validate_build_graph(repo_root)
        validate_editor_foundation(gem_root)
        validate_source_intake(gem_root)
        validate_catalog_and_governance(gem_root)
        validate_economy_authoring(gem_root)
        validate_public_project(repo_root)
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail SDK foundation validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail SDK foundation and decomposed build graph validation passed.")
    print(
        "Validated: Core/Framework/Editor ownership, public governance, workspace and pack editing, "
        "source/evidence intake, canonical catalog, typed atomic governance, item/recipe authoring, "
        "linked tests, and runtime separation."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
