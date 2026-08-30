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

import validate_pr_obligation_policy as policy


class PullRequestDeclarationPolicyTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        workflow = root / policy.WORKFLOW
        workflow.parent.mkdir(parents=True, exist_ok=True)
        workflow.write_text(
            "on:\n"
            "  pull_request:\n"
            "    types: [opened, synchronize, reopened, ready_for_review]\n"
            "  push:\n"
            "    branches: [main]\n"
            "  workflow_dispatch:\n"
            "permissions:\n"
            "  contents: read\n"
            "jobs:\n"
            "  static-validation:\n"
            "    runs-on: ubuntu-latest\n"
            "    steps:\n"
            "      - uses: actions/checkout@v4\n"
            "        with:\n"
            "          ref: ${{ github.event.pull_request.head.sha || github.sha }}\n"
            "          persist-credentials: false\n"
            "          fetch-depth: 0\n"
            "      - name: Validate pull-request declarations\n"
            "        run: python validate_pr_obligations.py --event \"$GITHUB_EVENT_PATH\"\n"
            "      - run: echo ${{ github.event.pull_request.base.sha }}\n"
            "      - run: git diff --check base head\n"
            "  canonical-interchange-compiled:\n"
            "    runs-on: windows-2022\n"
            "    steps:\n"
            "      - uses: actions/checkout@v4\n"
            "        with:\n"
            "          ref: ${{ github.event.pull_request.head.sha || github.sha }}\n"
            "          persist-credentials: false\n"
            "      - run: ctest --no-tests=error\n"
            "  windows-prerequisites:\n"
            "    name: Windows O3DE prerequisites\n"
            "    runs-on: windows-2022\n"
            "    steps:\n"
            "      - uses: actions/checkout@v4\n"
            "        with:\n"
            "          ref: ${{ github.event.pull_request.head.sha || github.sha }}\n"
            "          persist-credentials: false\n"
            "      - run: python developer_preview.py prerequisites\n",
            encoding="utf-8",
        )

        template = root / policy.TEMPLATE
        template.parent.mkdir(parents=True, exist_ok=True)
        template.write_text(
            "## Summary\nsubstantive\n\n"
            "## Change class\n"
            "- [ ] Routine <!-- change-class:routine -->\n"
            "- [ ] Significant <!-- change-class:significant -->\n"
            "- [ ] Critical <!-- change-class:critical -->\n\n"
            "## Scope\nsubstantive\n\n"
            "## Out of scope\nsubstantive\n\n"
            "## Validation\nPASSED FAILED PARTIAL BLOCKED NOT_RUN NOT_APPLICABLE\n\n"
            "## Risks and rollback\nsubstantive\n\n"
            "Validation claims describe only commands and evidence that actually ran\n",
            encoding="utf-8",
        )

        runtime = root / policy.RUNTIME
        runtime.parent.mkdir(parents=True, exist_ok=True)
        runtime.write_text(
            "REQUIRED_SECTIONS\n"
            "CHANGE_CLASSES\n"
            "STATUS_TOKENS\n"
            "if draft:\n"
            "    return\n"
            "missing required sections\n"
            "select exactly one change class\n"
            "has no substantive content\n"
            "must include an exact validation status\n"
            "Pull request declaration validation failed\n",
            encoding="utf-8",
        )
        return root

    def test_read_only_proportional_policy_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            policy.validate(self.make_repo(Path(temporary)))

    def test_pull_request_target_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / policy.WORKFLOW
            workflow.write_text(
                workflow.read_text(encoding="utf-8") + "pull_request_target:\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestObligationPolicyError,
                "pull_request_target",
            ):
                policy.validate(repo)

    def test_write_permission_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / policy.WORKFLOW
            workflow.write_text(
                workflow.read_text(encoding="utf-8") + "pull-requests: write\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestObligationPolicyError,
                "pull-requests: write",
            ):
                policy.validate(repo)

    def test_static_job_requires_reviewed_range(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / policy.WORKFLOW
            workflow.write_text(
                workflow.read_text(encoding="utf-8").replace(
                    "git diff --check",
                    "git diff --stat",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestObligationPolicyError,
                "git diff --check",
            ):
                policy.validate(repo)

    def test_template_requires_each_change_class_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            template = repo / policy.TEMPLATE
            template.write_text(
                template.read_text(encoding="utf-8").replace(
                    "<!-- change-class:critical -->",
                    "",
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestObligationPolicyError,
                "change-class:critical",
            ):
                policy.validate(repo)

    def test_legacy_merge_obligation_markers_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            template = repo / policy.TEMPLATE
            template.write_text(
                template.read_text(encoding="utf-8")
                + "\n<!-- merge-obligation:receipt -->\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestObligationPolicyError,
                "merge-obligation",
            ):
                policy.validate(repo)


if __name__ == "__main__":
    unittest.main()
