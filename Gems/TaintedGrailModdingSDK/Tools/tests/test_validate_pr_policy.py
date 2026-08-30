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

import validate_pr_policy as policy


def classification_body(*selected: str) -> str:
    lines = ["## Change classification", ""]
    for identity in policy.CLASSIFICATION_IDS:
        state = "x" if identity in selected else " "
        lines.append(
            f"- [{state}] {identity} "
            f"<!-- change-classification:{identity} -->"
        )
    lines.extend(("", "## Scope", "Focused"))
    return "\n".join(lines)


class PullRequestPolicyTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        template = root / policy.TEMPLATE
        template.parent.mkdir(parents=True, exist_ok=True)
        template.write_text(
            "## Summary\n\n"
            + classification_body()
            + "\n\n## Out of scope\n\n"
            "## Design / architecture impact\n\n"
            "## Compatibility, data, and rollback\n\n"
            "## Validation performed\n\nPASSED:\nFAILED:\n"
            "NOT_RUN / NOT_APPLICABLE:\n\n"
            "## Security / protected-data impact\n\n"
            "## Documentation\n\n"
            "## Author self-review\n\n"
            "## Maintainer review\n",
            encoding="utf-8",
        )

        workflow = root / policy.WORKFLOW
        workflow.parent.mkdir(parents=True, exist_ok=True)
        workflow.write_text(
            "- name: Validate pull-request policy contract\n"
            "  run: python validate_pr_policy.py --event \"$GITHUB_EVENT_PATH\"\n",
            encoding="utf-8",
        )
        return root

    def test_template_and_workflow_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            policy.validate_template(self.make_repo(Path(temporary)))

    def test_superseded_merge_obligation_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = self.make_repo(Path(temporary))
            template = root / policy.TEMPLATE
            template.write_text(
                template.read_text(encoding="utf-8")
                + "\n## Mandatory merge obligations\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestPolicyError,
                "superseded fragment",
            ):
                policy.validate_template(root)

    def test_ready_pull_request_requires_exactly_one_classification(self) -> None:
        policy.validate_body(classification_body("significant"), draft=False)
        for selected in ((), ("routine", "significant")):
            with self.assertRaisesRegex(
                policy.PullRequestPolicyError,
                "exactly one change classification",
            ):
                policy.validate_body(classification_body(*selected), draft=False)

    def test_draft_pull_request_may_be_incomplete(self) -> None:
        policy.validate_body("", draft=True)

    def test_event_validates_non_draft_body(self) -> None:
        policy.validate_event(
            {
                "pull_request": {
                    "body": classification_body("routine"),
                    "draft": False,
                }
            }
        )

    def test_malformed_event_fails_closed(self) -> None:
        with self.assertRaisesRegex(
            policy.PullRequestPolicyError,
            "body or draft state is malformed",
        ):
            policy.validate_event(
                {"pull_request": {"body": classification_body("routine")}}
            )


if __name__ == "__main__":
    unittest.main()
