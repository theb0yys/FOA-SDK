# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import hashlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/ElevationHelper/Source",
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageEngine/Source",
    REPO_ROOT / "Installer/SuiteWizard/ViewModel/Source",
):
    sys.path.insert(0, str(root))

import capability_elevation_helper as helper  # noqa: E402
from capability_elevation_helper import (  # noqa: E402
    ELEVATION_CAPABILITY,
    ElevationError,
    build_consent,
    build_elevation_grant,
    request_elevation,
    validate_consent,
    validate_elevation_grant,
)

SESSION_SHA = "1" * 64
LAUNCH_SHA = "2" * 64
ARGV_SHA = "3" * 64
ENV_SHA = "4" * 64
EXECUTABLE_SHA = hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest()


def fake_session(*, elevated: bool = True) -> dict[str, object]:
    capabilities = ["package-engine.execute.install", "package-engine.launch-process"]
    if elevated:
        capabilities.append(ELEVATION_CAPABILITY)
    return {
        "session_sha256": SESSION_SHA,
        "accepted_at_utc": "2026-07-22T12:00:00Z",
        "authorized_capabilities": sorted(capabilities),
    }


def fake_launch() -> dict[str, object]:
    return {
        "session_sha256": SESSION_SHA,
        "grant_sha256": LAUNCH_SHA,
        "executable_reference": "executable.foa-sdk.helper",
        "executable_sha256": EXECUTABLE_SHA,
        "argv": ["-c", "print('elevated')"],
        "argv_sha256": ARGV_SHA,
        "environment_sha256": ENV_SHA,
    }


class CapabilityElevationHelperTests(unittest.TestCase):
    def consent(self) -> dict[str, object]:
        return build_consent(
            consent_reference="consent.foa-sdk.elevation-0001",
            approved_by="FOA-SDK elevation reviewer",
            approved_at_utc="2026-07-22T12:01:00Z",
            executable_reference="executable.foa-sdk.helper",
            launch_grant_sha256=LAUNCH_SHA,
            rationale="Install reviewed files into a protected product directory.",
        )

    def grant(self, *, elevated: bool = True) -> dict[str, object]:
        with patch.object(helper, "validate_engine_session", return_value=fake_session(elevated=elevated)), patch.object(
            helper, "validate_launch_grant", return_value=fake_launch()
        ):
            return build_elevation_grant(
                fake_session(elevated=elevated),
                fake_launch(),
                self.consent(),
                issuer="FOA-SDK elevation authority",
                issued_at_utc="2026-07-22T12:02:00Z",
                expires_at_utc="2026-07-22T12:05:00Z",
                nonce="grant.foa-sdk.elevation-0001",
            )

    def test_consent_is_explicit_exact_and_deterministic(self) -> None:
        first = self.consent()
        second = self.consent()
        self.assertEqual(first, second)
        self.assertTrue(first["approved"])
        self.assertEqual(first["launch_grant_sha256"], LAUNCH_SHA)
        self.assertEqual(validate_consent(first), first)

    def test_grant_is_bound_to_session_launch_and_consent(self) -> None:
        grant = self.grant()
        self.assertEqual(grant["capability"], ELEVATION_CAPABILITY)
        self.assertEqual(grant["session_sha256"], SESSION_SHA)
        self.assertEqual(grant["launch_grant_sha256"], LAUNCH_SHA)
        self.assertEqual(grant["consent_sha256"], self.consent()["consent_sha256"])
        self.assertTrue(grant["authority"]["elevation"])
        self.assertEqual(sum(bool(value) for value in grant["authority"].values()), 1)
        with patch.object(helper, "validate_engine_session", return_value=fake_session()), patch.object(
            helper, "validate_launch_grant", return_value=fake_launch()
        ):
            self.assertEqual(validate_elevation_grant(grant), grant)

    def test_missing_elevation_capability_fails_closed(self) -> None:
        with patch.object(helper, "validate_engine_session", return_value=fake_session(elevated=False)), patch.object(
            helper, "validate_launch_grant", return_value=fake_launch()
        ):
            with self.assertRaisesRegex(ElevationError, "does not grant elevation"):
                build_elevation_grant(
                    fake_session(elevated=False), fake_launch(), self.consent(),
                    issuer="FOA-SDK elevation authority",
                    issued_at_utc="2026-07-22T12:02:00Z",
                    expires_at_utc="2026-07-22T12:05:00Z",
                    nonce="grant.foa-sdk.elevation-0001",
                )

    def test_consent_mismatch_expiry_and_tamper_fail_closed(self) -> None:
        wrong = build_consent(
            consent_reference="consent.foa-sdk.elevation-0002",
            approved_by="FOA-SDK elevation reviewer",
            approved_at_utc="2026-07-22T12:01:00Z",
            executable_reference="executable.foa-sdk.other",
            launch_grant_sha256=LAUNCH_SHA,
            rationale="Different executable.",
        )
        with patch.object(helper, "validate_engine_session", return_value=fake_session()), patch.object(
            helper, "validate_launch_grant", return_value=fake_launch()
        ):
            with self.assertRaisesRegex(ElevationError, "identity"):
                build_elevation_grant(
                    fake_session(), fake_launch(), wrong,
                    issuer="FOA-SDK elevation authority",
                    issued_at_utc="2026-07-22T12:02:00Z",
                    expires_at_utc="2026-07-22T12:05:00Z",
                    nonce="grant.foa-sdk.elevation-0001",
                )
        tampered = copy.deepcopy(self.consent())
        tampered["approved_by"] = "attacker"
        with self.assertRaisesRegex(ElevationError, "fingerprint"):
            validate_consent(tampered)

    def test_fake_backend_records_prompt_without_credentials_or_completion_claim(self) -> None:
        grant = self.grant()
        calls: list[tuple[Path, str, Path]] = []

        def backend(executable: Path, parameters: str, cwd: Path) -> int:
            calls.append((executable, parameters, cwd))
            return 42

        with tempfile.TemporaryDirectory() as temporary, patch.object(
            helper, "validate_engine_session", return_value=fake_session()
        ), patch.object(helper, "validate_launch_grant", return_value=fake_launch()):
            result = request_elevation(
                grant,
                Path(sys.executable).resolve(),
                Path(temporary).resolve(),
                requested_at_utc="2026-07-22T12:03:00Z",
                backend=backend,
            )
        self.assertEqual(len(calls), 1)
        self.assertEqual(result["backend_code"], 42)
        self.assertTrue(result["elevation_requested"])
        self.assertFalse(result["consent_ui_suppressed"])
        self.assertFalse(result["credentials_collected"])
        self.assertFalse(result["process_completion_observed"])

    def test_expired_grant_rejected_before_backend(self) -> None:
        grant = self.grant()
        invoked = False

        def backend(executable: Path, parameters: str, cwd: Path) -> int:
            nonlocal invoked
            invoked = True
            return 42

        with tempfile.TemporaryDirectory() as temporary, patch.object(
            helper, "validate_engine_session", return_value=fake_session()
        ), patch.object(helper, "validate_launch_grant", return_value=fake_launch()):
            with self.assertRaisesRegex(ElevationError, "expired"):
                request_elevation(
                    grant, Path(sys.executable).resolve(), Path(temporary).resolve(),
                    requested_at_utc="2026-07-22T12:05:01Z", backend=backend,
                )
        self.assertFalse(invoked)

    def test_source_has_no_credential_or_silent_elevation_surface(self) -> None:
        source = (REPO_ROOT / "Installer/Bootstrapper/ElevationHelper/Source/capability_elevation_helper.py").read_text(encoding="utf-8")
        for required in ("ShellExecuteW", '"runas"', "consent_ui_suppressed", "credentials_collected"):
            self.assertIn(required, source)
        for forbidden in ("password", "token_password", "sudo -S", "CreateProcessWithLogonW", "LOGON_WITH_PROFILE"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
