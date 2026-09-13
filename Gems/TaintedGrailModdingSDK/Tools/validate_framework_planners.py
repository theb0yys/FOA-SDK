#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

"""M4 ownership and pure-preview route guard; compiled and Editor proof are separate."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
CODE = Path("Gems/TaintedGrailModdingSDK/Code")
FAMILY = ("FrameworkPlannerService.cpp", "FrameworkPlannerService.h")
PANES = ("AdapterBuildManifestWidget", "AdapterPackageAssemblyPreviewWidget",
         "AdapterStagingDeploymentPreviewWidget", "AdapterDeploymentWorkOrderWidget",
         "AdapterPostDeploymentVerificationWidget", "AdapterWorkOrderPlanWidget")
PATHS = [CODE / "Source/ExecutionPlanning" / n for n in FAMILY]
PATHS += [CODE / f"Source/{n}{ext}" for n in PANES for ext in (".cpp", ".h")]
PATHS += [CODE / "Source/FoundationService.h", CODE / "Source/FoundationPlannerService.cpp",
          CODE / "taintedgrailmoddingsdk_framework_files.cmake",
          CODE / "taintedgrailmoddingsdk_catalog_tests_files.cmake",
          CODE / "Tests/FrameworkPlannerTests.cpp", CODE / "Tests/FrameworkExecutionOperationalTests.cpp"]

def validate(root=ROOT):
    errors = []
    try:
        directory = root / CODE / "Source/ExecutionPlanning"
        if tuple(sorted(p.name for p in directory.iterdir() if p.is_file())) != FAMILY:
            errors.append("M4 source inventory must match its reviewed pure adapter family.")
        manifest = (root / CODE / "taintedgrailmoddingsdk_framework_files.cmake").read_text(encoding="utf-8-sig")
        for name in FAMILY:
            if manifest.count("Source/ExecutionPlanning/" + name) != 1:
                errors.append("Planner source requires unique Framework ownership: " + name)
        for name in FAMILY:
            source = (directory / name).read_text(encoding="utf-8-sig")
            source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
            if re.search(r"\b(QProcess|CreateProcess\w*|ShellExecute\w*|SystemFile|fstream|filesystem|ProcessWatcher|ToolExecutionService)\b", source):
                errors.append("Planner adapter introduces an IO/process dependency: " + name)
            if re.search(r"\b(Register\w*|Submit|Confirm|Execute|Promote\w*|Open|Save|Delete|Launch)\s*\(", source):
                errors.append("Planner adapter performs a mutation or admission operation: " + name)
            if re.search(r"m_\w*(?:Allowed|Promoted|Published|Executed)\s*=\s*true", source):
                errors.append("Planner adapter promotes V1 authority: " + name)
        for pane in PANES:
            source = (root / CODE / f"Source/{pane}.cpp").read_text(encoding="utf-8-sig")
            header = (root / CODE / f"Source/{pane}.h").read_text(encoding="utf-8-sig")
            if "GetFrameworkPlanners()" not in source:
                errors.append("Preview pane bypasses Foundation: " + pane)
            if re.search(r"Adapter(?:WorkOrderPlanning|BuildManifest|PackageAssemblyPreview|StagingDeploymentPreview|DeploymentWorkOrder|PostDeploymentVerification)Service\s+m_", header):
                errors.append("Preview pane owns a planner: " + pane)
        tests = (root / CODE / "Tests/FrameworkPlannerTests.cpp").read_text(encoding="utf-8-sig")
        if not re.search(r"\bTEST_F\(FrameworkPlanner,", tests) or "GTEST_SKIP" in tests:
            errors.append("M4 requires non-skipped compiled planner tests.")
        manifest = (root / CODE / "taintedgrailmoddingsdk_catalog_tests_files.cmake").read_text(encoding="utf-8-sig")
        if manifest.count("Tests/FrameworkPlannerTests.cpp") != 1:
            errors.append("M4 compiled tests must run in the Catalog target.")
        native = (root / CODE / "Tests/FrameworkExecutionOperationalTests.cpp").read_text(encoding="utf-8-sig")
        if "M4PlannerSourceIsConsumedOnlyByTheExactFrameworkPreview" not in native:
            errors.append("M4 requires an actual M3 preview callback boundary test.")
    except (OSError, UnicodeError) as error:
        errors.append(str(error))
    return errors

if __name__ == "__main__":
    failures = validate()
    for failure in failures:
        print("ERROR: " + failure)
    print("FAILED" if failures else "PASSED: M4 pure planner ownership and Framework consumer routes.")
    raise SystemExit(bool(failures))
