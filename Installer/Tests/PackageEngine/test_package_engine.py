# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import ast
import copy
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
ENGINE_SOURCE = REPO_ROOT / "Installer" / "Bootstrapper" / "PackageEngine" / "Source"
HANDOFF_SOURCE = REPO_ROOT / "Installer" / "Bootstrapper" / "ExecutionHandoff" / "Source"
RECEIPT_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Receipt" / "Source"
HOST_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "Host" / "Source"
VIEW_MODEL_SOURCE = REPO_ROOT / "Installer" / "SuiteWizard" / "ViewModel" / "Source"
for source_root in (
    ENGINE_SOURCE,
    HANDOFF_SOURCE,
    RECEIPT_SOURCE,
    HOST_SOURCE,
    VIEW_MODEL_SOURCE,
):
    sys.path.insert(0, str(source_root))

from confirmation_receipt import build_receipt  # noqa: E402
from package_engine import (  # noqa: E402
    PackageEngineError,
    build_capability_token,
    build_engine_session,
    canonical_session_bytes,
    canonical_token_bytes,
    load_engine_session,
    load_token,
    publish_engine_session,
    publish_token,
    sha256,
    validate_capability_token,
    validate_engine_session,
    verify_token_for_handoff,
)
from receipt_execution_handoff import build_handoff  # noqa: E402
from wizard_confirmation_controller import WizardConfirmationController  # noqa: E402

INSTALLER_ROOT = REPO_ROOT / "Installer"
REQUESTED_BY = "FOA-SDK package engine tests"
REQUESTED_AT = "2026-07-22T12:00:00Z"
ISSUER = "FOA-SDK package-engine reviewer"
SUBJECT = "FOA-SDK package-engine intake"
ISSUED_AT = "2026-07-22T12:05:00Z"
EXPIRES_AT = "2026-07-22T12:30:00Z"
ACCEPTED_AT = "2026-07-22T12:10:00Z"
TARGET = "installation.foa-sdk.default"
SESSION = "session.foa-sdk.default"
NONCE = "token.foa-sdk.intake-0001"
PRIOR = "installation.foa-sdk.previous"


class PackageEngineCapabilityTests(unittest.TestCase):
    def accepted_receipt(self) -> dict[str, object]:
        controller = WizardConfirmationController(INSTALLER_ROOT)
        review = controller.resolve_review()
        for row in controller.acknowledgement_choices():
            controller.set_acknowledgement(str(row["acknowledgement_id"]), True)
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by="FOA-SDK package engine receipt",
            confirmed_at_utc="2026-07-22T11:00:00Z",
        )
        result = controller.review_result
        confirmation = controller.confirmation_result
        self.assertIsInstance(result, dict)
        self.assertIsInstance(confirmation, dict)
        assert isinstance(result, dict)
        assert isinstance(confirmation, dict)
        return build_receipt(dict(result["plan"]), dict(result["view_model"]), dict(confirmation))

    def handoff(self, operation: str = "install") -> dict[str, object]:
        return build_handoff(
            self.accepted_receipt(),
            operation=operation,
            target_reference=TARGET,
            prior_installation_reference=None if operation == "install" else PRIOR,
            requested_by=REQUESTED_BY,
            requested_at_utc=REQUESTED_AT,
        )

    def token(self, handoff: dict[str, object] | None = None) -> dict[str, object]:
        return build_capability_token(
            handoff or self.handoff(),
            issuer=ISSUER,
            subject=SUBJECT,
            issued_at_utc=ISSUED_AT,
            expires_at_utc=EXPIRES_AT,
            nonce=NONCE,
        )

    def test_capability_token_is_deterministic_exact_bound_and_non_secret(self) -> None:
        handoff = self.handoff()
        first = self.token(handoff)
        second = self.token(handoff)
        self.assertEqual(first, second)
        self.assertEqual(canonical_token_bytes(first), canonical_token_bytes(second))
        self.assertEqual(first["handoff_sha256"], handoff["handoff_sha256"])
        self.assertEqual(first["required_capabilities"], handoff["required_capabilities"])
        self.assertEqual(first["granted_capabilities"], handoff["required_capabilities"])
        self.assertEqual(first["audience"], "foa-sdk.package-engine")
        self.assertEqual(len(first["token_sha256"]), 64)
        self.assertTrue(all(value is False for value in first["authority"].values()))
        for forbidden_field in ("secret", "credential", "executable_path", "environment"):
            self.assertNotIn(forbidden_field, first)
        self.assertEqual(validate_capability_token(first), first)
        self.assertEqual(verify_token_for_handoff(first, handoff), first)

    def test_package_engine_session_authorizes_intake_without_effects(self) -> None:
        handoff = self.handoff("repair")
        token = self.token(handoff)
        first = build_engine_session(
            handoff,
            token,
            session_reference=SESSION,
            accepted_by="FOA-SDK package-engine tests",
            accepted_at_utc=ACCEPTED_AT,
        )
        second = build_engine_session(
            handoff,
            token,
            session_reference=SESSION,
            accepted_by="FOA-SDK package-engine tests",
            accepted_at_utc=ACCEPTED_AT,
        )
        self.assertEqual(first, second)
        self.assertEqual(canonical_session_bytes(first), canonical_session_bytes(second))
        self.assertEqual(first["handoff_sha256"], handoff["handoff_sha256"])
        self.assertEqual(first["token_sha256"], token["token_sha256"])
        self.assertEqual(first["authorized_capabilities"], ["package-engine.execute.repair"])
        self.assertEqual(first["session_state"], "capability-accepted-no-effects")
        self.assertTrue(all(value is False for value in first["effects"].values()))
        self.assertTrue(all(value is False for value in first["authority"].values()))
        self.assertEqual(validate_engine_session(first), first)

    def test_token_chronology_and_lifetime_fail_closed(self) -> None:
        handoff = self.handoff()
        with self.assertRaisesRegex(PackageEngineError, "must not precede"):
            build_capability_token(
                handoff,
                issuer=ISSUER,
                subject=SUBJECT,
                issued_at_utc="2026-07-22T11:59:59Z",
                expires_at_utc=EXPIRES_AT,
                nonce=NONCE,
            )
        with self.assertRaisesRegex(PackageEngineError, "after issued"):
            build_capability_token(
                handoff,
                issuer=ISSUER,
                subject=SUBJECT,
                issued_at_utc=ISSUED_AT,
                expires_at_utc=ISSUED_AT,
                nonce=NONCE,
            )
        with self.assertRaisesRegex(PackageEngineError, "one hour"):
            build_capability_token(
                handoff,
                issuer=ISSUER,
                subject=SUBJECT,
                issued_at_utc=ISSUED_AT,
                expires_at_utc="2026-07-22T13:05:01Z",
                nonce=NONCE,
            )

    def test_session_rejects_wrong_handoff_tampered_token_and_expiry(self) -> None:
        install_handoff = self.handoff("install")
        token = self.token(install_handoff)
        repair_handoff = self.handoff("repair")
        with self.assertRaisesRegex(PackageEngineError, "not bound"):
            verify_token_for_handoff(token, repair_handoff)

        tampered = copy.deepcopy(token)
        tampered["granted_capabilities"] = ["package-engine.elevation", "package-engine.execute.install"]
        unsigned = {key: value for key, value in tampered.items() if key != "token_sha256"}
        tampered["token_sha256"] = sha256(unsigned)
        with self.assertRaisesRegex(PackageEngineError, "must exactly match"):
            validate_capability_token(tampered)

        with self.assertRaisesRegex(PackageEngineError, "expired"):
            build_engine_session(
                install_handoff,
                token,
                session_reference=SESSION,
                accepted_by="FOA-SDK package-engine tests",
                accepted_at_utc="2026-07-22T12:30:01Z",
            )

    def test_token_and_session_publication_are_canonical_and_non_overwriting(self) -> None:
        handoff = self.handoff()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            token_path = root / "intake.foa-package-engine-token.json"
            session_path = root / "intake.foa-package-engine-session.json"
            published_token = publish_token(
                token_path,
                handoff,
                issuer=ISSUER,
                subject=SUBJECT,
                issued_at_utc=ISSUED_AT,
                expires_at_utc=EXPIRES_AT,
                nonce=NONCE,
            )
            self.assertEqual(published_token["status"], "published")
            token = load_token(token_path)
            self.assertEqual(token_path.read_bytes(), canonical_token_bytes(token))
            repeated_token = publish_token(
                token_path,
                handoff,
                issuer=ISSUER,
                subject=SUBJECT,
                issued_at_utc=ISSUED_AT,
                expires_at_utc=EXPIRES_AT,
                nonce=NONCE,
            )
            self.assertEqual(repeated_token["status"], "already-current")

            published_session = publish_engine_session(
                session_path,
                handoff,
                token,
                session_reference=SESSION,
                accepted_by="FOA-SDK package-engine tests",
                accepted_at_utc=ACCEPTED_AT,
            )
            self.assertEqual(published_session["status"], "published")
            session = load_engine_session(session_path)
            self.assertEqual(session_path.read_bytes(), canonical_session_bytes(session))
            repeated_session = publish_engine_session(
                session_path,
                handoff,
                token,
                session_reference=SESSION,
                accepted_by="FOA-SDK package-engine tests",
                accepted_at_utc=ACCEPTED_AT,
            )
            self.assertEqual(repeated_session["status"], "already-current")

            existing = root / "other.foa-package-engine-session.json"
            original = b"do-not-replace\n"
            existing.write_bytes(original)
            with self.assertRaisesRegex(PackageEngineError, "different bytes"):
                publish_engine_session(
                    existing,
                    handoff,
                    token,
                    session_reference=SESSION,
                    accepted_by="FOA-SDK package-engine tests",
                    accepted_at_utc=ACCEPTED_AT,
                )
            self.assertEqual(existing.read_bytes(), original)

    def test_source_has_no_copier_process_launcher_or_elevation_helper(self) -> None:
        source_path = ENGINE_SOURCE / "package_engine.py"
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
            "validate_handoff",
            "granted_capabilities",
            "authorized_capabilities",
            "os.O_EXCL",
            "os.fsync",
            "os.link",
            "capability-accepted-no-effects",
        ):
            self.assertIn(required, source)
        for forbidden in (
            "Popen(",
            "run(",
            "copyfile",
            "copytree",
            "ShellExecute",
            "msiexec",
            "elevate(",
            "install(",
            "repair(",
            "upgrade(",
            "rollback(",
            "uninstall(",
            "ProcessStartInfo",
        ):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
