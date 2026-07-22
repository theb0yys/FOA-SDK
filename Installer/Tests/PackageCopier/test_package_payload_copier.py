# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/PackageCopier/Source",
    REPO_ROOT / "Installer/Tests/Security",
):
    sys.path.insert(0, str(root))

from package_payload_copier import PackageCopyError, build_copy_grant, stage_payload, validate_copy_receipt  # noqa: E402
from security_test_support import InstallerSecurityFixture  # noqa: E402


class PackagePayloadCopierTests(unittest.TestCase):
    @staticmethod
    def _claim_path(fixture: InstallerSecurityFixture, grant: dict[str, object]) -> Path:
        return fixture.claim_root / "claim.package-copy-grant" / f"{grant['grant_sha256']}.claim.json"

    def test_stages_authenticated_inventory_with_no_replace_and_one_shot(self) -> None:
        with InstallerSecurityFixture() as fixture:
            grant = build_copy_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                issuer="FOA-SDK copy broker", issued_at_utc="2026-07-22T12:04:00Z",
                expires_at_utc="2026-07-22T12:10:00Z", nonce="grant.foa-sdk.copy-0001",
            )
            target = fixture.root / "staging"
            receipt = stage_payload(
                grant, REPO_ROOT, target, authority_key_path=fixture.authority_key,
                claim_root=fixture.claim_root, copied_at_utc="2026-07-22T12:05:00Z",
            )
            self.assertTrue(target.is_dir())
            self.assertEqual(validate_copy_receipt(receipt, authority_key_path=fixture.authority_key), receipt)
            self.assertTrue(receipt["copy_performed"])
            self.assertIn("claim_sha256", receipt)
            self.assertTrue(self._claim_path(fixture, grant).is_file())
            with self.assertRaisesRegex(PackageCopyError, "already been consumed"):
                stage_payload(
                    grant, REPO_ROOT, fixture.root / "staging-replay", authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root, copied_at_utc="2026-07-22T12:06:00Z",
                )
            self.assertFalse((fixture.root / "staging-replay").exists())

    def test_existing_destination_is_preserved_without_consuming_grant(self) -> None:
        with InstallerSecurityFixture() as fixture:
            grant = build_copy_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                issuer="FOA-SDK copy broker", issued_at_utc="2026-07-22T12:04:00Z",
                expires_at_utc="2026-07-22T12:10:00Z", nonce="grant.foa-sdk.copy-existing",
            )
            target = fixture.root / "existing"
            target.mkdir()
            marker = target / "keep"
            marker.write_text("keep", encoding="utf-8")
            with self.assertRaisesRegex(PackageCopyError, "absent absolute path"):
                stage_payload(
                    grant, REPO_ROOT, target, authority_key_path=fixture.authority_key,
                    claim_root=fixture.claim_root, copied_at_utc="2026-07-22T12:05:00Z",
                )
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")
            self.assertFalse(self._claim_path(fixture, grant).exists())

    def test_missing_source_root_does_not_consume_grant(self) -> None:
        with InstallerSecurityFixture() as fixture:
            grant = build_copy_grant(
                fixture.session, authority_key_path=fixture.authority_key,
                issuer="FOA-SDK copy broker", issued_at_utc="2026-07-22T12:04:00Z",
                expires_at_utc="2026-07-22T12:10:00Z", nonce="grant.foa-sdk.copy-missing-source",
            )
            with self.assertRaisesRegex(PackageCopyError, "Source root"):
                stage_payload(
                    grant, fixture.root / "missing-source", fixture.root / "staging",
                    authority_key_path=fixture.authority_key, claim_root=fixture.claim_root,
                    copied_at_utc="2026-07-22T12:05:00Z",
                )
            self.assertFalse(self._claim_path(fixture, grant).exists())


if __name__ == "__main__":
    unittest.main()
