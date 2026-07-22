#!/usr/bin/env python3
# Copyright (c) Contributors to the Open 3D Engine Project.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Require the complete Core-only Gate 5 canonical-interchange acceptance target."""
from __future__ import annotations

import re
import sys
from pathlib import Path


class CanonicalInterchangeCompiledTestError(RuntimeError):
    pass


def require(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        if fragment not in text:
            raise CanonicalInterchangeCompiledTestError(
                f"{label} is missing {fragment!r}."
            )


def validate(repo_root: Path) -> None:
    code = repo_root / "Gems/TaintedGrailModdingSDK/Code"
    cmake = (code / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(
        r"ly_add_target\(\s*NAME \$\{gem_name\}\.CanonicalInterchange\.Tests.*?\n\s*\)",
        cmake,
        flags=re.DOTALL,
    )
    if match is None:
        raise CanonicalInterchangeCompiledTestError(
            "Dedicated canonical-interchange compiled target is missing."
        )
    target = match.group(0)
    require(
        target,
        (
            "taintedgrailmoddingsdk_canonical_interchange_tests_files.cmake",
            "Gem::${gem_name}.Core.Static",
            "AZ::AzTest",
            "NO_UNITY",
        ),
        "Canonical target",
    )
    for forbidden in ("Framework.Static", "AzToolsFramework", "Qt", "Editor"):
        if forbidden in target:
            raise CanonicalInterchangeCompiledTestError(
                f"Canonical target has forbidden dependency {forbidden!r}."
            )

    manifest = (
        code / "taintedgrailmoddingsdk_canonical_interchange_tests_files.cmake"
    ).read_text(encoding="utf-8")
    required_files = (
        "Tests/Main.cpp",
        "Tests/CanonicalInterchangeContractTests.cpp",
        "Tests/CanonicalInterchangeAcceptanceFixtures.h",
        "Tests/CanonicalInterchangeTypesAcceptanceTests.cpp",
        "Tests/CanonicalInterchangeCanonicalAcceptanceTests.cpp",
        "Tests/CanonicalInterchangeValidationAcceptanceTests.cpp",
        "Tests/CanonicalInterchangeMigrationAcceptanceTests.cpp",
    )
    require(manifest, required_files, "Canonical test manifest")

    required_markers = {
        "CanonicalInterchangeContractTests.cpp": (
            "FixedBasisUsesAcceptedHyphenatedTokens",
            "ExactVersionGrammarFailsClosed",
            "FiniteNumbersUseShortestRoundTripBytes",
        ),
        "CanonicalInterchangeTypesAcceptanceTests.cpp": (
            "SemanticIdentifiersUseClosedGrammar",
            "VersionAndDigestTokensFailClosed",
            "PackagePathsRejectTraversalAliasesAndPlatformForms",
            "ClosedEnumTokensRoundTrip",
        ),
        "CanonicalInterchangeCanonicalAcceptanceTests.cpp": (
            "Utf8EscapingAndKnownDigestAreExact",
            "FiniteNumbersAndNegativeZeroAreCanonical",
            "SerializationDoesNotMutateCallerCollections",
            "PublicDocumentsExampleRoundTripsThroughCpp",
            "PublicAssetExampleRoundTripsThroughCpp",
        ),
        "CanonicalInterchangeValidationAcceptanceTests.cpp": (
            "StructuralFailureLeavesCallerOutputUntouched",
            "ForbiddenAuthorityFieldIsRejected",
            "MissingAndCyclicDependenciesAreDeterministic",
            "DeclaredRevisionDriftIsRejected",
            "IssueOrderingAndDuplicateCollapseAreStable",
        ),
        "CanonicalInterchangeMigrationAcceptanceTests.cpp": (
            "AlreadyCurrentPreservesExactSourceBytes",
            "InvalidSourceReturnsNoCandidate",
            "VersionDispatchFailsClosed",
            "ChangedIdentityCandidateIsSemanticDrift",
        ),
    }
    for filename, markers in required_markers.items():
        text = (code / "Tests" / filename).read_text(encoding="utf-8")
        require(text, markers, filename)

    workflow = (
        repo_root / ".github/workflows/tainted-grail-sdk-pr-validation.yml"
    ).read_text(encoding="utf-8")
    require(
        workflow,
        (
            "canonical-interchange-compiled:",
            "github.event.pull_request.head.sha",
            "68683f23fb747380d3efa2424bd5f30242e9c5a2",
            "-DO3DE_FETCHCONTENT_FORCE_GIT=ON",
            "TaintedGrailModdingSDK.CanonicalInterchange.Tests",
            "--no-tests=error",
            "gate5-canonical-interchange-compiled-${{ github.run_id }}",
            "operational_authority = $false",
        ),
        "Governed compiled-test workflow",
    )


def main() -> int:
    try:
        validate(Path(__file__).resolve().parents[3])
    except (OSError, CanonicalInterchangeCompiledTestError) as exc:
        print(
            f"Canonical interchange compiled-test validation failed: {exc}",
            file=sys.stderr,
        )
        return 1
    print("Canonical interchange Core-only compiled acceptance validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
