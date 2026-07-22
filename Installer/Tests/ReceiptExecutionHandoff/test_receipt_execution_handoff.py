# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import ast
import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
HANDOFF_SOURCE = (
    REPO_ROOT
    / "Installer"
    / "Bootstrapper"
    / "ExecutionHandoff"
    / "Source"
)
RECEIPT_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Receipt" / "Source"
HOST_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Host" / "Source"
VIEW_MODEL_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "ViewModel" / "Source"
for source_root in (
    HANDOFF_SOURCE,
    RECEIPT_SOURCE,
    HOST_SOURCE,
    VIEW_MODEL_SOURCE,
):
    sys.path.insert(0, str(source_root))

from confirmation_receipt import build_receipt  # noqa: E402
from receipt_execution_handoff import (  # noqa: E402
    ExecutionHandoffError,
    build_handoff,
    canonical_handoff_bytes,
    load_handoff,
    publish_handoff,
    validate_handoff,
    verify_handoff_against_receipt,
    verify_published_handoff,
)
from wizard_confirmation_controller import WizardConfirmationController  # noqa: E402
from wizard_view_model import (  # noqa: E402
    build_view_model,
    create_confirmation,
    required_acknowledgement_ids,
    sha256,
)

INSTALLER_ROOT = REPO_ROOT / "Installer"
REQUESTED_BY = "FOA-SDK execution handoff tests"
REQUESTED_AT = "2026-07-22T12:00:00Z"
TARGET = "installation.foa-sdk.default"
PRIOR = "installation.foa-sdk.previous"


class ReceiptExecutionHandoffTests(unittest.TestCase):
    def accepted_receipt(
        self,
        *,
        confirmed_by: str = "FOA-SDK handoff receipt",
    ) -> dict[str, object]:
        controller = WizardConfirmationController(INSTALLER_ROOT)
        review = controller.resolve_review()
        for row in controller.acknowledgement_choices():
            controller.set_acknowledgement(
                str(row["acknowledgement_id"]),
                True,
            )
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by=confirmed_by,
            confirmed_at_utc="2026-07-22T11:00:00Z",
        )
        result = controller.review_result
        confirmation = controller.confirmation_result
        self.assertIsInstance(result, dict)
        self.assertIsInstance(confirmation, dict)
        assert isinstance(result, dict)
        assert isinstance(confirmation, dict)
        return build_receipt(
            dict(result["plan"]),
            dict(result["view_model"]),
            dict(confirmation),
        )

    def test_install_handoff_is_deterministic_exact_bound_and_inert(self) -> None:
        receipt = self.accepted_receipt()
        first = build_handoff(
            receipt,
            operation="install",
            target_reference=TARGET,
            prior_installation_reference=None,
            requested_by=REQUESTED_BY,
            requested_at_utc=REQUESTED_AT,
        )
        second = build_handoff(
            receipt,
            operation="install",
            target_reference=TARGET,
            prior_installation_reference=None,
            requested_by=REQUESTED_BY,
            requested_at_utc=REQUESTED_AT,
        )

        self.assertEqual(first, second)
        self.assertEqual(
            canonical_handoff_bytes(first),
            canonical_handoff_bytes(second),
        )
        self.assertEqual(first["receipt_sha256"], receipt["receipt_sha256"])
        self.assertEqual(first["plan_sha256"], receipt["plan_sha256"])
        self.assertEqual(
            first["view_model_sha256"],
            receipt["view_model_sha256"],
        )
        self.assertEqual(
            first["confirmation_sha256"],
            receipt["confirmation_sha256"],
        )
        self.assertEqual(
            first["package_order"],
            receipt["plan"]["package_order"],
        )
        self.assertEqual(
            first["required_capabilities"],
            ["package-engine.execute.install"],
        )
        self.assertEqual(first["granted_capabilities"], [])
        self.assertTrue(
            all(value is False for value in first["authority"].values())
        )
        self.assertEqual(validate_handoff(first), first)

    def test_lifecycle_operations_require_prior_reference_and_plan_support(self) -> None:
        receipt = self.accepted_receipt()
        for operation in ("repair", "upgrade", "rollback", "uninstall"):
            with self.subTest(operation=operation):
                handoff = build_handoff(
                    receipt,
                    operation=operation,
                    target_reference=TARGET,
                    prior_installation_reference=PRIOR,
                    requested_by=REQUESTED_BY,
                    requested_at_utc=REQUESTED_AT,
                )
                self.assertEqual(
                    handoff["required_capabilities"],
                    [f"package-engine.execute.{operation}"],
                )
                self.assertEqual(
                    handoff["prior_installation_reference"],
                    PRIOR,
                )

                with self.assertRaisesRegex(
                    ExecutionHandoffError,
                    "require prior_installation_reference",
                ):
                    build_handoff(
                        receipt,
                        operation=operation,
                        target_reference=TARGET,
                        prior_installation_reference=None,
                        requested_by=REQUESTED_BY,
                        requested_at_utc=REQUESTED_AT,
                    )

        with self.assertRaisesRegex(
            ExecutionHandoffError,
            "must not declare prior_installation_reference",
        ):
            build_handoff(
                receipt,
                operation="install",
                target_reference=TARGET,
                prior_installation_reference=PRIOR,
                requested_by=REQUESTED_BY,
                requested_at_utc=REQUESTED_AT,
            )

    def test_unsupported_lifecycle_operation_fails_closed(self) -> None:
        receipt = self.accepted_receipt()
        altered_plan = copy.deepcopy(receipt["plan"])
        altered_plan["packages"][0]["lifecycle"]["repair_supported"] = False
        unsigned_plan = {
            key: value
            for key, value in altered_plan.items()
            if key != "plan_sha256"
        }
        altered_plan["plan_sha256"] = sha256(unsigned_plan)
        altered_view = build_view_model(altered_plan)
        altered_confirmation = create_confirmation(
            altered_plan,
            altered_view,
            expected_plan_sha256=altered_plan["plan_sha256"],
            acknowledged_ids=required_acknowledgement_ids(altered_view),
            confirmed_by="FOA-SDK unsupported lifecycle test",
            confirmed_at_utc="2026-07-22T11:15:00Z",
        )
        altered_receipt = build_receipt(
            altered_plan,
            altered_view,
            altered_confirmation,
        )

        with self.assertRaisesRegex(
            ExecutionHandoffError,
            "do not declare repair support",
        ):
            build_handoff(
                altered_receipt,
                operation="repair",
                target_reference=TARGET,
                prior_installation_reference=PRIOR,
                requested_by=REQUESTED_BY,
                requested_at_utc=REQUESTED_AT,
            )

    def test_publication_is_atomic_canonical_idempotent_and_non_overwriting(
        self,
    ) -> None:
        receipt = self.accepted_receipt()
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "install.foa-execution-handoff.json"
            published = publish_handoff(
                target,
                receipt,
                operation="install",
                target_reference=TARGET,
                prior_installation_reference=None,
                requested_by=REQUESTED_BY,
                requested_at_utc=REQUESTED_AT,
            )
            self.assertEqual(published["status"], "published")
            loaded = load_handoff(target)
            self.assertEqual(
                target.read_bytes(),
                canonical_handoff_bytes(loaded),
            )
            self.assertEqual(
                verify_published_handoff(target, receipt),
                loaded,
            )

            repeated = publish_handoff(
                target,
                receipt,
                operation="install",
                target_reference=TARGET,
                prior_installation_reference=None,
                requested_by=REQUESTED_BY,
                requested_at_utc=REQUESTED_AT,
            )
            self.assertEqual(repeated["status"], "already-current")
            self.assertEqual(
                repeated["handoff_sha256"],
                loaded["handoff_sha256"],
            )
            self.assertEqual(
                list(Path(temporary).glob(".*.tmp")),
                [],
            )

            other = Path(temporary) / "different.foa-execution-handoff.json"
            original = b"do-not-replace\n"
            other.write_bytes(original)
            with self.assertRaisesRegex(
                ExecutionHandoffError,
                "different bytes",
            ):
                publish_handoff(
                    other,
                    receipt,
                    operation="install",
                    target_reference=TARGET,
                    prior_installation_reference=None,
                    requested_by=REQUESTED_BY,
                    requested_at_utc=REQUESTED_AT,
                )
            self.assertEqual(other.read_bytes(), original)

    def test_tampering_and_mismatched_current_receipt_fail_closed(self) -> None:
        receipt = self.accepted_receipt()
        handoff = build_handoff(
            receipt,
            operation="install",
            target_reference=TARGET,
            prior_installation_reference=None,
            requested_by=REQUESTED_BY,
            requested_at_utc=REQUESTED_AT,
        )

        altered = copy.deepcopy(handoff)
        altered["granted_capabilities"] = [
            "package-engine.execute.install"
        ]
        unsigned = {
            key: value
            for key, value in altered.items()
            if key != "handoff_sha256"
        }
        altered["handoff_sha256"] = sha256(unsigned)
        with self.assertRaisesRegex(
            ExecutionHandoffError,
            "granted_capabilities must remain empty",
        ):
            validate_handoff(altered)

        other_receipt = self.accepted_receipt(
            confirmed_by="Different receipt confirmer"
        )
        with self.assertRaisesRegex(
            ExecutionHandoffError,
            "does not match the exact current verified receipt",
        ):
            verify_handoff_against_receipt(handoff, other_receipt)

        tampered_receipt = copy.deepcopy(receipt)
        tampered_receipt["statement"] = "altered"
        with self.assertRaisesRegex(
            ExecutionHandoffError,
            "Receipt verification failed",
        ):
            build_handoff(
                tampered_receipt,
                operation="install",
                target_reference=TARGET,
                prior_installation_reference=None,
                requested_by=REQUESTED_BY,
                requested_at_utc=REQUESTED_AT,
            )

    def test_noncanonical_paths_references_and_bytes_are_rejected(self) -> None:
        receipt = self.accepted_receipt()
        for invalid in (
            "../target",
            "C:\\target",
            "/target",
            "target",
            "Target.MixedCase",
        ):
            with self.subTest(invalid=invalid):
                with self.assertRaisesRegex(
                    ExecutionHandoffError,
                    "logical ID",
                ):
                    build_handoff(
                        receipt,
                        operation="install",
                        target_reference=invalid,
                        prior_installation_reference=None,
                        requested_by=REQUESTED_BY,
                        requested_at_utc=REQUESTED_AT,
                    )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(
                ExecutionHandoffError,
                "must end",
            ):
                publish_handoff(
                    root / "handoff.json",
                    receipt,
                    operation="install",
                    target_reference=TARGET,
                    prior_installation_reference=None,
                    requested_by=REQUESTED_BY,
                    requested_at_utc=REQUESTED_AT,
                )

            target = root / "install.foa-execution-handoff.json"
            publish_handoff(
                target,
                receipt,
                operation="install",
                target_reference=TARGET,
                prior_installation_reference=None,
                requested_by=REQUESTED_BY,
                requested_at_utc=REQUESTED_AT,
            )
            document = json.loads(target.read_text(encoding="utf-8"))
            target.write_text(
                json.dumps(document, indent=2) + "\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                ExecutionHandoffError,
                "not canonical JSON",
            ):
                load_handoff(target)

            real = root / "real"
            real.mkdir()
            linked = root / "linked"
            try:
                linked.symlink_to(real, target_is_directory=True)
            except (OSError, NotImplementedError):
                return
            with self.assertRaisesRegex(
                ExecutionHandoffError,
                "symbolic link",
            ):
                publish_handoff(
                    linked / "linked.foa-execution-handoff.json",
                    receipt,
                    operation="install",
                    target_reference=TARGET,
                    prior_installation_reference=None,
                    requested_by=REQUESTED_BY,
                    requested_at_utc=REQUESTED_AT,
                )

    def test_source_has_no_package_engine_or_execution_surface(self) -> None:
        source_path = HANDOFF_SOURCE / "receipt_execution_handoff.py"
        source = source_path.read_text(encoding="utf-8")
        tree = ast.parse(source)
        imported_roots = {
            alias.name.split(".")[0]
            for node in ast.walk(tree)
            if isinstance(node, ast.Import)
            for alias in node.names
        }
        imported_roots.update(
            node.module.split(".")[0]
            for node in ast.walk(tree)
            if isinstance(node, ast.ImportFrom) and node.module
        )

        self.assertTrue(
            {
                "subprocess",
                "socket",
                "urllib",
                "requests",
                "ftplib",
                "shutil",
            }.isdisjoint(imported_roots)
        )
        for required in (
            "validate_receipt",
            "granted_capabilities",
            "os.O_EXCL",
            "os.fsync",
            "os.link",
            "package-engine.execute.",
        ):
            self.assertIn(required, source)
        for forbidden in (
            "Popen(",
            "run(",
            "InstallerEngine",
            "PackageEngine",
            "msiexec",
            "copyfile",
            "copytree",
            "urlopen",
            "elevate(",
            "install(",
            "repair(",
            "upgrade(",
            "rollback(",
            "uninstall(",
        ):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
