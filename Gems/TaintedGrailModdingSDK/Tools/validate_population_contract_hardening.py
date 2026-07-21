#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate population contracts and explicit catalog helper extraction."""

from __future__ import annotations

import sys
from pathlib import Path


class PopulationContractHardeningError(RuntimeError):
    pass


def read(path: Path) -> str:
    if not path.is_file():
        raise PopulationContractHardeningError(f"Required file is missing: {path}")
    return path.read_text(encoding="utf-8")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise PopulationContractHardeningError(
            f"{label} is missing required fragment {fragment!r}."
        )


def reject(text: str, fragment: str, label: str) -> None:
    if fragment in text:
        raise PopulationContractHardeningError(
            f"{label} retains prohibited fragment {fragment!r}."
        )


def function_slice(text: str, start_marker: str, end_marker: str) -> str:
    start = text.find(start_marker)
    end = text.find(end_marker, start + len(start_marker))
    if start < 0 or end <= start:
        raise PopulationContractHardeningError(
            f"Unable to isolate {start_marker} for semantic validation."
        )
    return text[start:end]


def validate(repo_root: Path) -> None:
    gem = repo_root / "Gems/TaintedGrailModdingSDK"
    source = gem / "Code/Source"
    tests = gem / "Code/Tests"
    code = gem / "Code"

    evidence = read(source / "PopulationEvidenceValidation.h")
    authoring = read(source / "PopulationAuthoringService.cpp")
    catalog_database = read(source / "CatalogDatabase.cpp")
    catalog_base = read(source / "CatalogDatabaseBase.inl")
    integrity = read(source / "CatalogDatabaseIntegrity.cpp")
    integrity_base = read(source / "CatalogDatabaseIntegrityBase.inl")
    member_validation = read(source / "CatalogDatabasePopulationPart3.inl")
    hardening_tests = read(tests / "PopulationContractHardeningTests.cpp")
    authoring_tests = read(tests / "PopulationAuthoringTests.cpp")
    catalog_tests = read(tests / "PopulationCatalogTests.cpp")
    focused_manifest = read(
        code / "taintedgrailmoddingsdk_population_hardening_tests_files.cmake"
    )
    catalog_manifest = read(code / "taintedgrailmoddingsdk_catalog_tests_files.cmake")
    cmake = read(code / "CMakeLists.txt")

    for fragment in (
        "ValidatePopulationEvidenceCoverage",
        "AZStd::vector<bool> covered",
        "evidence proves an unrelated subject",
        "is missing evidence for exact subject",
        "PopulationEvidenceIsCompleteAndBound",
        "canonicalSubjects",
        "canonicalEvidenceIds",
    ):
        require(evidence, fragment, "Population evidence coverage helper")
    reject(
        evidence,
        "ValidateEvidenceForAnyPopulationSubject",
        "Population evidence coverage helper",
    )

    coverage = function_slice(
        evidence,
        "inline bool ValidatePopulationEvidenceCoverage(",
        "} // namespace TaintedGrailModdingSDK",
    )
    coverage_order = (
        "canonicalEvidenceIds",
        "canonicalSubjects",
        "PopulationEvidenceIsCompleteAndBound(",
        "evidence->m_subjectRef",
        "covered[",
        "if (!covered[index])",
    )
    positions = [coverage.find(fragment) for fragment in coverage_order]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        raise PopulationContractHardeningError(
            "Population evidence coverage must validate complete evidence, map exact "
            "subjects, mark coverage, and reject every uncovered binding in order."
        )

    for fragment in (
        '#include "PathPolicyService.h"',
        "PathPolicyService pathPolicy;",
        "pathPolicy.ValidateWorkspacePaths(workspace, workspaceRoot)",
        "path-policy-validated canonical workspace root",
        "EvidenceIsCompleteAndBound(",
        "ValidatePopulationEvidenceCoverage(",
        "ActorEvidenceIds(actor, catalog)",
        "TroopEvidenceIds(definition.m_profile, catalog)",
        "MemberEvidenceIds(member, catalog)",
        "definition.m_profile.m_evidenceIds",
        "member.m_evidenceIds",
    ):
        require(authoring, fragment, "Population authoring service")
    reject(
        authoring,
        "ValidateEvidenceForAnyPopulationSubject",
        "Population authoring service",
    )

    context = function_slice(
        authoring,
        "bool ValidateAuthoringContext(",
        "bool ValidateAuthoredRecord(",
    )
    path_position = context.find("pathPolicy.ValidateWorkspacePaths(workspace, workspaceRoot)")
    pack_position = context.find("activePack.HasStableIdentity()")
    if path_position < 0 or pack_position < 0 or path_position >= pack_position:
        raise PopulationContractHardeningError(
            "The public population candidate service must validate the canonical "
            "workspace path before accepting the active pack."
        )

    for fragment in (
        '#include "CatalogDatabaseBase.inl"',
        "candidate.ReplaceFromDocumentWithoutPopulation(",
        "ClearWithoutPopulation();",
        "BuildDocumentWithoutPopulation(",
    ):
        require(catalog_database, fragment, "Population-aware catalog wrapper")
    for fragment in (
        "bool CatalogDatabase::ReplaceFromDocumentWithoutPopulation(",
        "void CatalogDatabase::ClearWithoutPopulation()",
        "CatalogDocument CatalogDatabase::BuildDocumentWithoutPopulation(",
    ):
        require(catalog_base, fragment, "Explicit catalog base helpers")
    for fragment in (
        "bool CatalogDatabase::ReplaceFromDocument(",
        "void CatalogDatabase::Clear()",
        "CatalogDocument CatalogDatabase::BuildDocument(",
    ):
        reject(catalog_base, fragment, "Explicit catalog base helpers")

    for fragment in (
        '#include "CatalogDatabaseIntegrityBase.inl"',
        "ValidateIntegrityWithoutPopulation(",
    ):
        require(integrity, fragment, "Population-aware catalog integrity wrapper")
    for fragment in (
        "bool CatalogDatabase::ReplaceFromBoundDocumentWithoutPopulation(",
        "bool CatalogDatabase::ValidateIntegrityWithoutPopulation(",
        "candidate.ValidateIntegrityWithoutPopulation(",
    ):
        require(integrity_base, fragment, "Explicit catalog integrity helpers")
    for fragment in (
        "bool CatalogDatabase::ReplaceFromBoundDocument(",
        "bool CatalogDatabase::ValidateIntegrity(",
    ):
        reject(integrity_base, fragment, "Explicit catalog integrity helpers")

    macro_renames = (
        "#define ReplaceFromDocument",
        "#define Clear",
        "#define BuildDocument",
        "#define ReplaceFromBoundDocument",
        "#define ValidateIntegrity",
    )
    for label, text in (
        ("Catalog wrapper", catalog_database),
        ("Catalog base implementation", catalog_base),
        ("Catalog integrity wrapper", integrity),
        ("Catalog integrity base implementation", integrity_base),
    ):
        for macro in macro_renames:
            reject(text, macro, label)

    for fragment in (
        "if (member.m_required)",
        "requiredMinimum += member.m_minimumCount",
        "totalMaximum += member.m_maximumCount",
        "!matchingLeader->m_required",
        "matchingLeader->m_minimumCount == 0",
        "requiredMinimum > troop.m_maximumSize",
        "totalMaximum < troop.m_minimumSize",
    ):
        require(integrity, fragment, "Population integrity implementation")
    required_block = function_slice(
        integrity,
        "if (member.m_required)",
        "totalMaximum += member.m_maximumCount",
    )
    require(
        required_block,
        "requiredMinimum += member.m_minimumCount",
        "Required-member structural minimum block",
    )

    for fragment in (
        "const bool countRangeValid",
        "member.m_minimumCount <= member.m_maximumCount",
        "(!member.m_required || member.m_minimumCount > 0)",
        "positive minimum for required rows",
    ):
        require(member_validation, fragment, "Population member validation")
    reject(
        member_validation,
        "member.m_minimumCount == 0\n            || member.m_minimumCount > member.m_maximumCount",
        "Population member validation",
    )

    for fragment in (
        "EveryRequiredSubjectMustHaveEvidence",
        "OptionalMemberMayUseZeroMinimum",
        "OptionalMinimumDoesNotRaiseTroopRequirement",
        "PublicAuthoringRejectsUnvalidatedRoot",
        "missing evidence for exact subject",
        "m_required = true",
        "m_required = false",
        "path-policy-validated canonical workspace root",
    ):
        require(hardening_tests, fragment, "Population hardening compiled tests")

    for name in (
        "ActorProfilePersistsPublishesSnapshotAndNotifiesOnce",
        "TroopDefinitionIsAtomicOrderIndependentAndPreservesOmittedMembers",
        "StructuralFailuresAndLinkOwnerMovesDoNotPublish",
        "ContextEvidenceAndPrimaryOwnershipFailClosed",
        "PersistenceFailureLeavesPublishedCatalogUnchangedAndSilent",
    ):
        require(authoring_tests, name, "Population authoring compiled tests")
    for name in (
        "ActorProfilesAcceptExactResolvedAndUnresolvedTemplates",
        "TroopsAcceptExactResolvedAndUnresolvedLeaders",
        "MembersRejectBindingsBoundsRolesAndDuplicateConflicts",
    ):
        require(catalog_tests, name, "Population catalog compiled tests")

    require(
        focused_manifest,
        "Tests/PopulationContractHardeningTests.cpp",
        "Focused population hardening test manifest",
    )
    reject(
        focused_manifest,
        "Source/",
        "Focused population hardening test manifest",
    )
    for entry in (
        "Tests/PopulationAuthoringTests.cpp",
        "Tests/PopulationCatalogTests.cpp",
    ):
        require(catalog_manifest, entry, "Catalog compiled-test manifest")
    require(
        cmake,
        "taintedgrailmoddingsdk_population_hardening_tests_files.cmake",
        "Production-linked Catalog test target",
    )


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, PopulationContractHardeningError) as exc:
        print(f"Population contract hardening failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Population contract hardening passed: every authored binding requires exact "
        "evidence, optional member minima remain optional, public candidate construction "
        "enforces the canonical workspace path, and catalog helpers are macro-free."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
