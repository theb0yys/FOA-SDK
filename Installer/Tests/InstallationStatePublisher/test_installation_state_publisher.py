# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/InstallationStatePublisher/Source",
    REPO_ROOT / "Installer/Bootstrapper/LifecycleCoordinator/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageCopier/Source",
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

from capability_process_launcher import build_launch_grant, launch_process  # noqa: E402
from installation_state_publisher import (  # noqa: E402
    InstallationStatePublisherError,
    build_publication_grant,
    publish_state_record,
    validate_publication_grant,
    validate_publication_receipt,
    validate_state_record,
)
from lifecycle_execution_coordinator import build_lifecycle_grant, coordinate_lifecycle  # noqa: E402
from package_payload_copier import build_copy_grant, stage_payload  # noqa: E402
from security_test_support import InstallerSecurityFixture  # noqa: E402


class InstallationStatePublisherTests(unittest.TestCase):
    def accepted_receipt(self) -> dict[str, object]:
        with InstallerSecurityFixture() as fixture:
            return dict(fixture.handoff["receipt"])

    def _lifecycle(self, fixture: InstallerSecurityFixture) -> dict[str, object]:
        copy_grant = build_copy_grant(
            fixture.session,
            authority_key_path=fixture.authority_key,
            issuer="copy",
            issued_at_utc="2026-07-22T12:04:00Z",
            expires_at_utc="2026-07-22T12:10:00Z",
            nonce="grant.foa-sdk.state-copy",
        )
        copy_receipt = stage_payload(
            copy_grant,
            REPO_ROOT,
            fixture.root / "state-staging",
            authority_key_path=fixture.authority_key,
            claim_root=fixture.claim_root,
            copied_at_utc="2026-07-22T12:05:00Z",
        )
        launch_grant = build_launch_grant(
            fixture.session,
            authority_key_path=fixture.authority_key,
            helper_reference="helper.foa-sdk.install",
            issuer="launch",
            issued_at_utc="2026-07-22T12:04:00Z",
            expires_at_utc="2026-07-22T12:10:00Z",
            nonce="grant.foa-sdk.state-launch",
        )
        launch_result = launch_process(
            launch_grant,
            fixture.execution_root,
            authority_key_path=fixture.authority_key,
            claim_root=fixture.claim_root,
            launched_at_utc="2026-07-22T12:05:00Z",
        )
        lifecycle_grant = build_lifecycle_grant(
            fixture.session,
            authority_key_path=fixture.authority_key,
            operation="install",
            issuer="lifecycle",
            issued_at_utc="2026-07-22T12:04:00Z",
            expires_at_utc="2026-07-22T12:12:00Z",
            nonce="grant.foa-sdk.state-lifecycle",
        )
        return coordinate_lifecycle(
            lifecycle_grant,
            authority_key_path=fixture.authority_key,
            completed_at_utc="2026-07-22T12:06:00Z",
            copy_receipt=copy_receipt,
            launch_result=launch_result,
        )

    def _grant(
        self,
        fixture: InstallerSecurityFixture,
        lifecycle: dict[str, object],
        *,
        expected: str | None = None,
        nonce: str = "grant.foa-sdk.state-publication",
    ) -> dict[str, object]:
        return build_publication_grant(
            fixture.session,
            lifecycle,
            authority_key_path=fixture.authority_key,
            state_reference="installation.foa-sdk.default",
            expected_previous_state_sha256=expected,
            issuer="FOA-SDK state broker",
            issued_at_utc="2026-07-22T12:07:00Z",
            expires_at_utc="2026-07-22T12:12:00Z",
            nonce=nonce,
        )

    def test_publication_is_authenticated_capability_bound_and_one_shot(self) -> None:
        with InstallerSecurityFixture() as fixture:
            lifecycle = self._lifecycle(fixture)
            grant = self._grant(fixture, lifecycle)
            self.assertEqual(
                validate_publication_grant(grant, authority_key_path=fixture.authority_key), grant
            )
            state_root = fixture.root / "state"; state_root.mkdir()
            receipt = publish_state_record(
                grant,
                state_root,
                authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root,
                published_at_utc="2026-07-22T12:08:00Z",
            )
            self.assertEqual(
                validate_publication_receipt(receipt, authority_key_path=fixture.authority_key), receipt
            )
            self.assertEqual(
                validate_state_record(receipt["state_record"], authority_key_path=fixture.authority_key),
                receipt["state_record"],
            )
            with self.assertRaisesRegex(InstallationStatePublisherError, "already been consumed"):
                publish_state_record(
                    grant,
                    state_root,
                    authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root,
                    published_at_utc="2026-07-22T12:09:00Z",
                )

    def test_exact_previous_hash_controls_authenticated_replacement(self) -> None:
        with InstallerSecurityFixture() as fixture:
            lifecycle = self._lifecycle(fixture)
            state_root = fixture.root / "state"; state_root.mkdir()
            first = publish_state_record(
                self._grant(fixture, lifecycle),
                state_root,
                authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root,
                published_at_utc="2026-07-22T12:08:00Z",
            )
            second = publish_state_record(
                self._grant(
                    fixture,
                    lifecycle,
                    expected=str(first["state_file_sha256"]),
                    nonce="grant.foa-sdk.state-publication-replace",
                ),
                state_root,
                authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root,
                published_at_utc="2026-07-22T12:09:00Z",
            )
            self.assertTrue(second["atomic_replace_used"])
            wrong = copy.deepcopy(self._grant(
                fixture,
                lifecycle,
                expected="0" * 64,
                nonce="grant.foa-sdk.state-publication-wrong",
            ))
            with self.assertRaisesRegex(InstallationStatePublisherError, "Existing state hash"):
                publish_state_record(
                    wrong,
                    state_root,
                    authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root,
                    published_at_utc="2026-07-22T12:10:00Z",
                )


if __name__ == "__main__":
    unittest.main()
