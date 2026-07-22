#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import test_validate_population_actor_troop_editor as fixture_module  # noqa: E402
from validate_population_actor_troop_editor import (  # noqa: E402
    PopulationEditorContractError,
    validate_population_actor_troop_editor,
)


_CURRENT_DESIGN = (
    "Status: implemented vertical slice\n"
    "6. **Complete** \u2014 immutable population action-lane derivation\n"
    "7. **Complete** \u2014 deterministic synthetic population fixture and full vertical-slice local-validation integration\n"
    "8. **Complete** \u2014 public user, architecture/data-format, release-readiness documentation and twenty-six-pane checklist\n"
    "9. **Active acceptance gate** \u2014 exact-head O3DE configure/build, compiled Catalog tests, and Windows UI evidence\n"
    "independently tracked actor, troop, and unstaged-member drafts\n"
    "does not claim that compiled tests have run in an exact-head configured build or that Windows UI evidence exists\n"
)


class PopulationActorTroopCurrentStateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = fixture_module.PopulationActorTroopEditorValidatorTests(
            methodName="test_valid_contract_passes"
        )
        self.fixture.setUp()
        self.repo_root = self.fixture.repo_root
        (self.repo_root / "ROADMAP.md").write_text(
            "Actors and population implemented vertical slice.\n",
            encoding="utf-8",
        )
        self.design = (
            self.repo_root
            / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
        )
        self.design.write_text(_CURRENT_DESIGN, encoding="utf-8")

    def tearDown(self) -> None:
        self.fixture.tearDown()

    def test_current_implemented_state_passes(self) -> None:
        validate_population_actor_troop_editor(self.repo_root)

    def test_fixture_regression_to_next_work_fails(self) -> None:
        self.design.write_text(
            _CURRENT_DESIGN.replace(
                "7. **Complete** \u2014 deterministic synthetic population fixture",
                "7. **Next** \u2014 deterministic synthetic population fixture",
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            PopulationEditorContractError,
            r"7\. \*\*Complete|7\. \*\*Next",
        ):
            validate_population_actor_troop_editor(self.repo_root)


if __name__ == "__main__":
    unittest.main()
