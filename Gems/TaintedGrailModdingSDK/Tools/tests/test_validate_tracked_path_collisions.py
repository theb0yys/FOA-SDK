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
from unittest import mock


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "validate_tracked_path_collisions.py"
SPEC = importlib.util.spec_from_file_location("tg_validate_tracked_path_collisions", SCRIPT_PATH)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


class TrackedPathCollisionTests(unittest.TestCase):
    def test_distinct_paths_pass(self) -> None:
        self.assertEqual(
            validator.find_collisions(
                (
                    "SECURITY.md",
                    ".github/PULL_REQUEST_TEMPLATE.md",
                    "Gems/TaintedGrailModdingSDK/gem.json",
                )
            ),
            (),
        )

    def test_case_only_collision_is_reported(self) -> None:
        self.assertEqual(
            validator.find_collisions(("SECURITY.MD", "SECURITY.md")),
            (("SECURITY.MD", "SECURITY.md"),),
        )

    def test_nested_case_collision_is_reported(self) -> None:
        self.assertEqual(
            validator.find_collisions(
                (
                    ".github/PULL_REQUEST_TEMPLATE.md",
                    ".github/pull_request_template.md",
                )
            ),
            (
                (
                    ".github/PULL_REQUEST_TEMPLATE.md",
                    ".github/pull_request_template.md",
                ),
            ),
        )

    def test_unicode_normalization_collision_is_reported(self) -> None:
        composed = "docs/caf\N{LATIN SMALL LETTER E WITH ACUTE}.md"
        decomposed = "docs/cafe\N{COMBINING ACUTE ACCENT}.md"
        self.assertEqual(
            validator.find_collisions((composed, decomposed)),
            ((decomposed, composed),),
        )

    def test_exact_duplicates_are_ignored(self) -> None:
        self.assertEqual(
            validator.find_collisions(("README.md", "README.md")),
            (),
        )

    def test_validate_repository_reports_all_paths(self) -> None:
        with mock.patch.object(
            validator,
            "read_tracked_paths",
            return_value=("SECURITY.md", "SECURITY.MD"),
        ):
            with self.assertRaisesRegex(
                validator.TrackedPathCollisionError,
                r"SECURITY\.MD <> SECURITY\.md",
            ):
                validator.validate_repository(Path("/repo"))

    def test_main_returns_failure_for_collision(self) -> None:
        with mock.patch.object(
            validator,
            "validate_repository",
            side_effect=validator.TrackedPathCollisionError("collision"),
        ):
            self.assertEqual(validator.main(["--repo-root", "."]), 1)


if __name__ == "__main__":
    unittest.main()
