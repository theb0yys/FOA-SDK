# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/InstallationStatePublisher/Source",
    REPO_ROOT / "Installer/Bootstrapper/InstallationStateRegistry/Source",
    REPO_ROOT / "Installer/Bootstrapper/PackageEngine/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

PUBLISHER_TESTS = REPO_ROOT / "Installer/Tests/InstallationStatePublisher/test_installation_state_publisher.py"
PUBLISHER_SPEC = importlib.util.spec_from_file_location("foa_installation_state_publisher_closed_loop", PUBLISHER_TESTS)
if PUBLISHER_SPEC is None or PUBLISHER_SPEC.loader is None:
    raise RuntimeError(f"Unable to load publisher helper tests: {PUBLISHER_TESTS}")
PUBLISHER_MODULE = importlib.util.module_from_spec(PUBLISHER_SPEC)
sys.modules[PUBLISHER_SPEC.name] = PUBLISHER_MODULE
PUBLISHER_SPEC.loader.exec_module(PUBLISHER_MODULE)

from installation_state_publisher import publish_state_record, validate_publication_receipt  # noqa: E402
from installation_state_registry import build_lifecycle_eligibility, query_state_registry  # noqa: E402
from package_engine import validate_capability_token, validate_engine_session  # noqa: E402
from security_test_support import InstallerSecurityFixture  # noqa: E402


class NonElevatedInstallerClosedLoopTests(unittest.TestCase):
    def test_non_elevated_install_path_closes_loop_from_admission_to_active_state(self) -> None:
        publisher = PUBLISHER_MODULE.InstallationStatePublisherTests(
            methodName="test_publication_is_authenticated_capability_bound_and_one_shot"
        )
        with InstallerSecurityFixture() as fixture:
            self.assertEqual(
                fixture.token["admission_bound_handoff_sha256"],
                fixture.admission_bound_handoff["binding_sha256"],
            )
            self.assertEqual(
                fixture.session["admission_bound_handoff_sha256"],
                fixture.admission_bound_handoff["binding_sha256"],
            )
            self.assertEqual(
                validate_capability_token(fixture.token, authority_key_path=fixture.authority_key),
                fixture.token,
            )
            self.assertEqual(
                validate_engine_session(fixture.session, authority_key_path=fixture.authority_key),
                fixture.session,
            )
            lifecycle = publisher._lifecycle(fixture)
            publication_grant = publisher._grant(fixture, lifecycle)
            state_root = fixture.root / "state"
            state_root.mkdir()
            publication_receipt = publish_state_record(
                publication_grant,
                state_root,
                authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root,
                published_at_utc="2026-07-22T12:08:00Z",
            )
            self.assertEqual(
                validate_publication_receipt(publication_receipt, authority_key_path=fixture.authority_key),
                publication_receipt,
            )
            snapshot = query_state_registry(
                state_root,
                authority_key_path=fixture.authority_key,
                state_root_reference="state-root.foa-sdk.closed-loop",
                observed_at_utc="2026-07-22T12:09:00Z",
            )
            self.assertEqual(snapshot["record_count"], 1)
            self.assertEqual(snapshot["active_count"], 1)
            self.assertEqual(snapshot["removed_count"], 0)
            repair = build_lifecycle_eligibility(
                snapshot,
                operation="repair",
                target_reference="installation.foa-sdk.default",
                decided_at_utc="2026-07-22T12:10:00Z",
            )
            duplicate_install = build_lifecycle_eligibility(
                snapshot,
                operation="install",
                target_reference="installation.foa-sdk.default",
                decided_at_utc="2026-07-22T12:10:01Z",
            )
            self.assertTrue(repair["eligible"])
            self.assertFalse(duplicate_install["eligible"])
            self.assertEqual(duplicate_install["reason"], "blocked-current-installation-state-exists")


if __name__ == "__main__":
    unittest.main()
