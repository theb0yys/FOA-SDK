#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the read-only release-artifact provenance/signing-intent slice."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
GEM = ROOT / "Gems" / "TaintedGrailModdingSDK"
SOURCE = GEM / "Code" / "Source"

REQUIRED_FILES = (
    SOURCE / "AdapterReleaseArtifactContracts.h",
    SOURCE / "AdapterReleaseArtifactContracts.cpp",
    SOURCE / "AdapterReleaseArtifactProvenanceService.h",
    SOURCE / "AdapterReleaseArtifactProvenanceService.cpp",
    SOURCE / "AdapterReleaseArtifactProvenanceServicePart1.inl",
    SOURCE / "AdapterReleaseArtifactProvenanceServicePart2.inl",
    SOURCE / "AdapterReleaseArtifactWidget.h",
    SOURCE / "AdapterReleaseArtifactWidget.cpp",
    SOURCE / "AdapterReleaseArtifactPaneSystemComponent.h",
    SOURCE / "AdapterReleaseArtifactPaneSystemComponent.cpp",
    ROOT
    / "docs"
    / "tainted-grail-sdk"
    / "FOA_RELEASE_ARTIFACT_PROVENANCE_SIGNING_INTENT.md",
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

    contracts = read(SOURCE / "AdapterReleaseArtifactContracts.h")
    service = read(SOURCE / "AdapterReleaseArtifactProvenanceServicePart1.inl")
    serializer = read(SOURCE / "AdapterReleaseArtifactProvenanceServicePart2.inl")
    widget = read(SOURCE / "AdapterReleaseArtifactWidget.cpp")
    pane_component = read(SOURCE / "AdapterReleaseArtifactPaneSystemComponent.cpp")
    module = read(SOURCE / "TaintedGrailModdingSDKEditorModule.cpp")
    core_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_core_files.cmake")
    editor_manifest = read(GEM / "Code" / "taintedgrailmoddingsdk_editor_files.cmake")
    documentation = read(
        ROOT
        / "docs"
        / "tainted-grail-sdk"
        / "FOA_RELEASE_ARTIFACT_PROVENANCE_SIGNING_INTENT.md"
    )

    status_tokens = (
        "ReconciliationNotApproved",
        "PackageNotReady",
        "BindingMismatch",
        "ContentInvalid",
        "ChecksumDeclarationInvalid",
        "ProvenanceIncomplete",
        "LegalDispositionIncomplete",
        "SigningIntentInvalid",
        "PublicationTargetInvalid",
        "Ready",
    )
    for token in status_tokens:
        require(token in contracts, f"missing status token: {token}")

    for token in (
        "ValidateReleaseArtifactUpstream",
        "ValidateReleaseArtifactBinding",
        "ValidateReleaseArtifactContents",
        "ValidateReleaseArtifactProvenance",
        "ValidateReleaseArtifactLegalDispositions",
        "ValidateReleaseArtifactSigningIntent",
        "ValidateReleaseArtifactPublicationTargets",
        "ResolveReleaseArtifactStatus",
    ):
        require(token in service, f"missing validation gate: {token}")

    for token in (
        '"FilesRead"',
        '"FilesHashed"',
        '"ChecksumGenerated"',
        '"FilesCopied"',
        '"ArchiveAssembled"',
        '"SigningPerformed"',
        '"UploadPerformed"',
        '"ReleasePublished"',
        '"LaunchPerformed"',
        '"AdapterCalled"',
        '"DeploymentMutated"',
    ):
        require(token in serializer, f"missing canonical safety field: {token}")

    for banned in (
        "std::filesystem",
        "AZ::IO::FileIO",
        "ProcessWatcher",
        "QProcess",
        "curl",
        "CreateProcess",
    ):
        require(banned not in service and banned not in serializer, f"prohibited operation API: {banned}")

    require(
        "Tainted Grail Release Artifact Provenance and Signing Intent" in widget,
        "read-only pane title missing",
    )
    require("NoEditTriggers" in widget, "pane must remain non-editable")
    require(
        "RegisterViewPane<AdapterReleaseArtifactWidget>" in pane_component,
        "pane registration missing",
    )
    require(
        "AdapterReleaseArtifactPaneSystemComponent::CreateDescriptor" in module,
        "pane component descriptor missing",
    )
    require(
        "azrtti_typeid<AdapterReleaseArtifactPaneSystemComponent>" in module,
        "pane component is not required",
    )

    for filename in (
        "AdapterReleaseArtifactContracts.cpp",
        "AdapterReleaseArtifactContracts.h",
        "AdapterReleaseArtifactProvenanceService.cpp",
        "AdapterReleaseArtifactProvenanceService.h",
        "AdapterReleaseArtifactProvenanceServicePart1.inl",
        "AdapterReleaseArtifactProvenanceServicePart2.inl",
    ):
        require(filename in core_manifest, f"Core ownership missing: {filename}")

    for filename in (
        "AdapterReleaseArtifactWidget.cpp",
        "AdapterReleaseArtifactWidget.h",
        "AdapterReleaseArtifactPaneSystemComponent.cpp",
        "AdapterReleaseArtifactPaneSystemComponent.h",
    ):
        require(filename in editor_manifest, f"Editor ownership missing: {filename}")

    for phrase in (
        "no package file is opened",
        "no checksum is generated",
        "no signature was produced",
        "no upload or publication occurred",
    ):
        require(phrase in documentation.lower(), f"documentation boundary missing: {phrase}")

    print("Release-artifact provenance/signing-intent contract validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
