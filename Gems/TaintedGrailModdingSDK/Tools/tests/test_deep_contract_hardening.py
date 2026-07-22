# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[4]
SOURCE = REPO_ROOT / "Gems/TaintedGrailModdingSDK/Code/Source"
TESTS = REPO_ROOT / "Gems/TaintedGrailModdingSDK/Code/Tests"
MANIFEST = (
    REPO_ROOT
    / "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_extension_api_tests_files.cmake"
)
WORKFLOW = REPO_ROOT / ".github/workflows/tainted-grail-sdk-pr-validation.yml"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")


class DeepContractHardeningTests(unittest.TestCase):
    def assert_fragments(self, text: str, *fragments: str) -> None:
        for fragment in fragments:
            self.assertIn(fragment, text)

    def test_pr_governor_and_head_validation_use_distinct_concurrency_lanes(self) -> None:
        workflow = read(WORKFLOW)
        self.assertIn(
            "${{ github.workflow }}-${{ github.event_name }}-${{ github.event.pull_request.number || github.ref }}",
            workflow,
        )
        self.assertIn("pull_request_target:", workflow)
        self.assertIn("github.event.pull_request.base.sha", workflow)
        self.assertIn("github.event_name != 'pull_request_target'", workflow)

    def test_serializer_is_byte_safe_locale_independent_and_compiled(self) -> None:
        source = read(SOURCE / "DeterministicContractJson.h")
        tests = read(TESTS / "DeterministicContractJsonTests.cpp")
        manifest = read(MANIFEST)
        self.assert_fragments(
            source,
            "std::to_chars",
            "std::chars_format::general",
            "std::numeric_limits<double>::max_digits10",
            "std::isfinite",
            "byte < 0x20 || byte >= 0x7f",
        )
        self.assertNotIn("std::snprintf", source)
        self.assert_fragments(
            tests,
            "NonFiniteDoublesRemainValidJsonTokens",
            "HighAndControlBytesAreEscapedDeterministically",
        )
        self.assertIn("Tests/DeterministicContractJsonTests.cpp", manifest)

    def test_runtime_routes_require_canonical_paths_and_typed_states(self) -> None:
        source = read(SOURCE / "FoARuntimeAdapterRoutes.cpp")
        tests = read(TESTS / "FoARuntimeAdapterRoutesTests.cpp")
        self.assert_fragments(
            source,
            "IsCanonicalRelativePosixPath",
            'component == "." || component == ".."',
            "IsValidEvidenceState",
            "IsStableContractId(capability)",
        )
        self.assert_fragments(
            tests,
            "DotAndEmptyPathComponentsFailClosed",
            "UnknownEvidenceStateFailsClosed",
            "NonNamespacedCapabilityFailsClosed",
        )

    def test_road_atlas_nested_inputs_are_bounded(self) -> None:
        source = read(SOURCE / "RoadAtlasExtension.cpp")
        tests = read(TESTS / "RoadAtlasExtensionTests.cpp")
        self.assert_fragments(
            source,
            "MaximumConnectedSegmentCount",
            "MaximumTagCount",
            "MaximumEvidenceRequirementCount",
            "MaximumEvidenceIdsPerRequirement",
            "IsValidPromotionState",
            "IsValidEvidenceRequirementKind",
            "MaximumGeometryPointCount - totalGeometryPoints",
        )
        self.assert_fragments(
            tests,
            "OutOfRangeEnumValuesFailClosed",
            "NestedCollectionBoundsFailClosed",
            "ControlCharactersInNestedNotesFailClosed",
            "GeometryLimitFailsClosedWithoutOverflow",
        )

    def test_avalon_nested_plans_and_storage_identity_are_bounded(self) -> None:
        source = read(SOURCE / "AvalonAiExtension.cpp")
        tests = read(TESTS / "AvalonAiExtensionTests.cpp")
        self.assert_fragments(
            source,
            "MaximumActorRoles",
            "MaximumCapabilities",
            "MaximumConditionsPerGoal",
            "MaximumConditionsPerAction",
            "MaximumEffectsPerAction",
            "IsValidBlackboardScope",
            "SameStorageIdentity",
            "blackboard-key.storage-alias",
        )
        self.assert_fragments(
            tests,
            "UnknownBlackboardScopeFailsClosed",
            "DistinctKeyIdsCannotAliasStorageIdentity",
            "NestedPlannerCollectionsAreBounded",
            "TopLevelRolesAndCapabilitiesAreBounded",
        )


if __name__ == "__main__":
    unittest.main()
