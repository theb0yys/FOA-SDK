#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import importlib.util, json, shutil, sys, tempfile, unittest
from pathlib import Path
TOOLS_ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("foa_visual_asset_discovery_index", TOOLS_ROOT/"foa_visual_asset_discovery_index.py")
assert SPEC and SPEC.loader
indexer=importlib.util.module_from_spec(SPEC); sys.modules[SPEC.name]=indexer; SPEC.loader.exec_module(indexer)
class VisualAssetDiscoveryIndexTests(unittest.TestCase):
    def setUp(self):
        self.root=Path(tempfile.mkdtemp(prefix="foa-visual-index-tests-")); self.install=self.root/"game"/"FoA"; self.extracted=self.root/"workspace"/"Extracted"; aa=self.install/"Tainted Grail_Data"/"StreamingAssets"/"aa"; icons=self.install/"Tainted Grail_Data"/"LooseIcons"; aa.mkdir(parents=True); icons.mkdir(parents=True); self.extracted.mkdir(parents=True)
        (aa/"catalog.json").write_text("{}\n"); (aa/"items.bundle").write_bytes(b"bundle"); (icons/"iron.png").write_bytes(b"png"); (aa/"ignored.txt").write_text("ignored")
        self.workspace=self.root/"workspace.tgworkspace.json"; self.workspace.write_bytes(indexer.pretty_json({"SchemaVersion":1,"WorkspaceId":"test.workspace","DisplayName":"Test","RootPath":str(self.root/"workspace"),"OutputPath":str(self.root/"workspace"/"Build"),"StagingPath":str(self.root/"workspace"/"Staging"),"DeploymentPath":str(self.root/"workspace"/"Deploy"),"ActiveGameProfileId":"foa.mono.test","GameProfiles":[{"ProfileId":"foa.mono.test","DisplayName":"FoA","InstallPath":str(self.install),"GameVersion":"1.23.401","Branch":"mono","RuntimeTarget":"Mono","UnityVersion":"6000.0.64f1","BepInExVersion":"5.4.23.3","ManagedAssembliesPath":"","PluginPath":"","DiagnosticsPath":str(self.root/"workspace"/"Diagnostics"),"ExtractedDataPath":str(self.extracted),"DlcScopes":["base-game"]}]}))
    def tearDown(self): shutil.rmtree(self.root, ignore_errors=True)
    def test_index_generates_profile_bound_visual_asset_records(self):
        d=indexer.build_index(self.workspace,captured_at="2026-07-28T00:00:00Z"); self.assertFalse(d["PreviewGateStatus"]["FunctionCompleteAllowed"]); self.assertGreaterEqual(len(d["AssetRecords"]),3); self.assertTrue(all(r["NativeAssetRef"].startswith("$install/") for r in d["AssetRecords"])); self.assertFalse(any("ignored.txt" in r["NativeAssetRef"] for r in d["AssetRecords"]))
    def test_output_contains_no_absolute_private_paths(self): self.assertNotIn(str(self.root), json.dumps(indexer.build_index(self.workspace,captured_at="2026-07-28T00:00:00Z")))
    def test_write_verify_and_authority_rejection(self):
        d=indexer.build_index(self.workspace,captured_at="2026-07-28T00:00:00Z"); out=self.extracted/indexer.DEFAULT_INDEX_NAME; indexer.write_index(d,out); self.assertEqual(indexer.verify_index(out,self.workspace)["IndexId"],d["IndexId"]); d["OperationalAuthority"]["RuntimePermissionGranted"]=True; out.write_bytes(indexer.pretty_json(d)); self.assertRaises(indexer.DiscoveryError,indexer.verify_index,out,self.workspace)
    def test_missing_install_and_bad_timestamp_are_rejected(self):
        self.assertRaisesRegex(indexer.DiscoveryError,"whole-second",indexer.build_index,self.workspace,captured_at="2026-07-28T00:00:00.1Z"); data=json.loads(self.workspace.read_text()); data["GameProfiles"][0]["InstallPath"]=str(self.root/"missing"); self.workspace.write_bytes(indexer.pretty_json(data)); self.assertRaises(indexer.DiscoveryError,indexer.build_index,self.workspace,captured_at="2026-07-28T00:00:00Z")
    def test_fixture_and_cli_succeed(self):
        f=self.root/"fixture"; m=indexer.generate_fixture(f); self.assertGreaterEqual(m["AssetRecordCount"],3); out=self.extracted/"cli.json"; self.assertEqual(indexer.main(["index","--workspace",str(self.workspace),"--output",str(out),"--captured-at","2026-07-28T00:00:00Z"]),0); self.assertEqual(indexer.main(["verify","--input",str(out),"--workspace",str(self.workspace)]),0)
if __name__=="__main__": unittest.main()
