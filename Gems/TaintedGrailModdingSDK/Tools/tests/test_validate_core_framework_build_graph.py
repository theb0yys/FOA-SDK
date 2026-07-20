#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "validate_core_framework_build_graph.py"
SPEC = importlib.util.spec_from_file_location("tg_validate_core_framework_build_graph", SCRIPT_PATH)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


class CoreFrameworkBuildGraphValidatorTests(unittest.TestCase):
    def test_parse_manifest_text_reads_source_and_test_entries(self) -> None:
        self.assertEqual(
            validator.parse_manifest_text(
                """
set(FILES
    Source/Core.cpp
    Source/Core.h
    Tests/CoreTests.cpp
)
"""
            ),
            ("Source/Core.cpp", "Source/Core.h", "Tests/CoreTests.cpp"),
        )

    def test_find_call_block_returns_matching_target(self) -> None:
        cmake = """
ly_add_target(
    NAME Other STATIC
)
ly_add_target(
    NAME ${gem_name}.Core.Static STATIC
    BUILD_DEPENDENCIES PUBLIC AZ::AzCore
)
"""
        block = validator.find_call_block(
            cmake,
            "ly_add_target",
            "NAME ${gem_name}.Core.Static STATIC",
        )
        self.assertIn("AZ::AzCore", block)
        self.assertNotIn("NAME Other", block)

    def test_unique_production_ownership_accepts_one_owner(self) -> None:
        validator.validate_unique_production_ownership(
            ("Source/Core.cpp", "Source/Framework.cpp", "Source/Editor.cpp"),
            {
                "Core": ("Source/Core.cpp",),
                "Framework": ("Source/Framework.cpp",),
                "Editor": ("Source/Editor.cpp",),
            },
        )

    def test_duplicate_production_ownership_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            validator.BuildGraphContractError,
            "duplicate ownership",
        ):
            validator.validate_unique_production_ownership(
                ("Source/Core.cpp",),
                {
                    "Core": ("Source/Core.cpp",),
                    "Framework": ("Source/Core.cpp",),
                    "Editor": (),
                },
            )

    def test_unowned_production_source_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            validator.BuildGraphContractError,
            "unowned production sources",
        ):
            validator.validate_unique_production_ownership(
                ("Source/Core.cpp", "Source/Missing.cpp"),
                {"Core": ("Source/Core.cpp",), "Framework": (), "Editor": ()},
            )

    def test_test_manifest_cannot_recompile_production(self) -> None:
        with self.assertRaisesRegex(
            validator.BuildGraphContractError,
            "recompiles production sources",
        ):
            validator.validate_test_manifest(
                ("Source/Core.cpp", "Tests/CoreTests.cpp"),
                "test manifest",
            )

    def test_core_role_rejects_ui_or_persistence_ownership(self) -> None:
        with self.assertRaisesRegex(
            validator.BuildGraphContractError,
            "outside its boundary",
        ):
            validator.validate_role_names(
                "Core",
                ("Source/CatalogDatabase.cpp", "Source/WorkspacePersistenceService.cpp"),
                validator.CORE_FORBIDDEN_NAME_PARTS,
            )


if __name__ == "__main__":
    unittest.main()
