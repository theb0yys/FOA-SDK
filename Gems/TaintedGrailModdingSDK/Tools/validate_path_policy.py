#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate canonical workspace paths and the external-engine Editor-entry contract."""

from __future__ import annotations

import re
import sys
from pathlib import Path


class PathPolicyContractError(RuntimeError):
    """Raised when the reviewed path-policy contract is incomplete."""


def require_file(path: Path) -> str:
    if not path.is_file():
        raise PathPolicyContractError(f"Required path-policy file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require_fragments(path: Path, fragments: tuple[str, ...]) -> str:
    text = require_file(path)
    for fragment in fragments:
        if fragment not in text:
            raise PathPolicyContractError(f"{path} is missing required fragment {fragment!r}.")
    return text


def manifest_entries(path: Path) -> set[str]:
    return set(re.findall(r"^\s+((?:Source|Tests)/[^\s\)]+)\s*$", require_file(path), re.MULTILINE))


def validate_path_policy(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    code_root = gem_root / "Code"
    source_root = code_root / "Source"
    tests_root = code_root / "Tests"
    tools_root = gem_root / "Tools"
    tool_tests = tools_root / "tests"

    require_fragments(
        source_root / "PathPolicyService.h",
        (
            "class PathPolicyService",
            "ResolveWorkspaceDocumentPath",
            "ResolveWorkspaceRoot",
            "ValidateWorkspacePaths",
            "ResolvePackManifestPath",
            "IsCanonicalPathContained",
        ),
    )
    require_fragments(
        source_root / "PathPolicyService.cpp",
        (
            "std::filesystem",
            "Filesystem::canonical",
            "Filesystem::weakly_canonical",
            "PlatformPathsAreCaseInsensitive",
            ".tgworkspace.json",
            ".tgpack.json",
            "canonical workspace root",
        ),
    )
    require_fragments(
        source_root / "PathPolicyWorkspaceValidation.cpp",
        (
            "ValidateWorkspacePaths",
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
        source_root / "FoundationPersistenceBoundary.h",
        (
            "FoundationWorkspacePersistenceBoundary",
            "FoundationPackPersistenceBoundary",
            "LoadCandidate",
            "PublishResolvedPath",
        ),
    )
    require_fragments(
        source_root / "FoundationPersistenceBoundary.cpp",
        (
            "ResolveWorkspaceDocumentPath",
            "ResolvePackManifestPath",
            "EnsureValidatedParentDirectory",
            "std::filesystem::create_directories",
            "LoadCandidate",
            "m_persistence.Save",
            "m_persistence.Load",
        ),
    )
    if "QDir().mkpath" in require_file(source_root / "PackManagerWidget.cpp"):
        raise PathPolicyContractError(
            "Pack Manager must not create directories before canonical path validation."
        )
    require_fragments(
        source_root / "FoundationServiceConstruction.cpp",
        (
            "FoundationService::FoundationService()",
            "m_workspacePersistence(m_pathPolicy)",
            "m_packPersistence(",
            "return m_workspaceFilePath;",
            "LoadCandidate",
            "ValidateWorkspacePaths",
        ),
    )
    require_fragments(
        tests_root / "PathPolicyServiceTests.cpp",
        (
            "RelativeWorkspaceRootUsesWorkspaceDocumentDirectory",
            "LexicalPrefixOutsideWorkspaceIsRejected",
            "SymlinkEscapeIsRejectedAfterCanonicalization",
            "WindowsCaseFoldUsesPathComponents",
            "PackManifestSuffixIsRequired",
            "EveryConfiguredProfilePathCanValidate",
            "InactiveDiagnosticsEscapeIsRejected",
            "InactiveManagedAssembliesEscapeFromInstallIsRejected",
        ),
    )

    cmake = require_fragments(
        code_root / "CMakeLists.txt",
        (
            "NAME ${gem_name}.Framework.Static STATIC",
            "taintedgrailmoddingsdk_framework_files.cmake",
            "taintedgrailmoddingsdk_path_policy_tests_files.cmake",
            "Gem::${gem_name}.Framework.Static",
            'TG_SDK_PREVIEW_TEMPLATE_ROOT="${CMAKE_CURRENT_LIST_DIR}/../Preview/Template"',
        ),
    )
    if "taintedgrailmoddingsdk_path_policy_editor_files.cmake" in cmake:
        raise PathPolicyContractError("Path-policy sources must not remain in an Editor-only manifest.")

    framework_entries = manifest_entries(code_root / "taintedgrailmoddingsdk_framework_files.cmake")
    required_framework = {
        "Source/PathPolicyService.cpp",
        "Source/PathPolicyWorkspaceValidation.cpp",
        "Source/FoundationPersistenceBoundary.cpp",
        "Source/FoundationServiceConstruction.cpp",
    }
    if not required_framework.issubset(framework_entries):
        raise PathPolicyContractError(
            "Framework manifest is missing path-policy ownership: "
            + ", ".join(sorted(required_framework - framework_entries))
        )

    test_entries = manifest_entries(code_root / "taintedgrailmoddingsdk_path_policy_tests_files.cmake")
    expected_tests = {
        "Tests/FoundationServiceWorkspaceLoadTests.cpp",
        "Tests/PathPolicyServiceTests.cpp",
        "Tests/WorkspaceSchemaServiceTests.cpp",
    }
    if test_entries != expected_tests:
        raise PathPolicyContractError(
            f"Path-policy test manifest mismatch: expected {sorted(expected_tests)}, found {sorted(test_entries)}"
        )

    require_fragments(
        tools_root / "developer_preview.py",
        (
            "CMakeCache.txt",
            "CMAKE_HOME_DIRECTORY:INTERNAL",
            "configured_source != engine_root",
            "required_project = (product_root / PREVIEW_PROJECT_DIRECTORY)",
            "The build directory must not be inside the FOA-SDK checkout",
            "The build directory must not be inside the O3DE checkout",
        ),
    )
    require_fragments(
        tools_root / "developer_preview_path_policy.py",
        (
            "APPROVED_BUILD_DIRECTORY",
            "FOA_BUILD_ROOT",
            "TRUST_MODE_SOURCE_BUILD",
            "TRUST_MODE_DIAGNOSTIC_OVERRIDE",
            "def is_contained(",
            "def require_contained(",
            "developer_preview.load_engine_lock",
            "developer_preview.validate_engine_root",
            "developer_preview.validate_engine_pin",
            "developer_preview.validate_build_directory",
            "_require_cmake_source_binding",
            "CMAKE_HOME_DIRECTORY",
            "configured_projects",
            "required_project",
            "must resolve inside the FOA-SDK product checkout",
            "FOA-SDK product_root and O3DE engine_root must be separate checkouts",
            "approved external Developer Preview build directory",
            "validate_source_editor_binary",
            "PE_MACHINE_AMD64",
            "PE_SUBSYSTEM_WINDOWS_GUI",
            "Diagnostic overrides must not replace",
        ),
    )
    require_fragments(
        tools_root / "developer_preview_shortcut.py",
        (
            "developer_preview_path_policy",
            "diagnostic_override",
            '"trust_mode": paths.trust_mode',
            "resolve_source_built_entry",
            "resolve_diagnostic_entry",
            "TG_ENGINE_PATH",
        ),
    )
    entry_text = require_fragments(
        tools_root / "developer_preview_entry.py",
        (
            "resolve_source_built_entry",
            "_require_manifest_matches_policy",
            "Diagnostic override shortcuts are not verified source-built entries",
            "allow_diagnostic_override",
            "Verified source-built clickable entry from repository-owned policy",
            '"--engine-path"',
        ),
    )
    if "expected_target = Path(target_value)" in entry_text:
        raise PathPolicyContractError(
            "Source-built verification must not derive its expected target from the sidecar manifest."
        )

    require_fragments(
        tool_tests / "test_developer_preview_path_policy.py",
        (
            "test_component_containment_does_not_use_string_prefixes",
            "test_product_project_symlink_escape_is_rejected",
            "test_source_build_must_use_exact_external_build_directory",
            "test_source_build_uses_separate_product_engine_and_build_roots",
            "test_source_build_rejects_cache_bound_to_product_as_engine",
            "test_source_build_requires_foa_project_binding",
            "test_source_build_rejects_arbitrary_editor_contents",
            "test_diagnostic_override_keeps_external_engine_identity",
            "test_shortcut_outputs_are_outside_product_checkout",
            "test_shortcut_output_cannot_escape_external_build_root",
        ),
    )
    require_fragments(
        tool_tests / "test_developer_preview_shortcut.py",
        (
            "test_explicit_editor_requires_diagnostic_override",
            "test_diagnostic_override_is_labeled",
            "test_verify_detects_tampering",
        ),
    )
    require_fragments(
        tool_tests / "test_developer_preview_entry.py",
        (
            "test_self_consistent_manifest_with_unapproved_editor_is_rejected",
            "test_diagnostic_override_is_not_verified_by_default",
            "test_diagnostic_override_can_be_deliberately_inspected",
        ),
    )
    require_fragments(
        repo_root / ".github/workflows/tainted-grail-editor-entry.yml",
        (
            "Test canonical path policy",
            "Validate canonical path and executable policy",
            "exact external O3DE policy surface",
            "FOA_BUILD_ROOT: ${{ runner.temp }}/foa-build",
            "diagnostic-only Windows shortcut",
            "--diagnostic-override",
            "--allow-diagnostic-override",
        ),
    )
    require_fragments(
        repo_root / "docs/tainted-grail-sdk/PATH_POLICY.md",
        (
            "Resolve absolute canonical paths",
            "Compare containment by path components",
            "filesystem links",
            "Diagnostic override",
            "source-built",
        ),
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate_path_policy(repo_root)
    except (OSError, PathPolicyContractError) as exc:
        print(f"Tainted Grail path-policy validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail path policy passed: Framework-owned canonical persistence paths and "
        "separate product, engine, build, and trusted Editor-entry roots are enforced."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
