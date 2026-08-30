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

import validate_pr_policy as policy


class PullRequestPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = Path(__file__).resolve().parents[4]
        cls.template = (cls.repo_root / policy.TEMPLATE).read_text(encoding="utf-8")
        cls.workflow = (cls.repo_root / policy.WORKFLOW).read_text(encoding="utf-8")

    def test_real_repository_policy_passes(self) -> None:
        policy.validate(self.repo_root)

    def test_duplicate_classification_marker_is_rejected(self) -> None:
        marker = "<!-- change-classification:routine -->"
        with self.assertRaisesRegex(
            policy.PullRequestPolicyError,
            "exactly one",
        ):
            policy.validate_template(self.template + "\n" + marker + "\n")

    def test_missing_required_heading_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            policy.PullRequestPolicyError,
            "## Scope",
        ):
            policy.validate_template(self.template.replace("## Scope", "## Changed area"))

    def test_obsolete_merge_obligation_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            policy.PullRequestPolicyError,
            "obsolete universal gate",
        ):
            policy.validate_template(
                self.template + "\n## Mandatory merge obligations\n"
            )

    def test_old_runtime_validator_reference_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            policy.PullRequestPolicyError,
            "validate_pr_obligations.py",
        ):
            policy.validate_workflow(
                self.workflow + "\nrun: python validate_pr_obligations.py\n"
            )


if __name__ == "__main__":
    unittest.main()
