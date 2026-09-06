#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

import verify_installer_artifacts as artifacts


REPO_ROOT = Path(__file__).resolve().parents[4]
WORKFLOW = REPO_ROOT / ".github/workflows/tainted-grail-sdk-installer.yml"
INSTALLER_PAYLOAD = REPO_ROOT / "Installer/Launcher/Windows/InstallerPayload.cs"
FUNCTIONAL_READINESS_SCRIPT = (
    REPO_ROOT
    / "Installer/Tests/WindowsFunctionalReadiness/Invoke-FoaWindowsFunctionalReadiness.ps1"
)
INSTALLER_IMPLEMENTATION = (
    WORKFLOW,
    REPO_ROOT / "Installer/Packaging/Windows/CMakeLists.txt",
    REPO_ROOT / "Installer/Launcher/Windows/Program.cs",
    INSTALLER_PAYLOAD,
    REPO_ROOT / "Installer/Launcher/Windows/WindowsInstallerRunner.cs",
    REPO_ROOT / "Installer/Launcher/Windows/InstallerWizardForm.cs",
    REPO_ROOT / "Installer/Launcher/Windows/InstalledEditorLauncher.cpp",
)
VERSION = "0.1.0"


class InstallerPipelineContractTests(unittest.TestCase):
    def write_artifact(self, root: Path, name: str, contents: bytes) -> Path:
        artifact = root / name
        artifact.write_bytes(contents)
        digest = hashlib.sha256(contents).hexdigest()
        Path(f"{artifact}.sha256").write_text(
            f"{digest}  {name}\n",
            encoding="utf-8",
        )
        return artifact

    def make_artifact_set(self, root: Path) -> tuple[Path, Path, Path]:
        base = f"Tainted-Grail-FoA-SDK-{VERSION}-windows-x64"
        return (
            self.write_artifact(root, "FOA-SDK-Installer.exe", b"wizard"),
            self.write_artifact(root, f"{base}.msi", b"msi"),
            self.write_artifact(root, f"{base}.zip", b"zip"),
        )

    def test_complete_artifact_set_and_sidecars_verify(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wizard, msi, portable_zip = self.make_artifact_set(root)
            with mock.patch.object(artifacts.installer, "verify_archive") as verify_archive:
                verified = artifacts.verify_artifact_set(root, VERSION)
            verify_archive.assert_called_once_with(portable_zip)
            self.assertEqual(
                set(verified),
                {wizard.name, msi.name, portable_zip.name},
            )

    def test_cpack_work_files_cannot_leak_into_retained_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_artifact_set(root)
            (root / "_CPack_Packages").mkdir()
            with self.assertRaisesRegex(
                artifacts.ArtifactVerificationError,
                "artifact set mismatch",
            ):
                artifacts.verify_artifact_set(root, VERSION)

    def test_checksum_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wizard, _, _ = self.make_artifact_set(root)
            wizard.write_bytes(b"tampered")
            with self.assertRaisesRegex(artifacts.ArtifactVerificationError, "checksum mismatch"):
                artifacts.verify_checksum_pair(wizard)

    def test_checksum_record_must_be_canonical_and_name_exact_file(self) -> None:
        digest = "a" * 64
        self.assertEqual(
            artifacts.parse_checksum_record(f"{digest}  payload.msi\n", "payload.msi"),
            digest,
        )
        with self.assertRaisesRegex(artifacts.ArtifactVerificationError, "canonical"):
            artifacts.parse_checksum_record(f"{digest}  payload.msi\r\n", "payload.msi")
        with self.assertRaisesRegex(artifacts.ArtifactVerificationError, "lowercase sha256"):
            artifacts.parse_checksum_record(f"{digest}  other.msi\n", "payload.msi")

    def test_embedded_msi_parser_requires_the_same_canonical_record(self) -> None:
        source = INSTALLER_PAYLOAD.read_text(encoding="utf-8")
        self.assertIn('text.EndsWith("\\n", StringComparison.Ordinal)', source)
        self.assertIn("text.Contains('\\r')", source)
        self.assertIn('Split("  ", StringSplitOptions.None)', source)
        self.assertIn("parts.Length != 2", source)
        self.assertIn('Path.GetExtension(parts[1]), ".msi"', source)
        self.assertNotIn("text.Trim().Split", source)

    def test_workflow_enforces_the_approved_pipeline_order(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        ordered_steps = (
            "Run authoritative repository and compiled-test validation",
            "Configure prebuilt O3DE SDK layout",
            "Build canonical O3DE INSTALL target",
            "Build and verify installed FOA-SDK.exe launcher entry point",
            "Generate notices and third-party package inventory",
            "Generate exact installer inventory",
            "Upload inventory for redistribution review",
            "Bind package mode to exact redistribution review",
            "Stage reviewed payload, self-test launcher, and create portable ZIP",
            "Install pinned build-only WiX toolchain",
            "Install hash-pinned build-only CMake and CPack toolchain",
            "Build standard MSI from the same staged payload",
            "Build self-contained installer wizard with the reviewed MSI",
            "Verify retained installer artifacts and checksums",
            "Smoke install, configure, launch contract, repair, and uninstall",
            "Upload unsigned development installer artifacts",
        )
        positions = [workflow.index(f"- name: {name}") for name in ordered_steps]
        self.assertEqual(positions, sorted(positions))

    def test_inventory_and_stage_consume_only_the_o3de_install_root(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertEqual(workflow.count("--sdk-root $env:SDK_INSTALL"), 2)
        self.assertNotIn("--sdk-root $env:SDK_BUILD", workflow)
        self.assertNotIn("--sdk-root $env:SDK_VALIDATION_BUILD", workflow)
        self.assertIn("--scan-path \"$env:SDK_INSTALL\"", workflow)

    def test_inventory_requires_verified_installed_sdk_entrypoint(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        installer_source = (
            REPO_ROOT / "Gems/TaintedGrailModdingSDK/Tools/developer_preview_installer.py"
        ).read_text(encoding="utf-8")
        self.assertIn("--target TaintedGrailModdingEditorLauncher", workflow)
        self.assertIn("FOA-SDK.exe does not match the reviewed installed launcher build.", workflow)
        self.assertNotIn(
            "FOA-SDK.exe installed launcher self-test failed before inventory review.",
            workflow,
        )
        self.assertIn('$stagedLauncher = Join-Path $env:SDK_STAGE "bin/Windows/profile/Default/FOA-SDK.exe"', workflow)
        self.assertIn("& $stagedLauncher --self-test", workflow)
        self.assertIn(
            "FOA-SDK.exe staged self-contained launcher self-test failed.",
            workflow,
        )
        self.assertIn('SDK_ENTRYPOINT_PATH = BIN_DIRECTORY / "FOA-SDK.exe"', installer_source)
        self.assertIn("Installed FOA-SDK.exe launcher entry point", installer_source)

    def test_installed_sdk_launcher_resolves_self_contained_product_layout(self) -> None:
        launcher_source = (REPO_ROOT / "Installer/Launcher/Windows/InstalledEditorLauncher.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('InstalledBinRelativePath[] = L"bin\\\\Windows\\\\profile\\\\Default"', launcher_source)
        self.assertIn("launcherDirectory / EditorFileName", launcher_source)
        self.assertIn("installRoot / InstalledBinRelativePath / EditorFileName", launcher_source)
        self.assertIn("installRoot / EngineMetadataFileName", launcher_source)
        self.assertIn("installRoot / ProjectDirectoryName", launcher_source)
        self.assertIn("LOCALAPPDATA", launcher_source)
        self.assertIn("--engine-path", launcher_source)
        self.assertIn("QuoteArgument(engineRoot)", launcher_source)
        self.assertIn("--project-path", launcher_source)
        self.assertIn("MaterializedProjectDirectoryName", launcher_source)
        self.assertIn("ExternalDirectoryName", launcher_source)
        self.assertIn("QuoteArgument(launchProject)", launcher_source)
        self.assertIn("BundledCMakeBinRelativePath", launcher_source)
        self.assertIn("ConfigureBundledRuntimeEnvironment", launcher_source)
        self.assertIn('SetEnvironmentVariableW(L"LY_CMAKE_PATH"', launcher_source)
        self.assertIn("--project-cache-path", launcher_source)
        self.assertIn("--project-user-path", launcher_source)
        self.assertIn("--project-log-path", launcher_source)
        self.assertIn("asset_processor.setreg", launcher_source)
        self.assertIn("ProjectRegistryRelativePath", launcher_source)
        self.assertIn("self-contained FOA-SDK install", launcher_source)

    def test_zip_and_msi_are_built_from_the_same_verified_stage(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("--stage-root $env:SDK_STAGE", workflow)
        self.assertIn('-DTG_INSTALLER_PAYLOAD_ROOT="$env:SDK_STAGE"', workflow)
        self.assertIn(
            "python Gems/TaintedGrailModdingSDK/Tools/verify_installer_artifacts.py",
            workflow,
        )
        self.assertIn('--artifact-root "${{ env.SDK_ARTIFACTS }}"', workflow)

    def test_cpack_work_area_is_separate_from_retained_artifacts(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn(
            '"SDK_PACKAGE_OUTPUT=$env:RUNNER_TEMP/tg-sdk-package-output" >> $env:GITHUB_ENV',
            workflow,
        )
        self.assertIn("-G WIX -B $env:SDK_PACKAGE_OUTPUT", workflow)
        self.assertNotIn("-G WIX -B $env:SDK_ARTIFACTS", workflow)
        self.assertIn(
            "Copy-Item -LiteralPath $msi.FullName -Destination (Join-Path $env:SDK_ARTIFACTS $msi.Name)",
            workflow,
        )

    def test_msi_package_label_and_cabs_match_payload_role(self) -> None:
        packaging = (REPO_ROOT / "Installer/Packaging/Windows/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn('set(CPACK_PACKAGE_NAME "Tainted Grail FoA SDK MSI Payload")', packaging)
        self.assertIn("Windows Installer payload for the prebuilt", packaging)
        self.assertIn("set(CPACK_WIX_CAB_PER_COMPONENT ON)", packaging)
        self.assertIn('set(CPACK_WIX_BUILD_EXTRA_FLAGS "-sw1026")', packaging)
        self.assertIn("COMPONENT p_lib_base", packaging)
        self.assertIn('set(tg_lib_component "p_lib_${tg_lib_bucket}")', packaging)
        self.assertIn('COMPONENT "p_${tg_payload_component_suffix}"', packaging)

    def test_packaging_uses_exact_hash_pinned_cmake_4_3_toolchain(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("SDK_PACKAGE_CMAKE_VERSION: 4.3.4", workflow)
        self.assertIn(
            "SDK_PACKAGE_CMAKE_SHA256: "
            "86e5fcafb38bdf58346a78b187c7b6b4f252ae5242cffe24c463a92bbd2e77d1",
            workflow,
        )
        self.assertIn("https://cmake.org/files/v4.3/$archiveName", workflow)
        self.assertIn("$actualHash -cne $env:SDK_PACKAGE_CMAKE_SHA256", workflow)
        self.assertIn("Install hash-pinned CMake runtime into SDK payload", workflow)
        self.assertIn('$sdkCmakeRuntime = Join-Path $env:SDK_INSTALL "cmake/runtime"', workflow)
        self.assertIn('Copy-Item -Path (Join-Path $toolRoot "*") -Destination $sdkCmakeRuntime -Recurse -Force', workflow)
        self.assertIn("& $env:SDK_PACKAGE_CMAKE -S Installer/Packaging/Windows", workflow)
        self.assertIn("& $env:SDK_PACKAGE_CPACK --config", workflow)
        self.assertNotIn("\n          cmake -S Installer/Packaging/Windows", workflow)
        self.assertNotIn("\n          cpack --config", workflow)

    def test_lifecycle_smoke_proves_manifest_shortcut_repair_and_uninstall(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        script = FUNCTIONAL_READINESS_SCRIPT.read_text(encoding="utf-8")
        self.assertIn("Invoke-FoaWindowsFunctionalReadiness.ps1", workflow)
        self.assertIn("windows-functional-readiness", workflow)
        for fragment in (
            "Installed MSI manifest differs from the exact reviewed staging manifest",
            "CreateShortcut($startMenuEntry)",
            "MSI Start Menu entry targets",
            "WriteAllBytes($launcher",
            "MSI repair did not restore the reviewed product-owned launcher bytes",
            "repaired-launcher-self-test",
            "MSI uninstall left the product manifest installed",
            "MSI uninstall removed external workspace data",
            "functional-readiness-summary.json",
        ):
            self.assertIn(fragment, script)

    def test_explicitly_excluded_installer_capabilities_remain_absent(self) -> None:
        forbidden = (
            "signtool",
            "azuresigntool",
            "codesign",
            "gh release create",
            "softprops/action-gh-release",
            "backgroundservice",
            "new-service",
            "start-service",
            "schtasks",
            "httpclient",
            "webclient",
            "downloadfile",
            "winget install",
            "choco install",
            "vcredist",
            "bepinex",
            "harmony",
            "savegames",
            "telemetry",
            "merlin workshop",
        )
        for path in INSTALLER_IMPLEMENTATION:
            contents = path.read_text(encoding="utf-8").casefold()
            for fragment in forbidden:
                self.assertNotIn(fragment, contents, f"{path} contains excluded capability {fragment!r}")


if __name__ == "__main__":
    unittest.main()
