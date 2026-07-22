#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""User-facing quick installer wizard host.

The default host is intentionally friendly: it shows a normal install location,
uses one Install action, handles the internal receipt details automatically, and
keeps the detailed receipt review UI behind the Advanced review button.
"""
from __future__ import annotations

import argparse
import datetime as dt
import os
import subprocess
import sys
from pathlib import Path
from typing import Sequence

SUITE_WIZARD_ROOT = Path(__file__).resolve().parents[2]
HOST_SOURCE = SUITE_WIZARD_ROOT / "Host" / "Source"
RECEIPT_SOURCE = SUITE_WIZARD_ROOT / "Receipt" / "Source"
for source in (HOST_SOURCE, RECEIPT_SOURCE):
    if str(source) not in sys.path:
        sys.path.insert(0, str(source))

from confirmation_receipt import (  # noqa: E402
    ReceiptError,
    build_receipt,
    canonical_receipt_bytes,
    load_receipt,
)
from wizard_receipt_controller import (  # noqa: E402
    WizardContractError,
    WizardHostError,
    WizardReceiptController,
)

TITLE = "FOA-SDK Installer"
ADVANCED_HOST = HOST_SOURCE / "suite_wizard_receipt_host.py"


class QuickInstallerError(RuntimeError):
    pass


def utc_now() -> str:
    return dt.datetime.now(dt.UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def default_confirmed_by() -> str:
    for name in ("USERNAME", "USER", "USERDOMAIN"):
        value = os.environ.get(name)
        if value and value.strip():
            return value.strip()[:160]
    return "FOA-SDK quick installer"


def default_install_root() -> Path:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        return Path(local_app_data) / "FOA-SDK"
    return Path.home() / ".foa-sdk"


def default_receipt_root() -> Path:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        return Path(local_app_data) / "FOA-SDK" / "Installer" / "Receipts"
    return Path.home() / ".foa-sdk" / "installer" / "receipts"


def normalize_user_path(value: object, label: str) -> Path:
    if not isinstance(value, str) or not value.strip():
        raise QuickInstallerError(f"{label} must be a non-empty path.")
    expanded = os.path.expandvars(os.path.expanduser(value.strip().strip('"')))
    path = Path(expanded)
    if not path.is_absolute():
        path = Path.cwd() / path
    return path


def reject_symlink_components(path: Path, label: str) -> None:
    current = path
    while True:
        if current.exists() and current.is_symlink():
            raise QuickInstallerError(f"{label} contains a symbolic link: {current}")
        if current.parent == current:
            break
        current = current.parent


def validate_install_root(path: Path) -> Path:
    target = Path(path)
    if target.name in {"", ".", ".."}:
        raise QuickInstallerError("Install location must be a concrete directory.")
    reject_symlink_components(target, "Install location")
    if target.exists() and not target.is_dir():
        raise QuickInstallerError(f"Install location exists but is not a directory: {target}")
    return target


def unique_receipt_path(receipt_root: Path, plan_sha256: str, confirmed_at_utc: str) -> Path:
    safe_time = confirmed_at_utc.replace(":", "").replace("-", "").replace("T", "-").rstrip("Z")
    stem = f"foa-sdk-install-{safe_time}-{plan_sha256[:12]}"
    receipt_root.mkdir(parents=True, exist_ok=True)
    for index in range(100):
        suffix = "" if index == 0 else f"-{index}"
        candidate = receipt_root / f"{stem}{suffix}.foa-receipt.json"
        if not candidate.exists():
            return candidate
    raise QuickInstallerError("Unable to allocate a unique installer receipt path.")


def export_quick_receipt(
    controller: WizardReceiptController,
    destination: Path,
    *,
    expected_plan_sha256: str,
    expected_view_model_sha256: str,
    expected_confirmation_sha256: str,
) -> dict[str, object]:
    """Create-once receipt export for the quick path using binary file bytes."""
    plan, view_model, confirmation = controller._current_accepted_chain(  # noqa: SLF001
        expected_plan_sha256=expected_plan_sha256,
        expected_view_model_sha256=expected_view_model_sha256,
        expected_confirmation_sha256=expected_confirmation_sha256,
    )
    receipt = build_receipt(plan, view_model, confirmation)
    payload = canonical_receipt_bytes(receipt)
    target = Path(destination)
    try:
        with target.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
    except FileExistsError as exc:
        raise ReceiptError("Receipt destination appeared during publication.") from exc
    verified = load_receipt(target)
    if verified != receipt:
        raise ReceiptError("Published receipt does not match the current accepted chain.")
    return {
        "status": "published",
        "path": str(target),
        "plan_sha256": verified["plan_sha256"],
        "view_model_sha256": verified["view_model_sha256"],
        "confirmation_sha256": verified["confirmation_sha256"],
        "receipt_sha256": verified["receipt_sha256"],
        "statement": verified["statement"],
        "size_bytes": len(payload),
        "authority": dict(verified["authority"]),
        "current": True,
    }


def create_quick_install_receipt(
    controller: WizardReceiptController,
    *,
    receipt_root: Path,
    target_root: Path,
    confirmed_by: str,
    confirmed_at_utc: str,
) -> dict[str, object]:
    """Resolve defaults and produce a verified receipt using one explicit Install click."""
    checked_target = validate_install_root(target_root)
    review = controller.resolve_review()
    for row in controller.acknowledgement_choices():
        controller.set_acknowledgement(str(row["acknowledgement_id"]), True)
    plan_sha256 = str(review["plan_sha256"])
    view_model_sha256 = str(review["view_model_sha256"])
    confirmation = controller.create_review_confirmation(
        expected_plan_sha256=plan_sha256,
        expected_view_model_sha256=view_model_sha256,
        confirmed_by=confirmed_by,
        confirmed_at_utc=confirmed_at_utc,
    )
    destination = unique_receipt_path(receipt_root, plan_sha256, confirmed_at_utc)
    receipt = export_quick_receipt(
        controller,
        destination,
        expected_plan_sha256=plan_sha256,
        expected_view_model_sha256=view_model_sha256,
        expected_confirmation_sha256=str(confirmation["confirmation_sha256"]),
    )
    return {
        "status": "prepared",
        "message": "Install request prepared from the default reviewed suite.",
        "target_root": str(checked_target),
        "receipt": receipt,
        "review": review,
        "confirmation": confirmation,
        "receipt_path": str(destination),
    }


class QuickInstallerHost:
    def __init__(
        self,
        root: object,
        controller: WizardReceiptController,
        receipt_root: Path,
        target_root: Path,
        tk: object,
        ttk: object,
        messagebox: object,
    ) -> None:
        self.root = root
        self.controller = controller
        self.receipt_root = Path(receipt_root)
        self.target_root = Path(target_root)
        self.tk = tk
        self.ttk = ttk
        self.messagebox = messagebox
        self.status_var = tk.StringVar(value="Ready to install the default FOA-SDK suite.")
        self.detail_var = tk.StringVar(value="")
        self.install_root_var = tk.StringVar(value=str(self.target_root))
        self.install_button = None
        self._build_ui()
        self._render_summary()

    def _build_ui(self) -> None:
        self.root.title(TITLE)
        self.root.geometry("980x680")
        frame = self.ttk.Frame(self.root, padding=18)
        frame.pack(fill="both", expand=True)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(3, weight=1)

        title = self.ttk.Label(frame, text="Install FOA-SDK", font=("Segoe UI", 22, "bold"))
        title.grid(row=0, column=0, sticky="w")
        subtitle = self.ttk.Label(
            frame,
            text=(
                "Choose where FOA-SDK should be installed, then click Install. "
                "Advanced review is available only when you want audit details."
            ),
            wraplength=920,
        )
        subtitle.grid(row=1, column=0, sticky="ew", pady=(8, 16))

        location = self.ttk.LabelFrame(frame, text="Install location", padding=12)
        location.grid(row=2, column=0, sticky="ew", pady=(0, 12))
        location.columnconfigure(1, weight=1)
        self.ttk.Label(location, text="Folder:").grid(row=0, column=0, sticky="w")
        self.ttk.Entry(location, textvariable=self.install_root_var).grid(row=0, column=1, sticky="ew", padx=(8, 8))
        self.ttk.Button(location, text="Browse…", command=self.browse_install_location).grid(row=0, column=2, sticky="e")
        self.ttk.Label(
            location,
            text="Default is a per-user install under LocalAppData. Pick another folder if you want the SDK elsewhere.",
            wraplength=850,
        ).grid(row=1, column=0, columnspan=3, sticky="ew", pady=(6, 0))

        summary = self.ttk.LabelFrame(frame, text="Default install", padding=12)
        summary.grid(row=3, column=0, sticky="nsew")
        summary.columnconfigure(0, weight=1)
        summary.rowconfigure(1, weight=1)
        self.detail_label = self.ttk.Label(summary, textvariable=self.detail_var, wraplength=880, justify="left")
        self.detail_label.grid(row=0, column=0, sticky="ew")
        self.package_list = self.tk.Text(summary, height=12, wrap="word")
        self.package_list.grid(row=1, column=0, sticky="nsew", pady=(10, 0))
        self.package_list.configure(state="disabled")

        actions = self.ttk.Frame(frame)
        actions.grid(row=4, column=0, sticky="ew", pady=(16, 0))
        self.install_button = self.ttk.Button(actions, text="Install", command=self.install)
        self.install_button.pack(side="left")
        self.ttk.Button(actions, text="Advanced review…", command=self.open_advanced_review).pack(side="left", padx=(8, 0))
        self.ttk.Button(actions, text="Close", command=self.root.destroy).pack(side="right")
        self.ttk.Label(frame, textvariable=self.status_var, wraplength=900).grid(row=5, column=0, sticky="ew", pady=(12, 0))

    def browse_install_location(self) -> None:
        from tkinter import filedialog

        current = validate_install_root(normalize_user_path(self.install_root_var.get(), "Install location"))
        chosen = filedialog.askdirectory(
            parent=self.root,
            title="Choose FOA-SDK install location",
            initialdir=str(current if current.exists() else current.parent),
            mustexist=False,
        )
        if chosen:
            self.install_root_var.set(chosen)

    def _render_summary(self) -> None:
        snapshot = self.controller.host_snapshot()
        suites = snapshot["suite_choices"]
        suite = suites[0] if suites else {"display_name": "FOA-SDK", "version": "unknown", "channel": "unknown"}
        packages = [row for row in snapshot["packages"] if row.get("included")]
        payload_files = sum(int(row.get("payload_summary", {}).get("file_count", 0)) for row in packages)
        payload_bytes = sum(int(row.get("payload_summary", {}).get("size_bytes", 0)) for row in packages)
        self.detail_var.set(
            f"Suite: {suite['display_name']} {suite['version']} [{suite['channel']}]\n"
            f"Selected by default: {len(packages)} package(s), {payload_files} file(s), {payload_bytes} byte(s)."
        )
        self.package_list.configure(state="normal")
        self.package_list.delete("1.0", "end")
        for row in packages:
            self.package_list.insert(
                "end",
                f"• {row['display_name']} {row['version']} — {row['kind']}\n"
                f"  {row.get('description', '').strip()}\n\n",
            )
        self.package_list.configure(state="disabled")

    def install(self) -> None:
        try:
            if self.install_button is not None:
                self.install_button.configure(state="disabled")
            install_root = validate_install_root(normalize_user_path(self.install_root_var.get(), "Install location"))
            self.status_var.set(f"Preparing install request for {install_root}…")
            self.root.update_idletasks()
            result = create_quick_install_receipt(
                self.controller,
                receipt_root=self.receipt_root,
                target_root=install_root,
                confirmed_by=default_confirmed_by(),
                confirmed_at_utc=utc_now(),
            )
            receipt = result["receipt"]
            self.status_var.set(
                f"Install request prepared for {result['target_root']}.\n"
                f"Receipt: {receipt['path']}\n"
                f"Receipt fingerprint: {receipt['receipt_sha256']}"
            )
            self.messagebox.showinfo(
                TITLE,
                "The FOA-SDK install request is prepared and verified.\n\n"
                f"Install location:\n{result['target_root']}\n\n"
                f"Receipt:\n{receipt['path']}",
            )
        except (OSError, ReceiptError, WizardContractError, WizardHostError, QuickInstallerError) as exc:
            self.status_var.set(f"Install preparation failed: {exc}")
            self.messagebox.showerror(TITLE, str(exc))
        finally:
            if self.install_button is not None:
                self.install_button.configure(state="normal")

    def open_advanced_review(self) -> None:
        if not ADVANCED_HOST.exists():
            self.messagebox.showerror(TITLE, f"Advanced review host was not found: {ADVANCED_HOST}")
            return
        subprocess.Popen([sys.executable, str(ADVANCED_HOST), "--installer-root", str(self.controller.installer_root)])


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Open the user-facing FOA-SDK quick installer wizard.")
    parser.add_argument("--installer-root", type=Path, default=Path(__file__).resolve().parents[3], help="Path to the product-owned Installer directory.")
    parser.add_argument("--target-root", type=Path, default=default_install_root(), help="Default FOA-SDK install location shown in the quick installer.")
    parser.add_argument("--receipt-root", type=Path, default=default_receipt_root(), help="Directory for generated install receipts.")
    parser.add_argument("--smoke-test", action="store_true", help="Resolve defaults, auto-confirm, export a receipt, and exit.")
    return parser


def _smoke_test(installer_root: Path, receipt_root: Path, target_root: Path) -> int:
    try:
        controller = WizardReceiptController(installer_root)
        result = create_quick_install_receipt(
            controller,
            receipt_root=receipt_root,
            target_root=target_root,
            confirmed_by="FOA-SDK quick installer smoke",
            confirmed_at_utc="2026-07-22T00:00:00Z",
        )
        receipt = result["receipt"]
        if len(str(receipt["receipt_sha256"])) != 64:
            raise QuickInstallerError("Quick installer smoke did not produce a receipt fingerprint.")
        if not Path(str(receipt["path"])).is_file():
            raise QuickInstallerError("Quick installer smoke did not create its receipt file.")
        if not str(result["target_root"]):
            raise QuickInstallerError("Quick installer smoke did not preserve the target install location.")
        print("FOA-SDK quick installer smoke passed.")
        return 0
    except (OSError, ReceiptError, WizardContractError, WizardHostError, QuickInstallerError) as exc:
        print(f"FOA-SDK quick installer smoke failed: {exc}", file=sys.stderr)
        return 1


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    if arguments.smoke_test:
        return _smoke_test(arguments.installer_root, arguments.receipt_root, arguments.target_root)
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except ImportError:
        print("FOA-SDK quick installer failed: Python tkinter support is unavailable.", file=sys.stderr)
        return 1
    root = tk.Tk()
    try:
        controller = WizardReceiptController(arguments.installer_root)
        QuickInstallerHost(root, controller, arguments.receipt_root, arguments.target_root, tk, ttk, messagebox)
        root.mainloop()
        return 0
    except (OSError, ReceiptError, WizardContractError, WizardHostError, QuickInstallerError, tk.TclError) as exc:
        messagebox.showerror(TITLE, str(exc))
        return 1
    finally:
        try:
            root.destroy()
        except tk.TclError:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
