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

from validate_ci_runner_policy import (  # noqa: E402
    AUTOMATIC_STATIC_WORKFLOW,
    CiRunnerPolicyError,
    REMOVED_AUTOMATIC_WORKFLOWS,
    validate_agent_mode,
    validate_ci_runner_policy,
)


class AgentCiPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = Path(__file__).resolve().parents[4]
        cls.workflow = (cls.repo_root / AUTOMATIC_STATIC_WORKFLOW).read_text(encoding="utf-8")

    def test_real_repository_agent_policy_passes(self) -> None:
        validate_ci_runner_policy(self.repo_root)

    def test_pull_request_write_permission_is_rejected(self) -> None:
        with self.assertRaisesRegex(CiRunnerPolicyError, "pull-requests: write"):
            validate_agent_mode(self.repo_root, self.workflow + "\npull-requests: write\n")

    def test_pull_request_target_is_rejected(self) -> None:
        with self.assertRaisesRegex(CiRunnerPolicyError, "pull_request_target"):
            validate_agent_mode(self.repo_root, self.workflow + "\npull_request_target:\n")

    def test_removed_automatic_workflows_remain_absent(self) -> None:
        for relative_path in REMOVED_AUTOMATIC_WORKFLOWS:
            self.assertFalse((self.repo_root / relative_path).exists(), relative_path)


if __name__ == "__main__":
    unittest.main()
