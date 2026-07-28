#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Read-only visual asset discovery index.

Alpha slice only: FoA native asset reference -> version-bound discovery record.
Remaining required stages are local preview artefact -> generated O3DE preview product
-> typed authoring binding. FunctionCompleteAllowed is always false.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

TOOL_ID="foa.visual-asset-discovery-index"; TOOL_VERSION="0.1.0"; DOCUMENT_KIND="foa-visual-asset-discovery-index"; DEFAULT_INDEX_NAME="foa-visual-asset-index.json"
ALLOWLIST_EXTENSIONS=sorted({".assets",".bundle",".catalog",".dat",".dds",".hash",".jpeg",".jpg",".json",".png",".resource",".ress",".tga",".unity3d",".webp"})
AUTHORITY_FALSE_KEYS=("RuntimeInvocationAllowed","GameMutationAllowed","SaveAccessAllowed","CatalogPromotionAllowed","RuntimePermissionGranted","PreviewProductGenerated","O3deAssetProcessorInvoked","UnityInvoked","PayloadCopied")
ID=re.compile(r"^[a-z0-9][a-z0-9._-]{1,191}$"); UTC=re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$"); PRIVATE=re.compile(r"(^/|^~/|^[A-Za-z]:[\\/]|^\\\\)"); TOKEN=re.compile(r"^\$install(/[^\r\n]*)?$")
class DiscoveryError(RuntimeError): pass

def pretty_json(v:Any)->bytes: return (json.dumps(v,ensure_ascii=False,indent=2)+"\n").encode()
def canon(v:Any)->bytes: return (json.dumps(v,sort_keys=True,separators=(",",":"))+"\n").encode()
def sha(b:bytes)->str: return "sha256:"+hashlib.sha256(b).hexdigest()
def clean(v:Any):
    if isinstance(v,str) and PRIVATE.search(v): raise DiscoveryError("absolute/private path leaked")
    if isinstance(v,list): [clean(x) for x in v]
    if isinstance(v,dict): [clean(x) for x in v.values()]
def sid(v:str,n:str)->str:
    if not isinstance(v,str) or not ID.match(v): raise DiscoveryError(f"{n} must be stable id")
    return v
def utc(v:str)->str:
    if not isinstance(v,str) or not UTC.match(v): raise DiscoveryError("whole-second UTC required")
    datetime.strptime(v,"%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc); return v
def read_json(p:Path)->dict[str,Any]: return json.loads(p.read_text(encoding="utf-8",errors="strict"))
def resolve(v:str,base:Path)->Path:
    p=Path(v).expanduser(); return (base/p if not p.is_absolute() else p).resolve(False)
def inside(p:Path,r:Path)->bool:
    try: p.resolve(False).relative_to(r.resolve(False)); return True
    except ValueError: return False

def profile(workspace:Path)->dict[str,Any]:
    w=read_json(workspace); root=resolve(w["RootPath"],workspace.parent); active=sid(w["ActiveGameProfileId"],"ActiveGameProfileId")
    hits=[p for p in w.get("GameProfiles",[]) if p.get("ProfileId")==active]
    if len(hits)!=1: raise DiscoveryError("active profile must resolve once")
    p=hits[0]; install=resolve(p["InstallPath"],workspace.parent); extracted=resolve(p["ExtractedDataPath"],workspace.parent)
    if not install.is_dir(): raise DiscoveryError("Configured FoA install path does not exist or is not a directory")
    if not inside(extracted,root): raise DiscoveryError("ExtractedDataPath must remain inside workspace root")
    if p["RuntimeTarget"] not in {"Mono","IL2CPP"}: raise DiscoveryError("RuntimeTarget must be Mono or IL2CPP")
    return {"id":sid(p["ProfileId"],"ProfileId"),"game":p["GameVersion"],"branch":p["Branch"],"runtime":p["RuntimeTarget"],"install":install,"extracted":extracted}

def token_locator(path:Path,root:Path)->str:
    rel=path.resolve(False).relative_to(root.resolve(False))
    return "$install"+("/"+rel.as_posix() if rel.parts else "")
def kind(path:Path)->tuple[str,dict[str,Any]]:
    e=path.suffix.lower(); low=path.as_posix().lower()
    if e in {".png",".jpg",".jpeg",".webp",".tga",".dds"}: return "loose-texture",{"ThumbnailCandidate":True,"StaticPreviewCandidate":False,"RequiresExtraction":False,"Reason":"thumbnail candidate"}
    if e in {".bundle",".unity3d",".assets",".resource",".ress"}: return "preview-source-candidate",{"ThumbnailCandidate":False,"StaticPreviewCandidate":True,"RequiresExtraction":True,"Reason":"requires later extraction"}
    return "catalog-or-metadata-candidate",{"ThumbnailCandidate":False,"StaticPreviewCandidate":False,"RequiresExtraction":True,"Reason":"requires later classification"}
def hfile(p:Path)->str:
    h=hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""): h.update(b)
    return "sha256:"+h.hexdigest()

def scan(root:Path,max_files:int,max_depth:int)->list[Path]:
    out=[]
    for cur,dirs,names in os.walk(root,followlinks=False):
        cp=Path(cur); rel=cp.resolve(False).relative_to(root.resolve(False))
        if len(rel.parts)>=max_depth: dirs[:]=[]
        dirs[:]=sorted(d for d in dirs if not (cp/d).is_symlink())
        for n in sorted(names):
            p=cp/n
            if not p.is_symlink() and p.suffix.lower() in ALLOWLIST_EXTENSIONS: out.append(p)
            if len(out)>max_files: raise DiscoveryError("max file count exceeded")
    return out

def build_index(workspace:Path,*,captured_at:str|None=None,max_files:int=4096,max_depth:int=8)->dict[str,Any]:
    p=profile(workspace); cap=utc(captured_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")); rows=[]
    for i,path in enumerate(scan(p["install"],max_files,max_depth)):
        loc=token_locator(path,p["install"]); fk,prev=kind(path); fp=hfile(path); seed=canon([p["id"],loc,fp,fk])
        rows.append({"AssetRecordId":f"visual.asset.{p['id']}.{hashlib.sha256(seed).hexdigest()[:16]}","NativeAssetRef":loc,"ProfileId":p["id"],"GameVersion":p["game"],"Branch":p["branch"],"RuntimeTarget":p["runtime"],"Locator":loc,"FileName":path.name,"Extension":path.suffix.lower(),"FileKind":fk,"ByteSize":path.stat().st_size,"Sha256":fp,"FingerprintStatus":"hashed","PreviewEligibility":prev,"EvidenceKind":"visual-asset-discovery","Confidence":"observed","DiscoveryOrdinal":i,"CatalogPromotionAllowed":False,"RuntimePermissionGranted":False,"PreviewProductGenerated":False})
    rows.sort(key=lambda r:r["NativeAssetRef"]); idx=f"visual.index.{p['id']}.{hashlib.sha256(canon([(r['NativeAssetRef'],r['Sha256']) for r in rows])).hexdigest()[:16]}"
    doc={"SchemaVersion":1,"DocumentKind":DOCUMENT_KIND,"IndexId":idx,"ProfileId":p["id"],"GameVersion":p["game"],"Branch":p["branch"],"RuntimeTarget":p["runtime"],"ToolId":TOOL_ID,"ToolVersion":TOOL_VERSION,"CapturedAt":cap,"InstallRoot":"$install","OutputRoot":"$extracted","DiscoveryScope":{"ConfiguredInstallRootOnly":True,"AllowlistedExtensionsOnly":ALLOWLIST_EXTENSIONS,"RecursiveScanAllowed":True,"MaxDepth":max_depth,"MaxFiles":max_files,"FileContentCopyAllowed":False,"AssemblyLoadAllowed":False,"RuntimeInvocationAllowed":False},"PreviewGateStatus":{"VisualPreviewGateRequired":True,"FunctionCompleteAllowed":False,"Stage":"alpha.discovery-index","NextRequiredStages":["native-icon-thumbnail-extraction","unity-to-neutral-preview-handoff","neutral-to-o3de-preview-conversion","asset-browser-pane","item-recipe-visual-selectors"]},"AssetRecords":rows,"Issues":[],"OperationalAuthority":{k:False for k in AUTHORITY_FALSE_KEYS}}
    clean(doc); return doc

def default_output(workspace:Path)->Path: return profile(workspace)["extracted"]/DEFAULT_INDEX_NAME
def write_index(doc:dict[str,Any],out:Path,replace=False):
    if out.exists() and not replace: raise DiscoveryError("output exists")
    out.parent.mkdir(parents=True,exist_ok=True); tmp=out.with_suffix(out.suffix+".tmp"); tmp.write_bytes(pretty_json(doc)); os.replace(tmp,out)
def verify_index(path:Path,workspace:Path|None=None)->dict[str,Any]:
    d=read_json(path)
    if d.get("DocumentKind")!=DOCUMENT_KIND or d.get("SchemaVersion")!=1: raise DiscoveryError("not visual index")
    utc(d["CapturedAt"]); auth=d["OperationalAuthority"]
    for k in AUTHORITY_FALSE_KEYS:
        if auth.get(k) is not False: raise DiscoveryError(f"authority escalation: {k}")
    if d["PreviewGateStatus"].get("FunctionCompleteAllowed") is not False: raise DiscoveryError("visual gate must block completion")
    ids=set(); refs=set()
    for r in d.get("AssetRecords",[]):
        if r["Extension"] not in ALLOWLIST_EXTENSIONS or not TOKEN.match(r["NativeAssetRef"]): raise DiscoveryError("invalid asset record")
        if r["AssetRecordId"] in ids or r["NativeAssetRef"] in refs: raise DiscoveryError("duplicate asset identity")
        ids.add(r["AssetRecordId"]); refs.add(r["NativeAssetRef"])
        for k in ("CatalogPromotionAllowed","RuntimePermissionGranted","PreviewProductGenerated"):
            if r.get(k) is not False: raise DiscoveryError(f"asset authority escalation: {k}")
    if workspace:
        p=profile(workspace)
        if (d["ProfileId"],d["GameVersion"],d["Branch"],d["RuntimeTarget"])!=(p["id"],p["game"],p["branch"],p["runtime"]): raise DiscoveryError("profile mismatch")
        if not inside(path,p["extracted"]): raise DiscoveryError("index outside ExtractedDataPath")
    clean(d); return d

def generate_fixture(out:Path,replace=False)->dict[str,Any]:
    if out.exists():
        if replace: shutil.rmtree(out)
        else: raise DiscoveryError("fixture output is not empty")
    aa=out/"game"/"FoA"/"Tainted Grail_Data"/"StreamingAssets"/"aa"; icons=out/"game"/"FoA"/"Tainted Grail_Data"/"LooseIcons"; ext=out/"workspace"/"Extracted"
    aa.mkdir(parents=True); icons.mkdir(parents=True); ext.mkdir(parents=True)
    (aa/"catalog.json").write_text("{}\n"); (aa/"items.bundle").write_bytes(b"bundle"); (icons/"iron.png").write_bytes(b"png")
    ws={"SchemaVersion":1,"WorkspaceId":"fixture.workspace","DisplayName":"Fixture","RootPath":"./workspace","OutputPath":"./workspace/Build","StagingPath":"./workspace/Staging","DeploymentPath":"./workspace/Deploy","ActiveGameProfileId":"foa.mono.fixture","GameProfiles":[{"ProfileId":"foa.mono.fixture","DisplayName":"Fixture","InstallPath":"./game/FoA","GameVersion":"1.23.401","Branch":"mono","RuntimeTarget":"Mono","UnityVersion":"6000.0.64f1","BepInExVersion":"5.4.23.3","ManagedAssembliesPath":"","PluginPath":"","DiagnosticsPath":"./workspace/Diagnostics","ExtractedDataPath":"./workspace/Extracted","DlcScopes":["base-game"]}]}
    wp=out/"workspace.tgworkspace.json"; wp.write_bytes(pretty_json(ws)); doc=build_index(wp,captured_at="2026-07-28T00:00:00Z"); ip=ext/DEFAULT_INDEX_NAME; write_index(doc,ip,True); return {"IndexId":doc["IndexId"],"AssetRecordCount":len(doc["AssetRecords"]),"OperationalAuthority":{k:False for k in AUTHORITY_FALSE_KEYS}}

def main(argv:Sequence[str]|None=None)->int:
    ap=argparse.ArgumentParser(); sub=ap.add_subparsers(dest="cmd",required=True)
    i=sub.add_parser("index"); i.add_argument("--workspace",required=True,type=Path); i.add_argument("--output",type=Path); i.add_argument("--captured-at"); i.add_argument("--replace",action="store_true"); i.add_argument("--max-files",type=int,default=4096); i.add_argument("--max-depth",type=int,default=8)
    v=sub.add_parser("verify"); v.add_argument("--input",required=True,type=Path); v.add_argument("--workspace",type=Path)
    f=sub.add_parser("fixture"); f.add_argument("--output",required=True,type=Path); f.add_argument("--replace",action="store_true")
    a=ap.parse_args(argv)
    try:
        if a.cmd=="fixture": m=generate_fixture(a.output,a.replace); print(f"FoA visual asset discovery fixture wrote {m['AssetRecordCount']} asset records.")
        elif a.cmd=="index": out=a.output or default_output(a.workspace); d=build_index(a.workspace,captured_at=a.captured_at,max_files=a.max_files,max_depth=a.max_depth); write_index(d,out,a.replace); print(f"FoA visual asset discovery index wrote {len(d['AssetRecords'])} records to {out}.")
        else: d=verify_index(a.input,a.workspace); print(f"FoA visual asset discovery index verified: {d['IndexId']} with {len(d['AssetRecords'])} records.")
    except DiscoveryError as e: print(f"FoA visual asset discovery index failed: {e}"); return 1
    return 0
if __name__=="__main__": raise SystemExit(main())
