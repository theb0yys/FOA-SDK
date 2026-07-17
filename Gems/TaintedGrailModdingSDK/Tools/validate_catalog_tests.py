#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate that catalog and governance tests remain registered and cover safety rules."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_contains(text: str, fragment: str, path: Path) -> None:
    if fragment not in text:
        fail(f"Missing required fragment {fragment!r} in {path}")


def main() -> int:
    gem_root = Path(__file__).resolve().parents[1]
    code_root = gem_root / "Code"
    cmake_path = code_root / "CMakeLists.txt"
    manifest_path = code_root / "taintedgrailmoddingsdk_catalog_tests_files.cmake"
    database_tests_path = code_root / "Tests" / "CatalogDatabaseTests.cpp"
    governance_tests_path = code_root / "Tests" / "CatalogGovernanceServiceTests.cpp"

    try:
        for path in (cmake_path, manifest_path, database_tests_path, governance_tests_path):
            if not path.is_file():
                fail(f"Required catalog/governance test file is missing: {path}")

        cmake = cmake_path.read_text(encoding="utf-8")
        for fragment in (
            "PAL_TRAIT_BUILD_TESTS_SUPPORTED",
            "PAL_TRAIT_TEST_GOOGLE_TEST_SUPPORTED",
            "NAME ${gem_name}.Catalog.Tests",
            "taintedgrailmoddingsdk_catalog_tests_files.cmake",
            "AZ::AzTest",
            "AZ::AzToolsFramework",
            "ly_add_googletest",
        ):
            require_contains(cmake, fragment, cmake_path)

        manifest = manifest_path.read_text(encoding="utf-8")
        entries = set(re.findall(r"^\s+([^\s\)]+)\s*$", manifest, re.MULTILINE))
        expected = {
            "Source/CatalogDatabase.cpp",
            "Source/CatalogDatabase.h",
            "Source/CatalogGovernanceService.cpp",
            "Source/CatalogGovernanceService.h",
            "Source/FoundationModels.cpp",
            "Source/FoundationModels.h",
            "Source/SourceEvidenceRegistry.cpp",
            "Source/SourceEvidenceRegistry.h",
            "Tests/CatalogDatabaseTests.cpp",
            "Tests/CatalogGovernanceServiceTests.cpp",
        }
        if entries != expected:
            fail(f"Catalog test manifest mismatch: expected {sorted(expected)}, found {sorted(entries)}")

        database_tests = database_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "SameDisplayNameDoesNotMergeDistinctRecordIds",
            "DuplicateExactNativeReferenceIsRejected",
            "SyntheticRecordRequiresOwningPack",
            "QueryFiltersEvidencePermissionAndBlockedState",
            "RelationshipRequiresKnownRecordsAndEvidence",
        ):
            require_contains(database_tests, fragment, database_tests_path)

        governance_tests = governance_tests_path.read_text(encoding="utf-8")
        for fragment in (
            "ValidationDoesNotGrantUsagePermission",
            "PermissionRequiresCurrentStateAndValidatedProof",
            "StaleDecisionRevokesAllowedUsage",
            "SupersessionRevokesUsageAndRecordsReplacement",
            "RelationshipValidationAndPermissionUseSameProofGates",
            'm_axis = "permission"',
            'm_axis = "staleness"',
            'm_axis = "supersession"',
            'm_subjectKind = "relationship"',
        ):
            require_contains(governance_tests, fragment, governance_tests_path)
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail catalog/governance test validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail catalog and governance test contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
