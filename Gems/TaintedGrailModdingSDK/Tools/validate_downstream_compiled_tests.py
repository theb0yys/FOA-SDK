#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Fail when implemented downstream slices are absent from compiled tests."""

from __future__ import annotations

import re
import sys
from pathlib import Path


class DownstreamTestContractError(RuntimeError):
    pass


REQUIRED_MANIFESTS = {
    "taintedgrailmoddingsdk_post_deployment_verification_tests_files.cmake": (
        "Tests/AdapterPostDeploymentVerificationTests.cpp",
    ),
    "taintedgrailmoddingsdk_post_deployment_verifier_tests_files.cmake": (
        "Tests/AdapterPostDeploymentVerifierTests.cpp",
    ),
    "taintedgrailmoddingsdk_verifier_reconciliation_tests_files.cmake": (
        "Tests/AdapterVerifierEvidenceReconciliationTests.cpp",
    ),
    "taintedgrailmoddingsdk_release_artifact_tests_files.cmake": (
        "Tests/AdapterReleaseArtifactTests.cpp",
    ),
}

REQUIRED_TEST_MARKERS = {
    "Tests/AdapterPostDeploymentVerificationTests.cpp": (
        "ExactAcceptedExecutionEvidenceProducesReviewReadyReport",
        "EvidenceKindAloneCannotSatisfyRequiredBindingEvidence",
        "CallerSelectedExecutionFingerprintCannotEnterReport",
    ),
    "Tests/AdapterPostDeploymentVerifierTests.cpp": (
        "NotRunRemainsContractValidAdverseEvidence",
        "CallerSelectedVerifierFingerprintFailsClosed",
        "RegistryRequiresExactBoundValidation",
    ),
    "Tests/AdapterVerifierEvidenceReconciliationTests.cpp": (
        "MissingReviewIsNotMisclassifiedAsBindingMismatch",
        "HumanReviewCandidateClaimsMustMatchExactCandidateSet",
        "ReviewEvidenceMustBelongToCandidateEvidence",
        "RegistryStoresOnlyCompleteAcceptedReconciliations",
    ),
    "Tests/AdapterReleaseArtifactTests.cpp": (
        "CompleteDeclaredMetadataIsReadyWithoutPerformingOperations",
        "ChecksumDeclarationMustEqualReviewedPackageDigest",
        "RegistryRejectsNonReadyOrOperationalEnvelope",
    ),
}


def read(path: Path) -> str:
    if not path.is_file():
        raise DownstreamTestContractError(f"Required file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise DownstreamTestContractError(
            f"{label} is missing required fragment {fragment!r}."
        )


def validate(repo_root: Path) -> None:
    code_root = repo_root / "Gems/TaintedGrailModdingSDK/Code"
    cmake = read(code_root / "CMakeLists.txt")
    target_match = re.search(
        r"ly_add_target\(\s*NAME \$\{gem_name\}\.Catalog\.Tests.*?\n\)",
        cmake,
        flags=re.DOTALL,
    )
    if not target_match:
        raise DownstreamTestContractError("Unable to locate the compiled TG SDK test target.")
    target = target_match.group(0)
    require(target, "Gem::${gem_name}.Framework.Static", "Compiled test target")
    require(target, "AZ::AzTest", "Compiled test target")
    require(target, "Tests", "Compiled test target include directories")

    fixture = read(code_root / "Tests/AdapterResearchPipelineTestFixture.h")
    for marker in (
        "CalculateCanonicalSha256(workOrder.m_canonicalJson)",
        "CalculateDeploymentExecutionResultFingerprint(envelope)",
        "CalculatePostDeploymentVerifierResultFingerprint(envelope)",
        "reportService.BuildReport",
        "verifierService.BuildEvidenceReturn",
        "reconciliationService.BuildReconciliation",
        "releaseService.BuildEnvelope",
    ):
        require(fixture, marker, "Shared downstream fixture")

    for manifest_name, entries in REQUIRED_MANIFESTS.items():
        require(target, manifest_name, "Compiled test target")
        manifest_path = code_root / manifest_name
        manifest = read(manifest_path)
        for entry in entries:
            require(manifest, entry, manifest_name)
            test_path = code_root / entry
            test = read(test_path)
            if re.search(r"^\s*Source/", manifest, flags=re.MULTILINE):
                raise DownstreamTestContractError(
                    f"{manifest_name} recompiles a production source instead of linking it."
                )
            for marker in REQUIRED_TEST_MARKERS[entry]:
                require(test, marker, entry)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, DownstreamTestContractError) as exc:
        print(f"Downstream compiled-test validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Downstream compiled-test validation passed: post-deployment, verifier, "
        "reconciliation, and release-artifact slices are production-linked and wired."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
