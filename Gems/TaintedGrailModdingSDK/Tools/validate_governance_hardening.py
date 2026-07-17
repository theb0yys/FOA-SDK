#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the corrective catalog-governance reliability baseline."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def require_contains(path: Path, fragments: tuple[str, ...]) -> str:
    if not path.is_file():
        fail(f"Required hardening file is missing: {path}")
    text = path.read_text(encoding="utf-8")
    for fragment in fragments:
        if fragment not in text:
            fail(f"Missing required fragment {fragment!r} in {path}")
    return text


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    source_root = repo_root / "Gems/TaintedGrailModdingSDK/Code/Source"
    tests_root = repo_root / "Gems/TaintedGrailModdingSDK/Code/Tests"

    try:
        types_header = require_contains(
            source_root / "CatalogGovernanceTypes.h",
            (
                "enum class CatalogSubjectKind",
                "enum class GovernanceAxis",
                "enum class ResearchStage",
                "enum class ConfidenceLevel",
                "enum class OperationalRisk",
                "enum class ValidationState",
                "enum class StalenessState",
                "enum class PermissionDecision",
                "struct GovernedSubjectState",
            ),
        )
        if "AZStd::string m_validationState" in types_header:
            fail("GovernedSubjectState must use typed validation state, not a raw string")

        service_header = require_contains(
            source_root / "CatalogGovernanceService.h",
            (
                "CatalogGovernanceApplyResult",
                "CatalogValidationApplyResult",
                "const CatalogDatabase& catalog",
                "ApplyTypedTransition",
                "ApplyValidationState",
            ),
        )
        if re.search(r"SourceEvidenceRegistry& sourceRegistry,\s*CatalogDatabase& catalog", service_header):
            fail("CatalogGovernanceService must not accept a mutable caller-owned catalog")

        service = require_contains(
            source_root / "CatalogGovernanceService.cpp",
            (
                "CatalogDatabase candidate = catalog",
                "WriteSubjectState(state, candidate",
                "candidate.AddGovernanceEvent",
                "candidate.AddValidationEvent",
                "result.m_catalog = AZStd::move(candidate)",
            ),
        )
        for case_fragment in (
            "case GovernanceAxis::Maturity:",
            "case GovernanceAxis::Confidence:",
            "case GovernanceAxis::OperationalRisk:",
            "case GovernanceAxis::Staleness:",
            "case GovernanceAxis::Permission:",
            "case GovernanceAxis::Supersession:",
        ):
            if service.count(case_fragment) != 1:
                fail(f"Governance axis is not implemented exactly once: {case_fragment}")

        transaction = require_contains(
            source_root / "CatalogTransactionService.cpp",
            (
                "save(document, workspace.m_rootPath)",
                "result.m_catalog = candidate",
            ),
        )
        if transaction.find("save(document, workspace.m_rootPath)") > transaction.find("result.m_catalog = candidate"):
            fail("Catalog transaction must save before returning a publishable candidate")

        foundation = require_contains(
            source_root / "FoundationCatalogService.cpp",
            (
                "m_catalogTransaction.Commit(",
                "m_catalog = AZStd::move(committed.m_catalog)",
            ),
        )
        if foundation.find("m_catalogTransaction.Commit(") > foundation.find("m_catalog = AZStd::move(committed.m_catalog)"):
            fail("Foundation catalog state must publish only after transaction success")

        hardening_tests = require_contains(
            tests_root / "CatalogGovernanceHardeningTests.cpp",
            (
                "UnknownEvidenceLeavesOriginalCatalogUnchanged",
                "WrongEvidenceSubjectLeavesOriginalCatalogUnchanged",
                "WrongEvidenceProfileLeavesOriginalCatalogUnchanged",
                "DuplicateGovernanceIdRollsBackStateChange",
                "DuplicateValidationIdRollsBackStateChange",
                "PersistenceFailureDoesNotPublishCandidate",
                "CorruptedDuplicateHistoryDocumentDoesNotReplaceCatalog",
                "InvalidTypedStateDocumentDoesNotReplaceCatalog",
                "injected persistence failure",
            ),
        )
        if "existing.m_eventId).IsSuccess()" not in hardening_tests:
            fail("Duplicate governance-ID test must force the duplicate identifier")
        if "duplicateId).IsSuccess()" not in hardening_tests:
            fail("Duplicate validation-ID test must force the duplicate identifier")

        require_contains(
            tests_root / "CatalogGovernanceTypesTests.cpp",
            (
                "KnownValuesRoundTripThroughTypedBoundary",
                "TypographicalValuesFailAtBoundary",
                "LegacySchemaValuesRemainRepresentable",
            ),
        )
        require_contains(
            repo_root / "docs/tainted-grail-sdk/GOVERNANCE_HARDENING.md",
            (
                "Intrinsic service atomicity",
                "One governed subject state",
                "Required negative coverage",
                "persistence completes before publication",
            ),
        )
    except (OSError, RuntimeError) as exc:
        print(f"Tainted Grail governance hardening validation failed: {exc}", file=sys.stderr)
        return 1

    print("Tainted Grail governance hardening contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
