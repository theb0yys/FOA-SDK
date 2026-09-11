# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Prepare a disposable synthetic workspace for assets_localisation_live_smoke.py."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import developer_preview_fixture


def prepare(output):
    output = output.resolve()
    if output.exists():
        raise ValueError("Use a new output directory; existing workspaces are never replaced.")
    with tempfile.TemporaryDirectory(prefix="foa-asset-template-") as temporary:
        template = Path(temporary) / "fixture"
        developer_preview_fixture.generate_fixture(template)
        shutil.copytree(template, output, ignore=shutil.ignore_patterns("preview-fixture.manifest.json"))
    path = output / "preview.tgworkspace.json"
    workspace = json.loads(path.read_text(encoding="utf-8"))
    for key in ("RootPath", "OutputPath", "StagingPath", "DeploymentPath"):
        directory = output if key == "RootPath" else output / workspace[key]
        directory.mkdir(parents=True, exist_ok=True)
        workspace[key] = directory.as_posix()
    profile = workspace["GameProfiles"][0]
    install = output / "SyntheticGame"
    profile["InstallPath"] = install.as_posix()
    profile["ManagedAssembliesPath"] = (install / "Fall of Avalon_Data/Managed").as_posix()
    profile["PluginPath"] = (install / "BepInEx/plugins").as_posix()
    profile["DiagnosticsPath"] = (output / "Diagnostics").as_posix()
    profile["ExtractedDataPath"] = (output / "Extracted").as_posix()
    for key in ("InstallPath", "ManagedAssembliesPath", "PluginPath", "DiagnosticsPath", "ExtractedDataPath"):
        Path(profile[key]).mkdir(parents=True, exist_ok=True)
    # Layout sentinels only: no executable/game payload and no process launch.
    (install / "Fall of Avalon.exe").write_text("synthetic-fixture-not-executable", encoding="utf-8")
    (Path(profile["ManagedAssembliesPath"]) / "Assembly-CSharp.dll").write_text("synthetic-fixture-not-assembly", encoding="utf-8")
    profile["UnityVersion"] = "2022.3.22f1"
    profile["BepInExVersion"] = "5.4.23.3"
    path.write_text(json.dumps(workspace, indent=2), encoding="utf-8")
    pack = output / "Packs/preview.developer-preview-0/pack.tgpack.json"
    pack.parent.mkdir(parents=True, exist_ok=True)
    (output / "Packs/preview.developer-preview-0.tgpack.json").rename(pack)
    path = output / "Catalog/catalog.tgcatalog.json"
    catalog = json.loads(path.read_text(encoding="utf-8"))
    catalog["SchemaVersion"] = 6
    path.write_text(json.dumps(catalog, indent=2), encoding="utf-8")
    return output / "preview.tgworkspace.json"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    print(prepare(arguments.output))
