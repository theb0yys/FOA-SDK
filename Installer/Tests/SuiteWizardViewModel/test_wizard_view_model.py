# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations

import ast
import copy
import importlib.util
import unittest
from pathlib import Path

SOURCE = (
    Path(__file__).resolve().parents[2]
    / "SuiteWizard"
    / "ViewModel"
    / "Source"
    / "wizard_view_model.py"
)
SPEC = importlib.util.spec_from_file_location("foa_wizard_view_model", SOURCE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load wizard view-model module: {SOURCE}")
vm = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vm)


def package(
    package_id: str,
    *,
    kind: str = "plugin",
    elevation: bool = False,
    rollback: bool = False,
    payload: list[dict[str, object]] | None = None,
) -> dict[str, object]:
    return {
        "package_id": package_id,
        "display_name": package_id,
        "version": "1.0.0",
        "kind": kind,
        "status": "supported",
        "selection_reasons": ["suite:required"],
        "dependency_ids": [],
        "capabilities": [],
        "manifest_sha256": "1" * 64,
        "source": {
            "kind": "product",
            "repository": "https://github.com/theb0yys/FOA-SDK",
            "commit": "a" * 40,
            "path": f"Installer/Packages/{package_id}",
        },
        "lifecycle": {
            "install_scope": "per-machine" if elevation else "per-user",
            "elevation_required": elevation,
            "repair_supported": True,
            "upgrade_supported": True,
            "uninstall_supported": True,
            "rollback_required": rollback,
            "preserve_external_workspaces": True,
        },
        "legal": {
            "license_expression": "Apache-2.0 OR MIT",
            "redistribution_review": "approved",
            "notice_files": [],
        },
        "payload": payload or [],
    }


def plan(
    packages: list[dict[str, object]] | None = None,
    *,
    network: bool = False,
    warnings: list[str] | None = None,
) -> dict[str, object]:
    rows = packages or [package("foa.core")]
    base = {
        "schema_version": 1,
        "suite": {
            "suite_id": "foa.developer",
            "display_name": "Developer",
            "version": "1.0.0",
            "channel": "development",
            "manifest_sha256": "2" * 64,
        },
        "context": {
            "platform": "windows",
            "architecture": "x86_64",
            "runtime_target": "editor-only",
            "game_version": "",
            "branch": "",
        },
        "selection": {
            "selected_package_ids": [],
            "excluded_package_ids": [],
            "selected_feature_ids": [],
        },
        "policies": {
            "network_allowed": network,
            "elevation_allowed": True,
            "unreviewed_packages_allowed": False,
            "silent_install_allowed": False,
        },
        "requires_elevation": any(
            row["lifecycle"]["elevation_required"] for row in rows
        ),
        "package_order": [row["package_id"] for row in rows],
        "packages": rows,
        "summary": {
            "package_count": len(rows),
            "payload_file_count": sum(len(row["payload"]) for row in rows),
            "payload_size_bytes": sum(
                item["size_bytes"] for row in rows for item in row["payload"]
            ),
        },
        "warnings": warnings or [],
    }
    return {**base, "plan_sha256": vm.sha256(base)}


class WizardViewModelTests(unittest.TestCase):
    def test_stable_view_model_and_package_order(self) -> None:
        document = plan(
            [
                package("foa.base"),
                package(
                    "foa.ui",
                    payload=[
                        {
                            "source": "B",
                            "destination": "Bin/Z.dll",
                            "sha256": "3" * 64,
                            "size_bytes": 4,
                            "redistribution": "project-owned",
                        },
                        {
                            "source": "A",
                            "destination": "Bin/A.dll",
                            "sha256": "4" * 64,
                            "size_bytes": 2,
                            "redistribution": "approved",
                        },
                    ],
                ),
            ]
        )
        first = vm.build_view_model(document)
        second = vm.build_view_model(copy.deepcopy(document))
        self.assertEqual(vm.canonical_json(first), vm.canonical_json(second))
        self.assertEqual(
            [row["package_id"] for row in first["packages"]],
            ["foa.base", "foa.ui"],
        )
        self.assertEqual(
            [row["destination"] for row in first["payload"]],
            ["Bin/A.dll", "Bin/Z.dll"],
        )

    def test_plan_hash_and_summary_tampering_fail(self) -> None:
        document = plan()
        document["suite"]["display_name"] = "Changed"
        with self.assertRaisesRegex(vm.WizardContractError, "plan_sha256 mismatch"):
            vm.build_view_model(document)

        document = plan()
        document["summary"]["package_count"] = 9
        unsigned = {
            key: value for key, value in document.items() if key != "plan_sha256"
        }
        document["plan_sha256"] = vm.sha256(unsigned)
        with self.assertRaisesRegex(vm.WizardContractError, "summary"):
            vm.build_view_model(document)

    def test_conditional_acknowledgements(self) -> None:
        document = plan(
            [
                package(
                    "foa.runtime",
                    kind="runtime-adapter",
                    elevation=True,
                    rollback=True,
                    payload=[
                        {
                            "source": "Runtime.dll",
                            "destination": "Runtime/Runtime.dll",
                            "sha256": "5" * 64,
                            "size_bytes": 10,
                            "redistribution": "notice-required",
                        }
                    ],
                )
            ],
            network=True,
            warnings=["Experimental runtime path."],
        )
        model = vm.build_view_model(document)
        ids = set(vm.required_acknowledgement_ids(model))
        self.assertEqual(
            ids,
            {
                "review.elevation",
                "review.exact-plan",
                "review.external-workspaces",
                "review.licenses",
                "review.network-policy",
                "review.payload",
                "review.rollback",
                "review.runtime-adapter",
                "review.warnings",
            },
        )

    def test_acknowledgement_evaluation(self) -> None:
        model = vm.build_view_model(plan())
        required = vm.required_acknowledgement_ids(model)
        partial = vm.evaluate_acknowledgements(model, required[:-1])
        self.assertFalse(partial["ready"])
        self.assertEqual(
            partial["missing_acknowledgement_ids"], [required[-1]]
        )
        self.assertTrue(vm.evaluate_acknowledgements(model, required)["ready"])
        with self.assertRaisesRegex(
            vm.WizardContractError, "Unknown acknowledgement"
        ):
            vm.evaluate_acknowledgements(model, [*required, "review.unknown"])
        with self.assertRaisesRegex(
            vm.WizardContractError, "must not contain duplicates"
        ):
            vm.evaluate_acknowledgements(model, [required[0], required[0]])

    def test_confirmation_binds_plan_and_view_model(self) -> None:
        document = plan()
        model = vm.build_view_model(document)
        confirmation = vm.create_confirmation(
            document,
            model,
            expected_plan_sha256=document["plan_sha256"],
            acknowledged_ids=vm.required_acknowledgement_ids(model),
            confirmed_by="Maintainer",
            confirmed_at_utc="2026-07-22T03:00:00Z",
        )
        self.assertTrue(
            vm.confirmation_is_current(document, model, confirmation)
        )
        self.assertEqual(confirmation["confirmation_scope"], "review-only")
        self.assertTrue(
            all(value is False for value in confirmation["authority"].values())
        )

    def test_missing_acknowledgement_and_stale_expected_hash_fail(self) -> None:
        document = plan()
        model = vm.build_view_model(document)
        with self.assertRaisesRegex(vm.WizardContractError, "Expected plan hash"):
            vm.create_confirmation(
                document,
                model,
                expected_plan_sha256="0" * 64,
                acknowledged_ids=vm.required_acknowledgement_ids(model),
                confirmed_by="Maintainer",
                confirmed_at_utc="2026-07-22T03:00:00Z",
            )
        with self.assertRaisesRegex(
            vm.WizardContractError, "Missing required acknowledgements"
        ):
            vm.create_confirmation(
                document,
                model,
                expected_plan_sha256=document["plan_sha256"],
                acknowledged_ids=[],
                confirmed_by="Maintainer",
                confirmed_at_utc="2026-07-22T03:00:00Z",
            )

    def test_plan_change_invalidates_confirmation(self) -> None:
        original = plan()
        original_model = vm.build_view_model(original)
        confirmation = vm.create_confirmation(
            original,
            original_model,
            expected_plan_sha256=original["plan_sha256"],
            acknowledged_ids=vm.required_acknowledgement_ids(original_model),
            confirmed_by="Maintainer",
            confirmed_at_utc="2026-07-22T03:00:00Z",
        )
        changed = plan([package("foa.core"), package("foa.ui")])
        changed_model = vm.build_view_model(changed)
        self.assertFalse(
            vm.confirmation_is_current(changed, changed_model, confirmation)
        )
        with self.assertRaisesRegex(vm.WizardContractError, "stale"):
            vm.verify_confirmation(changed, changed_model, confirmation)

    def test_tampered_confirmation_and_time_fail(self) -> None:
        document = plan()
        model = vm.build_view_model(document)
        confirmation = vm.create_confirmation(
            document,
            model,
            expected_plan_sha256=document["plan_sha256"],
            acknowledged_ids=vm.required_acknowledgement_ids(model),
            confirmed_by="Maintainer",
            confirmed_at_utc="2026-07-22T03:00:00Z",
        )
        confirmation["confirmed_by"] = "Someone else"
        with self.assertRaisesRegex(vm.WizardContractError, "fingerprint"):
            vm.verify_confirmation(document, model, confirmation)
        with self.assertRaisesRegex(vm.WizardContractError, "ending in Z"):
            vm.create_confirmation(
                document,
                model,
                expected_plan_sha256=document["plan_sha256"],
                acknowledged_ids=vm.required_acknowledgement_ids(model),
                confirmed_by="Maintainer",
                confirmed_at_utc="2026-07-22T04:00:00+01:00",
            )

    def test_module_has_no_executor_surface(self) -> None:
        tree = ast.parse(SOURCE.read_text(encoding="utf-8"))
        imports = set()
        calls = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                imports.update(
                    alias.name.split(".", 1)[0] for alias in node.names
                )
            elif isinstance(node, ast.ImportFrom) and node.module:
                imports.add(node.module.split(".", 1)[0])
            elif isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
                calls.add(node.func.id)
        self.assertTrue(
            imports <= {"__future__", "datetime", "hashlib", "json", "re", "typing"}
        )
        self.assertTrue(
            {"open", "exec", "eval", "compile", "__import__"}.isdisjoint(calls)
        )

    def test_view_model_tampering_fails(self) -> None:
        document = plan()
        model = vm.build_view_model(document)
        model["review_state"] = "confirmed"
        with self.assertRaisesRegex(
            vm.WizardContractError, "view_model_sha256"
        ):
            vm.validate_view_model(document, model)

    def test_forged_resolver_output_fails_closed(self) -> None:
        document = plan()
        document["selection"]["selected_package_ids"] = "foa.core"
        unsigned = {
            key: value for key, value in document.items() if key != "plan_sha256"
        }
        document["plan_sha256"] = vm.sha256(unsigned)
        with self.assertRaisesRegex(
            vm.WizardContractError, "selected_package_ids"
        ):
            vm.build_view_model(document)

        document = plan()
        document["packages"][0]["status"] = "blocked"
        unsigned = {
            key: value for key, value in document.items() if key != "plan_sha256"
        }
        document["plan_sha256"] = vm.sha256(unsigned)
        with self.assertRaisesRegex(vm.WizardContractError, "not reviewable"):
            vm.build_view_model(document)


if __name__ == "__main__":
    unittest.main()
