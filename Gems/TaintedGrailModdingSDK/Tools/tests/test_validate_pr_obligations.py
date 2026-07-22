from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

MODULE = Path(__file__).resolve().parents[1] / "validate_pr_obligations.py"
SPEC = importlib.util.spec_from_file_location("validate_pr_obligations_test_target", MODULE)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


def body(*, checked: bool = True, omit: str | None = None, duplicate: str | None = None) -> str:
    state = "x" if checked else " "
    lines = ["## Mandatory merge obligations", ""]
    for identity in validator.OBLIGATION_IDS:
        if identity == omit:
            continue
        line = f"- [{state}] completed <!-- merge-obligation:{identity} -->"
        lines.append(line)
        if identity == duplicate:
            lines.append(line)
    lines.extend(("", "## Boundary", "text"))
    return "\n".join(lines)


class PullRequestObligationTests(unittest.TestCase):
    def test_draft_pull_request_may_remain_incomplete(self) -> None:
        validator.validate_body("", draft=True)

    def test_ready_pull_request_requires_every_checked_obligation(self) -> None:
        validator.validate_body(body(), draft=False)

    def test_ready_pull_request_rejects_incomplete_obligation(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestObligationError,
            "incomplete mandatory merge obligations",
        ):
            validator.validate_body(body(checked=False), draft=False)

    def test_ready_pull_request_rejects_missing_marker(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestObligationError,
            "missing mandatory obligation markers",
        ):
            validator.validate_body(body(omit="compiled-tests"), draft=False)

    def test_ready_pull_request_rejects_duplicate_marker(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestObligationError,
            "appears more than once",
        ):
            validator.validate_body(body(duplicate="receipt"), draft=False)

    def test_non_pull_request_event_is_ignored(self) -> None:
        validator.validate_event({"ref": "refs/heads/main"})


if __name__ == "__main__":
    unittest.main()
