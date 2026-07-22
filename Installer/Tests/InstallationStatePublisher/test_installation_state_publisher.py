# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import importlib.util
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

LIFECYCLE_TESTS = REPO_ROOT / "Installer/Tests/LifecycleCoordinator/test_lifecycle_execution_coordinator.py"
LIFECYCLE_SPEC = importlib.util.spec_from_file_location("foa_lifecycle_coordinator_helpers", LIFECYCLE_TESTS)
if LIFECYCLE_SPEC is None or LIFECYCLE_SPEC.loader is None:
    raise RuntimeError(f"Unable to load lifecycle helper tests: {LIFECYCLE_TESTS}")
LIFECYCLE_MODULE = importlib.util.module_from_spec(LIFECYCLE_SPEC)
sys.modules[LIFECYCLE_SPEC.name] = LIFECYCLE_MODULE
LIFECYCLE_SPEC.loader.exec_module(LIFECYCLE_MODULE)

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

    def _lifecycle_helper(self):
        return LIFECYCLE_MODULE.LifecycleExecutionCoordinatorTests(
            methodName="test_elevated_observation_completes_lifecycle_result"
        )

    def _elevated_lifecycle(self, fixture: InstallerSecurityFixture) -> dict[str, object]:
        helper = self._lifecycle_helper()
        copy_receipt = helper._copy_receipt(
            fixture,
            nonce="grant.foa-sdk.state-copy-elevated",
        )
        _elevation_result, observation = helper._elevated_result_and_observation(
            fixture,
            nonce="observation.foa-sdk.state-publication-elevated",
        )
        lifecycle_grant = helper._lifecycle_grant(
            fixture,
            nonce="grant.foa-sdk.state-lifecycle-elevated",
            allow_elevation_result=True,
        )
        return coordinate_lifecycle(
            lifecycle_grant,
            authority_key_path=fixture.authority_key,
            completed_at_utc="2026-07-22T12:10:00Z",
            copy_receipt=copy_receipt,
            elevated_completion_observation=observation,
        )

    def _raw_elevation_pending_lifecycle(self, fixture: InstallerSecurityFixture) -> dict[str, object]:
        helper = self._lifecycle_helper()
        copy_receipt = helper._copy_receipt(
            fixture,
            nonce="grant.foa-sdk.state-copy-elevation-pending",
        )
        elevation_result, _observation = helper._elevated_result_and_observation(
            fixture,
            nonce="observation.foa-sdk.state-publication-pending-unused",
        )
        lifecycle_grant = helper._lifecycle_grant(
            fixture,
            nonce="grant.foa-sdk.state-lifecycle-elevation-pending",
            allow_elevation_result=True,
        )
        return coordinate_lifecycle(
            lifecycle_grant,
            authority_key_path=fixture.authority_key,
            completed_at_utc="2026-07-22T12:10:00Z",
            copy_receipt=copy_receipt,
            elevation_result=elevation_result,
        )

    def _grant(
        self,
        fixture: InstallerSecurityFixture,
        lifecycle: dict[str, object],
        *,
        expected: str | None = None,
        nonce: str = "grant.foa-sdk.state-publication",
        issued_at_utc: str = "2026-07-22T12:07:00Z",
        expires_at_utc: str = "2026-07-22T12:12:00Z",
    ) -> dict[str, object]:
        return build_publication_grant(
            fixture.session,
            lifecycle,
            authority_key_path=fixture.authority_key,
            state_reference="installation.foa-sdk.default",
            expected_previous_state_sha256=expected,
            issuer="FOA-SDK state broker",
            issued_at_utc=issued_at_utc,
            expires_at_utc=expires_at_utc,
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

    def test_elevated_completed_lifecycle_can_publish_state(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            lifecycle = self._elevated_lifecycle(fixture)
            grant = self._grant(
                fixture,
                lifecycle,
                nonce="grant.foa-sdk.state-publication-elevated",
                issued_at_utc="2026-07-22T12:11:00Z",
                expires_at_utc="2026-07-22T12:15:00Z",
            )
            self.assertEqual(
                validate_publication_grant(grant, authority_key_path=fixture.authority_key),
                grant,
            )
            state_root = fixture.root / "elevated-state"; state_root.mkdir()
            receipt = publish_state_record(
                grant,
                state_root,
                authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root,
                published_at_utc="2026-07-22T12:12:00Z",
            )
            record = receipt["state_record"]
            self.assertTrue(record["elevation_request_confirmed"])
            self.assertTrue(record["elevated_completion_observed"])
            self.assertEqual(
                record["elevated_completion_observation_sha256"],
                lifecycle["elevated_completion_observation_sha256"],
            )
            self.assertEqual(
                validate_publication_receipt(receipt, authority_key_path=fixture.authority_key),
                receipt,
            )
            self.assertEqual(
                validate_state_record(record, authority_key_path=fixture.authority_key),
                record,
            )

    def test_raw_elevation_pending_lifecycle_remains_blocked_from_publication(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            pending = self._raw_elevation_pending_lifecycle(fixture)
            self.assertEqual(pending["status"], "elevation-requested-pending-completion-receipt")
            self.assertFalse(pending["completed"])
            with self.assertRaisesRegex(InstallationStatePublisherError, "completed lifecycle"):
                self._grant(
                    fixture,
                    pending,
                    nonce="grant.foa-sdk.state-publication-elevation-pending",
                    issued_at_utc="2026-07-22T12:11:00Z",
                    expires_at_utc="2026-07-22T12:15:00Z",
                )

    def test_elevated_blocked_lifecycle_remains_blocked_from_publication(self) -> None:
        with InstallerSecurityFixture(elevated=True, argv=["-c", "exit 7"]) as fixture:
            helper = self._lifecycle_helper()
            copy_receipt = helper._copy_receipt(
                fixture,
                nonce="grant.foa-sdk.state-copy-elevated-nonzero",
            )
            _elevation_result, observation = helper._elevated_result_and_observation(
                fixture,
                nonce="observation.foa-sdk.state-publication-elevated-nonzero",
            )
            lifecycle_grant = helper._lifecycle_grant(
                fixture,
                nonce="grant.foa-sdk.state-lifecycle-elevated-nonzero",
                allow_elevation_result=True,
            )
            blocked = coordinate_lifecycle(
                lifecycle_grant,
                authority_key_path=fixture.authority_key,
                completed_at_utc="2026-07-22T12:10:00Z",
                copy_receipt=copy_receipt,
                elevated_completion_observation=observation,
            )
            self.assertEqual(blocked["status"], "blocked-nonzero-return")
            with self.assertRaisesRegex(InstallationStatePublisherError, "completed lifecycle"):
                self._grant(
                    fixture,
                    blocked,
                    nonce="grant.foa-sdk.state-publication-elevated-nonzero",
                    issued_at_utc="2026-07-22T12:11:00Z",
                    expires_at_utc="2026-07-22T12:15:00Z",
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
