# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import hashlib
import inspect
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

from capability_process_launcher import ProcessLaunchError, build_launch_grant, launch_process, validate_launch_result  # noqa: E402
from security_test_support import InstallerSecurityFixture  # noqa: E402


class CapabilityProcessLauncherTests(unittest.TestCase):
    @staticmethod
    def _claim_path(fixture: InstallerSecurityFixture, grant: dict[str, object]) -> Path:
        return fixture.claim_root / "claim.package-launch-grant" / f"{grant['grant_sha256']}.claim.json"

    def test_grant_selects_signed_helper_without_caller_authored_executable_or_environment(self) -> None:
        parameters = inspect.signature(build_launch_grant).parameters
        for forbidden in ("executable_path", "executable_sha256", "support_files", "argv", "environment", "working_directory"):
            self.assertNotIn(forbidden, parameters)
        with InstallerSecurityFixture() as fixture:
            grant = build_launch_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                helper_reference="helper.foa-sdk.install", issuer="FOA-SDK launch broker",
                issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
                nonce="grant.foa-sdk.launch-0001",
            )
            self.assertEqual(grant["helper"], fixture.operation_plan["helpers"][0])
            self.assertEqual(grant["support_files_sha256"], hashlib.sha256(
                __import__("json").dumps(grant["helper"]["support_files"], ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8") + b"\n"
            ).hexdigest())

    def test_launches_private_bundle_with_exact_environment_and_replay_rejection(self) -> None:
        with InstallerSecurityFixture() as fixture:
            grant = build_launch_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                helper_reference="helper.foa-sdk.install", issuer="FOA-SDK launch broker",
                issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
                nonce="grant.foa-sdk.launch-0001",
            )
            result = launch_process(
                grant, fixture.execution_root, authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root, launched_at_utc="2026-07-22T12:05:00Z",
            )
            self.assertEqual(result["return_code"], 0)
            self.assertEqual(result["stdout_sha256"], hashlib.sha256(b"exact-environment").hexdigest())
            self.assertEqual(result["stderr_sha256"], hashlib.sha256(b"err").hexdigest())
            self.assertTrue(result["immutable_private_copy_used"])
            self.assertTrue(result["private_bundle_removed"])
            self.assertTrue(self._claim_path(fixture, grant).is_file())
            self.assertEqual(validate_launch_result(result, authority_key_path=fixture.authority_key), result)
            with self.assertRaisesRegex(ProcessLaunchError, "already been consumed"):
                launch_process(
                    grant, fixture.execution_root, authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root, launched_at_utc="2026-07-22T12:06:00Z",
                )
            self.assertFalse((fixture.claim_root / "private-executables" / str(grant["grant_sha256"])).exists())

    def test_tampered_support_file_fails_before_grant_consumption(self) -> None:
        with InstallerSecurityFixture() as fixture:
            grant = build_launch_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                helper_reference="helper.foa-sdk.install", issuer="FOA-SDK launch broker",
                issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
                nonce="grant.foa-sdk.launch-sidecar",
            )
            fixture.helper_support_path.write_bytes(b"tampered sidecar\n")
            with self.assertRaisesRegex(ProcessLaunchError, "support file hash or size mismatch"):
                launch_process(
                    grant, fixture.execution_root, authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root, launched_at_utc="2026-07-22T12:05:00Z",
                )
            self.assertFalse(self._claim_path(fixture, grant).exists())
            self.assertFalse((fixture.claim_root / "private-executables" / str(grant["grant_sha256"])).exists())

    def test_output_limit_kills_process_tree_and_records_combined_blocker(self) -> None:
        with InstallerSecurityFixture(output_limit_bytes=1024, argv=["-c", "while :; do printf 1234567890; printf abcdefghij >&2; done"]) as fixture:
            grant = build_launch_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                helper_reference="helper.foa-sdk.install", issuer="FOA-SDK launch broker",
                issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
                nonce="grant.foa-sdk.launch-output",
            )
            result = launch_process(
                grant, fixture.execution_root, authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root, launched_at_utc="2026-07-22T12:05:00Z",
            )
            self.assertTrue(result["output_limit_exceeded"])
            self.assertLessEqual(result["stdout_size_bytes"] + result["stderr_size_bytes"], 1024)
            self.assertGreater(result["total_output_observed_bytes"], 1024)

    def test_elevation_marked_helper_cannot_launch_directly_without_consuming_grant(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            grant = build_launch_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                helper_reference="helper.foa-sdk.install", issuer="FOA-SDK launch broker",
                issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
                nonce="grant.foa-sdk.launch-elevated",
            )
            with self.assertRaisesRegex(ProcessLaunchError, "controlled elevation"):
                launch_process(
                    grant, fixture.execution_root, authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root, launched_at_utc="2026-07-22T12:05:00Z",
                )
            self.assertFalse(self._claim_path(fixture, grant).exists())


if __name__ == "__main__":
    unittest.main()
