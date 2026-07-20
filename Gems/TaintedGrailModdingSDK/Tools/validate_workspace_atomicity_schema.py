#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the atomic workspace transition and durable schema-1 contract."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


class WorkspaceContractError(RuntimeError):
    """Raised when the reviewed workspace contract is incomplete."""


def require_file(path: Path) -> str:
    if not path.is_file():
        raise WorkspaceContractError(f"Required workspace contract file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    text = require_file(path)
    for fragment in fragments:
        if fragment not in text:
            raise WorkspaceContractError(f"{path} is missing required fragment {fragment!r}.")
    return text


def manifest_entries(path: Path) -> set[str]:
    return set(re.findall(r"^\s+((?:Source|Tests)/[^\s\)]+)\s*$", require_file(path), re.MULTILINE))


def validate_workspace_contract(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"

    schema_header = source_root / "WorkspaceSchemaService.h"
    schema_source = source_root / "WorkspaceSchemaService.cpp"
    persistence = source_root / "WorkspacePersistenceService.cpp"
    candidate_header = source_root / "FoundationWorkspaceLoadService.h"
    candidate_source = source_root / "FoundationWorkspaceLoadService.cpp"
    service_header = source_root / "FoundationService.h"
    service_source = source_root / "FoundationService.cpp"
    construction = source_root / "FoundationServiceConstruction.cpp"
    boundary = source_root / "FoundationPersistenceBoundary.cpp"
    path_validation = source_root / "PathPolicyWorkspaceValidation.cpp"
    integration_tests = tests_root / "FoundationServiceWorkspaceLoadTests.cpp"
    path_tests = tests_root / "PathPolicyServiceTests.cpp"
    schema_tests = tests_root / "WorkspaceSchemaServiceTests.cpp"
    framework_manifest = code_root / "taintedgrailmoddingsdk_framework_files.cmake"
    test_manifest = code_root / "taintedgrailmoddingsdk_path_policy_tests_files.cmake"
    cmake = code_root / "CMakeLists.txt"
    fixture = gem_root / "Preview/Template/preview.tgworkspace.json"
    design = repo_root / "docs/tainted-grail-sdk/WORKSPACE_ATOMICITY_AND_SCHEMA.md"

    require_fragments(
        schema_header,
        ("LegacySchemaVersion = 0", "CurrentSchemaVersion = 1", "MigrateAndValidate", "Validate(const WorkspaceModel& workspace)"),
    )
    require_fragments(
        schema_source,
        (
            "WorkspaceId must be a lowercase namespaced stable ID",
            "duplicate ProfileId",
            "ActiveGameProfileId does not bind",
            "Mono profiles require BepInExVersion and PluginPath",
            "Legacy workspace schema 0 cannot be migrated safely",
        ),
    )
    persistence_text = require_fragments(
        persistence,
        (
            'QStringLiteral("SchemaVersion")',
            "WorkspaceSchemaService::CurrentSchemaVersion",
            "WorkspaceSchemaService::LegacySchemaVersion",
            "DetectSchemaVersion",
            "MigrateAndValidate",
            "QSaveFile",
            "schemaVersion != WorkspaceSchemaService::LegacySchemaVersion",
            "Legacy workspace schema 0 cannot be migrated safely",
        ),
    )
    if "SaveObjectToFile(workspace" in persistence_text:
        raise WorkspaceContractError("Workspace persistence must emit the explicit durable schema-1 document.")
    detection_position = persistence_text.find("auto detected = DetectSchemaVersion(object)")
    envelope_position = persistence_text.find('if (object.contains(QStringLiteral("Type")))')
    if min(detection_position, envelope_position) < 0 or detection_position > envelope_position:
        raise WorkspaceContractError(
            "Workspace schema must be detected and rejected before selecting the legacy envelope parser."
        )

    require_fragments(
        candidate_header,
        (
            "struct FoundationWorkspaceLoadCandidate",
            "m_workspaceFilePath",
            "m_workspaceRootPath",
            "m_activeProfile",
            "m_sourceRegistry",
            "m_importIssues",
            "m_catalog",
            "m_catalogFilePath",
        ),
    )
    require_fragments(
        candidate_source,
        (
            "BuildCandidate",
            "ValidateSourceBinding",
            "ValidateEvidenceBinding",
            "RejectLoadErrors",
            "SourceEvidenceRegistry registry",
            "CatalogDatabase catalog",
            "ReplaceFromBoundDocument",
            "activeProfileCopy",
        ),
    )
    require_fragments(
        path_validation,
        (
            "ValidateWorkspacePaths",
            "ValidateProfilePaths",
            "for (const GameProfile& profile : workspace.m_gameProfiles)",
            "OutputPath",
            "StagingPath",
            "DeploymentPath",
            "DiagnosticsPath",
            "ExtractedDataPath",
            "ManagedAssembliesPath must be a dedicated directory inside the canonical InstallPath",
            "PluginPath must be a dedicated directory inside InstallPath for Mono profiles",
        ),
    )
    require_fragments(
        construction,
        ("LoadCandidate", "ValidateWorkspacePaths", "m_workspaceFilePath;", "FoundationWorkspaceLoadService"),
    )
    require_fragments(
        boundary,
        ("LoadCandidate", "PublishResolvedPath", "resolvedPath = resolved.TakeValue()"),
    )
    require_fragments(
        service_header,
        ("FoundationWorkspaceLoadService m_workspaceLoadService", "AZStd::string m_workspaceRootPath", "GetWorkspaceRootPath"),
    )

    load_match = re.search(
        r"bool FoundationService::LoadWorkspace\([^\{]+\{(?P<body>.*?)\n    \}",
        require_file(service_source),
        re.DOTALL,
    )
    if not load_match:
        raise WorkspaceContractError("Unable to locate FoundationService::LoadWorkspace.")
    body = load_match.group("body")
    candidate_position = body.find("BuildCandidate(filePath)")
    publish_position = body.find("m_workspace = AZStd::move(candidate.m_workspace)")
    snapshot_position = body.find("RefreshSnapshot()")
    if min(candidate_position, publish_position, snapshot_position) < 0:
        raise WorkspaceContractError("Workspace load is missing candidate, publication, or snapshot steps.")
    if not candidate_position < publish_position < snapshot_position:
        raise WorkspaceContractError("Workspace candidate must complete before any live publication.")
    for forbidden in ("ReloadSourceEvidence", "ReloadCatalog", "m_workspace = result.TakeValue"):
        if forbidden in body:
            raise WorkspaceContractError(
                f"Workspace load still contains pre-candidate live mutation path {forbidden!r}."
            )

    require_fragments(
        integration_tests,
        (
            "SuccessfulCandidatePublishesEveryWorkspaceObject",
            "WorkspaceDocumentFailurePreservesAllLiveState",
            "ActiveProfileFailurePreservesAllLiveState",
            "WorkspacePathFailurePreservesAllLiveState",
            "SourceLoadFailurePreservesAllLiveState",
            "ImportIssueFailurePreservesAllLiveState",
            "RegistryBindingFailurePreservesAllLiveState",
            "EvidenceBindingFailurePreservesAllLiveState",
            "CatalogLoadFailurePreservesAllLiveState",
            "CatalogBindingFailurePreservesAllLiveState",
            "CatalogValidationFailurePreservesAllLiveState",
            "StateSignature",
            "GetPacks().size()",
            "GetSnapshot().m_workspaceFilePath",
        ),
    )
    require_fragments(
        path_tests,
        (
            "EveryConfiguredProfilePathCanValidate",
            "WorkspaceOwnedPathEscapeIsRejected",
            "ActiveManagedAssembliesEscapeFromInstallIsRejected",
            "InactiveDiagnosticsEscapeIsRejected",
            "InactiveManagedAssembliesEscapeFromInstallIsRejected",
            "MonoPluginEscapeFromInstallIsRejected",
        ),
    )
    require_fragments(
        schema_tests,
        (
            "StableWorkspaceIdIsRequired",
            "ProfileIdsMustBeUnique",
            "ActiveProfileMustBindExactly",
            "UnknownSchemaVersionIsRejected",
            "UnknownSchemaVersionCannotHideBehindLegacyEnvelope",
            "MalformedLegacyWorkspaceHasMigrationError",
            "UnsafeLegacyWorkspaceIsClearlyRejected",
            "SchemaOneRoundTripIsStable",
            "PreviewFixtureLegacyShapeMigratesAndRoundTrips",
        ),
    )

    framework_entries = manifest_entries(framework_manifest)
    required_framework = {
        "Source/FoundationWorkspaceLoadService.cpp",
        "Source/PathPolicyWorkspaceValidation.cpp",
        "Source/WorkspaceSchemaService.cpp",
        "Source/WorkspacePersistenceService.cpp",
        "Source/FoundationPersistenceBoundary.cpp",
    }
    if not required_framework.issubset(framework_entries):
        raise WorkspaceContractError(
            "Framework manifest is missing workspace ownership: "
            + ", ".join(sorted(required_framework - framework_entries))
        )

    expected_tests = {
        "Tests/FoundationServiceWorkspaceLoadTests.cpp",
        "Tests/PathPolicyServiceTests.cpp",
        "Tests/WorkspaceSchemaServiceTests.cpp",
    }
    test_entries = manifest_entries(test_manifest)
    if test_entries != expected_tests:
        raise WorkspaceContractError(
            f"Workspace test manifest mismatch: expected {sorted(expected_tests)}, found {sorted(test_entries)}"
        )
    require_fragments(
        cmake,
        (
            "NAME ${gem_name}.Framework.Static STATIC",
            "Gem::${gem_name}.Framework.Static",
            "Tests/WorkspaceSchemaServiceTests.cpp",
            "TG_SDK_PREVIEW_TEMPLATE_ROOT",
        ),
    )

    fixture_document = json.loads(require_file(fixture))
    if fixture_document.get("SchemaVersion") != 1:
        raise WorkspaceContractError("Preview workspace fixture must use durable schema version 1.")

    require_fragments(
        design,
        (
            "legacy schema 0",
            "FoundationWorkspaceLoadCandidate",
            "publishes only after every stage succeeds",
            "leaves all previous objects, paths, packs and the previous snapshot unchanged",
        ),
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_workspace_contract(repo_root)
    except (OSError, json.JSONDecodeError, WorkspaceContractError) as exc:
        print(f"Tainted Grail workspace atomicity/schema validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail workspace contract passed: Framework-owned schema migration, all-profile path "
        "validation, candidate validation, atomic publication, and failure preservation are wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
