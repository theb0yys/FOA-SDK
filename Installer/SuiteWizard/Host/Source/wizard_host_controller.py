#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Pure state/controller layer for the graphical FOA-SDK Suite Wizard host."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Mapping

SUITE_WIZARD_ROOT = Path(__file__).resolve().parents[1]
CATALOG_SOURCE = SUITE_WIZARD_ROOT / "Catalog" / "Source"
if str(CATALOG_SOURCE) not in sys.path:
    sys.path.insert(0, str(CATALOG_SOURCE))

from wizard_catalog import (  # noqa: E402
    CatalogInterfaceError,
    create_selection,
    discover_catalog,
    resolve_selection,
    validate_catalog,
)


class WizardHostError(RuntimeError):
    """Raised when graphical host state or user input is invalid."""


def _mapping(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise WizardHostError(f"{label} must be an object.")
    return value


def _rows(value: object, label: str) -> list[object]:
    if not isinstance(value, list):
        raise WizardHostError(f"{label} must be an array.")
    return value


def _text(value: object, label: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not allow_empty and not value.strip()):
        raise WizardHostError(f"{label} must be a {'string' if allow_empty else 'non-empty string'}.")
    return value


class WizardHostController:
    """Maintains transient UI choices and delegates all decisions to Catalog/Resolver."""

    def __init__(self, installer_root: Path) -> None:
        self._installer_root = Path(installer_root)
        self._catalog: dict[str, object] = {}
        self._suite_id = ""
        self._selected_package_ids: set[str] = set()
        self._excluded_package_ids: set[str] = set()
        self._selected_feature_ids: set[str] = set()
        self._context: dict[str, str] = {}
        self._result: dict[str, object] | None = None
        self.refresh_catalog()

    @property
    def installer_root(self) -> Path:
        return self._installer_root

    @property
    def catalog_sha256(self) -> str:
        return _text(self._catalog.get("catalog_sha256"), "catalog.catalog_sha256")

    @property
    def selected_suite_id(self) -> str:
        return self._suite_id

    @property
    def review_result(self) -> dict[str, object] | None:
        return self._result

    def refresh_catalog(self) -> dict[str, object]:
        catalog = validate_catalog(discover_catalog(self._installer_root))
        suites = [_mapping(row, "catalog.suite") for row in _rows(catalog.get("suites"), "catalog.suites")]
        if not suites:
            raise WizardHostError("The reviewed catalogue contains no suites to display.")
        self._catalog = catalog
        self._suite_id = _text(suites[0].get("suite_id"), "catalog.suite.suite_id")
        self._reset_choices()
        return self.host_snapshot()

    def suite_choices(self) -> list[dict[str, str]]:
        result: list[dict[str, str]] = []
        for raw in _rows(self._catalog.get("suites"), "catalog.suites"):
            suite = _mapping(raw, "catalog.suite")
            result.append({
                "suite_id": _text(suite.get("suite_id"), "suite.suite_id"),
                "display_name": _text(suite.get("display_name"), "suite.display_name"),
                "version": _text(suite.get("version"), "suite.version"),
                "channel": _text(suite.get("channel"), "suite.channel"),
            })
        return result

    def select_suite(self, suite_id: str) -> dict[str, object]:
        requested = _text(suite_id, "suite_id")
        if requested not in {row["suite_id"] for row in self.suite_choices()}:
            raise WizardHostError(f"Unknown suite ID {requested!r}.")
        self._suite_id = requested
        self._reset_choices()
        return self.host_snapshot()

    def package_choices(self) -> list[dict[str, object]]:
        suite = self._suite()
        package_rows = {
            _text(row.get("package_id"), "package.package_id"): row
            for row in (
                _mapping(raw, "catalog.package")
                for raw in _rows(self._catalog.get("packages"), "catalog.packages")
            )
        }
        result: list[dict[str, object]] = []
        for raw_entry in _rows(suite.get("packages"), "suite.packages"):
            entry = _mapping(raw_entry, "suite.package")
            package_id = _text(entry.get("package_id"), "suite.package.package_id")
            package = package_rows.get(package_id)
            if package is None:
                raise WizardHostError(f"Suite references missing package {package_id!r}.")
            selection = _text(entry.get("selection"), f"{package_id}.selection")
            included = self._package_is_included(package_id, selection)
            result.append({
                "package_id": package_id,
                "display_name": _text(package.get("display_name"), f"{package_id}.display_name"),
                "description": _text(
                    package.get("description", ""), f"{package_id}.description", allow_empty=True
                ),
                "version": _text(package.get("version"), f"{package_id}.version"),
                "kind": _text(package.get("kind"), f"{package_id}.kind"),
                "status": _text(package.get("status"), f"{package_id}.status"),
                "selection": selection,
                "order": entry.get("order"),
                "included": included,
                "locked": selection == "required",
                "payload_summary": dict(_mapping(
                    package.get("payload_summary"), f"{package_id}.payload_summary"
                )),
                "license_expression": _text(
                    _mapping(package.get("legal"), f"{package_id}.legal").get("license_expression"),
                    f"{package_id}.legal.license_expression",
                ),
            })
        return result

    def feature_choices(self) -> list[dict[str, object]]:
        suite = self._suite()
        result: list[dict[str, object]] = []
        for raw in _rows(suite.get("features"), "suite.features"):
            feature = _mapping(raw, "suite.feature")
            feature_id = _text(feature.get("feature_id"), "feature.feature_id")
            result.append({
                "feature_id": feature_id,
                "display_name": _text(feature.get("display_name"), f"{feature_id}.display_name"),
                "description": _text(
                    feature.get("description", ""), f"{feature_id}.description", allow_empty=True
                ),
                "package_ids": list(_rows(feature.get("package_ids"), f"{feature_id}.package_ids")),
                "selected": feature_id in self._selected_feature_ids,
            })
        return result

    def set_package_included(self, package_id: str, included: bool) -> dict[str, object]:
        requested = _text(package_id, "package_id")
        entries = {
            _text(entry.get("package_id"), "suite.package.package_id"): _mapping(entry, "suite.package")
            for entry in (
                _mapping(raw, "suite.package")
                for raw in _rows(self._suite().get("packages"), "suite.packages")
            )
        }
        entry = entries.get(requested)
        if entry is None:
            raise WizardHostError(f"Package {requested!r} is outside the selected suite.")
        selection = _text(entry.get("selection"), f"{requested}.selection")
        if selection == "required":
            if not included:
                raise WizardHostError(f"Required package {requested!r} cannot be excluded.")
            return self.host_snapshot()
        if selection == "default":
            self._selected_package_ids.discard(requested)
            if included:
                self._excluded_package_ids.discard(requested)
            else:
                self._excluded_package_ids.add(requested)
        elif selection == "optional":
            self._excluded_package_ids.discard(requested)
            if included:
                self._selected_package_ids.add(requested)
            else:
                self._selected_package_ids.discard(requested)
        else:
            raise WizardHostError(f"Unsupported suite selection mode {selection!r}.")
        self._result = None
        return self.host_snapshot()

    def set_feature_selected(self, feature_id: str, selected: bool) -> dict[str, object]:
        requested = _text(feature_id, "feature_id")
        known = {row["feature_id"] for row in self.feature_choices()}
        if requested not in known:
            raise WizardHostError(f"Unknown feature ID {requested!r}.")
        if selected:
            self._selected_feature_ids.add(requested)
        else:
            self._selected_feature_ids.discard(requested)
        self._result = None
        return self.host_snapshot()

    def set_context(
        self,
        *,
        platform: str,
        architecture: str,
        runtime_target: str,
        game_version: str = "",
        branch: str = "",
    ) -> dict[str, object]:
        self._context = {
            "platform": _text(platform, "platform"),
            "architecture": _text(architecture, "architecture"),
            "runtime_target": _text(runtime_target, "runtime_target"),
            "game_version": _text(game_version, "game_version", allow_empty=True),
            "branch": _text(branch, "branch", allow_empty=True),
        }
        self._result = None
        return self.host_snapshot()

    def create_current_selection(self) -> dict[str, object]:
        return create_selection(
            self._catalog,
            suite_id=self._suite_id,
            platform=self._context["platform"],
            architecture=self._context["architecture"],
            runtime_target=self._context["runtime_target"],
            game_version=self._context["game_version"],
            branch=self._context["branch"],
            selected_package_ids=sorted(self._selected_package_ids),
            excluded_package_ids=sorted(self._excluded_package_ids),
            selected_feature_ids=sorted(self._selected_feature_ids),
        )

    def resolve_review(self) -> dict[str, object]:
        self._result = resolve_selection(self._installer_root, self.create_current_selection())
        return self.review_snapshot()

    def review_snapshot(self) -> dict[str, object]:
        if self._result is None:
            raise WizardHostError("Resolve the current review-only selection first.")
        result = self._result
        plan = _mapping(result.get("plan"), "result.plan")
        view_model = _mapping(result.get("view_model"), "result.view_model")
        return {
            "catalog_sha256": _text(result.get("catalog_sha256"), "result.catalog_sha256"),
            "selection_sha256": _text(result.get("selection_sha256"), "result.selection_sha256"),
            "plan_sha256": _text(plan.get("plan_sha256"), "plan.plan_sha256"),
            "view_model_sha256": _text(
                view_model.get("view_model_sha256"), "view_model.view_model_sha256"
            ),
            "result_sha256": _text(result.get("result_sha256"), "result.result_sha256"),
            "suite": dict(_mapping(view_model.get("suite"), "view_model.suite")),
            "summary": dict(_mapping(view_model.get("summary"), "view_model.summary")),
            "packages": list(_rows(view_model.get("packages"), "view_model.packages")),
            "payload": list(_rows(view_model.get("payload"), "view_model.payload")),
            "warnings": list(_rows(view_model.get("warnings"), "view_model.warnings")),
            "required_acknowledgements": list(_rows(
                view_model.get("required_acknowledgements"),
                "view_model.required_acknowledgements",
            )),
            "authority": dict(_mapping(view_model.get("authority"), "view_model.authority")),
        }

    def host_snapshot(self) -> dict[str, object]:
        suite = self._suite()
        return {
            "catalog_sha256": self.catalog_sha256,
            "suite_choices": self.suite_choices(),
            "selected_suite_id": self._suite_id,
            "suite_description": _text(
                suite.get("description", ""), "suite.description", allow_empty=True
            ),
            "context": dict(self._context),
            "packages": self.package_choices(),
            "features": self.feature_choices(),
            "review_available": self._result is not None,
            "authority": dict(_mapping(self._catalog.get("authority"), "catalog.authority")),
        }

    def _suite(self) -> dict[str, object]:
        matches = [
            _mapping(raw, "catalog.suite")
            for raw in _rows(self._catalog.get("suites"), "catalog.suites")
            if _mapping(raw, "catalog.suite").get("suite_id") == self._suite_id
        ]
        if len(matches) != 1:
            raise WizardHostError(f"Selected suite {self._suite_id!r} is unavailable.")
        return matches[0]

    def _reset_choices(self) -> None:
        self._selected_package_ids.clear()
        self._excluded_package_ids.clear()
        self._selected_feature_ids.clear()
        self._result = None
        suite = self._suite()
        compatibility = _mapping(suite.get("compatibility"), "suite.compatibility")
        platforms = list(_rows(compatibility.get("platforms"), "suite.compatibility.platforms"))
        architectures = list(_rows(
            compatibility.get("architectures"), "suite.compatibility.architectures"
        ))
        runtime_targets = self._runtime_targets_for_suite(suite)
        self._context = {
            "platform": _text(platforms[0], "suite.platform") if platforms else "windows",
            "architecture": _text(architectures[0], "suite.architecture") if architectures else "x86_64",
            "runtime_target": runtime_targets[0] if runtime_targets else "editor-only",
            "game_version": "",
            "branch": "",
        }

    def _runtime_targets_for_suite(self, suite: Mapping[str, object]) -> list[str]:
        entry_ids = {
            _text(_mapping(raw, "suite.package").get("package_id"), "suite.package.package_id")
            for raw in _rows(suite.get("packages"), "suite.packages")
        }
        targets: set[str] = set()
        for raw in _rows(self._catalog.get("packages"), "catalog.packages"):
            package = _mapping(raw, "catalog.package")
            package_id = _text(package.get("package_id"), "package.package_id")
            if package_id not in entry_ids:
                continue
            compatibility = _mapping(package.get("compatibility"), f"{package_id}.compatibility")
            targets.update(
                _text(value, f"{package_id}.runtime_target")
                for value in _rows(
                    compatibility.get("runtime_targets"), f"{package_id}.runtime_targets"
                )
            )
        return sorted(targets)

    def _package_is_included(self, package_id: str, selection: str) -> bool:
        if selection == "required":
            return True
        if selection == "default":
            return package_id not in self._excluded_package_ids
        if selection == "optional":
            return package_id in self._selected_package_ids
        raise WizardHostError(f"Unsupported suite selection mode {selection!r}.")
