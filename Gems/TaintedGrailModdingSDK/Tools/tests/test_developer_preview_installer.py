#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import developer_preview_installer as installer


SOURCE_COMMIT = "1" * 40
VERSION = "0.1.0"
REVIEW_TIME = "2026-07-21T12:00:00Z"


class DeveloperPreviewInstallerTests(unittest.TestCase):
    def make_inputs(self, root: Path) -> tuple[Path, Path, Path, Path]:
        repo = root / "repo"
        project = repo / installer.PROJECT_DIRECTORY
        (project / "ShaderLib").mkdir(parents=True)
        (project / "Levels").mkdir()
        (project / "project.json").write_text(
            json.dumps(
                {
                    "project_name": installer.PROJECT_DIRECTORY,
                    "summary": "Source-built project.",
                    "engine": "o3de",
                }
            ),
            encoding="utf-8",
        )
        (project / "preview.png").write_bytes(b"preview")
        (project / "TaintedGrailModdingEditor.ico").write_bytes(b"icon")
        (project / "ShaderLib" / "viewsrg.srgi").write_text("shader\n", encoding="utf-8")
        (project / "CMakeLists.txt").write_text("not distributed\n", encoding="utf-8")
        (project / "cmake").mkdir()
        (project / "cmake" / "EngineFinder.cmake").write_text("not distributed\n", encoding="utf-8")

        sdk = root / "sdk"
        binary = sdk / Path(installer.BIN_DIRECTORY.as_posix())
        binary.mkdir(parents=True)
        (sdk / "engine.json").write_text(
            json.dumps({"engine_name": installer.ENGINE_NAME, "sdk_engine": True}),
            encoding="utf-8",
        )
        (sdk / "LICENSE.txt").write_text("Apache-2.0 OR MIT\n", encoding="utf-8")
        (binary / "Editor.exe").write_bytes(b"editor")
        (binary / "TaintedGrailModdingEditorLauncher.exe").write_bytes(b"launcher")
        (binary / "AzCore.dll").write_bytes(b"runtime")
        (sdk / installer.PROJECT_DIRECTORY).mkdir()
        (sdk / installer.PROJECT_DIRECTORY / "preview.png").write_bytes(b"stale install preview")

        notices = root / "NOTICES.txt"
        notices.write_text("Third-party notices\n", encoding="utf-8")
        packages = root / "packages.json"
        packages.write_text(json.dumps([{"Name": "Example", "License": "MIT"}]), encoding="utf-8")
        return repo, sdk, notices, packages

    def inventory(self, root: Path):
        repo, sdk, notices, packages = self.make_inputs(root)
        document, sources = installer.build_inventory(
            repo,
            sdk,
            notices,
            packages,
            VERSION,
            SOURCE_COMMIT,
            "development",
        )
        return repo, sdk, notices, packages, document, sources

    def stage(self, root: Path, name: str = "stage") -> Path:
        _, _, _, _, inventory, sources = self.inventory(root)
        return installer.stage_payload(
            inventory,
            sources,
            root / name,
            str(inventory["inventory_sha256"]),
            "installer-reviewer",
            REVIEW_TIME,
            "review:synthetic-fixture",
        )

    def test_inventory_is_deterministic_and_uses_installed_project_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, _, _, first, first_sources = self.inventory(root)
            repo, sdk, notices, packages = (
                root / "repo",
                root / "sdk",
                root / "NOTICES.txt",
                root / "packages.json",
            )
            second, _ = installer.build_inventory(
                repo, sdk, notices, packages, VERSION, SOURCE_COMMIT, "development"
            )
            self.assertEqual(first, second)
            paths = {entry["path"] for entry in first["entries"]}
            self.assertIn(f"{installer.PROJECT_DIRECTORY}/project.json", paths)
            self.assertNotIn(f"{installer.PROJECT_DIRECTORY}/CMakeLists.txt", paths)
            self.assertNotIn(f"{installer.PROJECT_DIRECTORY}/cmake/EngineFinder.cmake", paths)

            project_source = next(
                source
                for source in first_sources
                if source.destination == PurePosixPath(installer.PROJECT_DIRECTORY) / "project.json"
            )
            project = json.loads(project_source.generated_bytes.decode("utf-8"))
            self.assertEqual(project["engine"], installer.ENGINE_NAME)
            self.assertIn("Prebuilt", project["summary"])

    def test_stage_requires_exact_inventory_redistribution_approval(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, _, _, inventory, sources = self.inventory(root)
            with self.assertRaisesRegex(installer.InstallerError, "exact current inventory"):
                installer.stage_payload(
                    inventory,
                    sources,
                    root / "stage",
                    "0" * 64,
                    "reviewer",
                    REVIEW_TIME,
                    "review:evidence",
                )
            self.assertFalse((root / "stage").exists())

    def test_stage_manifest_and_portable_archive_verify(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = self.stage(root)
            manifest = installer.verify_stage(stage)
            self.assertEqual(manifest["product_version"], VERSION)
            self.assertEqual(manifest["source_commit"], SOURCE_COMMIT)
            self.assertEqual(manifest["redistribution_review"]["reviewed_by"], "installer-reviewer")
            self.assertTrue((stage / installer.SBOM_NAME).is_file())
            sbom = json.loads((stage / installer.SBOM_NAME).read_text(encoding="utf-8"))
            verification = sbom["packages"][0]["packageVerificationCode"]
            file_sha1_values = sorted(
                hashlib.sha1(
                    (stage / file_entry["fileName"].removeprefix("./")).read_bytes(),
                    usedforsecurity=False,
                ).hexdigest()
                for file_entry in sbom["files"]
            )
            expected_verification_code = hashlib.sha1(
                "".join(file_sha1_values).encode("ascii"), usedforsecurity=False
            ).hexdigest()
            self.assertEqual(
                verification["packageVerificationCodeValue"], expected_verification_code
            )
            self.assertEqual(
                verification["packageVerificationCodeExcludedFiles"],
                [
                    f"./{installer.SBOM_NAME}",
                    f"./{installer.MANIFEST_NAME}",
                    f"./{installer.CHECKSUMS_NAME}",
                ],
            )
            project = json.loads(
                (stage / installer.PROJECT_DIRECTORY / "project.json").read_text(encoding="utf-8")
            )
            self.assertEqual(project["engine"], installer.ENGINE_NAME)

            archive, checksum = installer.archive_payload(stage, root / "sdk.zip")
            archived_manifest = installer.verify_archive(archive)
            self.assertEqual(archived_manifest["inventory_sha256"], manifest["inventory_sha256"])
            self.assertRegex(checksum.read_text(encoding="utf-8"), r"^[0-9a-f]{64}  sdk\.zip\n$")

    def test_portable_archives_are_byte_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            first_root = Path(temporary) / "first"
            second_root = Path(temporary) / "second"
            first_root.mkdir()
            second_root.mkdir()
            first_stage = self.stage(first_root)
            second_stage = self.stage(second_root)
            first_archive, _ = installer.archive_payload(first_stage, Path(temporary) / "first.zip")
            second_archive, _ = installer.archive_payload(second_stage, Path(temporary) / "second.zip")
            self.assertEqual(first_archive.read_bytes(), second_archive.read_bytes())

    def test_stage_detects_input_drift_after_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, sdk, _, _, inventory, sources = self.inventory(root)
            (sdk / Path(installer.BIN_DIRECTORY.as_posix()) / "AzCore.dll").write_bytes(b"changed")
            with self.assertRaisesRegex(installer.InstallerError, "changed after inventory"):
                installer.stage_payload(
                    inventory,
                    sources,
                    root / "stage",
                    str(inventory["inventory_sha256"]),
                    "reviewer",
                    REVIEW_TIME,
                    "review:evidence",
                )
            self.assertFalse((root / "stage").exists())

    def test_stage_verification_rejects_tampering_and_undeclared_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = self.stage(root)
            editor = stage / Path(installer.EDITOR_PATH.as_posix())
            editor.write_bytes(b"tampered")
            with self.assertRaisesRegex(installer.InstallerError, "checksum mismatch"):
                installer.verify_stage(stage)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = self.stage(root)
            (stage / "undeclared.txt").write_text("extra", encoding="utf-8")
            with self.assertRaisesRegex(installer.InstallerError, "file set mismatch"):
                installer.verify_stage(stage)

    def test_windows_path_aliases_and_reserved_names_are_rejected(self) -> None:
        with self.assertRaisesRegex(installer.InstallerError, "trailing Windows alias"):
            installer.validate_destination(PurePosixPath("data/name. "))
        with self.assertRaisesRegex(installer.InstallerError, "reserved Windows device"):
            installer.validate_destination(PurePosixPath("data/CON.txt"))

        first_identity = installer.validate_destination(PurePosixPath("Data/Value.txt"))
        second_identity = installer.validate_destination(PurePosixPath("data/value.TXT"))
        self.assertEqual(first_identity, second_identity)

    def test_symlinked_sdk_input_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, sdk, notices, packages = self.make_inputs(root)
            outside = root / "outside.dll"
            outside.write_bytes(b"outside")
            link = sdk / "linked.dll"
            try:
                link.symlink_to(outside)
            except OSError as exc:
                self.skipTest(f"symlinks unavailable: {exc}")
            with self.assertRaisesRegex(installer.InstallerError, "symlinks"):
                installer.build_inventory(
                    repo, sdk, notices, packages, VERSION, SOURCE_COMMIT, "development"
                )

    def test_invalid_sdk_engine_identity_and_version_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            repo, sdk, notices, packages = self.make_inputs(root)
            (sdk / "engine.json").write_text(
                json.dumps({"engine_name": "o3de", "sdk_engine": True}), encoding="utf-8"
            )
            with self.assertRaisesRegex(installer.InstallerError, "O3DE_INSTALL_ENGINE_NAME"):
                installer.build_inventory(
                    repo, sdk, notices, packages, VERSION, SOURCE_COMMIT, "development"
                )
        with self.assertRaisesRegex(installer.InstallerError, "three numeric components"):
            installer.require_version("0.1.0-alpha")
        with self.assertRaisesRegex(installer.InstallerError, "Channel must be one of"):
            installer.require_channel("nightly")

    def test_manifest_schema_and_json_keys_fail_closed(self) -> None:
        with self.assertRaisesRegex(installer.InstallerError, "Duplicate JSON key"):
            installer.parse_json_bytes(b'{"schema_version":1,"schema_version":2}', "fixture")

        with tempfile.TemporaryDirectory() as temporary:
            stage = self.stage(Path(temporary))
            manifest = installer.verify_stage(stage)
            manifest["unexpected"] = True
            with self.assertRaisesRegex(installer.InstallerError, "unexpected fields"):
                installer.parse_manifest_document(manifest)

            entry = dict(manifest["entries"][0])
            entry["source_kind"] = "caller-selected"
            with self.assertRaisesRegex(installer.InstallerError, "source kind is unsupported"):
                installer.validate_manifest_entries([entry])

    def test_source_commit_is_derived_only_from_a_clean_git_head(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary) / "repo"
            repo.mkdir()
            subprocess.run(("git", "init", "--quiet", str(repo)), check=True)
            subprocess.run(("git", "-C", str(repo), "config", "user.name", "Installer Test"), check=True)
            subprocess.run(
                ("git", "-C", str(repo), "config", "user.email", "installer-test@example.invalid"),
                check=True,
            )
            tracked = repo / "tracked.txt"
            tracked.write_text("clean\n", encoding="utf-8")
            subprocess.run(("git", "-C", str(repo), "add", "tracked.txt"), check=True)
            subprocess.run(("git", "-C", str(repo), "commit", "--quiet", "-m", "fixture"), check=True)
            expected = subprocess.run(
                ("git", "-C", str(repo), "rev-parse", "HEAD"),
                check=True,
                capture_output=True,
                text=True,
                encoding="utf-8",
            ).stdout.strip()
            self.assertEqual(installer.derive_clean_repository_commit(repo), expected)
            tracked.write_text("dirty\n", encoding="utf-8")
            with self.assertRaisesRegex(installer.InstallerError, "clean exact Git head"):
                installer.derive_clean_repository_commit(repo)


if __name__ == "__main__":
    unittest.main()
