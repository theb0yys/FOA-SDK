# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

import sys
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import validate_workspace_atomicity_schema as validator


class WorkspaceTransitionValidatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo = TOOLS_ROOT.parents[2]
        cls.source = (TOOLS_ROOT.parent / "Code/Source/FoundationService.cpp").read_text(encoding="utf-8")

    def test_current_repository_contract(self):
        validator.validate_workspace_contract(self.repo)

    def test_missing_transition_steps_are_rejected(self):
        for step in (
            "BuildCandidate(filePath)",
            "if (!BeginWorkspaceChange())",
            "ClearWorkspaceScopedState(true)",
            "m_catalogFilePath = AZStd::move(candidate.m_catalogFilePath)",
            "FinishWorkspaceChange();",
            "RefreshSnapshot();",
            "FoundationNotificationBus::Broadcast(&FoundationNotifications::OnWorkspaceChanged, *this);",
            "m_workspaceChangeInProgress = false;",
            "void FoundationService::FinishWorkspaceChange()",
        ):
            with self.subTest(step=step):
                self.assertIn(step, self.source)
                with self.assertRaises(validator.WorkspaceContractError):
                    validator.validate_workspace_transition(self.source.replace(step, ""))

    def test_reordered_transition_steps_are_rejected(self):
        for step, before in (
            ("ClearWorkspaceScopedState(true);", "if (!BeginWorkspaceChange())"),
            ("FinishWorkspaceChange();", "m_catalogFilePath = AZStd::move(candidate.m_catalogFilePath);"),
            ("RefreshSnapshot();", "m_workspaceChangeInProgress = false;"),
        ):
            with self.subTest(step=step):
                changed = self.source.replace(step, "")
                # The first two mutations publish before admission/completion; the
                # third moves snapshot refresh after the post-commit notification.
                changed = changed.replace(before, step + "\n        " + before)
                with self.assertRaises(validator.WorkspaceContractError):
                    validator.validate_workspace_transition(changed)

    def test_pre_candidate_live_mutation_is_rejected(self):
        for mutation in ("ReloadCatalog();", "ReloadSourceEvidence();", "RefreshSnapshot();"):
            with self.subTest(mutation=mutation):
                changed = self.source.replace(
                    "auto candidateResult =", mutation + "\n        auto candidateResult ="
                )
                with self.assertRaises(validator.WorkspaceContractError):
                    validator.validate_workspace_transition(changed)


if __name__ == "__main__":
    unittest.main()
