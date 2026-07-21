#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the hardened read-only release-signing result evidence slice."""

from __future__ import annotations

import sys
from pathlib import Path


class ReleaseSigningValidationError(RuntimeError):
    """Raised when the release-signing repository contract is incomplete."""


ROOT = Path(__file__).resolve().parents[3]
GEM = ROOT / "Gems" / "TaintedGrailModdingSDK"
SOURCE = GEM / "Code" / "Source"
TESTS = GEM / "Code" / "Tests"
TOOLS = GEM / "Tools"
DOCS = ROOT / "docs" / "tainted-grail-sdk"


def read(path: Path) -> str:
    if not path.is_file():
        raise ReleaseSigningValidationError(
            f"Required file is missing: {path.relative_to(ROOT)}"
        )
    return path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReleaseSigningValidationError(message)


def require_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        require(fragment in text, f"{label} is missing required fragment {fragment!r}.")


def reject_fragments(text: str, fragments: tuple[str, ...], label: str) -> None:
    for fragment in fragments:
        require(fragment not in text, f"{label} contains prohibited fragment {fragment!r}.")


def function_slice(text: str, name: str, next_name: str) -> str:
    start = text.find(name)
    end = text.find(next_name, start + len(name))
    require(start >= 0 and end > start, f"Unable to isolate {name} for semantic validation.")
    return text[start:end]


def validate(repo_root: Path) -> None:
    global ROOT, GEM, SOURCE, TESTS, TOOLS, DOCS
    ROOT = repo_root
    GEM = ROOT / "Gems" / "TaintedGrailModdingSDK"
    SOURCE = GEM / "Code" / "Source"
    TESTS = GEM / "Code" / "Tests"
    TOOLS = GEM / "Tools"
    DOCS = ROOT / "docs" / "tainted-grail-sdk"

    required_files = (
        SOURCE / "AdapterReleaseSigningResultContracts.h",
        SOURCE / "AdapterReleaseSigningResultContracts.cpp",
        SOURCE / "AdapterReleaseSigningEvidenceService.h",
        SOURCE / "AdapterReleaseSigningEvidenceService.cpp",
        SOURCE / "AdapterReleaseSigningHardening.h",
        SOURCE / "AdapterReleaseSigningResultWidget.h",
        SOURCE / "AdapterReleaseSigningResultWidget.cpp",
        SOURCE / "AdapterReleaseSigningPaneSystemComponent.h",
        SOURCE / "AdapterReleaseSigningPaneSystemComponent.cpp",
        TESTS / "AdapterReleaseSigningResultTests.cpp",
        TESTS / "AdapterReleaseSigningHardeningTests.cpp",
        GEM / "Code" / "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake",
        TOOLS / "run_local_validation.py",
        TOOLS / "tests" / "test_release_signing_results_validator.py",
        DOCS / "FOA_RELEASE_SIGNING_RESULTS.md",
        DOCS / "README.md",
        DOCS / "RELEASE_PROCESS.md",
        DOCS / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md",
        ROOT / "ROADMAP.md",
    )
    for path in required_files:
        read(path)

    contracts = read(SOURCE / "AdapterReleaseSigningResultContracts.h")
    contract_source = read(SOURCE / "AdapterReleaseSigningResultContracts.cpp")
    service_header = read(SOURCE / "AdapterReleaseSigningEvidenceService.h")
    service = read(SOURCE / "AdapterReleaseSigningEvidenceService.cpp")
    hardening = read(SOURCE / "AdapterReleaseSigningHardening.h")
    widget = read(SOURCE / "AdapterReleaseSigningResultWidget.cpp")
    pane_component = read(SOURCE / "AdapterReleaseSigningPaneSystemComponent.cpp")
    module = read(SOURCE / "TaintedGrailModdingSDKEditorModule.cpp")
    core_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_editor_files.cmake")
    cmake = read(GEM / "Code" / "CMakeLists.txt")
    tests = read(TESTS / "AdapterReleaseSigningResultTests.cpp")
    hardening_tests = read(TESTS / "AdapterReleaseSigningHardeningTests.cpp")
    test_manifest = read(
        GEM / "Code" / "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake"
    )
    local_validation = read(TOOLS / "run_local_validation.py")
    validator_tests = read(
        TOOLS / "tests" / "test_release_signing_results_validator.py"
    )
    documentation = read(DOCS / "FOA_RELEASE_SIGNING_RESULTS.md")
    docs_index = read(DOCS / "README.md")
    release_process = read(DOCS / "RELEASE_PROCESS.md")
    ui_checklist = read(DOCS / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")
    roadmap = read(ROOT / "ROADMAP.md")

    require_fragments(
        contracts,
        (
            "AssemblyResultNotAccepted",
            "SigningNotRequested",
            "SignerUnreviewed",
            "AssemblyBindingMismatch",
            "SigningIntentBindingMismatch",
            "EnvelopeInvalid",
            "SigningOutcomeMismatch",
            "SignatureArtifactBindingMismatch",
            "FailureDiagnosticBindingMismatch",
            "Accepted",
            "AdapterReleaseSigningFailureKind::Unknown",
            "AdapterReleaseSigningResultRegistry",
        ),
        "Release-signing contract",
    )
    require_fragments(
        contract_source,
        (
            '{ AdapterReleaseSigningFailureKind::Unknown, "unknown" }',
            "return TryParseEnum(value, kind, FailureKinds);",
            "IsReleaseSigningEnvelopeWithinLimits(envelope)",
            "AreReleaseSigningEnumsKnown(envelope)",
            "IsReleaseSigningEnvelopePublic(envelope)",
            "TryBuildPackagePathIdentity(",
            "MaximumReleaseSigningRegistryEnvelopes",
        ),
        "Release-signing parser and registry",
    )
    reject_fragments(
        contract_source,
        ('if (value == "unknown")',),
        "Release-signing parser",
    )
    registry = function_slice(
        contract_source,
        "bool AdapterReleaseSigningResultRegistry::RegisterEnvelope(",
        "void AdapterReleaseSigningResultRegistry::Clear()",
    )
    shape_position = registry.find("if (!HasRegistryShape(envelope))")
    capacity_position = registry.find(
        "if (m_envelopes.size() >= MaximumReleaseSigningRegistryEnvelopes)"
    )
    duplicate_position = registry.find(
        "for (const AdapterReleaseSigningResultEnvelope& existing : m_envelopes)"
    )
    append_position = registry.find("m_envelopes.push_back(envelope);")
    require(
        0 <= shape_position < capacity_position < duplicate_position < append_position,
        "Release-signing registry must validate shape and fixed capacity before "
        "duplicate scanning and appending an envelope.",
    )
    require(
        registry.count("m_envelopes.push_back(envelope);") == 1,
        "Release-signing registry must append exactly once after all refusal checks.",
    )

    require_fragments(
        hardening,
        (
            "MaximumReleaseSigningRegistryEnvelopes",
            "MaximumReleaseSigningSignatureArtifacts",
            "MaximumReleaseSigningPublicTextBytes",
            "MaximumReleaseSigningEditorRows",
            "IsKnownReleaseSigningOutcome",
            "IsKnownReleaseSignatureArtifactKind",
            "IsKnownReleaseSigningFailureKind",
            "IsKnownReleaseSigningDiagnosticKind",
            "HasReleaseSigningSensitiveMarker",
            "HasReleaseSigningPrivatePathShape",
            "SameReleaseAssemblyEvidenceReturn",
            "RebuildReleaseAssemblyEvidence",
            "ReleaseSigningChronologyIsValid",
            "BuildReleaseSigningAuthorityCanonicalJson",
            "CalculateReleaseSigningAuthorityFingerprint",
        ),
        "Release-signing hardening boundary",
    )

    require_fragments(
        service,
        (
            "ValidateAssemblyAcceptance",
            "RebuildReleaseAssemblyEvidence(",
            "SameReleaseAssemblyEvidenceReturn(",
            "ValidateSigningRequested",
            "ValidateSignerReview",
            "ValidateAssemblyBinding",
            "ValidateSigningIntentBinding",
            "ValidateEnvelopeShape",
            "ValidateSigningOutcome",
            "default:",
            "ReleaseSigningChronologyIsValid(",
            "ValidateSignatureArtifacts",
            "ValidateFailureDiagnosticBindings",
            "IsKnownReleaseSigningFailureKind(",
            "IsKnownReleaseSigningDiagnosticKind(",
            "const bool bounded =",
            "m_reportedResultFingerprint",
            "m_authoritativeResultFingerprint",
            "CalculateReleaseSigningAuthorityFingerprint(",
            '"application/octet-stream"',
            "BuildPrimaryEvidence",
            "BuildDiagnosticEvidence",
        ),
        "Release-signing evidence service",
    )
    outcome = function_slice(
        service,
        "void ValidateSigningOutcome(",
        "void ValidateSignatureArtifacts(",
    )
    require("bool valid = false;" in outcome, "Signing outcome must default closed.")
    require("default:" in outcome, "Signing outcome must reject unknown enum values.")
    reject_fragments(
        service,
        (
            '"text/plain"',
            "bool valid = true;\n            switch (envelope.m_outcome)",
            "std::filesystem",
            "AZ::IO::FileIO",
            "SystemFile",
            "ProcessWatcher",
            "QProcess",
            "CreateProcess",
            "QNetworkAccessManager",
            "openssl",
            "curl",
        ),
        "Release-signing evidence service",
    )

    require_fragments(
        service_header,
        (
            "m_reportedResultFingerprint",
            "m_authoritativeResultFingerprint",
            "m_filesRead = false",
            "m_filesHashed = false",
            "m_archiveOpened = false",
            "m_archiveModified = false",
            "m_keysLoaded = false",
            "m_identityResolved = false",
            "m_signingPerformed = false",
            "m_signatureVerified = false",
            "m_signatureArtifactsWritten = false",
            "m_uploadPerformed = false",
            "m_releasePublished = false",
            "m_launchPerformed = false",
            "m_adapterCalled = false",
            "m_deploymentMutated = false",
            "m_saveMutated = false",
        ),
        "Release-signing evidence return",
    )

    require_fragments(
        widget,
        (
            "Tainted Grail Release Signing Results",
            "QAbstractItemView::NoEditTriggers",
            "contract status=not evaluated",
            "Supplied diagnostics — not evaluated",
            "MaximumReleaseSigningEditorRows",
            "MaximumSigningCellCharacters",
            "display truncated",
            "reported fingerprint=",
            "does not infer, authenticate, declare diagnostics safe",
            "SDK read=no",
            "archive-open=no",
            "key=no",
            "sign=no",
            "verify=no",
            "upload=no",
            "publish=no",
            "deploy=no",
            "save=no",
        ),
        "Release-signing bounded read-only pane",
    )
    reject_fragments(
        widget,
        (
            'tr("Safe diagnostics")',
            "safe diagnostic references",
            "QPushButton",
            "QLineEdit",
            "QFileDialog",
            "QProcess",
            "QNetworkAccessManager",
            "setEditTriggers(QAbstractItemView::AllEditTriggers)",
        ),
        "Release-signing read-only pane",
    )

    require_fragments(
        pane_component,
        (
            "RegisterViewPane<AdapterReleaseSigningResultWidget>",
            "AdapterReleaseSigningResultRegistry::Get().Clear()",
        ),
        "Release-signing pane lifecycle",
    )
    require_fragments(
        module,
        (
            "AdapterReleaseSigningPaneSystemComponent::CreateDescriptor",
            "azrtti_typeid<AdapterReleaseSigningPaneSystemComponent>",
        ),
        "Editor module release-signing registration",
    )

    for filename in (
        "AdapterReleaseSigningResultContracts.cpp",
        "AdapterReleaseSigningResultContracts.h",
        "AdapterReleaseSigningEvidenceService.cpp",
        "AdapterReleaseSigningEvidenceService.h",
        "AdapterReleaseSigningHardening.h",
    ):
        require(filename in core_manifest, f"Core ownership is missing {filename}.")
    for filename in (
        "AdapterReleaseSigningResultWidget.cpp",
        "AdapterReleaseSigningResultWidget.h",
        "AdapterReleaseSigningPaneSystemComponent.cpp",
        "AdapterReleaseSigningPaneSystemComponent.h",
    ):
        require(filename in editor_manifest, f"Editor ownership is missing {filename}.")

    require(
        "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake" in cmake,
        "The release-signing compiled-test manifest is not linked.",
    )
    for test_file in (
        "Tests/AdapterReleaseSigningResultTests.cpp",
        "Tests/AdapterReleaseSigningHardeningTests.cpp",
    ):
        require(test_file in test_manifest, f"Compiled signing coverage is missing {test_file}.")
    require(
        "Source/" not in test_manifest,
        "Release-signing tests must link production libraries instead of recompiling sources.",
    )

    require_fragments(
        tests,
        (
            "ExactSucceededResultReturnsCandidateEvidence",
            "RejectedAssemblyEvidenceFailsFirst",
            "UnsignedIntentCannotBecomeSigningEvidence",
            "CandidateEvidenceOrderingIsDeterministic",
            "ValidationDoesNotMutateInputs",
            "RegistryRejectsDuplicateResultIdentity",
        ),
        "Original release-signing compiled coverage",
    )
    require_fragments(
        hardening_tests,
        (
            "MutatedAssemblyReturnFailsClosed",
            "UnknownNumericEnumsFailClosed",
            "UnknownExternalFailureIsRepresentable",
            "SignatureBeforeArchiveOrReviewIsRejected",
            "SecretLikePublicMetadataIsRejected",
            "OversizedEnvelopeStopsBeforeEvidenceConstruction",
            "ReportedFingerprintCannotControlSourceIdentity",
            "RegistryRejectsUnsafeDiagnosticPath",
            "RebuildReleaseAssemblyEvidence(",
            "m_filesRead = true",
            "static_cast<AdapterReleaseSigningOutcome>(255)",
            "MaximumReleaseSigningSignatureArtifacts",
            "m_authoritativeResultFingerprint",
        ),
        "Adversarial release-signing compiled coverage",
    )

    require(
        '"validate_release_signing_results.py"' in local_validation,
        "The authoritative local-validation gate does not run the release-signing validator.",
    )
    require_fragments(
        validator_tests,
        (
            "test_repository_fixture_passes",
            "test_registry_capacity_check_must_precede_append",
            "test_missing_hardening_test_fails_closed",
            "test_reconstructed_upstream_check_fails_closed",
            "test_unsafe_diagnostic_label_fails_closed",
            "test_unknown_diagnostic_media_type_fails_closed",
            "test_missing_local_gate_registration_fails_closed",
            "test_missing_documentation_hub_links_fail_closed",
            "test_missing_release_process_gate_fails_closed",
            "test_roadmap_future_marker_fails_closed",
            "test_twenty_two_pane_contract_fails_closed",
        ),
        "Release-signing validator executable negative coverage",
    )

    documentation_lower = documentation.lower()
    for phrase in (
        "separately reviewed external signer",
        "exact accepted release-assembly/checksum result",
        "rebuilds the deterministic assembly evidence return",
        "known enum values",
        "fixed collection limits",
        "reported result fingerprint",
        "sdk-derived authority fingerprint",
        "application/octet-stream",
        "supplied diagnostics — not evaluated",
        "does not load keys",
        "does not sign or verify",
        "no upload or publication occurs",
        "twenty-two-pane manual ui evidence",
    ):
        require(
            phrase in documentation_lower,
            f"Public release-signing documentation is missing hardening phrase {phrase!r}.",
        )

    require(
        docs_index.count("FOA_RELEASE_SIGNING_RESULTS.md") >= 2,
        "The documentation hub must link release-signing guidance for users and contributors.",
    )
    require(
        "typed transient release-signing result envelopes" in docs_index.lower(),
        "The documentation hub does not record the implemented release-signing capability.",
    )

    roadmap_lower = roadmap.lower()
    roadmap_heading = "### release-signing result envelope"
    require(
        roadmap_heading in roadmap_lower,
        "The roadmap does not contain the release-signing result section.",
    )
    roadmap_section = roadmap_lower.split(roadmap_heading, 1)[1].split("\n### ", 1)[0]
    require(
        "status: implemented" in roadmap_section
        and "twenty-two-pane windows manual ui coverage" in roadmap_section,
        "The roadmap does not record the implemented release-signing slice and UI coverage.",
    )
    require(
        "next ordered slice — release-signing result envelope" not in roadmap_lower,
        "The roadmap still describes the implemented release-signing slice as future work.",
    )

    release_process_lower = release_process.lower()
    require(
        "### release-signing result evidence gate" in release_process_lower
        and "foa_release_signing_results.md" in release_process_lower
        and "does not open the archive" in release_process_lower
        and "sign or verify data" in release_process_lower,
        "The release process is missing the release-signing evidence gate or safety boundary.",
    )

    require_fragments(
        ui_checklist,
        (
            "All twenty-two TG SDK panes",
            "Tainted Grail Release Signing Results",
            "zero registered release signing-result envelopes",
            "contract status=not evaluated",
            "release-signing-result file may appear",
        ),
        "Twenty-two-pane Windows manual UI contract",
    )


def main() -> int:
    try:
        validate(ROOT)
    except (OSError, ReleaseSigningValidationError) as exc:
        print(f"Release-signing result validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Release-signing result validation passed: executable adversarial coverage, "
        "bounded contracts, reconstructed upstream evidence, and read-only UI wiring are present."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
