#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

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


def body(
    *,
    selected: str | None = "routine",
    status: str = "PASSED",
    omit_section: str | None = None,
    empty_section: str | None = None,
    duplicate_section: str | None = None,
    duplicate_class: str | None = None,
    include_unknown_class: bool = False,
) -> str:
    section_content = {
        "Summary": "Repairs the bounded repository behavior.",
        "Change class": "",
        "Scope": "- affected/file.py",
        "Out of scope": "- runtime behavior",
        "Validation": f"Status: {status}\n- python focused_test.py",
        "Risks and rollback": "Low risk; revert the commit.",
    }

    class_lines: list[str] = []
    for identity in validator.CHANGE_CLASSES:
        state = "x" if identity == selected else " "
        line = f"- [{state}] class <!-- change-class:{identity} -->"
        class_lines.append(line)
        if identity == duplicate_class:
            class_lines.append(line)
    if include_unknown_class:
        class_lines.append("- [ ] unknown <!-- change-class:unknown -->")
    section_content["Change class"] = "\n".join(class_lines)

    lines: list[str] = []
    for name in validator.REQUIRED_SECTIONS:
        if name == omit_section:
            continue
        lines.extend((f"## {name}", ""))
        if name == empty_section:
            lines.append("<!-- placeholder only -->")
        else:
            lines.append(section_content[name])
        lines.append("")
        if name == duplicate_section:
            lines.extend((f"## {name}", section_content[name], ""))

    lines.extend(("## Documentation", "NOT_APPLICABLE"))
    return "\n".join(lines)


class PullRequestDeclarationTests(unittest.TestCase):
    def test_draft_pull_request_may_remain_incomplete(self) -> None:
        validator.validate_body("", draft=True)

    def test_ready_pull_request_accepts_complete_routine_record(self) -> None:
        validator.validate_body(body(), draft=False)

    def test_ready_pull_request_accepts_each_change_class(self) -> None:
        for identity in validator.CHANGE_CLASSES:
            with self.subTest(identity=identity):
                validator.validate_body(body(selected=identity), draft=False)

    def test_ready_pull_request_requires_every_section(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "missing required sections",
        ):
            validator.validate_body(
                body(omit_section="Out of scope"),
                draft=False,
            )

    def test_ready_pull_request_rejects_empty_substantive_section(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "has no substantive content",
        ):
            validator.validate_body(
                body(empty_section="Summary"),
                draft=False,
            )

    def test_ready_pull_request_requires_exactly_one_change_class(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "select exactly one change class",
        ):
            validator.validate_body(body(selected=None), draft=False)

        multiple = body(selected="routine").replace(
            "- [ ] class <!-- change-class:significant -->",
            "- [x] class <!-- change-class:significant -->",
        )
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "select exactly one change class",
        ):
            validator.validate_body(multiple, draft=False)

    def test_ready_pull_request_rejects_duplicate_class_marker(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "appears more than once",
        ):
            validator.validate_body(
                body(duplicate_class="routine"),
                draft=False,
            )

    def test_ready_pull_request_rejects_unknown_change_class(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "unsupported change classes",
        ):
            validator.validate_body(
                body(include_unknown_class=True),
                draft=False,
            )

    def test_ready_pull_request_rejects_duplicate_section(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "section 'Scope' appears more than once",
        ):
            validator.validate_body(
                body(duplicate_section="Scope"),
                draft=False,
            )

    def test_validation_section_requires_exact_status(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "exact validation status",
        ):
            validator.validate_body(body(status="complete"), draft=False)

    def test_ready_event_uses_body_and_draft_state(self) -> None:
        validator.validate_event(
            {"pull_request": {"body": body(selected="significant"), "draft": False}}
        )

    def test_malformed_pull_request_payload_fails_closed(self) -> None:
        with self.assertRaisesRegex(
            validator.PullRequestDeclarationError,
            "body or draft state is malformed",
        ):
            validator.validate_event(
                {"pull_request": {"body": body(), "draft": "false"}}
            )

    def test_non_pull_request_event_is_ignored(self) -> None:
        validator.validate_event({"ref": "refs/heads/main"})


if __name__ == "__main__":
    unittest.main()
