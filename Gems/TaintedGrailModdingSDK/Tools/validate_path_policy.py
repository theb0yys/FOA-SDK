#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the canonical workspace, pack, and shortcut path-policy contract."""

from __future__ import annotations

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


def validate_path_policy(repo_root: Path) -> None:
    gem_root = repo_root / "Gems/TaintedGrailModdingSDK"
    source_root = gem_root / "Code/Source"
    tests_root = gem_root / "Code/Tests"
    tools_root = gem_root / "Tools"
    tool_tests = tools_root / "tests"

    path_header = source_root / "PathPolicyService.h"
    path_source = source_root / "PathPolicyService.cpp"
    workspace_paths = source_root / "PathPolicyWorkspaceValidation.cpp"
    boundary_header = source_root / "FoundationPersistenceBoundary.h"
    boundary_source = source_root / "FoundationPersistenceBoundary.cpp"
    construction_source = source_root / "FoundationServiceConstruction.cpp"
    pack_manager_source = source_root / "PackManagerWidget.cpp"
    cpp_tests = tests_root / "PathPolicyServiceTests.cpp"
    cmake_path = gem_root / "Code/CMakeLists.txt"
    editor_manifest = gem_root / "Code/taintedgrailmoddingsdk_path_policy_editor_files.cmake"
    test_manifest = gem_root / "Code/taintedgrailmoddingsdk_path_policy_tests_files.cmake"
    developer_preview_command = tools_root / "developer_preview.py"
    python_policy = tools_root / "developer_preview_path_policy.py"
    shortcut = tools_root / "developer_preview_shortcut.py"
    entry = tools_root / "developer_preview_entry.py"
    python_policy_tests = tool_tests / "test_developer_preview_path_policy.py"
    shortcut_tests = tool_tests / "test_developer_preview_shortcut.py"
    entry_tests = tool_tests / "test_developer_preview_entry.py"
    workflow = repo_root / ".github/workflows/tainted-grail-editor-entry.yml"
    documentation = repo_root / "docs/tainted-grail-sdk/PATH_POLICY.md"

    require_fragments(
        path_header,
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
        path_source,
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
        workspace_paths,
        (
            "ValidateWorkspacePaths",
            "OutputPath",
            "StagingPath",
            "DeploymentPath",
            "DiagnosticsPath",
            "ExtractedDataPath",
            "ManagedAssembliesPath must remain inside",
            "PluginPath must remain inside",
        ),
    )
    require_fragments(
        boundary_header,
        (
            "FoundationWorkspacePersistenceBoundary",
            "FoundationPackPersistenceBoundary",
            "LoadCandidate",
            "PublishResolvedPath",
        ),
    )
    require_fragments(
        boundary_source,
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
    if "QDir().mkpath" in require_file(pack_manager_source):
        raise PathPolicyContractError(
            "Pack Manager must not create directories before canonical path validation."
        )
    require_fragments(
        construction_source,
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
        cpp_tests,
        (
            "RelativeWorkspaceRootUsesWorkspaceDocumentDirectory",
            "LexicalPrefixOutsideWorkspaceIsRejected",
            "SymlinkEscapeIsRejectedAfterCanonicalization",
            "WindowsCaseFoldUsesPathComponents",
            "PackManifestSuffixIsRequired",
        ),
    )
    require_fragments(
        cmake_path,
        (
            "taintedgrailmoddingsdk_path_policy_editor_files.cmake",
            "taintedgrailmoddingsdk_path_policy_tests_files.cmake",
            'TG_SDK_PREVIEW_TEMPLATE_ROOT="${CMAKE_CURRENT_LIST_DIR}/../Preview/Template"',
        ),
    )
    require_fragments(
        editor_manifest,
        (
            "Source/PathPolicyService.cpp",
            "Source/PathPolicyWorkspaceValidation.cpp",
            "Source/FoundationPersistenceBoundary.cpp",
            "Source/FoundationServiceConstruction.cpp",
        ),
    )
    require_fragments(
        test_manifest,
        (
            "Source/PathPolicyService.cpp",
            "Source/PathPolicyWorkspaceValidation.cpp",
            "Tests/PathPolicyServiceTests.cpp",
        ),
    )

    require_fragments(
        developer_preview_command,
        (
            "CMakeCache.txt",
            "CMAKE_HOME_DIRECTORY:INTERNAL",
            "configured_source != repo_root",
        ),
    )
    require_fragments(
        python_policy,
        (
            "APPROVED_BUILD_DIRECTORY",
            "TRUST_MODE_SOURCE_BUILD",
            "TRUST_MODE_DIAGNOSTIC_OVERRIDE",
            "def is_contained(",
            "def require_contained(",
            "developer_preview.validate_build_directory",
            "_require_cmake_source_binding",
            "CMAKE_HOME_DIRECTORY",
            "must resolve inside the repository checkout",
            "validate_source_editor_binary",
            "PE_MACHINE_AMD64",
            "PE_SUBSYSTEM_WINDOWS_GUI",
            "Diagnostic overrides must not replace",
        ),
    )
    require_fragments(
        shortcut,
        (
            "developer_preview_path_policy",
            "diagnostic_override",
            '"trust_mode": paths.trust_mode',
            "resolve_source_built_entry",
            "resolve_diagnostic_entry",
        ),
    )
    entry_text = require_fragments(
        entry,
        (
            "resolve_source_built_entry",
            "_require_manifest_matches_policy",
            "Diagnostic override shortcuts are not verified source-built entries",
            "allow_diagnostic_override",
            "Verified source-built clickable entry from repository-owned policy",
        ),
    )
    if "expected_target = Path(target_value)" in entry_text:
        raise PathPolicyContractError(
            "Source-built verification must not derive its expected target from the sidecar manifest."
        )

    require_fragments(
        python_policy_tests,
        (
            "test_component_containment_does_not_use_string_prefixes",
            "test_symlink_escape_is_rejected",
            "test_repository_project_symlink_escape_is_rejected",
            "test_shortcut_build_root_symlink_escape_is_rejected",
            "test_source_build_must_use_exact_approved_directory",
            "test_source_build_requires_explicit_cmake_source_binding",
            "test_source_build_rejects_arbitrary_editor_contents",
            "test_diagnostic_override_cannot_use_standard_output",
        ),
    )
    require_fragments(
        shortcut_tests,
        (
            "test_explicit_editor_requires_diagnostic_override",
            "test_diagnostic_override_is_labeled",
            "test_verify_detects_tampering",
        ),
    )
    require_fragments(
        entry_tests,
        (
            "test_self_consistent_manifest_with_unapproved_editor_is_rejected",
            "test_diagnostic_override_is_not_verified_by_default",
            "test_diagnostic_override_can_be_deliberately_inspected",
        ),
    )

    workflow_text = require_fragments(
        workflow,
        (
            "Test canonical path policy",
            "Validate canonical path and executable policy",
            "diagnostic-only Windows shortcut",
            "--diagnostic-override",
            "--allow-diagnostic-override",
        ),
    )
    if "CMAKE_HOME_DIRECTORY:INTERNAL=$env:GITHUB_WORKSPACE" in workflow_text:
        raise PathPolicyContractError(
            "CI must not manufacture source-build provenance for an empty Editor.exe."
        )

    require_fragments(
        documentation,
        (
            "One logical policy, two language bindings",
            "Compare containment by path components",
            "symlink or junction",
            "must not create directories before the persistence boundary validates it",
            "The sidecar records evidence",
            "Diagnostic override",
        ),
    )


def main() -> int:
    try:
        validate_path_policy(Path(__file__).resolve().parents[3])
    except (OSError, PathPolicyContractError) as exc:
        print(f"Tainted Grail canonical path-policy validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Grail canonical path-policy contract passed: workspace and pack persistence "
        "use canonical containment, and shortcut trust is repository-derived."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
