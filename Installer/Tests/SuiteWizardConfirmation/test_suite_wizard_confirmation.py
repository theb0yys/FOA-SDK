# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import ast
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Host" / "Source"
sys.path.insert(0, str(SOURCE))

from wizard_confirmation_controller import (  # noqa: E402
    WizardConfirmationController,
    WizardContractError,
    WizardHostError,
)

INSTALLER_ROOT = REPO_ROOT / "Installer"
CONFIRMED_BY = "FOA-SDK confirmation tests"
CONFIRMED_AT = "2026-07-22T00:00:00Z"


class SuiteWizardConfirmationTests(unittest.TestCase):
    def controller(self) -> WizardConfirmationController:
        return WizardConfirmationController(INSTALLER_ROOT)

    @staticmethod
    def acknowledge_all(controller: WizardConfirmationController) -> None:
        for row in controller.acknowledgement_choices():
            controller.set_acknowledgement(str(row["acknowledgement_id"]), True)

    def test_complete_acknowledgements_create_exact_review_only_confirmation(self) -> None:
        controller = self.controller()
        review = controller.resolve_review()
        evaluation = dict(review["acknowledgement_evaluation"])
        self.assertFalse(evaluation["ready"])
        self.assertGreater(len(evaluation["missing_acknowledgement_ids"]), 0)

        self.acknowledge_all(controller)
        ready = controller.acknowledgement_evaluation()
        self.assertTrue(ready["ready"])
        confirmation = controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by=CONFIRMED_BY,
            confirmed_at_utc=CONFIRMED_AT,
        )

        self.assertEqual(confirmation["confirmation_scope"], "review-only")
        self.assertEqual(confirmation["plan_sha256"], review["plan_sha256"])
        self.assertEqual(confirmation["view_model_sha256"], review["view_model_sha256"])
        self.assertEqual(len(confirmation["confirmation_sha256"]), 64)
        self.assertEqual(confirmation["confirmed_by"], CONFIRMED_BY)
        self.assertEqual(confirmation["confirmed_at_utc"], CONFIRMED_AT)
        self.assertEqual(
            confirmation["acknowledged_ids"],
            ready["acknowledged_ids"],
        )
        self.assertTrue(confirmation["current"])
        self.assertTrue(all(value is False for value in confirmation["authority"].values()))

    def test_missing_acknowledgement_fails_closed(self) -> None:
        controller = self.controller()
        review = controller.resolve_review()
        choices = controller.acknowledgement_choices()
        for row in choices[:-1]:
            controller.set_acknowledgement(str(row["acknowledgement_id"]), True)
        with self.assertRaisesRegex(WizardContractError, "Missing required acknowledgements"):
            controller.create_review_confirmation(
                expected_plan_sha256=str(review["plan_sha256"]),
                expected_view_model_sha256=str(review["view_model_sha256"]),
                confirmed_by=CONFIRMED_BY,
                confirmed_at_utc=CONFIRMED_AT,
            )

    def test_displayed_plan_and_view_model_hashes_are_both_required(self) -> None:
        controller = self.controller()
        review = controller.resolve_review()
        self.acknowledge_all(controller)
        with self.assertRaisesRegex(WizardHostError, "plan fingerprint is stale"):
            controller.create_review_confirmation(
                expected_plan_sha256="0" * 64,
                expected_view_model_sha256=str(review["view_model_sha256"]),
                confirmed_by=CONFIRMED_BY,
                confirmed_at_utc=CONFIRMED_AT,
            )
        with self.assertRaisesRegex(WizardHostError, "view-model fingerprint is stale"):
            controller.create_review_confirmation(
                expected_plan_sha256=str(review["plan_sha256"]),
                expected_view_model_sha256="0" * 64,
                confirmed_by=CONFIRMED_BY,
                confirmed_at_utc=CONFIRMED_AT,
            )

    def test_acknowledgement_or_upstream_change_invalidates_confirmation(self) -> None:
        controller = self.controller()
        review = controller.resolve_review()
        self.acknowledge_all(controller)
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by=CONFIRMED_BY,
            confirmed_at_utc=CONFIRMED_AT,
        )
        self.assertTrue(controller.host_snapshot()["confirmation_available"])

        first_ack = str(controller.acknowledgement_choices()[0]["acknowledgement_id"])
        controller.set_acknowledgement(first_ack, False)
        self.assertFalse(controller.host_snapshot()["confirmation_available"])
        with self.assertRaisesRegex(WizardHostError, "Create the current review confirmation"):
            controller.confirmation_snapshot()

        controller.set_acknowledgement(first_ack, True)
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by=CONFIRMED_BY,
            confirmed_at_utc=CONFIRMED_AT,
        )
        controller.set_context(
            platform="windows",
            architecture="x86_64",
            runtime_target="editor-only",
            branch="changed-after-confirmation",
        )
        self.assertFalse(controller.host_snapshot()["review_available"])
        self.assertFalse(controller.host_snapshot()["confirmation_available"])

    def test_identity_and_timestamp_contracts_fail_closed(self) -> None:
        controller = self.controller()
        review = controller.resolve_review()
        self.acknowledge_all(controller)
        with self.assertRaisesRegex(WizardContractError, "confirmed_by"):
            controller.create_review_confirmation(
                expected_plan_sha256=str(review["plan_sha256"]),
                expected_view_model_sha256=str(review["view_model_sha256"]),
                confirmed_by="",
                confirmed_at_utc=CONFIRMED_AT,
            )
        with self.assertRaisesRegex(WizardContractError, "UTC timestamp"):
            controller.create_review_confirmation(
                expected_plan_sha256=str(review["plan_sha256"]),
                expected_view_model_sha256=str(review["view_model_sha256"]),
                confirmed_by=CONFIRMED_BY,
                confirmed_at_utc="2026-07-22 00:00:00",
            )

    def test_graphical_confirmation_surface_has_no_writer_or_executor(self) -> None:
        paths = [
            SOURCE / "suite_wizard_host.py",
            SOURCE / "wizard_confirmation_controller.py",
        ]
        sources = [path.read_text(encoding="utf-8") for path in paths]
        combined = "\n".join(sources)
        imported_roots: set[str] = set()
        for source in sources:
            tree = ast.parse(source)
            imported_roots.update(
                alias.name.split(".")[0]
                for node in ast.walk(tree)
                if isinstance(node, ast.Import)
                for alias in node.names
            )
            imported_roots.update(
                node.module.split(".")[0]
                for node in ast.walk(tree)
                if isinstance(node, ast.ImportFrom) and node.module
            )

        self.assertIn('text="3. Confirm"', combined)
        self.assertIn("Create review confirmation", combined)
        self.assertIn("expected_plan_sha256", combined)
        self.assertIn("expected_view_model_sha256", combined)
        self.assertIn("verify_confirmation", combined)
        self.assertTrue(
            {"subprocess", "socket", "urllib", "requests", "shutil"}.isdisjoint(imported_roots)
        )
        for forbidden in (
            "open(",
            "write_bytes",
            "write_text",
            "os.replace",
            "Popen(",
            "InstallerEngine",
            "install_button",
        ):
            self.assertNotIn(forbidden, combined)


if __name__ == "__main__":
    unittest.main()
