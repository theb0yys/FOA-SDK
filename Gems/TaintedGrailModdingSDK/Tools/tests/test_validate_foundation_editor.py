# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_ROOT))

import validate_foundation as validator

GEM_ROOT = TOOLS_ROOT.parent


class FoundationEditorValidatorTests(unittest.TestCase):
    def test_current_home_and_workspace_contract_passes(self) -> None:
        validator.validate_editor_foundation(GEM_ROOT)

    def test_missing_workspace_save_route_fails(self) -> None:
        source_root, sources = validator.read_sources(GEM_ROOT)
        with patch.object(
            validator,
            "read_sources",
            return_value=(source_root, sources.replace("SaveWorkspace", "RemovedSave")),
        ):
            with self.assertRaisesRegex(RuntimeError, "SaveWorkspace"):
                validator.validate_editor_foundation(GEM_ROOT)

    def test_missing_home_authoring_group_fails(self) -> None:
        source_root, sources = validator.read_sources(GEM_ROOT)
        with patch.object(
            validator,
            "read_sources",
            return_value=(source_root, sources.replace('tr("Create and edit")', 'tr("Removed")')),
        ):
            with self.assertRaisesRegex(RuntimeError, "Create and edit"):
                validator.validate_editor_foundation(GEM_ROOT)


if __name__ == "__main__":
    unittest.main()
