# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import ast
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = TOOLS_ROOT.parents[2]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import validate_item_viewer_working_lifecycle as contract


class ItemViewerWorkingLifecycleTests(unittest.TestCase):
    FIXTURE_PATHS = (
        ".github/workflows/item-viewer-windows-validation.yml",
        "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
        "Gems/TaintedGrailModdingSDK/Code/Source/AssetBrowserPreviewRefreshService.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualSelectorInstallerSystemComponent.cpp",
        "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualSelectorWidget.cpp",
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake",
        "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_files.cmake",
        "Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_refresh.py",
        "Gems/TaintedGrailModdingSDK/Tools/editor_tests/alpha_item_viewer_live_smoke.py",
    )

    def copy_fixture(self, root: Path) -> None:
        for relative in self.FIXTURE_PATHS:
            source = REPO_ROOT / relative
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def mutate(self, root: Path, relative: str, old: str, new: str, *, all_occurrences: bool = False) -> None:
        path = root / relative
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text)
        path.write_text(
            text.replace(old, new) if all_occurrences else text.replace(old, new, 1),
            encoding="utf-8",
        )

    def test_current_item_viewer_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            contract.validate_item_viewer(root)

    def test_engine_lfs_fetch_must_wait_for_pinned_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(root, ".github/workflows/item-viewer-windows-validation.yml",
                        "lfs: false", "lfs: true", all_occurrences=True)
            with self.assertRaisesRegex(RuntimeError, "after checkout so .lfsconfig is available"):
                contract.validate_item_viewer(root)

    def test_engine_lfs_download_cannot_be_removed_or_retargeted(self) -> None:
        for replacement in ("# download omitted", "git lfs pull"):
            with self.subTest(replacement=replacement), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                self.copy_fixture(root)
                self.mutate(root, ".github/workflows/item-viewer-windows-validation.yml",
                            "git -C o3de lfs pull", replacement)
                with self.assertRaisesRegex(RuntimeError, "pinned engine LFS download"):
                    contract.validate_item_viewer(root)

    def test_engine_lfs_download_must_precede_build(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / ".github/workflows/item-viewer-windows-validation.yml"
            workflow = path.read_text(encoding="utf-8")
            start = workflow.index("      - name: Download pinned O3DE LFS assets")
            end = workflow.index("      - name: Set up Python", start)
            step = workflow[start:end]
            path.write_text(workflow[:start] + workflow[end:] + "\n" + step, encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "before the Editor build"):
                contract.validate_item_viewer(root)

    def test_engine_lfs_failure_must_stop_build(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(root, ".github/workflows/item-viewer-windows-validation.yml",
                        "if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }", "# error ignored")
            with self.assertRaisesRegex(RuntimeError, "LFS failure propagation"):
                contract.validate_item_viewer(root)

    def test_missing_editor_build_registration_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake",
                "Source/ItemVisualSelectorWidget.cpp",
                "Source/RemovedItemVisualSelectorWidget.cpp",
            )
            with self.assertRaisesRegex(RuntimeError, "Editor production build registration"):
                contract.validate_item_viewer(root)

    def test_refresh_service_must_be_framework_owned(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_framework_files.cmake",
                "Source/AssetBrowserPreviewRefreshService.cpp",
                "Source/RemovedAssetBrowserPreviewRefreshService.cpp",
            )
            with self.assertRaisesRegex(RuntimeError, "Framework production ownership"):
                contract.validate_item_viewer(root)

    def test_refresh_service_cannot_leak_back_into_editor_ownership(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            editor_manifest = root / "Gems/TaintedGrailModdingSDK/Code/taintedgrailmoddingsdk_editor_files.cmake"
            editor_manifest.write_text(
                editor_manifest.read_text(encoding="utf-8")
                + "\nset(LEAKED Source/AssetBrowserPreviewRefreshService.cpp)\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "duplicate Editor ownership"):
                contract.validate_item_viewer(root)

    def test_raw_model_chooser_must_remain_hidden(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
                "m_chooseModel->hide();",
                "m_chooseModel->show();",
            )
            with self.assertRaisesRegex(RuntimeError, "hidden raw JSON chooser"):
                contract.validate_item_viewer(root)

    def test_automatic_profile_filter_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
                "CandidateMatchesActiveProfile",
                "CandidateMatchesAnyProfile",
                all_occurrences=True,
            )
            with self.assertRaisesRegex(RuntimeError, "exact-profile candidate filtering"):
                contract.validate_item_viewer(root)

    def test_refresh_must_regenerate_the_shared_model(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp",
                "RefreshActiveProfileModel",
                "LoadLatestAvailableModel",
            )
            with self.assertRaisesRegex(RuntimeError, "refresh-to-generation service integration"):
                contract.validate_item_viewer(root)

    def test_refresh_must_use_embedded_python_not_an_external_process(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / "Gems/TaintedGrailModdingSDK/Code/Source/AssetBrowserPreviewRefreshService.cpp"
            path.write_text(path.read_text(encoding="utf-8") + "\nQProcess forbidden;\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "remain inside the Editor process"):
                contract.validate_item_viewer(root)

    def test_embedded_refresh_adapter_cannot_use_process_exit_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / "Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_refresh.py"
            path.write_text(path.read_text(encoding="utf-8") + "\nraise SystemExit(0)\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "process-exit contract"):
                contract.validate_item_viewer(root)

    def test_qualified_process_exit_reference_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / "Gems/TaintedGrailModdingSDK/Tools/foa_asset_browser_pane_refresh.py"
            path.write_text(
                path.read_text(encoding="utf-8") + "\nimport builtins\nraise builtins.SystemExit(0)\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "process-exit contract"):
                contract.validate_item_viewer(root)

    def test_refresh_tooling_must_ship_with_the_installed_editor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            self.mutate(
                root,
                "Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt",
                "scripts/foa-sdk",
                "scripts/missing-item-viewer-tool",
            )
            with self.assertRaisesRegex(RuntimeError, "private installed refresh tooling location"):
                contract.validate_item_viewer(root)

    def test_internal_model_path_cannot_return_to_settings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.copy_fixture(root)
            path = root / "Gems/TaintedGrailModdingSDK/Code/Source/ItemVisualLifecycleWidget.cpp"
            path.write_text(
                path.read_text(encoding="utf-8")
                + '\nsettings.setValue(prefix + QStringLiteral("modelPath"), QStringLiteral("forbidden"));\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "stale UX state"):
                contract.validate_item_viewer(root)


class ItemViewerPaneLookupTests(unittest.TestCase):
    def test_lookup_uses_live_registered_dock_when_floating_title_matches(self) -> None:
        class Dock:
            def __init__(self, object_name: str, *, visible: bool = True, valid: bool = True):
                self.name, self.visible, self.valid = object_name, visible, valid

            def objectName(self):
                if not self.valid:
                    raise RuntimeError("Deleted native dock")
                return self.name

            def windowTitle(self):
                return "Tainted Grail Item and Recipe Editor"

            def isVisible(self):
                return self.visible

        expected = Dock("TaintedGrailModdingSDK.ItemRecipeEditor")
        widgets = [Dock("floating-container"),
                   Dock(expected.name, valid=False), Dock(expected.name, visible=False), expected]
        smoke = ast.parse((TOOLS_ROOT / "editor_tests/alpha_item_viewer_live_smoke.py").read_text(encoding="utf-8"))
        constants = [node for node in smoke.body if isinstance(node, ast.Assign)
                     and any(isinstance(target, ast.Name) and target.id in
                             {"PANE_NAME", "STATUS_PANE_NAME", "PANE_OBJECT_NAMES"} for target in node.targets)]
        test_function = next(node for node in smoke.body if isinstance(node, ast.FunctionDef)
                             and node.name == "ItemViewerLifecycleSmoke")
        lookup = next(node for node in test_function.body if isinstance(node, ast.FunctionDef)
                      and node.name == "find_pane")
        namespace = {"application": type("Application", (), {"allWidgets": lambda self: widgets})(),
                     "QtWidgets": type("Widgets", (), {"QDockWidget": Dock}),
                     "isValid": lambda widget: widget.valid}
        exec(compile(ast.Module(body=constants + [lookup], type_ignores=[]), "pane-lookup", "exec"), namespace)
        self.assertIs(expected, namespace["find_pane"]("Tainted Grail Item and Recipe Editor"))


class ItemViewerCloseWaitTests(unittest.TestCase):
    def closed_condition(self, *, pane_valid: bool, container_valid: bool | None, visible: bool = False):
        smoke = ast.parse((TOOLS_ROOT / "editor_tests/alpha_item_viewer_live_smoke.py").read_text(encoding="utf-8"))
        condition = next(node for node in ast.walk(smoke) if isinstance(node, ast.Lambda)
                         and "not isValid(pane)" in ast.unparse(node))
        namespace = {"pane": {"valid": pane_valid},
                     "closing_container": None if container_valid is None else {"valid": container_valid},
                     "general": type("General", (), {"is_pane_visible": staticmethod(lambda name: visible)}),
                     "PANE_NAME": "fixture pane", "isValid": lambda widget: widget["valid"]}
        return eval(compile(ast.Expression(body=condition), "pane-close-condition", "eval"), namespace)()

    def test_waits_for_pending_floating_container_deletion(self) -> None:
        self.assertFalse(self.closed_condition(pane_valid=False, container_valid=True))
        self.assertTrue(self.closed_condition(pane_valid=False, container_valid=False))

    def test_docked_close_still_requires_pane_destruction_and_invisibility(self) -> None:
        self.assertFalse(self.closed_condition(pane_valid=True, container_valid=None))
        self.assertFalse(self.closed_condition(pane_valid=False, container_valid=None, visible=True))
        self.assertTrue(self.closed_condition(pane_valid=False, container_valid=None))


@unittest.skipUnless(shutil.which("pwsh"), "PowerShell 7 is required for the runner fixture copy")
class ItemViewerRunnerSetupTests(unittest.TestCase):
    def run_copy(self, root: Path, seed: Path, destination: Path) -> subprocess.CompletedProcess[str]:
        runner = (TOOLS_ROOT / "run_alpha_item_viewer_windows_validation.ps1").read_text(encoding="utf-8")
        # Execute the actual runner setup block without invoking configure/build or Editor.
        start = runner.index('        $seedO3deRoot = Join-Path $seedRoot ')
        end = runner.index('        $assetBrowserRoot = ', start)
        script = root / "copy-fixture.ps1"
        script.write_text(
            'param([string]$seedRoot, [string]$o3deRoot)\n$ErrorActionPreference = "Stop"\n'
            + runner[start:end], encoding="utf-8",
        )
        return subprocess.run(
            [shutil.which("pwsh"), "-NoLogo", "-NoProfile", "-NonInteractive", "-File", str(script),
             "-seedRoot", str(seed), "-o3deRoot", str(destination)],
            capture_output=True, text=True, timeout=30, check=False,
        )

    def test_runner_copies_nested_fixture_with_literal_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            seed = root / "seed [literal] with spaces"
            source = seed / "workspace/Extracted/PreviewArtifacts/O3DE"
            destination = root / "destination [literal]"
            destination.mkdir()
            files = {"[proof].json": b'{"ImportProofId":"fixture.proof"}',
                     "nested assets/model.bin": bytes(range(256)), ".fixture-marker": b"owned fixture"}
            for relative, data in files.items():
                path = source / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            result = self.run_copy(root, seed, destination)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            copied = {p.relative_to(destination).as_posix(): p.read_bytes()
                      for p in destination.rglob("*") if p.is_file()}
            self.assertEqual(files, copied)

    def test_runner_supplies_one_test_case_name_for_the_smoke_script(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runner = (TOOLS_ROOT / "run_alpha_item_viewer_windows_validation.ps1").read_text(encoding="utf-8")
            start = runner.index('        $editorArguments = @(')
            end = runner.index('        $argumentLine = ', start)
            script = root / "launch-arguments.ps1"
            script.write_text(
                'param([string]$projectRoot, [string]$smokeScript)\n'
                + runner[start:end] + '\nConvertTo-Json -Compress -InputObject $editorArguments\n',
                encoding="utf-8",
            )
            smoke = root / "smoke with spaces.py"
            result = subprocess.run(
                [shutil.which("pwsh"), "-NoLogo", "-NoProfile", "-NonInteractive", "-File", str(script),
                 "-projectRoot", str(root), "-smokeScript", str(smoke)],
                capture_output=True, text=True, timeout=30, check=False,
            )
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            arguments = json.loads(result.stdout)
            self.assertEqual(str(smoke), arguments[arguments.index("--runpythontest") + 1])
            cases = [arg.split("=", 1)[1] for arg in arguments if arg.startswith("--pythontestcase=")]
            self.assertEqual(["ItemViewerLifecycleSmoke"], cases)

    def test_runner_missing_fixture_fails_without_changing_destination(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            destination = root / "destination"
            destination.mkdir()
            sentinel = destination / "existing.json"
            sentinel.write_bytes(b"existing fixture")
            result = self.run_copy(root, root / "missing-seed", destination)
            self.assertNotEqual(0, result.returncode)
            self.assertEqual([sentinel], list(destination.iterdir()))
            self.assertEqual(b"existing fixture", sentinel.read_bytes())


if __name__ == "__main__":
    unittest.main()
