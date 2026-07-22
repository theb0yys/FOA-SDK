#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Apply the reviewed project-wide execution-security and Gate 5 repair bundle."""
from __future__ import annotations

import base64
import io
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUNDLE_PARTS = ROOT / "Gems/TaintedGrailModdingSDK/Tools/project_wide_security_repair_bundle"

def replace_exact(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"Expected exactly one patch anchor: {path}: {old[:120]!r}; found {text.count(old)}")
    path.write_text(text.replace(old, new), encoding="utf-8")

def append_once(path: Path, marker: str, content: str) -> None:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        return
    if not text.endswith("\n"):
        text += "\n"
    path.write_text(text + "\n" + content.strip() + "\n", encoding="utf-8")

def extract_bundle() -> None:
    encoded = ''.join(path.read_text(encoding='ascii').strip() for path in sorted(BUNDLE_PARTS.glob('*.part')))
    if not encoded:
        raise RuntimeError('The temporary repair bundle is empty.')
    payload = base64.b64decode(encoded.encode('ascii'), validate=True)
    with tarfile.open(fileobj=io.BytesIO(payload), mode="r:gz") as archive:
        for member in archive.getmembers():
            if not member.isfile():
                continue
            relative = Path(member.name)
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"Unsafe embedded path: {member.name}")
            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"Unable to read embedded file: {member.name}")
            target = ROOT / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read())

def main() -> int:
    extract_bundle()
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.cpp', "    bool IsExactVersionTokenV1(AZStd::string_view value)\n    {\n        if (value.empty() || value.size() > MaxSemanticTokenBytesV1) { return false; }\n        for (char current : value) { if (!(IsLowerAlpha(current) || IsDigit(current) || current == '.' || current == '_' || current == '-')) { return false; } }\n        return true;\n    }\n", "    bool IsExactVersionTokenV1(AZStd::string_view value)\n    {\n        if (value.empty() || value.size() > MaxSemanticTokenBytesV1 ||\n            !IsLowerAlpha(value.front()) || value.back() == '.' ||\n            value.back() == '_' || value.back() == '-')\n        {\n            return false;\n        }\n        char previous = 0;\n        for (char current : value)\n        {\n            if (!IsTokenByte(current))\n            {\n                return false;\n            }\n            if ((current == '.' || current == '_' || current == '-') &&\n                (previous == '.' || previous == '_' || previous == '-'))\n            {\n                return false;\n            }\n            previous = current;\n        }\n        return true;\n    }\n")
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Code/CMakeLists.txt', '    ly_add_googletest(\n        NAME Gem::${gem_name}.Catalog.Tests\n        LABELS REQUIRES_tiaf\n    )\nendif()\n', '    ly_add_googletest(\n        NAME Gem::${gem_name}.Catalog.Tests\n        LABELS REQUIRES_tiaf\n    )\n\n    # Gate 5 canonical interchange tests deliberately link Core only. This\n    # prevents Framework, Editor, provider, runtime, Qt, or AzToolsFramework\n    # dependencies from silently entering the inert canonical contract.\n    ly_add_target(\n        NAME ${gem_name}.CanonicalInterchange.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}\n        NAMESPACE Gem\n        NO_UNITY\n        FILES_CMAKE\n            taintedgrailmoddingsdk_canonical_interchange_tests_files.cmake\n        INCLUDE_DIRECTORIES\n            PRIVATE\n                Source\n                Tests\n        BUILD_DEPENDENCIES\n            PRIVATE\n                Gem::${gem_name}.Core.Static\n                AZ::AzTest\n    )\n\n    ly_add_googletest(\n        NAME Gem::${gem_name}.CanonicalInterchange.Tests\n        LABELS REQUIRES_tiaf\n    )\nendif()\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py', '    "validate_core_framework_build_graph.py",\n', '    "validate_core_framework_build_graph.py",\n    "validate_canonical_interchange_compiled_tests.py",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py', '        "Compiled TG SDK catalog tests",\n', '        "Compiled TG SDK catalog and canonical interchange tests",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py', '            "TaintedGrailModdingSDK.Catalog.Tests",\n', '            r"TaintedGrailModdingSDK\\.(Catalog|CanonicalInterchange)\\.Tests",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py', '            "Run FOA-SDK Python tests, validators, fixtures, pinned O3DE source "\n            "policy, and mandatory compiled Catalog CTest coverage."\n', '            "Run FOA-SDK Python tests, validators, fixtures, pinned O3DE source "\n            "policy, and mandatory compiled Catalog plus CanonicalInterchange CTest coverage."\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/developer_preview.py', 'CATALOG_TEST_TARGET = "TaintedGrailModdingSDK.Catalog.Tests"\nCATALOG_TEST_PATTERN = r"TaintedGrailModdingSDK\\.Catalog\\.Tests"\n', 'CATALOG_TEST_TARGET = "TaintedGrailModdingSDK.Catalog.Tests"\nCANONICAL_INTERCHANGE_TEST_TARGET = "TaintedGrailModdingSDK.CanonicalInterchange.Tests"\nCATALOG_TEST_PATTERN = r"TaintedGrailModdingSDK\\.(Catalog|CanonicalInterchange)\\.Tests"\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/developer_preview.py', '        EDITOR_TARGET,\n        ASSET_PROCESSOR_BATCH_TARGET,\n        CATALOG_TEST_TARGET,\n', '        EDITOR_TARGET,\n        ASSET_PROCESSOR_BATCH_TARGET,\n        CATALOG_TEST_TARGET,\n        CANONICAL_INTERCHANGE_TEST_TARGET,\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/developer_preview.py', '        CommandStep(\n            "o3de-source-policy",\n', '        CommandStep(\n            "canonical-interchange-contract",\n            (sys.executable, str(tools / "validate_canonical_interchange_compiled_tests.py")),\n            str(product_root),\n        ),\n        CommandStep(\n            "o3de-source-policy",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/developer_preview.py', '            "compiled-catalog-tests",\n', '            "compiled-catalog-and-canonical-interchange-tests",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/developer_preview.py', '        help="Build the Editor, asset preflight, and TG SDK catalog tests.",\n', '        help="Build the Editor, asset preflight, catalog tests, and Core-only canonical interchange tests.",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/developer_preview.py', '    validate = subparsers.add_parser("validate", help="Run all focused validators and compiled catalog tests.")\n', '    validate = subparsers.add_parser("validate", help="Run all focused validators and both mandatory compiled test targets.")\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/tests/test_developer_preview.py', '            "validate_catalog_tests.py",\n', '            "validate_catalog_tests.py",\n            "validate_canonical_interchange_compiled_tests.py",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/tests/test_developer_preview.py', '        self.assertEqual(\n            command[-3:],\n            ("Editor", "AssetProcessorBatch", "TaintedGrailModdingSDK.Catalog.Tests"),\n        )\n', '        self.assertEqual(\n            command[-4:],\n            (\n                "Editor",\n                "AssetProcessorBatch",\n                "TaintedGrailModdingSDK.Catalog.Tests",\n                "TaintedGrailModdingSDK.CanonicalInterchange.Tests",\n            ),\n        )\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/tests/test_developer_preview.py', '                "catalog-contract",\n                "o3de-source-policy",\n                "compiled-catalog-tests",\n', '                "catalog-contract",\n                "canonical-interchange-contract",\n                "o3de-source-policy",\n                "compiled-catalog-and-canonical-interchange-tests",\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/tests/test_developer_preview.py', '        compiled = next(step for step in plan if step.name == "compiled-catalog-tests")\n', '        compiled = next(step for step in plan if step.name == "compiled-catalog-and-canonical-interchange-tests")\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview.py', '                \'"catalog-contract"\',\n                \'"o3de-source-policy"\',\n                \'"compiled-catalog-tests"\',\n', '                \'"catalog-contract"\',\n                \'"canonical-interchange-contract"\',\n                \'"o3de-source-policy"\',\n                \'"compiled-catalog-and-canonical-interchange-tests"\',\n')
    replace_exact(ROOT / 'Gems/TaintedGrailModdingSDK/Tools/validate_developer_preview.py', '                "test_configure_command_uses_external_engine_and_product_project",\n                "test_validation_plan_separates_product_and_engine_tools",\n', '                "test_configure_command_uses_external_engine_and_product_project",\n                "test_build_command_has_fixed_profile_editor_asset_and_catalog_targets",\n                "test_validation_plan_separates_product_and_engine_tools",\n')
    append_once(ROOT / 'Installer/Bootstrapper/README.md', '## Authenticated execution security', '## Authenticated execution security\n\n`Security/` owns the shared trust-anchor, resolver-bound operation-plan, one-shot claim, bounded-process, immutable-helper, and atomic no-replace primitives used by every operational bootstrapper lane. Copy, launch, elevation, lifecycle coordination, and installation-state publication must consume the exact authenticated session capability and may not mint authority from caller text.')
    append_once(ROOT / 'Installer/Tests/README.md', '## Execution-security regression lane', '## Execution-security regression lane\n\n`Security/` and the operational bootstrapper suites prove wrong-key rejection, exact resolver and helper binding, capability denial, one-shot nonce consumption, bounded output, process-tree timeout handling, controlled elevation bootstrapper requests, no-replace staging publication, and authenticated installation-state publication.')
    append_once(ROOT / 'Installer/Bootstrapper/PackageEngine/README.md', '## Trust-anchor hardening', '## Trust-anchor hardening\n\nPackage-engine tokens and sessions are Schema 2 authenticated records. A token consumes a trusted broker proof for one resolver-bound operation plan, and session acceptance atomically claims the token nonce.')
    append_once(ROOT / 'Installer/Bootstrapper/PackageCopier/README.md', '## Security hardening', '## Security hardening\n\nPayload staging now requires the explicit authenticated copy capability, consumes its grant once, signs its receipt, and publishes the staging directory with an atomic no-replace primitive.')
    append_once(ROOT / 'Installer/Bootstrapper/ProcessLauncher/README.md', '## Security hardening', '## Security hardening\n\nCallers select only a reviewed helper reference. Executable hash, argv, working directory, environment, timeout, and output limit derive from the authenticated operation plan. Execution uses a verified private copy, bounded streaming output, process-tree termination, and a one-shot claim.')
    append_once(ROOT / 'Installer/Bootstrapper/ElevationHelper/README.md', '## Security hardening', '## Security hardening\n\nElevation launches only the reviewed controlled bootstrapper. Authenticated consent and grants bind the exact helper request; the bootstrapper receives its reviewed configuration argv plus one one-shot request path and produces a separately authenticated completion receipt.')
    append_once(ROOT / 'Installer/Bootstrapper/LifecycleCoordinator/README.md', '## Security hardening', '## Security hardening\n\nLifecycle grants and results are authenticated, one-shot records and every consumed copy, launch, elevation, or bootstrapper-completion receipt is reverified against the same session trust anchor.')
    append_once(ROOT / 'Installer/Bootstrapper/InstallationStatePublisher/README.md', '## Security hardening', '## Security hardening\n\nInstallation-state publication requires its explicit session capability and a completed authenticated lifecycle result. Grants are one-shot; records and receipts are authenticated; initial creation is no-replace and replacement requires the exact previous-state hash.')
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
