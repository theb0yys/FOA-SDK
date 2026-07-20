#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the cross-cutting fail-closed research-contract hardening."""

from __future__ import annotations

import sys
from pathlib import Path


class ResearchContractHardeningError(RuntimeError):
    pass


def read(repo_root: Path, relative: str) -> str:
    path = repo_root / relative
    if not path.is_file():
        raise ResearchContractHardeningError(f"Required file is missing: {relative}")
    return path.read_text(encoding="utf-8")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise ResearchContractHardeningError(
            f"{label} is missing required fragment {fragment!r}."
        )


def forbid(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise ResearchContractHardeningError(
            f"{label} retains prohibited fragment {fragment!r}."
        )


def require_all(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        require(text, fragment, label)


def validate_workspace_and_persistence(repo_root: Path) -> None:
    foundation = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Code/Source/FoundationService.cpp",
    )
    require_all(
        foundation,
        (
            "ClearWorkspaceScopedState(true);",
            "m_workspaceFilePath.clear();",
            "m_workspaceRootPath.clear();",
            "m_packs.clear();",
            "m_activePackId.clear();",
            "m_activePackFilePath.clear();",
            "m_sourceRegistry.Clear();",
            "m_catalog.Clear();",
            "ClearWorkspaceScopedState(true);\n        m_workspace = AZStd::move(candidate.m_workspace);",
        ),
        "Workspace switching",
    )

    persistence = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Code/Source/SourceEvidencePersistenceService.cpp",
    )
    require_all(
        persistence,
        (
            "IsSafePersistenceId(sourceDocument.m_source.m_sourceId)",
            "IsContainedPath(sourcesRoot, finalDirectory)",
            'QStringLiteral(".pending-")',
            "RemovePendingPair(",
            "QDir().rename(pendingDirectory, finalDirectory)",
            "Unable to atomically publish the source/evidence document pair.",
        ),
        "Transactional source/evidence persistence",
    )

    registry = read(
        repo_root,
        "Gems/TaintedGrailModdingSDK/Code/Source/SourceEvidenceRegistry.cpp",
    )
    require_all(
        registry,
        (
            "ReserveStableStorage();",
            "m_sources.reserve(MaximumSourceCount);",
            "m_evidence.reserve(MaximumEvidenceCount);",
            "IsSafePersistenceId(source.m_sourceId)",
            "IsSha256Fingerprint(source.m_fingerprint)",
            "IsUsableImportStatus(source.m_importStatus)",
            "IsStrictUtcTimestamp(source.m_capturedAt)",
            "IsStableContractId(evidence.m_evidenceId)",
            "!IsUsableImportStatus(source->m_importStatus)",
            "IsStrictUtcTimestamp(evidence.m_extractedAt)",
        ),
        "Source/evidence registry",
    )


def validate_canonical_trust_chain(repo_root: Path) -> None:
    code_root = "Gems/TaintedGrailModdingSDK/Code/Source/"
    canonical_files = {
        "Runtime result": "AdapterRuntimeResultCanonical.cpp",
        "Deployment execution result": "AdapterDeploymentExecutionResultCanonical.cpp",
        "Post-deployment verifier result": "AdapterPostDeploymentVerifierResultCanonical.cpp",
    }
    for label, filename in canonical_files.items():
        text = read(repo_root, code_root + filename)
        require_all(
            text,
            (
                "CalculateCanonicalSha256(",
                "SerializeCanonical",
                "m_resultFingerprint",
            ),
            label,
        )

    runtime_registry = read(repo_root, code_root + "AdapterRuntimeResultContracts.cpp")
    require_all(
        runtime_registry,
        (
            "CanonicalSha256Matches(",
            "RuntimeResultFingerprintMatches(envelope)",
            "AdapterRuntimeFailureKind::Unknown",
            "IsStrictUtcTimestamp(envelope.m_capturedAt)",
        ),
        "Runtime-result registry",
    )

    execution_registry = read(
        repo_root,
        code_root + "AdapterDeploymentExecutionResultContracts.cpp",
    )
    require_all(
        execution_registry,
        (
            "Unbound deployment execution-result registration is prohibited",
            "CanonicalSha256Matches(",
            "DeploymentExecutionResultFingerprintMatches(envelope)",
            "service.BuildEvidenceReturn(workOrder, envelope)",
            "AdapterDeploymentExecutionFailureKind::Unknown",
        ),
        "Deployment execution-result registry",
    )

    verifier_registry = read(
        repo_root,
        code_root + "AdapterPostDeploymentVerifierContracts.cpp",
    )
    require_all(
        verifier_registry,
        (
            "Unbound post-deployment verifier-result registration is prohibited",
            "PostDeploymentVerifierResultFingerprintMatches(envelope)",
            "service.BuildEvidenceReturn(",
            'if (value == "unknown")',
        ),
        "Post-deployment verifier registry",
    )

    plan_serializer = read(
        repo_root,
        code_root + "AdapterWorkOrderPlanningServicePart8.inl",
    ) + read(
        repo_root,
        code_root + "AdapterWorkOrderPlanningServicePart9.inl",
    )
    require_all(
        plan_serializer,
        (
            'output += canonicalPlan.m_executionAllowed ? "true" : "false";',
            'output += step.m_executionAllowed ? "true" : "false";',
        ),
        "Plan canonical serialization",
    )


def validate_pipeline_handoffs_and_evidence(repo_root: Path) -> None:
    code_root = "Gems/TaintedGrailModdingSDK/Code/"
    staging_header = read(
        repo_root,
        code_root + "Source/AdapterStagingDeploymentPreviewService.h",
    )
    forbid(
        staging_header,
        "DeploymentPreviewBlockerCollection",
        "Staging/deployment blocker storage",
    )
    require(
        staging_header,
        "AZStd::vector<AdapterDeploymentPreviewBlocker> m_blockers;",
        "Staging/deployment blocker storage",
    )

    staging_service = read(
        repo_root,
        code_root + "Source/AdapterStagingDeploymentPreviewService.cpp",
    )
    require_all(
        staging_service,
        (
            'if (code == "deployment.preview_only")',
            "#define AddBlocker AddDeploymentPreviewFinding",
        ),
        "Ready-preview handoff",
    )

    handoff_test = read(
        repo_root,
        code_root + "Tests/AdapterDeploymentPipelineHandoffTests.cpp",
    )
    require_all(
        handoff_test,
        (
            "previewService.BuildPreview",
            "EXPECT_TRUE(preview.m_blockers.empty());",
            "workOrderService.BuildWorkOrder",
            "AdapterDeploymentWorkOrderStatus::ReviewReady",
        ),
        "Slice 13-to-14 compiled handoff test",
    )

    report_validation = read(
        repo_root,
        code_root + "Source/AdapterPostDeploymentVerificationServicePart1.inl",
    )
    require_all(
        report_validation,
        (
            "HasExactWorkOrderBindingEvidence",
            "HasExactExecutorReviewEvidence",
            'evidence.m_recordPath == "WorkOrderBinding"',
            'evidence.m_recordPath == "ExecutorReview"',
            "CanonicalSha256Matches(",
            "DeploymentExecutionResultFingerprintMatches(envelope)",
            "evidence.m_claim == expectedClaim",
        ),
        "Post-deployment evidence completeness",
    )

    verifier_validation = read(
        repo_root,
        code_root
        + "Source/AdapterPostDeploymentVerifierEvidenceServicePart2.inl",
    ) + read(
        repo_root,
        code_root
        + "Source/AdapterPostDeploymentVerifierEvidenceServicePart3.inl",
    )
    require_all(
        verifier_validation,
        (
            "VerifierReviewEvidenceIsBound",
            '"post-deployment-verifier:" + review.m_verifierId',
            "PostDeploymentVerifierResultFingerprintMatches(envelope)",
            "AdapterPostDeploymentVerifierCheckOutcome::NotRun",
            "flags.m_observationMismatch,",
        ),
        "Independent-verifier evidence path",
    )


def validate_reconciliation_and_release(repo_root: Path) -> None:
    source_root = "Gems/TaintedGrailModdingSDK/Code/Source/"
    reconciliation = read(
        repo_root,
        source_root + "AdapterVerifierEvidenceReconciliationServicePart1.inl",
    ) + read(
        repo_root,
        source_root + "AdapterVerifierEvidenceReconciliationServicePart2.inl",
    ) + read(
        repo_root,
        source_root + "AdapterVerifierEvidenceReconciliationServicePart3.inl",
    )
    require_all(
        reconciliation,
        (
            "SameStableIdSet(",
            "m_candidateSourceIds",
            "m_candidateEvidenceIds",
            '"CandidateSourceIds"',
            '"CandidateEvidenceIds"',
            "EvidenceIdsSupportFinding",
            "flags.m_reviewMissing",
        ),
        "Verifier reconciliation",
    )

    reconciliation_registry = read(
        repo_root,
        source_root + "AdapterVerifierEvidenceReconciliationContracts.cpp",
    )
    require_all(
        reconciliation_registry,
        (
            "Unbound verifier reconciliation registration is prohibited",
            "RequestWithinBounds(request)",
            "service.BuildReconciliation(",
            "if (!result.m_accepted)",
        ),
        "Verifier reconciliation registry",
    )

    release_registry = read(
        repo_root,
        source_root + "AdapterReleaseArtifactContracts.cpp",
    )
    require_all(
        release_registry,
        (
            "AdapterReleaseArtifactEnvelopeStatus::Ready",
            "!envelope.m_metadataReady",
            "HasOperationalMutation(envelope)",
            "service.SerializeCanonicalEnvelope(envelope)",
        ),
        "Release-artifact registry",
    )

    roadmap = read(repo_root, "ROADMAP.md")
    require_all(
        roadmap,
        (
            "### Verifier evidence reconciliation and release-decision envelope",
            "### Release-artifact provenance and signing-intent contract",
            "### Release-assembly and checksum-result envelope",
            "### Next ordered slice \u2014 release-signing result envelope",
        ),
        "Roadmap state",
    )
    forbid(
        roadmap,
        "### Next ordered slice \u2014 release-assembly and checksum-result envelope",
        "Roadmap state",
    )


def validate_catalog_planning_and_paths(repo_root: Path) -> None:
    source_root = "Gems/TaintedGrailModdingSDK/Code/Source/"
    planning = read(
        repo_root,
        source_root + "AdapterWorkOrderPlanningServicePart3.inl",
    ) + read(
        repo_root,
        source_root + "AdapterWorkOrderPlanningServicePart5.inl",
    ) + read(
        repo_root,
        source_root + "AdapterWorkOrderPlanningServicePart8.inl",
    )
    require_all(
        planning,
        (
            'blocker.m_severity != "error"',
            "blocker.m_affectedUsages.empty()",
            "HasOpenRelationshipBlocker",
            '"relationship:" + relationship.m_relationshipId',
            "CollectRelationshipValidationProof",
            "FindEffectivePermissionEvent",
            '"economy-recipe-ingredient:" + ingredient.m_linkId',
            '"economy-recipe-output:" + output.m_linkId',
        ),
        "Planning and blocker evaluation",
    )

    catalog_integrity = read(
        repo_root,
        source_root + "CatalogDatabaseIntegrity.cpp",
    )
    require_all(
        catalog_integrity,
        (
            "FindLatestValidationForSubject",
            "FindEffectiveGovernanceEvent",
            "Backdated validation history cannot supersede",
            "AddValidationEventBound",
            "AddGovernanceEventBound",
            "IsStrictUtcTimestamp",
        ),
        "Catalog validation and governance integrity",
    )

    governance = read(
        repo_root,
        source_root + "CatalogGovernanceBlockerService.cpp",
    )
    require_all(
        governance,
        (
            "EvidenceProvesSubject",
            "IsUsableImportStatus(source->m_importStatus)",
            "for (const AZStd::string& validationId : event.m_validationIds)",
            "FindEffectiveGovernanceEvent",
        ),
        "Governance blocker proof",
    )

    path_policy = read(
        repo_root,
        source_root + "PathPolicyWorkspaceValidation.cpp",
    )
    require_all(
        path_policy,
        (
            "ResolveExistingDirectory",
            "HasStorageIndirection",
            "FILE_ATTRIBUTE_REPARSE_POINT",
            "is_directory",
            "s_writeProbeCounter",
        ),
        "Workspace path policy",
    )


def validate_external_toolchain(repo_root: Path) -> None:
    discovery = read(
        repo_root,
        "Gems/ExternalToolchain/Code/Source/ExternalToolchainDiscoveryService.cpp",
    )
    require_all(
        discovery,
        (
            "MaximumAbandonedProbeThreads",
            "wait_for(",
            "worker.detach();",
            "IsAbsoluteHostPath",
            "FILE_ATTRIBUTE_REPARSE_POINT",
            "std::filesystem::canonical",
            "The final filesystem identity differs",
            "A probe that declares a version key requires one configured semantic version.",
            "The probe path configuration has an invalid type or value.",
            "seenPaths",
        ),
        "ExternalToolchain discovery",
    )


def validate(repo_root: Path) -> None:
    validate_workspace_and_persistence(repo_root)
    validate_canonical_trust_chain(repo_root)
    validate_pipeline_handoffs_and_evidence(repo_root)
    validate_reconciliation_and_release(repo_root)
    validate_catalog_planning_and_paths(repo_root)
    validate_external_toolchain(repo_root)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, ResearchContractHardeningError) as exc:
        print(f"Research-contract hardening validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Research-contract hardening validation passed: workspace isolation, "
        "transactional persistence, canonical fingerprints, exact evidence, "
        "bound registries, governance chronology, path policy, and bounded "
        "ExternalToolchain discovery are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
