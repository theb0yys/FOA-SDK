# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/ElevatedCompletionReceipt/Source",
    REPO_ROOT / "Installer/Bootstrapper/ElevationHelper/Source",
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

from capability_elevation_helper import build_consent, build_elevation_grant, request_elevation  # noqa: E402
from capability_process_launcher import build_launch_grant  # noqa: E402
from controlled_elevation_bootstrapper import execute_bootstrap_request, load_request  # noqa: E402
from elevated_completion_receipt import (  # noqa: E402
    OBSERVATION_CAPABILITY,
    ElevatedCompletionReceiptError,
    canonical_observation_bytes,
    observe_elevated_completion,
    validate_elevated_completion_observation,
)
from security_test_support import InstallerSecurityFixture  # noqa: E402


class ElevatedCompletionReceiptTests(unittest.TestCase):
    def _elevation_result_and_completion(
        self,
        fixture: InstallerSecurityFixture,
        *,
        completed_at_utc: str = "2026-07-22T12:08:00Z",
    ) -> tuple[dict[str, object], dict[str, object]]:
        launch = build_launch_grant(
            fixture.session,
            authority_key_path=fixture.authority_key,
            helper_reference="helper.foa-sdk.install",
            issuer="FOA-SDK launch broker",
            issued_at_utc="2026-07-22T12:04:00Z",
            expires_at_utc="2026-07-22T12:10:00Z",
            nonce="grant.foa-sdk.elevated-completion-launch",
        )
        consent = build_consent(
            authority_key_path=fixture.authority_key,
            consent_reference="consent.foa-sdk.elevated-completion",
            approved_by="FOA-SDK trusted UI",
            approved_at_utc="2026-07-22T12:05:00Z",
            helper_reference="helper.foa-sdk.install",
            launch_grant_sha256=str(launch["grant_sha256"]),
            rationale="Protected installation target.",
        )
        elevation_grant = build_elevation_grant(
            fixture.session,
            launch,
            consent,
            authority_key_path=fixture.authority_key,
            issuer="FOA-SDK elevation broker",
            issued_at_utc="2026-07-22T12:06:00Z",
            expires_at_utc="2026-07-22T12:09:00Z",
            nonce="grant.foa-sdk.elevated-completion",
        )
        captured: dict[str, object] = {}

        def backend(executable: Path, parameters: list[str], cwd: Path) -> int:
            captured["request"] = load_request(Path(parameters[-1]), authority_key_path=fixture.authority_key)
            return 42

        elevation_result = request_elevation(
            elevation_grant,
            fixture.execution_root,
            fixture.request_root,
            authority_key_path=fixture.authority_key,
            claim_root=fixture.claim_root,
            request_reference="request.foa-sdk.elevated-completion",
            requested_at_utc="2026-07-22T12:07:00Z",
            backend=backend,
        )
        completion_claims = fixture.root / "completion-claims"
        completion_claims.mkdir()
        completion = execute_bootstrap_request(
            captured["request"],
            fixture.execution_root,
            authority_key_path=fixture.authority_key,
            claim_root=completion_claims,
            completed_at_utc=completed_at_utc,
        )
        return elevation_result, completion

    def _observe(
        self,
        fixture: InstallerSecurityFixture,
        elevation_result: dict[str, object],
        completion: dict[str, object],
        *,
        nonce: str = "observation.foa-sdk.elevated-completion",
        observed_at_utc: str = "2026-07-22T12:09:00Z",
    ) -> dict[str, object]:
        return observe_elevated_completion(
            elevation_result,
            completion,
            authority_key_path=fixture.authority_key,
            observed_by="FOA-SDK elevation observer",
            observed_at_utc=observed_at_utc,
            nonce=nonce,
        )

    def test_successful_elevated_completion_observation_is_deterministic(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            elevation_result, completion = self._elevation_result_and_completion(fixture)
            first = self._observe(fixture, elevation_result, completion)
            second = self._observe(fixture, elevation_result, completion)
            self.assertEqual(first, second)
            self.assertEqual(first["capability"], OBSERVATION_CAPABILITY)
            self.assertEqual(first["status"], "completed")
            self.assertTrue(first["completed"])
            self.assertTrue(first["process_completion_observed"])
            self.assertEqual(first["return_code"], 0)
            self.assertEqual(first["elevation_result_sha256"], elevation_result["result_sha256"])
            self.assertEqual(first["completion_sha256"], completion["completion_sha256"])
            self.assertEqual(
                validate_elevated_completion_observation(first, authority_key_path=fixture.authority_key),
                first,
            )
            self.assertEqual(
                canonical_observation_bytes(first, authority_key_path=fixture.authority_key),
                canonical_observation_bytes(second, authority_key_path=fixture.authority_key),
            )

    def test_nonzero_elevated_completion_is_observed_but_blocked(self) -> None:
        with InstallerSecurityFixture(elevated=True, argv=["-c", "exit 7"]) as fixture:
            elevation_result, completion = self._elevation_result_and_completion(fixture)
            observed = self._observe(
                fixture,
                elevation_result,
                completion,
                nonce="observation.foa-sdk.elevated-completion-nonzero",
            )
            self.assertEqual(observed["status"], "blocked-nonzero-return")
            self.assertFalse(observed["completed"])
            self.assertEqual(observed["return_code"], 7)

    def test_mismatched_or_tampered_inputs_fail_closed(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            elevation_result, completion = self._elevation_result_and_completion(fixture)
            tampered_completion = copy.deepcopy(completion)
            tampered_completion["request_sha256"] = "0" * 64
            with self.assertRaises(ElevatedCompletionReceiptError):
                self._observe(fixture, elevation_result, tampered_completion)
            observed = self._observe(fixture, elevation_result, completion)
            tampered_observation = copy.deepcopy(observed)
            tampered_observation["status"] = "completed-anyway"
            with self.assertRaises(ElevatedCompletionReceiptError):
                validate_elevated_completion_observation(
                    tampered_observation,
                    authority_key_path=fixture.authority_key,
                )

    def test_temporal_bounds_fail_closed(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            elevation_result, completion = self._elevation_result_and_completion(fixture)
            with self.assertRaisesRegex(ElevatedCompletionReceiptError, "must follow completion"):
                self._observe(
                    fixture,
                    elevation_result,
                    completion,
                    observed_at_utc="2026-07-22T12:07:59Z",
                )
            with self.assertRaisesRegex(ElevatedCompletionReceiptError, "within 15 minutes"):
                self._observe(
                    fixture,
                    elevation_result,
                    completion,
                    observed_at_utc="2026-07-22T12:24:01Z",
                )

    def test_source_does_not_reintroduce_execution_or_mutation_surfaces(self) -> None:
        source = (
            REPO_ROOT / "Installer/Bootstrapper/ElevatedCompletionReceipt/Source/elevated_completion_receipt.py"
        ).read_text(encoding="utf-8")
        for forbidden in (
            "subprocess",
            "ShellExecute",
            "ctypes",
            "request_elevation(",
            "execute_bootstrap_request(",
            "run_bounded_process(",
            "stage_payload(",
            "publish_state_record(",
            "coordinate_lifecycle(",
            "os.",
            "read_bytes",
            "write_text",
            "write_bytes",
            "unlink(",
            "mkdir(",
            "socket",
            "requests",
            "password",
            "credential",
        ):
            self.assertNotIn(forbidden, source)
        for required in (
            "package-engine.observe-elevated-completion",
            "validate_elevation_result",
            "validate_completion_receipt",
            "process_completion_observed",
            "blocked-nonzero-return",
        ):
            self.assertIn(required, source)


if __name__ == "__main__":
    unittest.main()
