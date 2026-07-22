# SPDX-License-Identifier: Apache-2.0 OR MIT
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
            "  pull_request_target:\n"
            "    types: [auto_merge_enabled]\n"
            "jobs:\n"
            "  enforce-obligations:\n"
            "    name: Enforce ready-PR obligations\n"
            "    if: github.event_name == 'pull_request_target'\n"
            "    permissions:\n"
            "      pull-requests: write\n"
            "    steps:\n"
            "      - uses: actions/checkout@v4\n"
            "        with:\n"
            "          ref: ${{ github.event.pull_request.base.sha }}\n"
            "          persist-credentials: false\n"
            "      - run: python validate_pr_obligations.py\n"
            "      - run: echo convertPullRequestToDraft PULL_REQUEST_NODE_ID\n"
            "  static-validation:\n"
            "    if: github.event_name != 'pull_request_target'\n"
            "    steps:\n"
            "      - name: Validate mandatory merge obligations\n"
            "        run: python validate_pr_obligations.py --event \"$GITHUB_EVENT_PATH\"\n"
            "      - uses: actions/checkout@v4\n"
            "        with:\n"
            "          ref: ${{ github.event.pull_request.head.sha }}\n"
            "      - run: echo ${{ github.event.pull_request.base.sha }}\n"
            "      - run: git diff --check base head\n"
            "  windows-prerequisites:\n"
            "    name: Windows O3DE prerequisites\n"
            "    steps:\n"
            "      - uses: actions/checkout@v4\n"
            "        with:\n"
            "          ref: ${{ github.event.pull_request.head.sha }}\n",
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

    def test_base_bound_governor_and_exact_head_checks_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            policy.validate(self.make_repo(Path(temporary)))

    def test_privileged_governor_cannot_checkout_pr_head(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            workflow = repo / policy.WORKFLOW
            workflow.write_text(
                workflow.read_text(encoding="utf-8").replace(
                    "github.event.pull_request.base.sha",
                    "github.event.pull_request.head.sha",
                    1,
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                policy.PullRequestObligationPolicyError,
                "Privileged PR-target job",
            ):
                policy.validate(repo)

    def test_read_only_jobs_require_exact_head_and_reviewed_range(self) -> None:
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
                "Read-only exact-head jobs",
            ):
                policy.validate(repo)


if __name__ == "__main__":
    unittest.main()
