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


class PullRequestObligationPolicyTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        workflow = root / policy.WORKFLOW
        workflow.parent.mkdir(parents=True, exist_ok=True)
        workflow.write_text(
            "on:\n"
            "  pull_request:\n"
            "    types: [ready_for_review]\n"
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
            "## Mandatory merge obligations\n"
            "<!-- merge-head:REPLACE_WITH_CURRENT_40_CHARACTER_HEAD_SHA -->\n"
            + "\n".join(
                f"- [ ] required <!-- merge-obligation:{identity} -->"
                for identity in policy.OBLIGATION_IDS
            ),
            encoding="utf-8",
        )

        runtime = root / policy.RUNTIME
        runtime.parent.mkdir(parents=True, exist_ok=True)
        runtime.write_text(
            "if draft:\n"
            "    pass\n"
            "HEAD_MARKER_RE GIT_SHA_RE head_sha\n"
            "missing its exact merge-head marker\n"
            "merge-head marker appears more than once\n"
            "malformed merge-head marker\n"
            "merge obligations are stale\n"
            "incomplete mandatory merge obligations\n"
            "appears more than once\n"
            "unsupported obligation markers\n"
            + "\n".join(f'\"{identity}\"' for identity in policy.OBLIGATION_IDS),
            encoding="utf-8",
        )
        return root

    def test_read_only_exact_head_policy_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            policy.validate(self.make_repo(Path(temporary)))

    def test_pull_request_target_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / policy.WORKFLOW
            workflow.write_text(
                workflow.read_text(encoding="utf-8")
                + "pull_request_target:\n",
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
                workflow.read_text(encoding="utf-8")
                + "pull-requests: write\n",
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


if __name__ == "__main__":
    unittest.main()
