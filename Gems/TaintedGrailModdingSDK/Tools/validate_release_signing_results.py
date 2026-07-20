#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the read-only release-signing result evidence slice."""

from __future__ import annotations

import sys
from pathlib import Path


class ReleaseSigningValidationError(RuntimeError):
    """Raised when the release-signing repository contract is incomplete."""


ROOT = Path(__file__).resolve().parents[3]
GEM = ROOT / "Gems" / "TaintedGrailModdingSDK"
SOURCE = GEM / "Code" / "Source"
TESTS = GEM / "Code" / "Tests"

REQUIRED_FILES = (
    SOURCE / "AdapterReleaseSigningResultContracts.h",
    SOURCE / "AdapterReleaseSigningResultContracts.cpp",
    SOURCE / "AdapterReleaseSigningEvidenceService.h",
    SOURCE / "AdapterReleaseSigningEvidenceService.cpp",
    SOURCE / "AdapterReleaseSigningResultWidget.h",
    SOURCE / "AdapterReleaseSigningResultWidget.cpp",
    SOURCE / "AdapterReleaseSigningPaneSystemComponent.h",
    SOURCE / "AdapterReleaseSigningPaneSystemComponent.cpp",
    TESTS / "AdapterReleaseSigningResultTests.cpp",
    GEM / "Code" / "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake",
)


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


def validate(repo_root: Path) -> None:
    global ROOT, GEM, SOURCE, TESTS
    ROOT = repo_root
    GEM = ROOT / "Gems" / "TaintedGrailModdingSDK"
    SOURCE = GEM / "Code" / "Source"
    TESTS = GEM / "Code" / "Tests"

    required_files = (
        SOURCE / "AdapterReleaseSigningResultContracts.h",
        SOURCE / "AdapterReleaseSigningResultContracts.cpp",
        SOURCE / "AdapterReleaseSigningEvidenceService.h",
        SOURCE / "AdapterReleaseSigningEvidenceService.cpp",
        SOURCE / "AdapterReleaseSigningResultWidget.h",
        SOURCE / "AdapterReleaseSigningResultWidget.cpp",
        SOURCE / "AdapterReleaseSigningPaneSystemComponent.h",
        SOURCE / "AdapterReleaseSigningPaneSystemComponent.cpp",
        TESTS / "AdapterReleaseSigningResultTests.cpp",
        GEM / "Code" / "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake",
    )
    for path in required_files:
        read(path)

    contracts = read(SOURCE / "AdapterReleaseSigningResultContracts.h")
    service_header = read(SOURCE / "AdapterReleaseSigningEvidenceService.h")
    service = read(SOURCE / "AdapterReleaseSigningEvidenceService.cpp")
    widget = read(SOURCE / "AdapterReleaseSigningResultWidget.cpp")
    pane_component = read(SOURCE / "AdapterReleaseSigningPaneSystemComponent.cpp")
    module = read(SOURCE / "TaintedGrailModdingSDKEditorModule.cpp")
    core_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_editor_files.cmake")
    cmake = read(GEM / "Code" / "CMakeLists.txt")
    tests = read(TESTS / "AdapterReleaseSigningResultTests.cpp")
    test_manifest = read(
        GEM / "Code" / "taintedgrailmoddingsdk_release_signing_result_tests_files.cmake"
    )

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
        ),
        "Release-signing status contract",
    )
    require_fragments(
        contracts,
        (
            "AdapterReleaseSignerReview",
            "AdapterReleaseSignerCapability",
            "AdapterReleaseSigningOutcome",
            "AdapterReleaseSignatureArtifact",
            "AdapterReleaseSigningFailure",
            "AdapterReleaseSigningDiagnosticReference",
            "AdapterReleaseSigningResultRegistry",
        ),
        "Release-signing result contract",
    )
    require_fragments(
        service,
        (
            "ValidateAssemblyAcceptance",
            "ValidateSigningRequested",
            "ValidateSignerReview",
            "ValidateAssemblyBinding",
            "ValidateSigningIntentBinding",
            "ValidateEnvelopeShape",
            "ValidateSigningOutcome",
            "ValidateSignatureArtifacts",
            "ValidateFailureDiagnosticBindings",
            "ResolveStatus",
            "BuildPrimaryEvidence",
            "BuildDiagnosticEvidence",
        ),
        "Release-signing evidence service",
    )
    require_fragments(
        service_header,
        (
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
        "Release-signing no-operation return",
    )
    reject_fragments(
        service,
        (
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
        widget,
        (
            "Tainted Grail Release Signing Results",
            "QAbstractItemView::NoEditTriggers",
            "contract status=not evaluated",
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
        "Release-signing read-only pane",
    )
    reject_fragments(
        widget,
        (
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
    require(
        "Tests/AdapterReleaseSigningResultTests.cpp" in test_manifest,
        "The release-signing compiled test is not owned by its manifest.",
    )
    require(
        "Source/" not in test_manifest,
        "The release-signing test manifest must link production libraries "
        "instead of recompiling sources.",
    )
    require_fragments(
        tests,
        (
            "ExactSucceededResultReturnsCandidateEvidence",
            "RejectedAssemblyEvidenceFailsFirst",
            "UnsignedIntentCannotBecomeSigningEvidence",
            "MissingSignerCapabilityIsRejected",
            "ArchiveFingerprintDriftFailsClosed",
            "SigningIntentIdentityDriftFailsClosed",
            "MalformedResultFingerprintIsRejected",
            "SucceededOutcomeRequiresSignatureArtifact",
            "ValidFailedOutcomeRemainsAcceptedEvidence",
            "UnsafeSignatureReferenceIsRejected",
            "CaseFoldedSignaturePathCollisionIsRejected",
            "ArtifactChronologyAfterCompletionIsRejected",
            "OrphanDiagnosticReferenceIsRejected",
            "NonReciprocalDiagnosticBindingIsRejected",
            "CandidateEvidenceOrderingIsDeterministic",
            "ValidationDoesNotMutateInputs",
            "RegistryRejectsDuplicateResultIdentity",
        ),
        "Release-signing compiled negative coverage",
    )


def main() -> int:
    try:
        validate(Path(__file__).resolve().parents[3])
    except (OSError, ReleaseSigningValidationError) as exc:
        print(f"Release-signing result validation failed: {exc}", file=sys.stderr)
        return 1
    print("Release-signing result contract validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
