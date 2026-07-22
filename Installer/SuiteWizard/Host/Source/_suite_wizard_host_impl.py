#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Native graphical review host for the FOA-SDK Suite Wizard catalogue."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

HOST_SOURCE = Path(__file__).resolve().parent
if str(HOST_SOURCE) not in sys.path:
    sys.path.insert(0, str(HOST_SOURCE))

from wizard_host_controller import WizardHostController, WizardHostError  # noqa: E402
from wizard_catalog import CatalogInterfaceError  # noqa: E402
from suite_package_resolver import ResolverError  # noqa: E402
from wizard_view_model import WizardContractError  # noqa: E402


class SuiteWizardHost:
    """Tk/ttk rendering surface over the pure WizardHostController."""

    def __init__(self, root: object, controller: WizardHostController, tk: object, ttk: object, messagebox: object) -> None:
        self.root = root
        self.controller = controller
        self.tk = tk
        self.ttk = ttk
        self.messagebox = messagebox
        self.package_vars: dict[str, object] = {}
        self.feature_vars: dict[str, object] = {}

        self.status_var = tk.StringVar(value="Ready. Review-only mode; no files will be installed.")
        self.catalog_hash_var = tk.StringVar()
        self.suite_var = tk.StringVar()
        self.platform_var = tk.StringVar()
        self.architecture_var = tk.StringVar()
        self.runtime_target_var = tk.StringVar()
        self.game_version_var = tk.StringVar()
        self.branch_var = tk.StringVar()
        self.summary_var = tk.StringVar(value="Resolve a selection to display its exact review model.")
        self.review_hash_vars = {
            name: tk.StringVar()
            for name in (
                "catalog_sha256",
                "selection_sha256",
                "plan_sha256",
                "view_model_sha256",
                "result_sha256",
            )
        }

        root.title("FOA-SDK Suite Wizard")
        root.minsize(1080, 760)
        root.geometry("1220x840")
        root.protocol("WM_DELETE_WINDOW", root.destroy)
        root.bind("<Control-r>", lambda _event: self.refresh_catalog())
        root.bind("<Control-Return>", lambda _event: self.resolve_review())
        root.bind("<Escape>", lambda _event: root.destroy())

        self._build_ui()
        self.refresh_catalog(initial=True)

    def _build_ui(self) -> None:
        ttk = self.ttk
        root_frame = ttk.Frame(self.root, padding=12)
        root_frame.pack(fill="both", expand=True)

        title = ttk.Label(root_frame, text="FOA-SDK Suite Wizard", font=("TkDefaultFont", 17, "bold"))
        title.pack(anchor="w")
        ttk.Label(
            root_frame,
            text=(
                "Select reviewed catalogue choices and inspect the exact resolver-backed review model. "
                "This host cannot acquire, install, elevate, launch, deploy, sign, or publish."
            ),
            wraplength=1120,
        ).pack(anchor="w", pady=(2, 10))

        fingerprint = ttk.Frame(root_frame)
        fingerprint.pack(fill="x", pady=(0, 8))
        ttk.Label(fingerprint, text="Catalogue fingerprint:").pack(side="left")
        entry = ttk.Entry(fingerprint, textvariable=self.catalog_hash_var, state="readonly")
        entry.pack(side="left", fill="x", expand=True, padx=(8, 0))

        self.notebook = ttk.Notebook(root_frame)
        self.notebook.pack(fill="both", expand=True)
        self.choose_tab = ttk.Frame(self.notebook, padding=10)
        self.review_tab = ttk.Frame(self.notebook, padding=10)
        self.notebook.add(self.choose_tab, text="1. Choose")
        self.notebook.add(self.review_tab, text="2. Review")

        self._build_choose_tab()
        self._build_review_tab()

        status = ttk.Label(root_frame, textvariable=self.status_var, anchor="w")
        status.pack(fill="x", pady=(8, 0))

    def _build_choose_tab(self) -> None:
        ttk = self.ttk
        self.choose_tab.columnconfigure(0, weight=1)
        self.choose_tab.rowconfigure(4, weight=1)

        suite_frame = ttk.LabelFrame(self.choose_tab, text="Reviewed suite", padding=10)
        suite_frame.grid(row=0, column=0, sticky="ew")
        suite_frame.columnconfigure(1, weight=1)
        ttk.Label(suite_frame, text="Suite:").grid(row=0, column=0, sticky="w")
        self.suite_combo = ttk.Combobox(suite_frame, textvariable=self.suite_var, state="readonly")
        self.suite_combo.grid(row=0, column=1, sticky="ew", padx=(8, 0))
        self.suite_combo.bind("<<ComboboxSelected>>", self._on_suite_selected)
        self.suite_description = ttk.Label(suite_frame, wraplength=1050)
        self.suite_description.grid(row=1, column=0, columnspan=2, sticky="w", pady=(8, 0))

        context = ttk.LabelFrame(self.choose_tab, text="Compatibility context", padding=10)
        context.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        for column in range(6):
            context.columnconfigure(column, weight=1 if column % 2 else 0)
        ttk.Label(context, text="Platform:").grid(row=0, column=0, sticky="w")
        self.platform_combo = ttk.Combobox(context, textvariable=self.platform_var)
        self.platform_combo.grid(row=0, column=1, sticky="ew", padx=(6, 14))
        ttk.Label(context, text="Architecture:").grid(row=0, column=2, sticky="w")
        self.architecture_combo = ttk.Combobox(context, textvariable=self.architecture_var)
        self.architecture_combo.grid(row=0, column=3, sticky="ew", padx=(6, 14))
        ttk.Label(context, text="Runtime target:").grid(row=0, column=4, sticky="w")
        self.runtime_combo = ttk.Combobox(context, textvariable=self.runtime_target_var)
        self.runtime_combo.grid(row=0, column=5, sticky="ew", padx=(6, 0))
        ttk.Label(context, text="Game version (optional):").grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(context, textvariable=self.game_version_var).grid(
            row=1, column=1, sticky="ew", padx=(6, 14), pady=(8, 0)
        )
        ttk.Label(context, text="Branch (optional):").grid(row=1, column=2, sticky="w", pady=(8, 0))
        ttk.Entry(context, textvariable=self.branch_var).grid(
            row=1, column=3, sticky="ew", padx=(6, 14), pady=(8, 0)
        )

        choices = ttk.Panedwindow(self.choose_tab, orient="horizontal")
        choices.grid(row=4, column=0, sticky="nsew", pady=(8, 0))
        self.package_frame = ttk.LabelFrame(choices, text="Packages", padding=8)
        self.feature_frame = ttk.LabelFrame(choices, text="Features", padding=8)
        choices.add(self.package_frame, weight=3)
        choices.add(self.feature_frame, weight=2)

        actions = ttk.Frame(self.choose_tab)
        actions.grid(row=5, column=0, sticky="ew", pady=(10, 0))
        ttk.Button(actions, text="Refresh catalogue  Ctrl+R", command=self.refresh_catalog).pack(side="left")
        self.resolve_button = ttk.Button(
            actions,
            text="Resolve review  Ctrl+Enter",
            command=self.resolve_review,
        )
        self.resolve_button.pack(side="right")
        ttk.Button(actions, text="Close  Esc", command=self.root.destroy).pack(side="right", padx=(0, 8))

    def _build_review_tab(self) -> None:
        ttk = self.ttk
        self.review_tab.columnconfigure(0, weight=1)
        self.review_tab.rowconfigure(2, weight=1)

        ttk.Label(self.review_tab, textvariable=self.summary_var, wraplength=1080).grid(
            row=0, column=0, sticky="ew"
        )

        hashes = ttk.LabelFrame(self.review_tab, text="Exact review chain", padding=8)
        hashes.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        hashes.columnconfigure(1, weight=1)
        labels = {
            "catalog_sha256": "Catalogue",
            "selection_sha256": "Selection",
            "plan_sha256": "Plan",
            "view_model_sha256": "View model",
            "result_sha256": "Result",
        }
        for row, name in enumerate(labels):
            ttk.Label(hashes, text=f"{labels[name]}:").grid(row=row, column=0, sticky="w")
            ttk.Entry(hashes, textvariable=self.review_hash_vars[name], state="readonly").grid(
                row=row, column=1, sticky="ew", padx=(8, 0), pady=1
            )

        review_panes = ttk.Notebook(self.review_tab)
        review_panes.grid(row=2, column=0, sticky="nsew", pady=(8, 0))
        packages_page = ttk.Frame(review_panes, padding=6)
        payload_page = ttk.Frame(review_panes, padding=6)
        notices_page = ttk.Frame(review_panes, padding=6)
        review_panes.add(packages_page, text="Resolved packages")
        review_panes.add(payload_page, text="Planned files")
        review_panes.add(notices_page, text="Warnings and acknowledgements")

        packages_page.columnconfigure(0, weight=1)
        packages_page.rowconfigure(0, weight=1)
        self.package_tree = ttk.Treeview(
            packages_page,
            columns=("order", "name", "version", "status", "files", "bytes"),
            show="headings",
        )
        package_headings = (
            ("order", "Order", 65),
            ("name", "Package", 360),
            ("version", "Version", 100),
            ("status", "Status", 100),
            ("files", "Files", 75),
            ("bytes", "Bytes", 110),
        )
        for key, title, width in package_headings:
            self.package_tree.heading(key, text=title)
            self.package_tree.column(key, width=width, stretch=key == "name")
        package_scroll = ttk.Scrollbar(packages_page, orient="vertical", command=self.package_tree.yview)
        self.package_tree.configure(yscrollcommand=package_scroll.set)
        self.package_tree.grid(row=0, column=0, sticky="nsew")
        package_scroll.grid(row=0, column=1, sticky="ns")

        payload_page.columnconfigure(0, weight=1)
        payload_page.rowconfigure(0, weight=1)
        self.payload_tree = ttk.Treeview(
            payload_page,
            columns=("package", "destination", "bytes", "sha256"),
            show="headings",
        )
        payload_headings = (
            ("package", "Package", 220),
            ("destination", "Destination", 410),
            ("bytes", "Bytes", 100),
            ("sha256", "SHA-256", 500),
        )
        for key, title, width in payload_headings:
            self.payload_tree.heading(key, text=title)
            self.payload_tree.column(key, width=width, stretch=key in {"destination", "sha256"})
        payload_scroll = ttk.Scrollbar(payload_page, orient="vertical", command=self.payload_tree.yview)
        self.payload_tree.configure(yscrollcommand=payload_scroll.set)
        self.payload_tree.grid(row=0, column=0, sticky="nsew")
        payload_scroll.grid(row=0, column=1, sticky="ns")

        notices_page.columnconfigure(0, weight=1)
        notices_page.rowconfigure(1, weight=1)
        ttk.Label(notices_page, text="Resolver warnings").grid(row=0, column=0, sticky="w")
        self.warning_list = self.tk.Listbox(notices_page, height=6)
        self.warning_list.grid(row=1, column=0, sticky="nsew", pady=(4, 10))
        ttk.Label(notices_page, text="Required acknowledgements").grid(row=2, column=0, sticky="w")
        self.ack_list = self.tk.Listbox(notices_page, height=9)
        self.ack_list.grid(row=3, column=0, sticky="nsew", pady=(4, 0))
        notices_page.rowconfigure(3, weight=1)

        actions = ttk.Frame(self.review_tab)
        actions.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        ttk.Button(
            actions,
            text="Back to choices",
            command=lambda: self.notebook.select(self.choose_tab),
        ).pack(side="left")
        ttk.Label(
            actions,
            text="Review only — no confirmation or execution authority is granted.",
        ).pack(side="right")

    def refresh_catalog(self, initial: bool = False) -> None:
        try:
            snapshot = self.controller.refresh_catalog() if not initial else self.controller.host_snapshot()
            self._render_choice_snapshot(snapshot)
            self._clear_review()
            self.status_var.set("Catalogue refreshed. Review-only mode; no files will be installed.")
        except (OSError, CatalogInterfaceError, ResolverError, WizardContractError, WizardHostError) as exc:
            self._show_error("Catalogue discovery failed", exc)

    def resolve_review(self) -> None:
        try:
            self.controller.set_context(
                platform=self.platform_var.get(),
                architecture=self.architecture_var.get(),
                runtime_target=self.runtime_target_var.get(),
                game_version=self.game_version_var.get(),
                branch=self.branch_var.get(),
            )
            review = self.controller.resolve_review()
            self._render_review(review)
            self.notebook.select(self.review_tab)
            self.status_var.set("Exact resolver-backed review model is current and displayed.")
        except (OSError, CatalogInterfaceError, ResolverError, WizardContractError, WizardHostError) as exc:
            self._show_error("Selection could not be resolved", exc)

    def _on_suite_selected(self, _event: object) -> None:
        try:
            suite_id = self._suite_id_for_display(self.suite_var.get())
            snapshot = self.controller.select_suite(suite_id)
            self._render_choice_snapshot(snapshot)
            self._clear_review()
        except (CatalogInterfaceError, WizardHostError) as exc:
            self._show_error("Suite selection failed", exc)

    def _render_choice_snapshot(self, snapshot: dict[str, object]) -> None:
        self.catalog_hash_var.set(str(snapshot["catalog_sha256"]))
        suites = list(snapshot["suite_choices"])
        self._suite_display_to_id = {
            f"{row['display_name']}  {row['version']}  [{row['channel']}]": row["suite_id"]
            for row in suites
        }
        displays = list(self._suite_display_to_id)
        self.suite_combo["values"] = displays
        selected_id = str(snapshot["selected_suite_id"])
        selected_display = next(
            display for display, suite_id in self._suite_display_to_id.items() if suite_id == selected_id
        )
        self.suite_var.set(selected_display)
        self.suite_description.configure(text=str(snapshot["suite_description"]))

        context = dict(snapshot["context"])
        self.platform_var.set(str(context["platform"]))
        self.architecture_var.set(str(context["architecture"]))
        self.runtime_target_var.set(str(context["runtime_target"]))
        self.game_version_var.set(str(context["game_version"]))
        self.branch_var.set(str(context["branch"]))
        self.platform_combo["values"] = tuple(sorted({str(context["platform"]), "windows"}))
        self.architecture_combo["values"] = tuple(sorted({str(context["architecture"]), "x86_64"}))
        self.runtime_combo["values"] = tuple(sorted({str(context["runtime_target"]), "editor-only"}))

        self._clear_children(self.package_frame)
        self.package_vars.clear()
        packages = list(snapshot["packages"])
        for row_index, raw in enumerate(packages):
            package = dict(raw)
            package_id = str(package["package_id"])
            variable = self.tk.BooleanVar(value=bool(package["included"]))
            self.package_vars[package_id] = variable
            state = "disabled" if package["locked"] else "normal"
            checkbox = self.ttk.Checkbutton(
                self.package_frame,
                variable=variable,
                state=state,
                command=lambda pid=package_id, var=variable: self._set_package(pid, bool(var.get())),
            )
            checkbox.grid(row=row_index * 2, column=0, rowspan=2, sticky="n", padx=(0, 7))
            self.ttk.Label(
                self.package_frame,
                text=f"{package['display_name']}  {package['version']}  [{package['selection']}]",
                font=("TkDefaultFont", 10, "bold"),
            ).grid(row=row_index * 2, column=1, sticky="w")
            payload = dict(package["payload_summary"])
            detail = (
                f"{package['description']}\n"
                f"Status: {package['status']}   Kind: {package['kind']}   "
                f"Payload: {payload['file_count']} file(s), {payload['size_bytes']} bytes   "
                f"Licence: {package['license_expression']}"
            )
            self.ttk.Label(self.package_frame, text=detail, wraplength=650).grid(
                row=row_index * 2 + 1, column=1, sticky="w", pady=(0, 10)
            )
        self.package_frame.columnconfigure(1, weight=1)

        self._clear_children(self.feature_frame)
        self.feature_vars.clear()
        features = list(snapshot["features"])
        if not features:
            self.ttk.Label(self.feature_frame, text="This suite declares no optional feature groups.").pack(anchor="w")
        for raw in features:
            feature = dict(raw)
            feature_id = str(feature["feature_id"])
            variable = self.tk.BooleanVar(value=bool(feature["selected"]))
            self.feature_vars[feature_id] = variable
            checkbox = self.ttk.Checkbutton(
                self.feature_frame,
                text=str(feature["display_name"]),
                variable=variable,
                command=lambda fid=feature_id, var=variable: self._set_feature(fid, bool(var.get())),
            )
            checkbox.pack(anchor="w")
            self.ttk.Label(
                self.feature_frame,
                text=(
                    f"{feature['description']}\nPackages: "
                    + ", ".join(str(value) for value in feature["package_ids"])
                ),
                wraplength=390,
            ).pack(anchor="w", padx=(24, 0), pady=(0, 10))

    def _set_package(self, package_id: str, included: bool) -> None:
        try:
            snapshot = self.controller.set_package_included(package_id, included)
            self._render_choice_snapshot(snapshot)
            self._clear_review()
        except WizardHostError as exc:
            self._show_error("Package selection failed", exc)
            self._render_choice_snapshot(self.controller.host_snapshot())

    def _set_feature(self, feature_id: str, selected: bool) -> None:
        try:
            snapshot = self.controller.set_feature_selected(feature_id, selected)
            self._render_choice_snapshot(snapshot)
            self._clear_review()
        except WizardHostError as exc:
            self._show_error("Feature selection failed", exc)
            self._render_choice_snapshot(self.controller.host_snapshot())

    def _render_review(self, review: dict[str, object]) -> None:
        for name, variable in self.review_hash_vars.items():
            variable.set(str(review[name]))
        summary = dict(review["summary"])
        suite = dict(review["suite"])
        self.summary_var.set(
            f"{suite['display_name']} {suite['version']} — "
            f"{summary['package_count']} package(s), {summary['payload_file_count']} file(s), "
            f"{summary['payload_size_bytes']} bytes."
        )
        self._clear_tree(self.package_tree)
        for row in list(review["packages"]):
            package = dict(row)
            self.package_tree.insert(
                "",
                "end",
                values=(
                    package["order"],
                    package["display_name"],
                    package["version"],
                    package["status"],
                    package["payload_file_count"],
                    package["payload_size_bytes"],
                ),
            )
        self._clear_tree(self.payload_tree)
        for row in list(review["payload"]):
            payload = dict(row)
            self.payload_tree.insert(
                "",
                "end",
                values=(
                    payload["package_id"],
                    payload["destination"],
                    payload["size_bytes"],
                    payload["sha256"],
                ),
            )
        self.warning_list.delete(0, "end")
        warnings = list(review["warnings"])
        if not warnings:
            self.warning_list.insert("end", "No resolver warnings.")
        else:
            for warning in warnings:
                self.warning_list.insert("end", str(warning))
        self.ack_list.delete(0, "end")
        for raw in list(review["required_acknowledgements"]):
            acknowledgement = dict(raw)
            self.ack_list.insert(
                "end",
                f"{acknowledgement['title']} — {acknowledgement['detail']}",
            )

    def _clear_review(self) -> None:
        self.summary_var.set("Resolve a selection to display its exact review model.")
        for variable in self.review_hash_vars.values():
            variable.set("")
        self._clear_tree(self.package_tree)
        self._clear_tree(self.payload_tree)
        self.warning_list.delete(0, "end")
        self.ack_list.delete(0, "end")

    def _show_error(self, title: str, error: BaseException) -> None:
        message = str(error)
        self.status_var.set(f"{title}: {message}")
        self.messagebox.showerror(title, message, parent=self.root)

    def _suite_id_for_display(self, display: str) -> str:
        suite_id = self._suite_display_to_id.get(display)
        if suite_id is None:
            raise WizardHostError("The selected suite row is no longer present.")
        return str(suite_id)

    @staticmethod
    def _clear_children(widget: object) -> None:
        for child in widget.winfo_children():
            child.destroy()

    @staticmethod
    def _clear_tree(tree: object) -> None:
        for item in tree.get_children():
            tree.delete(item)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Open the graphical FOA-SDK Suite Wizard review host.")
    parser.add_argument(
        "--installer-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
        help="Path to the product-owned Installer directory.",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except ImportError:
        print(
            "FOA-SDK Suite Wizard host failed: Python tkinter support is unavailable.",
            file=sys.stderr,
        )
        return 1

    try:
        controller = WizardHostController(arguments.installer_root)
        root = tk.Tk()
        SuiteWizardHost(root, controller, tk, ttk, messagebox)
        root.mainloop()
        return 0
    except (OSError, CatalogInterfaceError, ResolverError, WizardContractError, WizardHostError, tk.TclError) as exc:
        print(f"FOA-SDK Suite Wizard host failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
