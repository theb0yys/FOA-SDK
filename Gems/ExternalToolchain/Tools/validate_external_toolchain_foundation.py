#!/usr/bin/env python3
"""Validate the ExternalToolchain host-foundation repository contract."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
GEM = ROOT / "Gems" / "ExternalToolchain"


REQUIRED_FILES = (
    GEM / "gem.json",
    GEM / "CMakeLists.txt",
    GEM / "Code" / "CMakeLists.txt",
    GEM / "Code" / "Include" / "ExternalToolchain" / "ExternalToolchainBus.h",
    GEM / "Code" / "Include" / "ExternalToolchain" / "ExternalToolchainTypeIds.h",
    GEM / "Code" / "Include" / "ExternalToolchain" / "ExternalToolchainTypes.h",
    GEM / "Code" / "Source" / "ExternalToolchainRegistry.cpp",
    GEM / "Code" / "Source" / "Editor" / "ExternalToolchainEditorSystemComponent.cpp",
    GEM / "Code" / "Source" / "Editor" / "ExternalToolchainDiagnosticsWidget.cpp",
    GEM / "Code" / "Tests" / "ExternalToolchainRegistryTests.cpp",
    GEM / "Registry" / "external_toolchain.setreg",
    GEM / "README.md",
    GEM / "docs" / "ARCHITECTURE.md",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"ERROR: {message}")


def main() -> int:
    for path in REQUIRED_FILES:
        require(path.is_file(), f"missing required file: {path.relative_to(ROOT)}")

    gem_json = json.loads((GEM / "gem.json").read_text(encoding="utf-8"))
    require(gem_json.get("gem_name") == "ExternalToolchain", "unexpected gem name")
    require(gem_json.get("type") == "Tool", "Gem must remain host-tools-only")

    engine_json = json.loads((ROOT / "engine.json").read_text(encoding="utf-8"))
    require(
        "Gems/ExternalToolchain" in engine_json.get("external_subdirectories", []),
        "engine.json does not register Gems/ExternalToolchain",
    )

    project_json = json.loads(
        (ROOT / "TaintedGrailModdingEditor" / "project.json").read_text(encoding="utf-8")
    )
    require(
        "ExternalToolchain" in project_json.get("gem_names", []),
        "TaintedGrailModdingEditor does not enable ExternalToolchain",
    )

    cmake = (GEM / "Code" / "CMakeLists.txt").read_text(encoding="utf-8")
    require("${gem_name}.API INTERFACE" in cmake, "public provider API target missing")
    require("${gem_name}.Tools" in cmake, "Tools alias missing")
    require("${gem_name}.Builders" in cmake, "Builders alias missing")
    require("${gem_name}.Clients" not in cmake, "runtime Client alias is prohibited")
    require("${gem_name}.Servers" not in cmake, "runtime Server alias is prohibited")
    require("${gem_name}.Unified" not in cmake, "runtime Unified alias is prohibited")

    bus = (GEM / "Code" / "Include" / "ExternalToolchain" / "ExternalToolchainBus.h").read_text(
        encoding="utf-8"
    )
    for token in (
        "AZ_RTTI(ExternalToolchainRequests",
        "ExternalToolchainRequestsTypeId",
        "RegisterProvider",
        "FinalizeProviderRegistration",
        "IsProviderRegistrationFinalized",
        "EnumerateProviders",
        "ExternalToolchainInterface",
    ):
        require(token in bus, f"public contract token missing: {token}")

    type_ids = (
        GEM / "Code" / "Include" / "ExternalToolchain" / "ExternalToolchainTypeIds.h"
    ).read_text(encoding="utf-8")
    require(
        "ExternalToolchainRequestsTypeId" in type_ids,
        "request interface TypeId is missing",
    )

    registry = (GEM / "Code" / "Source" / "ExternalToolchainRegistry.cpp").read_text(
        encoding="utf-8"
    )
    for token in (
        "Provider registration is already finalized",
        "Provider ID is already registered",
        "minimum host API version is incompatible",
        "Command IDs must be unique",
        "AZStd::sort",
    ):
        require(token in registry, f"registry invariant missing: {token}")

    settings = json.loads(
        (GEM / "Registry" / "external_toolchain.setreg").read_text(encoding="utf-8")
    )
    host = settings["O3DE"]["ExternalToolchain"]["Host"]
    require(host.get("ApiVersion") == "1.0.0", "host API version must be 1.0.0")
    require(host.get("AllowShellCommands") is False, "shell commands must be disabled")
    require(host.get("ProcessExecutionEnabled") is False, "execution must remain disabled")
    require(host.get("AssetHandoffEnabled") is False, "asset handoff must remain disabled")

    tests = (GEM / "Code" / "Tests" / "ExternalToolchainRegistryTests.cpp").read_text(
        encoding="utf-8"
    )
    require(tests.count("TEST(") >= 8, "expected focused registry test coverage")

    print("ExternalToolchain host-foundation contract validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
