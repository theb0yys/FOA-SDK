#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from run_local_validation import VALIDATORS
from validate_installer_workflow import (
    REQUIRED_FILE_FRAGMENTS,
    InstallerWorkflowValidationError,
    validate_installer_workflow,
)


class InstallerWorkflowValidatorTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        for relative_path, fragments in REQUIRED_FILE_FRAGMENTS.items():
            path = repo / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("\n".join(fragments) + "\n", encoding="utf-8")
        runner = repo / "Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py"
        runner.parent.mkdir(parents=True, exist_ok=True)
        runner.write_text('"validate_installer_workflow.py"\n', encoding="utf-8")
        return repo

    def test_approved_installer_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            validate_installer_workflow(self.make_repo(Path(temporary)))

    def test_missing_exact_inventory_gate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / ".github/workflows/tainted-grail-sdk-installer.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8").replace("approved_inventory_sha256:", ""),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(InstallerWorkflowValidationError, "approved_inventory"):
                validate_installer_workflow(repo)

    def test_public_release_command_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / ".github/workflows/tainted-grail-sdk-installer.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8") + "gh release create v0.1.0\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(InstallerWorkflowValidationError, "public action"):
                validate_installer_workflow(repo)

    def test_installer_validator_is_in_authoritative_gate(self) -> None:
        self.assertIn("validate_installer_workflow.py", VALIDATORS)

    def test_launcher_source_is_owned_by_installer_component(self) -> None:
        installer_source = (
            "Gems/TaintedGrailModdingSDK/Installer/Source/Windows/InstalledEditorLauncher.cpp"
        )
        legacy_core_source = (
            "Gems/TaintedGrailModdingSDK/Code/Source/Platform/Windows/InstalledEditorLauncher.cpp"
        )
        self.assertIn(installer_source, REQUIRED_FILE_FRAGMENTS)
        self.assertNotIn(legacy_core_source, REQUIRED_FILE_FRAGMENTS)


if __name__ == "__main__":
    unittest.main()
