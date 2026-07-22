#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(TOOLS_ROOT))

import validate_tainted_system_ports as validator

FIXTURE_PATHS = (
    "Gems/TaintedGrailModdingSDK/Code/Source/SourceEvidenceIntakeWidget.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/TaintedFrameworkKnowledge.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/RoadAtlasExtension.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/AvalonAiExtension.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/AvalonAiExtension.h",
    "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.h",
    "Gems/TaintedGrailModdingSDK/Code/Tests/GameInformationAcquisitionTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/RoadAtlasExtensionTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/AvalonAiExtensionTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/FoARuntimeAdapterRoutesTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/TaintedFrameworkKnowledgeTests.cpp",
    "Gems/TaintedGrailModdingSDK/Code/Tests/TaintedInterfaceUiUtilitiesTests.cpp",
    (
        "Gems/TaintedGrailModdingSDK/Code/"
        "taintedgrailmoddingsdk_framework_files.cmake"
    ),
    (
        "Gems/TaintedGrailModdingSDK/Code/"
        "taintedgrailmoddingsdk_core_files.cmake"
    ),
    (
        "Gems/TaintedGrailModdingSDK/Code/"
        "taintedgrailmoddingsdk_extension_api_tests_files.cmake"
    ),
    (
        "Gems/TaintedGrailModdingSDK/Code/"
        "taintedgrailmoddingsdk_tainted_framework_tests_files.cmake"
    ),
    "Research/tainted-grail-system-ports/README.md",
    "Research/tainted-grail-system-ports/SOURCE_REGISTER.md",
    "Research/tainted-grail-system-ports/PORT_GATES.md",
)


class TaintedSystemPortValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="tainted-system-port-")
        self.root = Path(self.temporary.name)
        for relative in FIXTURE_PATHS:
            source = REPO_ROOT / relative
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def mutate(self, relative: str, old: str, new: str) -> None:
        path = self.root / relative
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def assert_fails(self, fragment: str) -> None:
        with self.assertRaisesRegex(validator.TaintedSystemPortError, fragment):
            validator.validate(self.root)

    def test_repository_fixture_passes(self) -> None:
        validator.validate(self.root)

    def test_millisecond_timestamp_regression_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/SourceEvidenceIntakeWidget.cpp",
            "toString(Qt::ISODate)",
            "toString(Qt::ISODateWithMs)",
        )
        self.assert_fails("whole-second UTC")

    def test_unlinked_framework_source_fails(self) -> None:
        self.mutate(
            (
                "Gems/TaintedGrailModdingSDK/Code/"
                "taintedgrailmoddingsdk_framework_files.cmake"
            ),
            "    Source/TaintedFrameworkKnowledge.cpp\n",
            "",
        )
        self.assert_fails("TaintedFrameworkKnowledge.cpp")

    def test_canonical_framework_branch_drift_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedFrameworkKnowledge.cpp",
            'm_supportedBranches = { "Mono" }',
            'm_supportedBranches = { "mono" }',
        )
        self.assert_fails("Canonical branch")

    def test_asset_payload_escalation_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.h",
            "m_upstreamPayloadEmbedded = false",
            "m_upstreamPayloadEmbedded = true",
        )
        self.assert_fails("Payload boundary")

    def test_licence_boundary_removal_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/TaintedInterfaceUiUtilities.cpp",
            '"kane.tgfoa.tainted-interface",\n            "NOASSERTION",',
            '"kane.tgfoa.tainted-interface",\n            "unknown",',
        )
        self.assert_fails("Licence boundary")

    def test_scope_exclusion_removal_fails(self) -> None:
        self.mutate(
            "Research/tainted-grail-system-ports/README.md",
            "Waning or Bannerlord",
            "unrelated external",
        )
        self.assert_fails("Scope exclusion")

    def test_merlin_auto_promotion_escalation_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.cpp",
            "m_canPromoteAutomatically = false",
            "m_canPromoteAutomatically = true",
        )
        self.assert_fails("m_canPromoteAutomatically = false")

    def test_runtime_route_authority_escalation_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp",
            'AppendBool(output, "build", route.m_buildAllowed)',
            'AppendBool(output, "build", true)',
        )
        self.assert_fails("route.m_buildAllowed")

    def test_runtime_route_registry_dependency_removal_fails(self) -> None:
        self.mutate(
            "Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.h",
            "const SourceEvidenceRegistry& evidenceRegistry",
            "const AZStd::string& evidenceRegistry",
        )
        self.assert_fails("Runtime evidence dependency")


if __name__ == "__main__":
    unittest.main()
