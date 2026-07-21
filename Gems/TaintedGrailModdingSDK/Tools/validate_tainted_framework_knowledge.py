#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""Validate the pinned Tainted Framework knowledge pack and Editor boundary."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
from pathlib import Path


class TaintedFrameworkKnowledgeError(RuntimeError):
    pass


GENERATOR_PATH = Path(__file__).with_name("tainted_framework_knowledge.py")
GENERATOR_SPEC = importlib.util.spec_from_file_location(
    "tainted_framework_knowledge_generator",
    GENERATOR_PATH,
)
generator = importlib.util.module_from_spec(GENERATOR_SPEC)
assert GENERATOR_SPEC and GENERATOR_SPEC.loader
GENERATOR_SPEC.loader.exec_module(generator)


ALLOWED_CLASSIFICATIONS = {
    "portable_authoring_contract",
    "portable_fixture",
    "documentation_evidence",
    "mono_runtime_only",
    "il2cpp_runtime_only",
    "game_linked",
    "editor_utility_candidate",
    "deferred",
    "prohibited_in_editor",
}

EXPECTED_COMMIT = "d7e740e7f167b73152b53409e483dab07d80d048"
EXPECTED_REPOSITORY = "theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods"
EXPECTED_VERSION = "0.1.33"
EXPECTED_CONFIGURATION_KEYS = [
    "General.Enabled",
    "Safety.DryRunOnly",
    "Safety.ReportOnlyMode",
]


def canonical_bytes(value: object) -> bytes:
    return (
        json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        + "\n"
    ).encode("utf-8")


def load_canonical(path: Path) -> object:
    try:
        raw = path.read_bytes()
        value = json.loads(raw.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise TaintedFrameworkKnowledgeError(f"Invalid JSON file {path}: {exc}") from exc
    if raw != canonical_bytes(value):
        raise TaintedFrameworkKnowledgeError(f"Knowledge file is not canonical JSON: {path}")
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        raise TaintedFrameworkKnowledgeError(message)


def validate(repo_root: Path) -> None:
    gem = repo_root / "Gems" / "TaintedGrailModdingSDK"
    root = gem / "Knowledge" / "TaintedFramework"
    intake = load_canonical(root / "upstream-intake.json")
    inventory = load_canonical(root / "component-inventory.json")
    knowledge = load_canonical(root / "knowledge.json")
    fixtures = load_canonical(root / "golden" / "fixtures.json")
    manifest = load_canonical(root / "manifest.json")

    for label, document in (
        ("intake", intake),
        ("inventory", inventory),
        ("knowledge", knowledge),
        ("fixtures", fixtures),
        ("manifest", manifest),
    ):
        require(document.get("schema_version") == 1, f"{label} schema must be exactly 1")

    require(intake.get("repository") == EXPECTED_REPOSITORY, "Upstream repository drifted")
    require(intake.get("commit") == EXPECTED_COMMIT, "Upstream commit drifted")
    require(intake.get("branch") == "main", "Upstream branch drifted")
    require(intake.get("framework_version") == EXPECTED_VERSION, "Framework version drifted")
    require(
        intake.get("license_expression") == "NOASSERTION",
        "Unverified licence must remain NOASSERTION",
    )
    require(
        intake.get("license_status") == "unverified_no_repository_license_file_found",
        "Licence provenance status drifted",
    )
    sources = intake.get("sources", [])
    require(len(sources) >= 7, "Pinned intake must cover the selected upstream source set")
    paths = [entry.get("path") for entry in sources]
    require(paths == sorted(paths), "Pinned source paths must be sorted")
    require(len(paths) == len(set(paths)), "Pinned source paths must be unique")
    for entry in sources:
        require(
            entry.get("fingerprint_kind") == "git_blob_sha1",
            "Fingerprint kind must be explicit",
        )
        fingerprint = entry.get("fingerprint", "")
        require(
            len(fingerprint) == 40
            and all(character in "0123456789abcdef" for character in fingerprint),
            "Pinned source fingerprint must be lowercase Git blob SHA-1",
        )

    components = inventory.get("components", [])
    require(len(components) >= 13, "Framework inventory is incomplete")
    component_ids = [entry.get("id") for entry in components]
    require(len(component_ids) == len(set(component_ids)), "Component IDs must be unique")
    classifications = {entry.get("classification") for entry in components}
    require(classifications <= ALLOWED_CLASSIFICATIONS, "Unknown component classification")
    for required in ALLOWED_CLASSIFICATIONS:
        require(required in classifications, f"Missing component classification: {required}")

    framework = knowledge.get("framework", {})
    require(framework.get("id") == "framework.tainted", "Framework identity drifted")
    require(framework.get("version") == EXPECTED_VERSION, "Knowledge version drifted")
    compatibility = knowledge.get("compatibility", [])
    mono = [row for row in compatibility if row.get("branch") == "Mono"]
    il2cpp = [row for row in compatibility if row.get("branch") == "IL2CPP"]
    require(
        len(mono) == 1 and mono[0].get("status") == "live_load_validated",
        "Mono compatibility observation must remain exact and evidence-backed",
    )
    require(mono[0].get("game_version") == "1.23.401", "Mono game version drifted")
    require(
        len(il2cpp) == 1 and il2cpp[0].get("status") == "blocked",
        "IL2CPP must remain blocked in the pinned knowledge pack",
    )

    surfaces = knowledge.get("api_surfaces", [])
    ready = [row for row in surfaces if row.get("consumer_ready")]
    require(
        len(ready) == 1 and ready[0].get("id") == "runtime-report",
        "Only runtime-report may be consumer-ready",
    )
    require(
        ready[0].get("consumer") == "mods/Tainted-Diagnostic Tool",
        "Runtime-report consumer binding drifted",
    )
    require(
        knowledge.get("capabilities")
        == ["read_active_profile", "query_catalog", "submit_candidate_evidence"],
        "Extension capability declaration escalated or reordered",
    )
    require(
        knowledge.get("configuration_keys") == EXPECTED_CONFIGURATION_KEYS,
        "Configuration vocabulary drifted from the pinned Mono host",
    )

    require(
        fixtures.get("branch_compatibility", {}).get("IL2CPP") == "blocked",
        "Golden fixture must preserve IL2CPP blocking",
    )
    require(
        fixtures.get("duplicate_registration", {}).get("expected") == "rejected",
        "Golden fixture must reject duplicate registration",
    )
    require(
        fixtures.get("capabilities", {}).get("unknown") == "rejected",
        "Golden fixture must reject unknown capabilities",
    )
    require(
        fixtures.get("future_schema", {}).get("expected") == "rejected",
        "Golden fixture must reject future schemas",
    )
    require(
        [row[0] for row in fixtures.get("configuration", [])]
        == EXPECTED_CONFIGURATION_KEYS,
        "Golden configuration fixture drifted",
    )

    expected_manifest_paths = {
        "component-inventory.json",
        "golden/fixtures.json",
        "knowledge.json",
        "upstream-intake.json",
    }
    entries = manifest.get("files", [])
    require(
        {entry.get("path") for entry in entries} == expected_manifest_paths,
        "Knowledge manifest file set drifted",
    )
    for entry in entries:
        path = root / entry["path"]
        raw = path.read_bytes()
        require(len(raw) == entry.get("bytes"), f"Byte size mismatch for {entry['path']}")
        require(
            hashlib.sha256(raw).hexdigest() == entry.get("sha256"),
            f"SHA-256 mismatch for {entry['path']}",
        )

    try:
        generator.verify(root)
    except generator.TaintedFrameworkKnowledgeGenerationError as exc:
        raise TaintedFrameworkKnowledgeError(
            f"Generated knowledge does not match the canonical pack: {exc}"
        ) from exc

    framework_manifest = (
        gem / "Code" / "taintedgrailmoddingsdk_framework_files.cmake"
    ).read_text(encoding="utf-8")
    require(
        "Source/TaintedFrameworkKnowledge.cpp" in framework_manifest,
        "Knowledge implementation is not owned by the Framework target",
    )
    for prohibited in ("Tainted.Host.Mono", "Tainted.Host.IL2CPP", "BepInEx", "UnityEngine"):
        require(
            prohibited not in framework_manifest,
            f"Runtime host leaked into the Editor/Framework build graph: {prohibited}",
        )

    header = (gem / "Code" / "Source" / "TaintedFrameworkKnowledge.h").read_text(
        encoding="utf-8"
    )
    require(
        "ExtensionAPI::Service&" in header,
        "First consumer must register through ExtensionAPI",
    )
    for prohibited in ("CatalogDatabase&", "SourceEvidenceRegistry&", "GameProfile&", "WorkspaceModel&"):
        require(
            prohibited not in header,
            f"Public knowledge surface exposes mutable authority: {prohibited}",
        )

    implementation = (
        gem / "Code" / "Source" / "TaintedFrameworkKnowledge.cpp"
    ).read_text(encoding="utf-8")
    for fragment in (
        EXPECTED_COMMIT,
        '"0.1.33"',
        '"1.23.401"',
        '"Mono"',
        '"IL2CPP"',
        '"General.Enabled"',
        '"Safety.DryRunOnly"',
        '"Safety.ReportOnlyMode"',
        "ExtensionAPI::Capability::ReadActiveProfile",
        "ExtensionAPI::Capability::QueryCatalog",
        "ExtensionAPI::Capability::SubmitCandidateEvidence",
    ):
        require(fragment in implementation, f"Compiled knowledge is missing {fragment}")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    try:
        validate(repo_root)
    except (OSError, TaintedFrameworkKnowledgeError) as exc:
        print(f"Tainted Framework knowledge validation failed: {exc}", file=sys.stderr)
        return 1
    print(
        "Tainted Framework knowledge validation passed: pinned provenance, "
        "complete classification, reproducible golden fixtures, branch isolation, "
        "governed ExtensionAPI registration, and no runtime-host build leakage."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
