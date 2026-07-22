import ast
import unittest
from pathlib import Path

PACKAGE = Path(__file__).parent / "semantic_repair"
FORBIDDEN_IMPORT_ROOTS = {
    "ctypes",
    "http",
    "importlib",
    "requests",
    "socket",
    "subprocess",
    "urllib",
}
FORBIDDEN_RUNTIME_MARKERS = (
    "UnityEngine",
    "TG.Main",
    "BepInEx",
    "HarmonyLib",
    ".dll",
)


class OfflineAuthorityBoundaryTests(unittest.TestCase):
    def test_package_has_no_runtime_or_network_imports(self):
        violations = []
        for path in sorted(PACKAGE.glob("*.py")):
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                names = []
                if isinstance(node, ast.Import):
                    names = [alias.name for alias in node.names]
                elif isinstance(node, ast.ImportFrom) and node.module:
                    names = [node.module]
                for name in names:
                    if name.split(".", 1)[0] in FORBIDDEN_IMPORT_ROOTS:
                        violations.append(f"{path.name}: {name}")
        self.assertEqual(violations, [])

    def test_package_contains_no_game_or_loader_identity_markers(self):
        violations = []
        for path in sorted(PACKAGE.glob("*.py")):
            text = path.read_text(encoding="utf-8")
            for marker in FORBIDDEN_RUNTIME_MARKERS:
                if marker in text:
                    violations.append(f"{path.name}: {marker}")
        self.assertEqual(violations, [])


if __name__ == "__main__":
    unittest.main()
