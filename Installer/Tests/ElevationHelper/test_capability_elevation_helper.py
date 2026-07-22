# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import hashlib
import os
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/ElevationHelper/Source",
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Bootstrapper/Security/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

from capability_elevation_helper import (  # noqa: E402
    ElevationError,
    build_consent,
    build_elevation_grant,
    request_elevation,
    validate_consent,
    validate_elevation_result,
)
from capability_process_launcher import build_launch_grant  # noqa: E402
from controlled_elevation_bootstrapper import (  # noqa: E402
    execute_bootstrap_request,
    main as bootstrapper_main,
    validate_completion_receipt,
)
from execution_security import read_strict_json_file  # noqa: E402
from security_test_support import InstallerSecurityFixture  # noqa: E402


class CapabilityElevationHelperTests(unittest.TestCase):
    def _grant(self, fixture: InstallerSecurityFixture) -> dict[str, object]:
        launch = build_launch_grant(
            fixture.session, authority_key_path=fixture.authority_key,
            helper_reference="helper.foa-sdk.install", issuer="FOA-SDK launch broker",
            issued_at_utc="2026-07-22T12:04:00Z", expires_at_utc="2026-07-22T12:10:00Z",
            nonce="grant.foa-sdk.launch-elevated",
        )
        consent = build_consent(
            authority_key_path=fixture.authority_key,
            consent_reference="consent.foa-sdk.elevation-0001", approved_by="FOA-SDK trusted UI",
            approved_at_utc="2026-07-22T12:05:00Z", helper_reference="helper.foa-sdk.install",
            launch_grant_sha256=str(launch["grant_sha256"]), rationale="Protected installation target.",
        )
        self.assertEqual(validate_consent(consent, authority_key_path=fixture.authority_key), consent)
        return build_elevation_grant(
            fixture.session, launch, consent, authority_key_path=fixture.authority_key,
            issuer="FOA-SDK elevation broker", issued_at_utc="2026-07-22T12:06:00Z",
            expires_at_utc="2026-07-22T12:09:00Z", nonce="grant.foa-sdk.elevation-0001",
        )

    @staticmethod
    def _capture_request(
        fixture: InstallerSecurityFixture,
        grant: dict[str, object],
        *,
        request_reference: str,
    ) -> tuple[dict[str, object], dict[str, object], Path]:
        captured: dict[str, object] = {}
        launched_path: Path | None = None

        def backend(executable: Path, parameters: list[str], cwd: Path) -> int:
            nonlocal launched_path
            from controlled_elevation_bootstrapper import load_request
            launched_path = executable
            request_path = Path(parameters[-1])
            captured["request"] = load_request(request_path, authority_key_path=fixture.authority_key)
            captured["request_path"] = request_path
            return 42

        result = request_elevation(
            grant, fixture.execution_root, fixture.request_root,
            authority_key_path=fixture.authority_key, claim_root=fixture.claim_root,
            request_reference=request_reference, requested_at_utc="2026-07-22T12:07:00Z",
            backend=backend,
        )
        assert launched_path is not None
        return result, captured, launched_path

    def test_consent_is_authenticated_and_wrong_key_fails(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            grant = self._grant(fixture)
            consent = grant["consent"]
            wrong = fixture.root / "wrong.key"
            wrong.write_bytes(bytes.fromhex("24" * 32))
            os.chmod(wrong, 0o600)
            with self.assertRaises(ElevationError):
                validate_consent(consent, authority_key_path=wrong)

    def test_request_launches_verified_private_bootstrapper_and_is_one_shot(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            grant = self._grant(fixture)
            result, captured, launched = self._capture_request(
                fixture, grant, request_reference="request.foa-sdk.elevation-0001"
            )
            self.assertNotEqual(launched, fixture.bootstrapper_path)
            self.assertTrue(launched.is_relative_to(fixture.claim_root / "private-executables"))
            self.assertEqual(
                hashlib.sha256(launched.read_bytes()).hexdigest(),
                hashlib.sha256(fixture.bootstrapper_path.read_bytes()).hexdigest(),
            )
            self.assertTrue(Path(captured["request_path"]).is_file())
            self.assertTrue(result["controlled_bootstrapper_only"])
            self.assertTrue(result["private_bootstrapper_bundle_used"])
            self.assertEqual(validate_elevation_result(result, authority_key_path=fixture.authority_key), result)
            with self.assertRaisesRegex(ElevationError, "already been consumed|already exists for this authority"):
                request_elevation(
                    grant, fixture.execution_root, fixture.request_root,
                    authority_key_path=fixture.authority_key, claim_root=fixture.claim_root,
                    request_reference="request.foa-sdk.elevation-replay", requested_at_utc="2026-07-22T12:08:00Z",
                    backend=lambda executable, parameters, cwd: 42,
                )

    def test_controlled_bootstrapper_reverifies_request_and_applies_exact_environment(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            grant = self._grant(fixture)
            _, captured, _ = self._capture_request(
                fixture, grant, request_reference="request.foa-sdk.elevation-bootstrap"
            )
            completion_claims = fixture.root / "completion-claims"
            completion_claims.mkdir()
            completion = execute_bootstrap_request(
                captured["request"], fixture.execution_root, authority_key_path=fixture.authority_key,
                claim_root=completion_claims, completed_at_utc="2026-07-22T12:08:00Z",
            )
            self.assertTrue(completion["process_completion_observed"])
            self.assertTrue(completion["exact_environment_applied"])
            self.assertTrue(completion["private_bundle_removed"])
            self.assertEqual(completion["return_code"], 0)

    def test_bootstrapper_cli_publishes_full_canonical_completion_create_once(self) -> None:
        with InstallerSecurityFixture(elevated=True) as fixture:
            grant = self._grant(fixture)
            _, captured, _ = self._capture_request(
                fixture, grant, request_reference="request.foa-sdk.elevation-cli"
            )
            output = fixture.root / "published-completion.json"
            exit_code = bootstrapper_main([
                "--request", str(captured["request_path"]),
                "--execution-root", str(fixture.execution_root),
                "--authority-key", str(fixture.authority_key),
                "--claim-root", str(fixture.bootstrap_claim_root),
                "--completed-at-utc", "2026-07-22T12:08:00Z",
                "--output", str(output),
            ])
            self.assertEqual(exit_code, 0)
            loaded = read_strict_json_file(output, "Elevated completion receipt", require_canonical=True)
            self.assertIsInstance(loaded, dict)
            assert isinstance(loaded, dict)
            self.assertEqual(validate_completion_receipt(loaded, authority_key_path=fixture.authority_key), loaded)
            self.assertGreater(output.stat().st_size, 0)
            self.assertEqual(
                bootstrapper_main([
                    "--request", str(captured["request_path"]),
                    "--execution-root", str(fixture.execution_root),
                    "--authority-key", str(fixture.authority_key),
                    "--claim-root", str(fixture.bootstrap_claim_root),
                    "--completed-at-utc", "2026-07-22T12:08:01Z",
                    "--output", str(fixture.root / "replay-completion.json"),
                ]),
                1,
            )


if __name__ == "__main__":
    unittest.main()
