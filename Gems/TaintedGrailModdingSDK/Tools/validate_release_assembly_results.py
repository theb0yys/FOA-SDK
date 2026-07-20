#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the read-only release-assembly/checksum-result slice."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
GEM = ROOT / "Gems" / "TaintedGrailModdingSDK"
SOURCE = GEM / "Code" / "Source"
DOCS = ROOT / "docs" / "tainted-grail-sdk"

REQUIRED_FILES = (
    SOURCE / "AdapterReleaseAssemblyResultContracts.h",
    SOURCE / "AdapterReleaseAssemblyResultContracts.cpp",
    SOURCE / "AdapterReleaseAssemblyEvidenceService.h",
    SOURCE / "AdapterReleaseAssemblyEvidenceService.cpp",
    SOURCE / "AdapterReleaseAssemblyResultWidget.h",
    SOURCE / "AdapterReleaseAssemblyResultWidget.cpp",
    SOURCE / "AdapterReleaseAssemblyPaneSystemComponent.h",
    SOURCE / "AdapterReleaseAssemblyPaneSystemComponent.cpp",
    GEM / "Code" / "Tests" / "AdapterReleaseAssemblyResultTests.cpp",
    GEM / "Code" / "taintedgrailmoddingsdk_release_assembly_result_tests_files.cmake",
    DOCS / "FOA_RELEASE_ASSEMBLY_CHECKSUM_RESULTS.md",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"ERROR: {message}")


def read(path: Path) -> str:
    require(path.is_file(), f"missing required file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def main() -> int:
    for path in REQUIRED_FILES:
        require(path.is_file(), f"missing required file: {path.relative_to(ROOT)}")

    contracts = read(SOURCE / "AdapterReleaseAssemblyResultContracts.h")
    service_header = read(SOURCE / "AdapterReleaseAssemblyEvidenceService.h")
    service = read(SOURCE / "AdapterReleaseAssemblyEvidenceService.cpp")
    widget = read(SOURCE / "AdapterReleaseAssemblyResultWidget.cpp")
    pane_component = read(SOURCE / "AdapterReleaseAssemblyPaneSystemComponent.cpp")
    module = read(SOURCE / "TaintedGrailModdingSDKEditorModule.cpp")
    core_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_editor_files.cmake")
    cmake = read(GEM / "Code" / "CMakeLists.txt")
    tests = read(GEM / "Code" / "Tests" / "AdapterReleaseAssemblyResultTests.cpp")
    test_manifest = read(
        GEM / "Code" / "taintedgrailmoddingsdk_release_assembly_result_tests_files.cmake"
    )
    documentation = read(DOCS / "FOA_RELEASE_ASSEMBLY_CHECKSUM_RESULTS.md")
    roadmap = read(ROOT / "ROADMAP.md")
    ui_checklist = read(DOCS / "DEVELOPER_PREVIEW_MANUAL_UI_SMOKE.md")

    for token in (
        "ArtifactNotReady",
        "AssemblerUnreviewed",
        "ArtifactBindingMismatch",
        "EnvelopeInvalid",
        "ContentCoverageIncomplete",
        "ChecksumObservationMismatch",
        "ArchiveBindingMismatch",
        "FailureDiagnosticBindingMismatch",
        "Accepted",
    ):
        require(token in contracts, f"missing status token: {token}")

    for token in (
        "ContentChecksum",
        "ArchiveAssembly",
        "ArchiveChecksum",
        "AdapterReleaseChecksumObservation",
        "AdapterReleaseArchiveResult",
        "AdapterReleaseAssemblyFailure",
        "AdapterReleaseAssemblyDiagnosticReference",
        "AdapterReleaseAssemblyResultRegistry",
    ):
        require(token in contracts, f"missing result-contract token: {token}")

    for token in (
        "ValidateArtifactReadiness",
        "ValidateAssemblerReview",
        "ValidateArtifactBinding",
        "ValidateEnvelopeShape",
        "ValidateChecksumCoverage",
        "ValidateArchiveResult",
        "ValidateFailureDiagnosticBindings",
        "ResolveStatus",
        "BuildPrimaryEvidence",
        "BuildDiagnosticEvidence",
    ):
        require(token in service, f"missing validation/evidence gate: {token}")

    for token in (
        "m_filesRead = false",
        "m_filesHashed = false",
        "m_archiveAssembled = false",
        "m_signingPerformed = false",
        "m_uploadPerformed = false",
        "m_releasePublished = false",
        "m_launchPerformed = false",
        "m_adapterCalled = false",
        "m_deploymentMutated = false",
    ):
        require(token in service_header, f"missing false safety field: {token}")

    for banned in (
        "std::filesystem",
        "AZ::IO::FileIO",
        "SystemFile",
        "ProcessWatcher",
        "QProcess",
        "CreateProcess",
        "curl",
    ):
        require(banned not in service, f"prohibited operation API: {banned}")

    require(
        "Tainted Grail Release Assembly and Checksum Results" in widget,
        "read-only pane title missing",
    )
    require("NoEditTriggers" in widget, "pane must remain non-editable")
    for action in (
        "QPushButton",
        "QLineEdit",
        "QFileDialog",
        "setEditTriggers(QAbstractItemView::AllEditTriggers)",
    ):
        require(action not in widget, f"mutable/action widget is prohibited: {action}")
    require(
        "RegisterViewPane<AdapterReleaseAssemblyResultWidget>" in pane_component,
        "pane registration missing",
    )
    require(
        "AdapterReleaseAssemblyResultRegistry::Get().Clear()" in pane_component,
        "transient registry cleanup missing",
    )
    require(
        "AdapterReleaseAssemblyPaneSystemComponent::CreateDescriptor" in module,
        "pane component descriptor missing",
    )
    require(
        "azrtti_typeid<AdapterReleaseAssemblyPaneSystemComponent>" in module,
        "pane component is not required",
    )

    for filename in (
        "AdapterReleaseAssemblyResultContracts.cpp",
        "AdapterReleaseAssemblyResultContracts.h",
        "AdapterReleaseAssemblyEvidenceService.cpp",
        "AdapterReleaseAssemblyEvidenceService.h",
    ):
        require(filename in core_manifest, f"Core ownership missing: {filename}")

    for filename in (
        "AdapterReleaseAssemblyResultWidget.cpp",
        "AdapterReleaseAssemblyResultWidget.h",
        "AdapterReleaseAssemblyPaneSystemComponent.cpp",
        "AdapterReleaseAssemblyPaneSystemComponent.h",
    ):
        require(filename in editor_manifest, f"Editor ownership missing: {filename}")

    require(
        "taintedgrailmoddingsdk_release_assembly_result_tests_files.cmake" in cmake,
        "release assembly-result test manifest is not linked",
    )
    require(
        "Tests/AdapterReleaseAssemblyResultTests.cpp" in test_manifest,
        "release assembly-result test ownership missing",
    )
    for token in (
        "ExactMatchedResultReturnsCandidateEvidence",
        "DifferentCanonicalArtifactIsRejected",
        "MissingContentObservationFailsClosed",
        "ValidAdverseOutcomesRemainAccepted",
        "UnsafeDiagnosticReferenceIsRejected",
        "RegistryRejectsDuplicateResultIdentity",
    ):
        require(token in tests, f"missing production-linked test: {token}")

    for phrase in (
        "separately reviewed external assembler/checksummer",
        "every content row",
        "safe relative reference",
        "candidate evidence",
        "does not read or hash package files",
        "no signature is produced",
        "no upload or publication occurs",
    ):
        require(
            phrase in documentation.lower(),
            f"documentation boundary missing: {phrase}",
        )

    require(
        "status: implemented" in roadmap.lower()
        and "release-assembly and checksum-result envelope" in roadmap.lower(),
        "roadmap does not record the implemented slice",
    )
    require(
        "All twenty-one TG SDK panes" in ui_checklist
        and "zero registered release assembly-result envelopes" in ui_checklist,
        "twenty-one-pane Windows acceptance contract missing",
    )

    print("Release-assembly/checksum-result contract validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
