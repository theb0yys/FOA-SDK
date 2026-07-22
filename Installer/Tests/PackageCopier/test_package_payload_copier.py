# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/PackageCopier/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageEngine/Source",
    REPO_ROOT / "Installer/Bootstrapper/ExecutionHandoff/Source",
    REPO_ROOT / "Installer/SuiteWizard/Receipt/Source",
    REPO_ROOT / "Installer/SuiteWizard/Host/Source",
    REPO_ROOT / "Installer/SuiteWizard/ViewModel/Source",
):
    sys.path.insert(0, str(root))

from confirmation_receipt import build_receipt  # noqa: E402
from package_engine import build_capability_token, build_engine_session  # noqa: E402
from package_payload_copier import (  # noqa: E402
    COPY_CAPABILITY,
    PackageCopyError,
    build_copy_grant,
    stage_payload,
    validate_copy_grant,
)
from receipt_execution_handoff import build_handoff  # noqa: E402
from wizard_confirmation_controller import WizardConfirmationController  # noqa: E402


class PackagePayloadCopierTests(unittest.TestCase):
    def session(self) -> dict[str, object]:
        controller = WizardConfirmationController(REPO_ROOT / "Installer")
        review = controller.resolve_review()
        for row in controller.acknowledgement_choices():
            controller.set_acknowledgement(str(row["acknowledgement_id"]), True)
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by="FOA-SDK copier tests",
            confirmed_at_utc="2026-07-22T11:00:00Z",
        )
        result = controller.review_result
        confirmation = controller.confirmation_result
        assert isinstance(result, dict) and isinstance(confirmation, dict)
        receipt = build_receipt(dict(result["plan"]), dict(result["view_model"]), dict(confirmation))
        handoff = build_handoff(
            receipt,
            operation="install",
            target_reference="installation.foa-sdk.default",
            prior_installation_reference=None,
            requested_by="FOA-SDK copier tests",
            requested_at_utc="2026-07-22T12:00:00Z",
        )
        token = build_capability_token(
            handoff,
            issuer="FOA-SDK package-engine reviewer",
            subject="FOA-SDK copier intake",
            issued_at_utc="2026-07-22T12:05:00Z",
            expires_at_utc="2026-07-22T12:30:00Z",
            nonce="token.foa-sdk.copier-0001",
        )
        return build_engine_session(
            handoff,
            token,
            session_reference="session.foa-sdk.copier",
            accepted_by="FOA-SDK copier tests",
            accepted_at_utc="2026-07-22T12:10:00Z",
        )

    def grant(self) -> dict[str, object]:
        return build_copy_grant(
            self.session(),
            issuer="FOA-SDK copy reviewer",
            issued_at_utc="2026-07-22T12:11:00Z",
            expires_at_utc="2026-07-22T12:25:00Z",
            nonce="grant.foa-sdk.copy-0001",
        )

    def test_grant_is_exact_bound_and_copy_specific(self) -> None:
        first = self.grant()
        second = self.grant()
        self.assertEqual(first, second)
        self.assertEqual(first["capability"], COPY_CAPABILITY)
        self.assertEqual(first["session_sha256"], first["session"]["session_sha256"])
        self.assertTrue(all(value is False for value in first["authority"].values()))
        self.assertEqual(validate_copy_grant(first), first)

    def test_stages_exact_production_inventory_atomically(self) -> None:
        grant = self.grant()
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "foa-staging"
            receipt = stage_payload(grant, REPO_ROOT, target, copied_at_utc="2026-07-22T12:12:00Z")
            self.assertTrue(target.is_dir())
            self.assertTrue(receipt["copy_performed"])
            self.assertFalse(receipt["process_launched"])
            self.assertFalse(receipt["elevation_requested"])
            self.assertFalse(receipt["installation_finalized"])
            self.assertEqual(receipt["file_count"], grant["session"]["summary"]["payload_file_count"])
            self.assertEqual(receipt["size_bytes"], grant["session"]["summary"]["payload_size_bytes"])
            self.assertEqual(list(Path(temporary).glob(".*.tmp")), [])

    def test_expired_grant_and_existing_target_fail_before_mutation(self) -> None:
        grant = self.grant()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(PackageCopyError, "expired"):
                stage_payload(grant, REPO_ROOT, root / "expired", copied_at_utc="2026-07-22T12:25:01Z")
            existing = root / "existing"
            existing.mkdir()
            marker = existing / "keep.txt"
            marker.write_text("keep", encoding="utf-8")
            with self.assertRaisesRegex(PackageCopyError, "must not already exist"):
                stage_payload(grant, REPO_ROOT, existing, copied_at_utc="2026-07-22T12:12:00Z")
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

    def test_tampered_grant_fails_closed(self) -> None:
        grant = copy.deepcopy(self.grant())
        grant["capability"] = "package-engine.execute.install"
        with self.assertRaises(PackageCopyError):
            validate_copy_grant(grant)

    def test_source_has_no_process_or_elevation_surface(self) -> None:
        source = (REPO_ROOT / "Installer/Bootstrapper/PackageCopier/Source/package_payload_copier_impl.py").read_text(encoding="utf-8")
        for forbidden in ("subprocess", "Popen(", "ShellExecute", "msiexec", "elevate(", "socket", "requests"):
            self.assertNotIn(forbidden, source)
        for required in ("os.O_NOFOLLOW", "os.fsync", "os.rename", "COPY_CAPABILITY"):
            self.assertIn(required, source)


if __name__ == "__main__":
    unittest.main()
