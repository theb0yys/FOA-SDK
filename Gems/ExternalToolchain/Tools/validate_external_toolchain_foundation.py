#!/usr/bin/env python3
"""Validate the ExternalToolchain registration, configuration, and discovery contract."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
GEM = ROOT / "Gems" / "ExternalToolchain"
CODE = GEM / "Code"
INCLUDE = CODE / "Include" / "ExternalToolchain"
SOURCE = CODE / "Source"
EDITOR = SOURCE / "Editor"
TESTS = CODE / "Tests"


REQUIRED_FILES = (
    GEM / "gem.json",
    GEM / "CMakeLists.txt",
    CODE / "CMakeLists.txt",
    INCLUDE / "ExternalToolchainBus.h",
    INCLUDE / "ExternalToolchainTypeIds.h",
    INCLUDE / "ExternalToolchainTypes.h",
    SOURCE / "ExternalToolchainConfigurationService.h",
    SOURCE / "ExternalToolchainConfigurationService.cpp",
    SOURCE / "ExternalToolchainDiscoveryService.h",
    SOURCE / "ExternalToolchainDiscoveryService.cpp",
    SOURCE / "ExternalToolchainRegistry.cpp",
    EDITOR / "ExternalToolchainEditorSystemComponent.cpp",
    EDITOR / "ExternalToolchainDiagnosticsWidget.cpp",
    TESTS / "ExternalToolchainConfigurationTests.cpp",
    TESTS / "ExternalToolchainDiscoveryTests.cpp",
    TESTS / "ExternalToolchainRegistryTests.cpp",
    GEM / "Registry" / "external_toolchain.setreg",
    GEM / "README.md",
    GEM / "docs" / "ARCHITECTURE.md",
    GEM / "docs" / "DISCOVERY_AND_CONFIGURATION.md",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"ERROR: {message}")


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    for path in REQUIRED_FILES:
        require(path.is_file(), f"missing required file: {path.relative_to(ROOT)}")

    gem_json = json.loads(read(GEM / "gem.json"))
    require(gem_json.get("gem_name") == "ExternalToolchain", "unexpected gem name")
    require(gem_json.get("type") == "Tool", "Gem must remain host-tools-only")
    require(gem_json.get("version") == "0.2.0", "discovery slice Gem version must be 0.2.0")

    engine_json = json.loads(read(ROOT / "engine.json"))
    require(
        "Gems/ExternalToolchain" in engine_json.get("external_subdirectories", []),
        "engine.json does not register Gems/ExternalToolchain",
    )

    project_json = json.loads(
        read(ROOT / "TaintedGrailModdingEditor" / "project.json")
    )
    require(
        "ExternalToolchain" in project_json.get("gem_names", []),
        "TaintedGrailModdingEditor does not enable ExternalToolchain",
    )

    cmake = read(CODE / "CMakeLists.txt")
    require("${gem_name}.API INTERFACE" in cmake, "public provider API target missing")
    require("${gem_name}.Tools" in cmake, "Tools alias missing")
    require("${gem_name}.Builders" in cmake, "Builders alias missing")
    require("${gem_name}.Clients" not in cmake, "runtime Client alias is prohibited")
    require("${gem_name}.Servers" not in cmake, "runtime Server alias is prohibited")
    require("${gem_name}.Unified" not in cmake, "runtime Unified alias is prohibited")

    core_manifest = read(CODE / "externaltoolchain_core_files.cmake")
    for token in (
        "ExternalToolchainConfigurationService.cpp",
        "ExternalToolchainDiscoveryService.cpp",
        "ExternalToolchainRegistry.cpp",
    ):
        require(token in core_manifest, f"Core source manifest is missing {token}")

    types = read(INCLUDE / "ExternalToolchainTypes.h")
    for token in (
        "HostApiVersion{ 1, 1, 0 }",
        "ProjectConfigurationRoot",
        "UserConfigurationRoot",
        "ConfigurationLayer",
        "DiscoveryStatus",
        "ExternalToolConfigurationDescriptor",
        "ExternalToolDiscoveryProbeDescriptor",
        "ExternalToolResolvedConfigurationValue",
        "ExternalToolDiscoveryResult",
        "TryCompareSemanticVersions",
    ):
        require(token in types, f"public discovery/configuration type missing: {token}")

    bus = read(INCLUDE / "ExternalToolchainBus.h")
    for token in (
        "AZ_RTTI(ExternalToolchainRequests",
        "ExternalToolchainRequestsTypeId",
        "RegisterProvider",
        "FinalizeProviderRegistration",
        "SetSessionConfigurationOverride",
        "SetSessionProviderEnabled",
        "EnumerateResolvedConfiguration",
        "RefreshProviderDiscovery",
        "EnumerateDiscoveryResults",
        "GetDiscoveryResult",
        "ExternalToolchainInterface",
    ):
        require(token in bus, f"public contract token missing: {token}")

    registry = read(SOURCE / "ExternalToolchainRegistry.cpp")
    for token in (
        "Provider registration is already finalized",
        "Provider ID is already registered",
        "minimum host API version is incompatible",
        "Configuration keys must be unique",
        "Discovery probe IDs must be unique",
        "Discovery probes must reference one declared path configuration key",
        "Discovery minimum version must not exceed the maximum version",
        "NormalizeDescriptor",
        "AZStd::sort",
    ):
        require(token in registry, f"registry invariant missing: {token}")

    configuration = read(SOURCE / "ExternalToolchainConfigurationService.cpp")
    for token in (
        "ProjectConfigurationRoot",
        "UserConfigurationRoot",
        "ConfigurationLayer::Project",
        "ConfigurationLayer::User",
        "ConfigurationLayer::Session",
        "m_valueValid",
    ):
        require(token in configuration, f"configuration invariant missing: {token}")

    discovery = read(SOURCE / "ExternalToolchainDiscoveryService.cpp")
    for token in (
        "AZ::IO::SystemFile::Exists",
        "AZ::IO::SystemFile::IsDirectory",
        "Network and URI paths are prohibited",
        "Discovery paths must be absolute local paths",
        "parent traversal segments",
        "configured semantic tool version",
        "Multiple distinct compatible installations",
        "best-effort probe timeout",
        "Provider discovery budget",
        "AZStd::sort",
    ):
        require(token in discovery, f"discovery invariant missing: {token}")
    for forbidden in (
        "ProcessWatcher",
        "ProcessLauncher",
        "subprocess",
        "system(",
        "RemoteTools",
        "AssetProcessor",
    ):
        require(forbidden not in discovery, f"discovery service contains prohibited execution token: {forbidden}")

    settings = json.loads(read(GEM / "Registry" / "external_toolchain.setreg"))
    host = settings["O3DE"]["ExternalToolchain"]["Host"]
    require(host.get("ApiVersion") == "1.1.0", "host API version must be 1.1.0")
    require(host.get("AllowShellCommands") is False, "shell commands must be disabled")
    require(host.get("ProcessExecutionEnabled") is False, "execution must remain disabled")
    require(host.get("AssetHandoffEnabled") is False, "asset handoff must remain disabled")
    discovery_settings = host["Discovery"]
    require(discovery_settings.get("Enabled") is True, "local discovery must be enabled")
    require(discovery_settings.get("LocalPathsOnly") is True, "discovery must remain local-path-only")
    require(discovery_settings.get("ProbeExecutionEnabled") is False, "probe execution must remain disabled")
    require(1 <= discovery_settings.get("MaximumProviders", 0) <= 1024, "provider bound is invalid")
    require(1 <= discovery_settings.get("MaximumProbesPerProvider", 0) <= 128, "probe bound is invalid")
    require(1 <= discovery_settings.get("ProviderBudgetMilliseconds", 0) <= 5000, "provider time budget is invalid")

    widget = read(EDITOR / "ExternalToolchainDiagnosticsWidget.cpp")
    for token in (
        "Refresh local discovery",
        "Resolved configuration",
        "<hidden>",
        "does not launch tools",
    ):
        require(token in widget, f"diagnostics pane token missing: {token}")

    test_text = "\n".join(read(path) for path in TESTS.glob("*.cpp"))
    require(test_text.count("TEST(") >= 30, "expected at least thirty focused C++ tests")
    for token in (
        "SessionOverridesEveryPersistentLayer",
        "UnsupportedConfiguredVersionIsReported",
        "MultipleCompatibleInstallationsAreAmbiguous",
        "NetworkPathIsRejectedWithoutInspection",
        "ProviderCountLimitFailsClosed",
        "ProbeMustReferenceDeclaredPathConfiguration",
    ):
        require(token in test_text, f"focused negative test is missing: {token}")

    docs = read(GEM / "docs" / "DISCOVERY_AND_CONFIGURATION.md")
    for token in (
        "session > user > project > provider default",
        "not_installed",
        "unsupported_version",
        "misconfigured",
        "ambiguous",
        "does not claim to pre-empt",
        "does not prove application authenticity",
    ):
        require(token in docs, f"discovery documentation is missing: {token}")

    print("ExternalToolchain discovery/configuration contract validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
