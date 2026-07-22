#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the exact-source Tainted Grail system-port and authority boundary."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PINNED_COMMIT = "d7e740e7f167b73152b53409e483dab07d80d048"


class TaintedSystemPortError(RuntimeError):
    """Raised when the selective-port boundary is incomplete or weakened."""


def read_required(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file():
        raise TaintedSystemPortError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise TaintedSystemPortError(f"{label} is missing required fragment: {fragment}")


def reject_host_linkage(source: str) -> None:
    if "#include <Unity" in source or "#include <BepInEx" in source:
        raise TaintedSystemPortError(
            "Portable system ports must not link Unity or BepInEx hosts."
        )


def validate(root: Path = REPO_ROOT) -> None:
    intake = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/SourceEvidenceIntakeWidget.cpp",
    )
    if "ISODateWithMs" in intake or intake.count("toString(Qt::ISODate)") < 2:
        raise TaintedSystemPortError(
            "Source Intake must generate whole-second UTC values before and after import."
        )

    framework_manifest = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_files.cmake",
    )
    for name in (
        "Source/ExtensionAPIClient.cpp",
        "Source/FoundationEvidenceReviewService.cpp",
        "Source/FoundationExtensionAPI.cpp",
        "Source/TaintedFrameworkKnowledge.cpp",
        "Source/FoundationTaintedInterfaceUiUtilities.cpp",
        "Source/TaintedInterfaceUiUtilities.cpp",
        "Source/TaintedInterfaceUiUtilities.h",
    ):
        require(framework_manifest, name, "Framework build manifest")

    core_manifest = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_core_files.cmake",
    )
    for name in (
        "Source/GameInformationAcquisition.cpp",
        "Source/GameInformationAcquisition.h",
        "Source/RoadAtlasExtension.cpp",
        "Source/RoadAtlasExtension.h",
        "Source/AvalonAiExtension.cpp",
        "Source/AvalonAiExtension.h",
        "Source/FoARuntimeAdapterRoutes.cpp",
        "Source/FoARuntimeAdapterRoutes.h",
    ):
        require(core_manifest, name, "Core build manifest")

    test_manifest = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_extension_api_tests_files.cmake",
    )
    for name in (
        "Tests/AvalonAiExtensionTests.cpp",
        "Tests/FoARuntimeAdapterRoutesTests.cpp",
        "Tests/GameInformationAcquisitionTests.cpp",
        "Tests/RoadAtlasExtensionTests.cpp",
        "Tests/SourceEvidenceRegistryCandidateTests.cpp",
        "Tests/TaintedInterfaceUiUtilitiesTests.cpp",
    ):
        require(test_manifest, name, "Extension test manifest")

    framework_test_manifest = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_tainted_framework_tests_files.cmake",
    )
    require(
        framework_test_manifest,
        "Tests/TaintedFrameworkKnowledgeTests.cpp",
        "Tainted Framework test manifest",
    )

    framework = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/TaintedFrameworkKnowledge.cpp",
    )
    require(framework, PINNED_COMMIT, "Tainted Framework knowledge")
    require(framework, 'm_supportedBranches = { "Mono" }', "Canonical branch")
    require(framework, 'pack.m_licenseExpression = "NOASSERTION"', "Licence boundary")

    interface = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
    )
    interface_header = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.h",
    )
    require(interface, PINNED_COMMIT, "Tainted Interface knowledge")
    require(
        interface,
        '"kane.tgfoa.tainted-interface",\n            "NOASSERTION",',
        "Licence boundary",
    )
    require(interface, "upstream_payload_blocked_noassertion", "Payload boundary")
    for fragment in (
        "m_upstreamPayloadEmbedded = false",
        "m_upstreamRedistributionAllowed = false",
        "m_projectOwnedFallbackEmbedded = false",
    ):
        require(interface_header, fragment, "Payload boundary")
    for fragment in (
        "IsSemanticAssetId",
        "ProjectOwnedFallback",
        "m_redistributionAllowed = true",
        "FOA-SDK project-owned generated semantic fallback",
    ):
        require(interface, fragment, "Safe UI utility boundary")
    reject_host_linkage(interface)

    acquisition = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.cpp",
    )
    acquisition_header = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.h",
    )
    acquisition_tests = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Tests/GameInformationAcquisitionTests.cpp",
    )
    for fragment in (
        "provider.foa-local-capture",
        "provider.tainted-grail-github",
        "provider.merlin-workshop",
        "QualificationState::ExactInstallBound",
        "m_autoPromotionAllowed",
        "m_canPromoteAutomatically = false",
        "m_grantsRuntimePermission = false",
        "FindDuplicateObservationIds",
        "left.m_claimId",
        "conflict.m_claimId",
    ):
        require(acquisition, fragment, "Acquisition provider boundary")
    require(acquisition_header, "AZStd::string m_claimId;", "Typed acquisition claim")
    for fragment in (
        "DifferentTypedClaimsForOneSubjectDoNotConflict",
        "DuplicateObservationIdentityHasOneRejectedDisposition",
    ):
        require(acquisition_tests, fragment, "Acquisition negative coverage")

    road = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/RoadAtlasExtension.cpp",
    )
    road_tests = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Tests/RoadAtlasExtensionTests.cpp",
    )
    require(road, PINNED_COMMIT, "Road Atlas source binding")
    for fragment in (
        "RoadAtlasSchemaContracts.cs",
        "ApprovedForPlanning",
        "element.m_evidenceRequirements.empty()",
        "hasRequiredEvidence",
        "DeterministicContractJson",
        "snapshot.m_runtimeMutationAllowed",
        "CalculateCanonicalSha256",
    ):
        require(road, fragment, "Road Atlas boundary")
    require(
        road_tests,
        "PlanningApprovalRequiresNonEmptyRequiredEvidenceSet",
        "Road Atlas negative coverage",
    )

    ai = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/AvalonAiExtension.cpp",
    )
    ai_header = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/AvalonAiExtension.h",
    )
    ai_tests = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Tests/AvalonAiExtensionTests.cpp",
    )
    require(ai, PINNED_COMMIT, "Avalon AI source binding")
    for fragment in (
        "AvalonAI.Contracts.V2",
        "manifest.m_requiredRuntimeApi.m_minor > knowledge.m_apiMinor",
        "Contains(keyIds, action.m_targetKeyId)",
        "DeterministicContractJson",
        "manifest.m_executionEnabled",
        "manifest.m_hostLinked",
        "m_executionAllowed = false",
    ):
        require(ai, fragment, "Avalon AI authoring boundary")
    require(ai_header, "AZStd::string m_keyId;", "Avalon stable key identity")
    for fragment in (
        "UnsupportedApiMinorFailsClosed",
        "ActionTargetMustResolveToDeclaredStableKey",
        "CanonicalFingerprintPreservesCollectionBoundaries",
    ):
        require(ai_tests, fragment, "Avalon AI negative coverage")

    routes = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp",
    )
    route_header = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.h",
    )
    route_tests = read_required(
        root,
        "Gems/TaintedGrailModdingSDK/Code/Tests/FoARuntimeAdapterRoutesTests.cpp",
    )
    require(routes, PINNED_COMMIT, "Runtime adapter route source binding")
    for fragment in (
        "adapter.foa.mono",
        "adapter.foa.il2cpp",
        'mono.m_frameworkVersion = "0.1.33"',
        'il2Cpp.m_frameworkVersion = "0.1.36"',
        "active-evidence-registry-required",
        "adapter-identity",
        "adapter-install-parity",
        "requested-authority-not-granted-by-route-import",
        'AppendBool(output, "build", route.m_buildAllowed)',
        'AppendBool(output, "deployment", route.m_deploymentAllowed)',
        'AppendBool(output, "execution", route.m_executionAllowed)',
        'AppendBool(output, "runtime_mutation", route.m_runtimeMutationAllowed)',
        'AppendBool(output, "save_access", route.m_saveAccessAllowed, false)',
    ):
        require(routes, fragment, "Runtime adapter route boundary")
    require(
        route_header,
        "const SourceEvidenceRegistry& evidenceRegistry",
        "Runtime evidence dependency",
    )
    for fragment in (
        "RegistryFreeQualificationFailsClosed",
        "FabricatedOrPendingEvidenceCannotQualify",
        "OneEvidenceRecordCannotSatisfyBothClasses",
        "CanonicalFingerprintBindsActualAuthorityFields",
    ):
        require(route_tests, fragment, "Runtime route negative coverage")

    for source in (acquisition, road, ai, routes):
        reject_host_linkage(source)

    research = read_required(root, "Research/tainted-grail-system-ports/README.md")
    source_register = read_required(
        root,
        "Research/tainted-grail-system-ports/SOURCE_REGISTER.md",
    )
    gates = read_required(root, "Research/tainted-grail-system-ports/PORT_GATES.md")
    require(research, PINNED_COMMIT, "Research baseline")
    require(research, "Waning or Bannerlord", "Scope exclusion")
    require(source_register, "Tainted Interface", "Source register")
    require(source_register, "exact FoA game version not recorded", "Source register")
    require(source_register, "Avalon AI Runtime", "Source register")
    require(source_register, "Merlin Workshop", "Source register")
    require(gates, "Source Intake compatibility", "Port gates")
    require(gates, "Runtime adapters", "Port gates")


def main() -> int:
    try:
        validate()
    except TaintedSystemPortError as exc:
        print(f"Tainted Grail system port validation failed: {exc}", file=sys.stderr)
        return 1
    print("Tainted Grail exact-source system port boundary passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
