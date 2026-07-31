#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
REQ=["foa-visual-asset-discovery-index","FoA native asset reference","version-bound discovery record","local preview artefact","generated O3DE preview product","typed authoring binding","FunctionCompleteAllowed","VisualPreviewGateRequired","PreviewProductGenerated","O3deAssetProcessorInvoked","UnityInvoked","PayloadCopied","FileContentCopyAllowed","AssemblyLoadAllowed","RuntimeInvocationAllowed","CatalogPromotionAllowed","RuntimePermissionGranted","ALLOWLIST_EXTENSIONS","token_locator","verify_index"]
BAD=["import subprocess","subprocess.","Popen(","os.system","Unity.exe","AssetProcessorBatch","Catalog/catalog.tgcatalog.json","shutil.copyfile","copy2("]
DOCREQ=["Status: Alpha implementation slice","not function-complete","read-only asset discovery and indexing","no preview product","no Unity invocation","no Asset Processor invocation","outside repository and engine source trees","foa-visual-asset-index.json","FoA native asset reference","version-bound discovery record","local preview artefact","generated O3DE preview product","typed authoring binding"]
def read(rel):
    p=ROOT/rel
    if not p.is_file(): raise RuntimeError(f"missing {rel}")
    return p.read_text(encoding="utf-8", errors="strict")
def check():
    tool=read("Gems/TaintedGrailModdingSDK/Tools/foa_visual_asset_discovery_index.py"); tests=read("Gems/TaintedGrailModdingSDK/Tools/tests/test_foa_visual_asset_discovery_index.py"); doc=read("docs/tainted-grail-sdk/FOA_VISUAL_ASSET_DISCOVERY_INDEX.md")
    for x in REQ:
        if x not in tool: raise RuntimeError(f"tool missing {x}")
    for x in BAD:
        if x in tool: raise RuntimeError(f"tool contains prohibited {x}")
    for x in ("test_index_generates_profile_bound_visual_asset_records","test_output_contains_no_absolute_private_paths","test_write_verify_and_authority_rejection","test_fixture_and_cli_succeed"):
        if x not in tests: raise RuntimeError(f"tests missing {x}")
    for x in DOCREQ:
        if x not in doc: raise RuntimeError(f"doc missing {x}")
def main():
    try: check()
    except RuntimeError as e: print(f"FoA visual asset discovery index validation failed: {e}", file=sys.stderr); return 1
    print("FoA visual asset discovery index boundary passed."); return 0
if __name__=="__main__": raise SystemExit(main())
