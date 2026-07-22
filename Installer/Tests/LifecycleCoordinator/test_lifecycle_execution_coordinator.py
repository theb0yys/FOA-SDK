# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/LifecycleCoordinator/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageCopier/Source",
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Bootstrapper/ElevatedCompletionReceipt/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

ELEVATED_TESTS = REPO_ROOT / "Installer/Tests/ElevatedCompletionReceipt/test_elevated_completion_receipt.py"
ELEVATED_SPEC = importlib.util.spec_from_file_location("foa_elevated_completion_helpers", ELEVATED_TESTS)
if ELEVATED_SPEC is None or ELEVATED_SPEC.loader is None:
    raise RuntimeError(f"Unable to load elevated-completion helper tests: {ELEVATED_TESTS}")
ELEVATED_MODULE = importlib.util.module_from_spec(ELEVATED_SPEC)
sys.modules[ELEVATED_SPEC.name] = ELEVATED_MODULE
ELEVATED_SPEC.loader.exec_module(ELEVATED_MODULE)

from capability_process_launcher import build_launch_grant, launch_process  # noqa: E402
from lifecycle_execution_coordinator import (  # noqa: E402
    LifecycleCoordinatorError,
    build_lifecycle_grant,
    coordinate_lifecycle,
    validate_lifecycle_result,
)
from package_payload_copier import build_copy_grant, stage_payload  # noqa: E402
from security_test_support import InstallerSecurityFixture  # noqa: E402


class LifecycleExecutionCoordinatorTests(unittest.TestCase):
    def _copy_receipt(
        self,
        fixture: InstallerSecurityFixture,
        *,
        nonce: str = "grant.foa-sdk.copy-lifecycle",
    ) -> dict[str, object]:
        copy_grant = build_copy_grant(
            fixture.session, authority_key_path=fixture.authority_key,
            issuer="copy", issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
            nonce=nonce,
        )
        return stage_payload(
            copy_grant, REPO_ROOT, fixture.root / "staging", authority_key_path=fixture.authority_key,
            claim_root=fixture.claim_root, copied_at_utc="2026-07-22T12:05:00Z",
        )

    def _elevated_helper(self):
        return ELEVATED_MODULE.ElevatedCompletionReceiptTests(
            methodName="test_successful_elevated_completion_observation_is_deterministic"
        )

    def _elevated_result_and_observation(
        self,
        fixture: InstallerSecurityFixture,
        *,
        nonce: str = "observation.foa-sdk.lifecycle-elevated-completion",
    ) -> tuple[dict[str, object], dict[str, object]]:
        helper = self._elevated_helper()
        elevation_result, completion = helper._elevation_result_and_completion(fixture)
        observation = helper._observe(fixture, elevation_result, completion, nonce=nonce)
        return elevation_result, observation

    def _lifecycle_grant(
        self,
        fixture: InstallerSecurityFixture,
        *,
        nonce: str,
        allow_elevation_result: bool = False,
    ) -> dict[str, object]:
        return build_lifecycle_grant(
            fixture.session, authority_key_path=fixture.authority_key, operation="install", issuer="lifecycle",
            issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:12:00Z",
            nonce=nonce, allow_elevation_result=allow_elevation_result,
        )

    def test_install_completes_from_authenticated_copy_and_bounded_launch_evidence(self) -> None:
        with InstallerSecurityFixture() as fixture:
            copy_receipt = self._copy_receipt(fixture)
            launch_grant = build_launch_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                helper_reference="helper.foa-sdk.install", issuer="launch",
                issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
                nonce="grant.foa-sdk.launch-lifecycle",
            )
            launch_result = launch_process(
                launch_grant, fixture.execution_root, authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root, launched_at_utc="2026-07-22T12:05:00Z",
            )
            lifecycle = self._lifecycle_grant(fixture, nonce="grant.foa-sdk.lifecycle-0001")
            result = coordinate_lifecycle(
                lifecycle, authority_key_path=fixture.authority_key, completed_at_utc="2026-07-22T12:06:00Z",
                copy_receipt=copy_receipt, launch_result=launch_result,
            )
            self.assertTrue(result["completed"])
            self.assertEqual(result["status"], "completed")
            self.assertEqual(validate_lifecycle_result(result, authority_key_path=fixture.authority_key), result)

    def test_elevated_observation_completes_lifecycle_result(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            copy_receipt = self._copy_receipt(fixture, nonce="grant.foa-sdk.copy-lifecycle-elevated")
            _elevation_result, observation = self._elevated_result_and_observation(fixture)
            lifecycle = self._lifecycle_grant(
                fixture,
                nonce="grant.foa-sdk.lifecycle-elevated-completion",
                allow_elevation_result=True,
            )
            result = coordinate_lifecycle(
                lifecycle,
                authority_key_path=fixture.authority_key,
                completed_at_utc="2026-07-22T12:10:00Z",
                copy_receipt=copy_receipt,
                elevated_completion_observation=observation,
            )
            self.assertEqual(result["status"], "completed")
            self.assertTrue(result["completed"])
            self.assertTrue(result["elevation_request_confirmed"])
            self.assertTrue(result["elevated_completion_observed"])
            self.assertEqual(result["elevated_completion_observation_sha256"], observation["observation_sha256"])
            self.assertEqual(result["elevation_result_sha256"], observation["elevation_result_sha256"])
            self.assertEqual(validate_lifecycle_result(result, authority_key_path=fixture.authority_key), result)

    def test_elevated_observation_blocks_nonzero_lifecycle_result(self) -> None:
        with InstallerSecurityFixture(elevated=True, argv=["-c", "exit 7"]) as fixture:
            copy_receipt = self._copy_receipt(fixture, nonce="grant.foa-sdk.copy-lifecycle-elevated-nonzero")
            _elevation_result, observation = self._elevated_result_and_observation(
                fixture,
                nonce="observation.foa-sdk.lifecycle-elevated-nonzero",
            )
            lifecycle = self._lifecycle_grant(
                fixture,
                nonce="grant.foa-sdk.lifecycle-elevated-nonzero",
                allow_elevation_result=True,
            )
            result = coordinate_lifecycle(
                lifecycle,
                authority_key_path=fixture.authority_key,
                completed_at_utc="2026-07-22T12:10:00Z",
                copy_receipt=copy_receipt,
                elevated_completion_observation=observation,
            )
            self.assertEqual(result["status"], "blocked-nonzero-return")
            self.assertFalse(result["completed"])
            self.assertTrue(result["elevated_completion_observed"])
            self.assertEqual(validate_lifecycle_result(result, authority_key_path=fixture.authority_key), result)

    def test_raw_elevation_result_remains_pending_until_completion_observation(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            copy_receipt = self._copy_receipt(fixture, nonce="grant.foa-sdk.copy-lifecycle-elevation-pending")
            elevation_result, _observation = self._elevated_result_and_observation(
                fixture,
                nonce="observation.foa-sdk.lifecycle-elevation-pending-unused",
            )
            lifecycle = self._lifecycle_grant(
                fixture,
                nonce="grant.foa-sdk.lifecycle-elevation-pending",
                allow_elevation_result=True,
            )
            result = coordinate_lifecycle(
                lifecycle,
                authority_key_path=fixture.authority_key,
                completed_at_utc="2026-07-22T12:10:00Z",
                copy_receipt=copy_receipt,
                elevation_result=elevation_result,
            )
            self.assertEqual(result["status"], "elevation-requested-pending-completion-receipt")
            self.assertFalse(result["completed"])
            self.assertFalse(result["elevated_completion_observed"])
            self.assertEqual(validate_lifecycle_result(result, authority_key_path=fixture.authority_key), result)

    def test_elevated_observation_requires_elevation_enabled_lifecycle_grant(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            copy_receipt = self._copy_receipt(fixture, nonce="grant.foa-sdk.copy-lifecycle-elevated-disallowed")
            _elevation_result, observation = self._elevated_result_and_observation(
                fixture,
                nonce="observation.foa-sdk.lifecycle-elevated-disallowed",
            )
            lifecycle = self._lifecycle_grant(
                fixture,
                nonce="grant.foa-sdk.lifecycle-elevated-disallowed",
                allow_elevation_result=False,
            )
            with self.assertRaisesRegex(LifecycleCoordinatorError, "does not allow elevation"):
                coordinate_lifecycle(
                    lifecycle,
                    authority_key_path=fixture.authority_key,
                    completed_at_utc="2026-07-22T12:10:00Z",
                    copy_receipt=copy_receipt,
                    elevated_completion_observation=observation,
                )

    def test_mixed_launch_and_elevated_observation_evidence_fails_closed(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            copy_receipt = self._copy_receipt(fixture, nonce="grant.foa-sdk.copy-lifecycle-mixed")
            _elevation_result, observation = self._elevated_result_and_observation(
                fixture,
                nonce="observation.foa-sdk.lifecycle-mixed",
            )
            lifecycle = self._lifecycle_grant(
                fixture,
                nonce="grant.foa-sdk.lifecycle-mixed",
                allow_elevation_result=True,
            )
            with self.assertRaisesRegex(LifecycleCoordinatorError, "exactly one authenticated"):
                coordinate_lifecycle(
                    lifecycle,
                    authority_key_path=fixture.authority_key,
                    completed_at_utc="2026-07-22T12:10:00Z",
                    copy_receipt=copy_receipt,
                    elevation_result=observation["elevation_result"],
                    elevated_completion_observation=observation,
                )

    def test_missing_copy_evidence_fails_closed(self) -> None:
        with InstallerSecurityFixture() as fixture:
            lifecycle = self._lifecycle_grant(fixture, nonce="grant.foa-sdk.lifecycle-missing-copy")
            with self.assertRaisesRegex(LifecycleCoordinatorError, "copy receipt"):
                coordinate_lifecycle(
                    lifecycle, authority_key_path=fixture.authority_key,
                    completed_at_utc="2026-07-22T12:06:00Z",
                )


if __name__ == "__main__":
    unittest.main()
