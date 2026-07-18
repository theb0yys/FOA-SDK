from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "validate_developer_preview_project.py"
SPEC = importlib.util.spec_from_file_location("tg_validate_developer_preview_project", SCRIPT_PATH)
assert SPEC and SPEC.loader
contract = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = contract
SPEC.loader.exec_module(contract)


class DeveloperPreviewProjectContractTests(unittest.TestCase):
    def make_repo(self, root: Path) -> Path:
        repo = root / "repo"
        (repo / "AutomatedTesting").mkdir(parents=True)
        (repo / "Gems/TaintedGrailModdingSDK/Tools").mkdir(parents=True)
        (repo / "docs/tainted-grail-sdk").mkdir(parents=True)

        (repo / "engine.json").write_text(
            json.dumps(
                {
                    "external_subdirectories": ["Gems/TaintedGrailModdingSDK"],
                    "projects": ["AutomatedTesting"],
                }
            ),
            encoding="utf-8",
        )
        (repo / "AutomatedTesting/project.json").write_text(
            json.dumps(
                {
                    "project_name": "AutomatedTesting",
                    "gem_names": ["Atom", "TaintedGrailModdingSDK"],
                }
            ),
            encoding="utf-8",
        )
        (repo / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md").write_text(
            "developer_preview_open.py\nAutomatedTesting\nTools → Tainted Grail SDK\n"
            "AutomatedTesting/user/log/Editor.log\n",
            encoding="utf-8",
        )
        (repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py").write_text(
            'PREVIEW_PROJECT = Path("AutomatedTesting")\n'
            "validate_preview_project(repo_root)\n"
            "developer_preview_launch.main(arguments)\n",
            encoding="utf-8",
        )
        (repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py").write_text(
            'parser.add_argument("--project")\n'
            'command.extend(("--project-path", str(project)))\n'
            "validate_project_path(project)\n",
            encoding="utf-8",
        )
        return repo

    def test_valid_project_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            contract.validate_preview_project(repo)

    def test_missing_gem_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "AutomatedTesting/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["gem_names"].remove("TaintedGrailModdingSDK")
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "must enable"):
                contract.validate_preview_project(repo)

    def test_duplicate_gem_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "AutomatedTesting/project.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["gem_names"].append("TaintedGrailModdingSDK")
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "exactly once"):
                contract.validate_preview_project(repo)

    def test_unregistered_project_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "engine.json"
            document = json.loads(path.read_text(encoding="utf-8"))
            document["projects"] = []
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "register"):
                contract.validate_preview_project(repo)

    def test_quickstart_must_name_repository_project(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "docs/tainted-grail-sdk/OPEN_AND_TEST_EDITOR.md"
            path.write_text("launch another project\n", encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "missing required"):
                contract.validate_preview_project(repo)

    def test_opener_must_delegate_to_reviewed_launcher(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_open.py"
            path.write_text('PREVIEW_PROJECT = Path("AutomatedTesting")\n', encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "project opener fragment"):
                contract.validate_preview_project(repo)

    def test_launcher_must_forward_project_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = self.make_repo(Path(temporary))
            path = repo / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_launch.py"
            path.write_text('parser.add_argument("--project")\nvalidate_project_path(project)\n', encoding="utf-8")
            with self.assertRaisesRegex(contract.PreviewProjectContractError, "--project-path"):
                contract.validate_preview_project(repo)


if __name__ == "__main__":
    unittest.main()
