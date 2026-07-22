# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import hashlib
import os
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageEngine/Source",
    REPO_ROOT / "Installer/Bootstrapper/ExecutionHandoff/Source",
    REPO_ROOT / "Installer/SuiteWizard/Receipt/Source",
    REPO_ROOT / "Installer/SuiteWizard/Host/Source",
    REPO_ROOT / "Installer/SuiteWizard/ViewModel/Source",
):
    sys.path.insert(0, str(root))

from capability_process_launcher import (  # noqa: E402
    LAUNCH_CAPABILITY,
    ProcessLaunchError,
    build_launch_grant,
    canonical_grant_bytes,
    canonical_result_bytes,
    launch_process,
    validate_launch_grant,
    validate_launch_result,
)
from confirmation_receipt import build_receipt  # noqa: E402
from package_engine import build_capability_token, build_engine_session  # noqa: E402
from receipt_execution_handoff import build_handoff  # noqa: E402
from wizard_confirmation_controller import WizardConfirmationController  # noqa: E402


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class CapabilityProcessLauncherTests(unittest.TestCase):
    def session(self) -> dict[str, object]:
        controller = WizardConfirmationController(REPO_ROOT / "Installer")
        review = controller.resolve_review()
        for row in controller.acknowledgement_choices():
            controller.set_acknowledgement(str(row["acknowledgement_id"]), True)
        controller.create_review_confirmation(
            expected_plan_sha256=str(review["plan_sha256"]),
            expected_view_model_sha256=str(review["view_model_sha256"]),
            confirmed_by="FOA-SDK process launcher tests",
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
            requested_by="FOA-SDK process launcher tests",
            requested_at_utc="2026-07-22T12:00:00Z",
        )
        token = build_capability_token(
            handoff,
            issuer="FOA-SDK package-engine reviewer",
            subject="FOA-SDK process launcher intake",
            issued_at_utc="2026-07-22T12:05:00Z",
            expires_at_utc="2026-07-22T12:30:00Z",
            nonce="token.foa-sdk.launcher-0001",
        )
        return build_engine_session(
            handoff,
            token,
            session_reference="session.foa-sdk.launcher",
            accepted_by="FOA-SDK process launcher tests",
            accepted_at_utc="2026-07-22T12:10:00Z",
        )

    def grant(self, *, argv: list[str] | None = None, executable_hash: str | None = None) -> dict[str, object]:
        executable = Path(sys.executable).resolve()
        return build_launch_grant(
            self.session(),
            executable_reference="executable.python.test-fixture",
            executable_sha256=executable_hash or file_sha256(executable),
            argv=argv or ["-c", "import os,sys;sys.stdout.write(os.environ.get('FOA_TEST','missing'));sys.stderr.write('err')"],
            environment={"FOA_TEST": "exact-environment"},
            timeout_seconds=10,
            output_limit_bytes=4096,
            issuer="FOA-SDK process launch reviewer",
            issued_at_utc="2026-07-22T12:11:00Z",
            expires_at_utc="2026-07-22T12:20:00Z",
            nonce="grant.foa-sdk.launch-0001",
        )

    def test_grant_is_deterministic_and_exact_bound(self) -> None:
        first = self.grant()
        second = self.grant()
        self.assertEqual(first, second)
        self.assertEqual(canonical_grant_bytes(first), canonical_grant_bytes(second))
        self.assertEqual(first["capability"], LAUNCH_CAPABILITY)
        self.assertEqual(first["session_sha256"], first["session"]["session_sha256"])
        self.assertTrue(all(value is False for value in first["authority"].values()))
        self.assertEqual(validate_launch_grant(first), first)

    def test_launches_exact_executable_without_shell_or_environment_inheritance(self) -> None:
        grant = self.grant()
        executable = Path(sys.executable).resolve()
        with tempfile.TemporaryDirectory() as temporary:
            result = launch_process(
                grant,
                executable,
                Path(temporary).resolve(),
                launched_at_utc="2026-07-22T12:12:00Z",
            )
        self.assertEqual(result["return_code"], 0)
        self.assertFalse(result["timed_out"])
        self.assertTrue(result["process_launched"])
        self.assertFalse(result["shell_used"])
        self.assertFalse(result["elevation_requested"])
        self.assertEqual(result["stdout_sha256"], hashlib.sha256(b"exact-environment").hexdigest())
        self.assertEqual(result["stderr_sha256"], hashlib.sha256(b"err").hexdigest())
        self.assertTrue(all(value is False for value in result["authority"].values()))
        self.assertEqual(validate_launch_result(result), result)
        self.assertEqual(canonical_result_bytes(result), canonical_result_bytes(validate_launch_result(result)))

    def test_hash_mismatch_expiry_and_relative_executable_fail_before_launch(self) -> None:
        executable = Path(sys.executable).resolve()
        with tempfile.TemporaryDirectory() as temporary:
            cwd = Path(temporary).resolve()
            with self.assertRaisesRegex(ProcessLaunchError, "SHA-256"):
                launch_process(self.grant(executable_hash="0" * 64), executable, cwd, launched_at_utc="2026-07-22T12:12:00Z")
            with self.assertRaisesRegex(ProcessLaunchError, "expired"):
                launch_process(self.grant(), executable, cwd, launched_at_utc="2026-07-22T12:20:01Z")
            with self.assertRaisesRegex(ProcessLaunchError, "absolute"):
                launch_process(self.grant(), Path(os.path.basename(sys.executable)), cwd, launched_at_utc="2026-07-22T12:12:00Z")

    def test_tampered_grant_and_unsafe_bounds_fail_closed(self) -> None:
        grant = copy.deepcopy(self.grant())
        grant["capability"] = "package-engine.elevation"
        with self.assertRaises(ProcessLaunchError):
            validate_launch_grant(grant)
        with self.assertRaisesRegex(ProcessLaunchError, "between 1 and 120"):
            build_launch_grant(
                self.session(), executable_reference="executable.python.test-fixture",
                executable_sha256=file_sha256(Path(sys.executable).resolve()), argv=["--version"], environment={},
                timeout_seconds=121, output_limit_bytes=1024, issuer="reviewer",
                issued_at_utc="2026-07-22T12:11:00Z", expires_at_utc="2026-07-22T12:20:00Z",
                nonce="grant.foa-sdk.launch-bounds",
            )

    def test_source_has_no_shell_elevation_or_network_surface(self) -> None:
        source = (REPO_ROOT / "Installer/Bootstrapper/ProcessLauncher/Source/capability_process_launcher.py").read_text(encoding="utf-8")
        for required in ("subprocess.run(", "shell=False", "stdin=subprocess.DEVNULL", "env=dict(checked", "close_fds=True"):
            self.assertIn(required, source)
        for forbidden in ("shell=True", "os.system", "ShellExecute", "runas", "sudo", "msiexec", "socket", "requests", "urllib"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
