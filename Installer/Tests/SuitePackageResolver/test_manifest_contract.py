# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[3] / "Installer" / "SuiteWizard" / "Resolver" / "Source"
sys.path.insert(0, str(SOURCE))

from suite_package_resolver import (  # noqa: E402
    ResolverError,
    _validate_package_document,
    _validate_suite_document,
)

COMMIT = "a" * 40
SHA256 = "0" * 64


def valid_suite() -> dict[str, object]:
    return {
        "$schema": "../../suite.schema.json",
        "schema_version": 1,
        "suite_id": "foa.developer",
        "display_name": "FOA Developer Suite",
        "version": "1.0.0",
        "channel": "development",
        "packages": [{"package_id": "foa.core", "selection": "required", "order": 0, "feature_ids": []}],
        "features": [],
        "compatibility": {"platforms": ["windows"], "architectures": ["x86_64"]},
        "policies": {
            "network_allowed": False,
            "elevation_allowed": False,
            "unreviewed_packages_allowed": False,
            "silent_install_allowed": False,
        },
        "provenance": {
            "source_repository": "https://github.com/theb0yys/FOA-SDK",
            "source_commit": COMMIT,
            "reviewed_by": "maintainer",
            "reviewed_at_utc": "2026-07-22T00:00:00Z",
            "evidence": [],
        },
    }


def valid_package() -> dict[str, object]:
    return {
        "$schema": "../../package.schema.json",
        "schema_version": 1,
        "package_id": "foa.core",
        "display_name": "FOA Core",
        "version": "1.0.0",
        "kind": "core-foundation",
        "status": "supported",
        "source": {
            "kind": "product",
            "repository": "https://github.com/theb0yys/FOA-SDK",
            "commit": COMMIT,
            "path": "Gems/TaintedGrailModdingSDK",
            "fingerprint_sha256": SHA256,
        },
        "compatibility": {
            "platforms": ["windows"],
            "architectures": ["x86_64"],
            "game_versions": [],
            "branches": [],
            "runtime_targets": ["editor-only"],
        },
        "dependencies": [],
        "conflicts": [],
        "capabilities": ["authoring.read"],
        "payload": [{
            "source": "payload/foa-core.bin",
            "destination": "components/foa-core.bin",
            "sha256": SHA256,
            "size_bytes": 1,
            "redistribution": "project-owned",
        }],
        "lifecycle": {
            "install_scope": "per-user",
            "elevation_required": False,
            "repair_supported": True,
            "upgrade_supported": True,
            "uninstall_supported": True,
            "rollback_required": True,
            "preserve_external_workspaces": True,
        },
        "legal": {
            "license_expression": "Apache-2.0 OR MIT",
            "redistribution_review": "approved",
            "notice_files": [],
        },
        "authority": {
            "game_launch": False,
            "runtime_execution": False,
            "deployment": False,
            "save_mutation": False,
            "signing": False,
            "publication": False,
            "catalog_mutation": False,
            "evidence_promotion": False,
        },
    }


class ManifestContractTests(unittest.TestCase):
    def test_reviewed_manifest_contracts_pass(self) -> None:
        _validate_suite_document(valid_suite(), "suite Developer")
        _validate_package_document(valid_package(), "package Core")

    def test_schema_version_drift_fails(self) -> None:
        document = valid_suite()
        document["schema_version"] = 2
        with self.assertRaisesRegex(ResolverError, "schema_version must be exactly 1"):
            _validate_suite_document(document, "suite Developer")

    def test_missing_review_provenance_fails(self) -> None:
        document = valid_suite()
        document["provenance"]["reviewed_by"] = ""
        with self.assertRaisesRegex(ResolverError, "reviewed_by"):
            _validate_suite_document(document, "suite Developer")

    def test_empty_host_compatibility_fails(self) -> None:
        document = valid_package()
        document["compatibility"]["platforms"] = []
        with self.assertRaisesRegex(ResolverError, "must contain at least one"):
            _validate_package_document(document, "package Core")

    def test_empty_payload_fails(self) -> None:
        document = valid_package()
        document["payload"] = []
        with self.assertRaisesRegex(ResolverError, "payload must contain at least one file"):
            _validate_package_document(document, "package Core")

    def test_invalid_install_scope_fails(self) -> None:
        document = valid_package()
        document["lifecycle"]["install_scope"] = "invalid"
        with self.assertRaisesRegex(ResolverError, "install_scope must be one of"):
            _validate_package_document(document, "package Core")


if __name__ == "__main__":
    unittest.main()
