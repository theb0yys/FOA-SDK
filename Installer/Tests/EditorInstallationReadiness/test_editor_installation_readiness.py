# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
for root in (
    REPO_ROOT / "Installer/Bootstrapper/EditorInstallationReadiness/Source",
    REPO_ROOT / "Installer/Bootstrapper/InstallationStateRegistry/Source",
    REPO_ROOT / "Installer/SuiteWizard/ViewModel/Source",
):
    sys.path.insert(0, str(root))

from editor_installation_readiness import (  # noqa: E402
    EDITOR_READINESS_CAPABILITY,
    EditorInstallationReadinessError,
    build_editor_installation_readiness,
    validate_editor_installation_readiness,
)
from installation_state_registry import (  # noqa: E402
    QUERY_CAPABILITY,
    SNAPSHOT_SCOPE,
    SNAPSHOT_STATEMENT,
    validate_registry_snapshot,
)
from wizard_view_model import AUTHORITY_FIELDS, sha256  # noqa: E402


HASH_A = "a" * 64
HASH_B = "b" * 64
HASH_C = "c" * 64
HASH_D = "d" * 64
HASH_E = "e" * 64
HASH_F = "f" * 64
HASH_1 = "1" * 64
HASH_2 = "2" * 64
HASH_3 = "3" * 64
HASH_4 = "4" * 64


class EditorInstallationReadinessTests(unittest.TestCase):
    def _authority(self) -> dict[str, bool]:
        return {field: False for field in AUTHORITY_FIELDS}

    def _record(
        self,
        *,
        state_reference: str = "installation.foa-sdk.default",
        state_status: str = "active",
        target_reference: str = "target.foa-sdk.editor",
        state_file_sha256: str = HASH_B,
        state_record_sha256: str = HASH_C,
        elevated: bool = False,
    ) -> dict[str, object]:
        record: dict[str, object] = {
            "state_reference": state_reference,
            "state_status": state_status,
            "operation": "install",
            "target_reference": target_reference,
            "prior_installation_reference": None,
            "published_at_utc": "2026-07-22T12:12:00Z",
            "session_sha256": HASH_A,
            "handoff_sha256": HASH_B,
            "lifecycle_result_sha256": HASH_C,
            "copy_receipt_sha256": HASH_D,
            "launch_result_sha256": HASH_E,
            "state_record_sha256": state_record_sha256,
            "state_file_sha256": state_file_sha256,
            "package_order": ["package.foa-sdk.core"],
            "summary": {
                "package_count": 1,
                "payload_file_count": 3,
                "payload_size_bytes": 128,
            },
        }
        if elevated:
            record.update(
                {
                    "elevation_request_confirmed": True,
                    "elevated_completion_observed": True,
                    "elevation_result_sha256": HASH_F,
                    "elevated_completion_observation_sha256": HASH_1,
                }
            )
        return record

    def _snapshot(self, records: list[dict[str, object]]) -> dict[str, object]:
        active = sum(1 for row in records if row["state_status"] == "active")
        removed = sum(1 for row in records if row["state_status"] == "removed")
        rolled_back = sum(1 for row in records if row["state_status"] == "rolled-back")
        base = {
            "schema_version": 1,
            "snapshot_scope": SNAPSHOT_SCOPE,
            "capability": QUERY_CAPABILITY,
            "state_root_reference": "state-root.foa-sdk.local",
            "observed_at_utc": "2026-07-22T12:13:00Z",
            "record_count": len(records),
            "active_count": active,
            "removed_count": removed,
            "rolled_back_count": rolled_back,
            "records": records,
            "product_or_game_directory_mutated": False,
            "runtime_executed": False,
            "save_mutated": False,
            "signing_performed": False,
            "network_publication_performed": False,
            "catalog_mutated": False,
            "evidence_promoted": False,
            "statement": SNAPSHOT_STATEMENT,
            "authority": self._authority(),
        }
        return {**base, "snapshot_sha256": sha256(base)}

    def _readiness(
        self,
        snapshot: dict[str, object],
        *,
        target_reference: str | None = None,
    ) -> dict[str, object]:
        return build_editor_installation_readiness(
            snapshot,
            editor_reference="editor.foa-sdk.tg",
            target_reference=target_reference,
            requested_at_utc="2026-07-22T12:14:00Z",
        )

    def test_empty_registry_blocks_editor_readiness(self) -> None:
        snapshot = self._snapshot([])
        self.assertEqual(validate_registry_snapshot(snapshot), snapshot)
        readiness = self._readiness(snapshot)
        self.assertFalse(readiness["ready"])
        self.assertEqual(readiness["status"], "blocked-no-active-installation")
        self.assertEqual(readiness["capability"], EDITOR_READINESS_CAPABILITY)
        self.assertIsNone(readiness["state_reference"])
        self.assertEqual(validate_editor_installation_readiness(readiness), readiness)

    def test_active_installation_snapshot_makes_editor_ready(self) -> None:
        record = self._record()
        snapshot = self._snapshot([record])
        readiness = self._readiness(snapshot)
        second = self._readiness(snapshot)
        self.assertEqual(readiness, second)
        self.assertTrue(readiness["ready"])
        self.assertEqual(readiness["status"], "ready")
        self.assertEqual(readiness["target_reference"], "target.foa-sdk.editor")
        self.assertEqual(readiness["state_reference"], "installation.foa-sdk.default")
        self.assertEqual(readiness["state_file_sha256"], HASH_B)
        self.assertEqual(readiness["state_record_sha256"], HASH_C)
        self.assertEqual(readiness["evidence"]["lifecycle_result_sha256"], HASH_C)
        self.assertEqual(validate_editor_installation_readiness(readiness), readiness)

    def test_elevated_installation_provenance_is_preserved_for_editor(self) -> None:
        record = self._record(elevated=True)
        snapshot = self._snapshot([record])
        readiness = self._readiness(snapshot)
        self.assertTrue(readiness["ready"])
        self.assertTrue(readiness["evidence"]["elevation_request_confirmed"])
        self.assertTrue(readiness["evidence"]["elevated_completion_observed"])
        self.assertEqual(readiness["evidence"]["elevation_result_sha256"], HASH_F)
        self.assertEqual(readiness["evidence"]["elevated_completion_observation_sha256"], HASH_1)
        self.assertEqual(validate_editor_installation_readiness(readiness), readiness)

    def test_removed_or_mismatched_installation_blocks_editor_readiness(self) -> None:
        removed = self._record(state_status="removed")
        snapshot = self._snapshot([removed])
        readiness = self._readiness(snapshot)
        self.assertFalse(readiness["ready"])
        self.assertEqual(readiness["status"], "blocked-no-active-installation")
        active_other_target = self._record(target_reference="target.foa-sdk.other")
        other_snapshot = self._snapshot([active_other_target])
        targeted = self._readiness(other_snapshot, target_reference="target.foa-sdk.editor")
        self.assertFalse(targeted["ready"])
        self.assertEqual(targeted["status"], "blocked-no-active-installation")

    def test_multiple_active_installations_fail_ambiguously(self) -> None:
        first = self._record(
            state_reference="installation.foa-sdk.first",
            state_file_sha256=HASH_1,
            state_record_sha256=HASH_2,
        )
        second = self._record(
            state_reference="installation.foa-sdk.second",
            state_file_sha256=HASH_3,
            state_record_sha256=HASH_4,
        )
        snapshot = self._snapshot([first, second])
        readiness = self._readiness(snapshot)
        self.assertFalse(readiness["ready"])
        self.assertEqual(readiness["status"], "blocked-ambiguous-active-installation")
        self.assertEqual(readiness["active_match_count"], 2)
        self.assertEqual(validate_editor_installation_readiness(readiness), readiness)

    def test_tampered_readiness_fails_closed(self) -> None:
        readiness = self._readiness(self._snapshot([self._record()]))
        tampered = copy.deepcopy(readiness)
        tampered["ready"] = False
        with self.assertRaises(EditorInstallationReadinessError):
            validate_editor_installation_readiness(tampered)

    def test_source_does_not_reintroduce_execution_or_mutation_surfaces(self) -> None:
        source = (
            REPO_ROOT
            / "Installer/Bootstrapper/EditorInstallationReadiness/Source/editor_installation_readiness.py"
        ).read_text(encoding="utf-8")
        for forbidden in (
            "subprocess",
            "ShellExecute",
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
            "tg-editor.consume-installation-state",
            "validate_registry_snapshot",
            "blocked-no-active-installation",
            "blocked-ambiguous-active-installation",
        ):
            self.assertIn(required, source)


if __name__ == "__main__":
    unittest.main()
