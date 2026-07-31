#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Asset Browser pane model built from O3DE import-proof evidence only."""
from __future__ import annotations
import argparse, hashlib, json, shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

TOOL_ID='foa.asset-browser-pane-model'; TOOL_VERSION='0.1.0'
DOCUMENT_KIND='foa-asset-browser-pane-model'; PROOF_KIND='foa-o3de-asset-processor-import-proof'
DEFAULT_NAME='foa-asset-browser-pane-model.json'
FALSE_KEYS=('RuntimeInvocationAllowed','GameMutationAllowed','SaveAccessAllowed','CatalogPromotionAllowed','RuntimePermissionGranted','UnityInvoked','O3deAssetProcessorInvokedByThisTool','O3deAssetBrowserMutated','AssetBrowserEntryCreated','TypedAuthoringBindingCreated','DeploymentAllowed','RepositoryCommitAllowed','RedistributionAllowed','FunctionCompleteAllowed')

class AssetBrowserPaneError(RuntimeError): pass

def pretty(v:Any)->bytes: return (json.dumps(v,ensure_ascii=False,indent=2)+'\n').encode()
def canon(v:Any)->bytes: return (json.dumps(v,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
def sha(b:bytes)->str: return 'sha256:'+hashlib.sha256(b).hexdigest()
def read_json(p:Path)->dict[str,Any]:
    try: v=json.loads(p.read_text(encoding='utf-8',errors='strict'))
    except Exception as e: raise AssetBrowserPaneError(f'invalid JSON {p}: {e}') from e
    if not isinstance(v,dict): raise AssetBrowserPaneError('JSON document must be object')
    return v

def sid(v:Any,name:str)->str:
    chars='abcdefghijklmnopqrstuvwxyz0123456789._-'
    if not isinstance(v,str) or len(v)<2 or len(v)>191 or v[0] not in chars[:36] or any(c not in chars for c in v): raise AssetBrowserPaneError(f'{name} must be stable lowercase id')
    return v

def utc(v:Any,name:str)->str:
    if not isinstance(v,str): raise AssetBrowserPaneError(f'{name} must be UTC string')
    datetime.strptime(v,'%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc); return v

def no_private(v:Any,label='document')->None:
    if isinstance(v,str) and (v.startswith('/') or v.startswith('~/') or v.startswith('\\\\') or (len(v)>1 and v[1]==':')): raise AssetBrowserPaneError(f'{label} contains private path')
    if isinstance(v,list):
        for i,x in enumerate(v): no_private(x,f'{label}[{i}]')
    if isinstance(v,dict):
        for k,x in v.items(): no_private(x,f'{label}.{k}')

def resolve(raw:str,base:Path)->Path:
    p=Path(raw).expanduser(); return (base/p if not p.is_absolute() else p).resolve(False)
def inside(p:Path,r:Path)->bool:
    try: p.resolve(False).relative_to(r.resolve(False)); return True
    except ValueError: return False

def profile(ws:Path)->dict[str,Any]:
    w=read_json(ws); root=resolve(str(w.get('RootPath','')),ws.parent); active=sid(w.get('ActiveGameProfileId'),'ActiveGameProfileId')
    hits=[p for p in w.get('GameProfiles',[]) if isinstance(p,dict) and p.get('ProfileId')==active]
    if len(hits)!=1: raise AssetBrowserPaneError('active profile must resolve once')
    p=hits[0]; runtime=p.get('RuntimeTarget'); install=resolve(str(p.get('InstallPath','')),ws.parent); extracted=resolve(str(p.get('ExtractedDataPath','')),ws.parent)
    if runtime not in {'Mono','IL2CPP'}: raise AssetBrowserPaneError('RuntimeTarget must be Mono or IL2CPP')
    if not install.is_dir(): raise AssetBrowserPaneError('install path missing')
    if not inside(extracted,root): raise AssetBrowserPaneError('ExtractedDataPath outside workspace root')
    return {'ProfileId':sid(p.get('ProfileId'),'ProfileId'),'GameVersion':str(p.get('GameVersion','')),'Branch':str(p.get('Branch','')),'RuntimeTarget':runtime,'InstallPath':install,'ExtractedDataPath':extracted}

def bind(doc:Mapping[str,Any],p:Mapping[str,Any],label:str)->None:
    for k in ('ProfileId','GameVersion','Branch','RuntimeTarget'):
        if doc.get(k)!=p[k]: raise AssetBrowserPaneError(f'{label} profile mismatch')

def list_of(doc:Mapping[str,Any],key:str)->list[dict[str,Any]]: return [x for x in doc.get(key,[]) if isinstance(x,dict)]
def products(proof:Mapping[str,Any])->list[dict[str,Any]]:
    direct=list_of(proof,'ImportedProducts')
    if direct: return direct
    out=[]
    for e in list_of(proof,'O3dePreviewProductEvidence'):
        for p in e.get('ImportedProducts',[]) or []:
            if isinstance(p,dict): q=dict(p); q.setdefault('ProductEvidenceId',e.get('ProductEvidenceId','')); q.setdefault('O3dePreviewSourceId',e.get('O3dePreviewSourceId','')); out.append(q)
    return out

def failures(proof:Mapping[str,Any])->list[dict[str,Any]]:
    direct=list_of(proof,'ImportFailures')
    if direct: return direct
    out=[]
    for e in list_of(proof,'O3dePreviewProductEvidence'):
        for f in e.get('Failures',[]) or []:
            if isinstance(f,dict): q=dict(f); q.setdefault('ProductEvidenceId',e.get('ProductEvidenceId','')); q.setdefault('O3dePreviewSourceId',e.get('O3dePreviewSourceId','')); out.append(q)
    return out

def logs(proof:Mapping[str,Any])->list[dict[str,Any]]:
    for k in ('ImportLogs','ImportLogEvidence','LogEvidence'):
        v=list_of(proof,k)
        if v: return v
    return []

def validate_proof(proof:Mapping[str,Any],p:Mapping[str,Any]):
    if proof.get('SchemaVersion')!=1 or proof.get('DocumentKind')!=PROOF_KIND: raise AssetBrowserPaneError('must consume import-proof evidence, not raw conversion')
    bind(proof,p,'import proof')
    st=proof.get('PreviewStageStatus')
    if not isinstance(st,dict) or st.get('FunctionCompleteAllowed') is not False: raise AssetBrowserPaneError('proof must keep FunctionCompleteAllowed=false')
    if st.get('AssetBrowserEntryCreated') is not False or st.get('TypedAuthoringBindingCreated') is not False: raise AssetBrowserPaneError('proof must not claim browser entries or bindings')
    auth=proof.get('OperationalAuthority')
    if not isinstance(auth,dict): raise AssetBrowserPaneError('proof authority missing')
    for k,v in auth.items():
        if k!='O3deAssetProcessorInvocationObserved' and v is not False: raise AssetBrowserPaneError(f'proof authority escalation: {k}')
    ps,fs,ls=products(proof),failures(proof),logs(proof)
    if not ps and not fs: raise AssetBrowserPaneError('proof has no products or failures')
    for x in ps:
        sid(x.get('O3dePreviewSourceId'),'O3dePreviewSourceId'); sid(x.get('ProductEvidenceId'),'ProductEvidenceId'); sid(x.get('ProductAssetId') or x.get('ProductAssetIds',[''])[0],'ProductAssetId')
        cp=x.get('ProductCachePath') or x.get('ProductCachePaths',[''])[0]
        if not isinstance(cp,str) or not cp.startswith('$assetcache/'): raise AssetBrowserPaneError('product cache path must be $assetcache token')
    for x in fs: sid(x.get('O3dePreviewSourceId'),'Failure.O3dePreviewSourceId'); sid(x.get('FailureId'),'FailureId')
    for x in ls:
        if x.get('Path') and not str(x['Path']).startswith('$importproof/'): raise AssetBrowserPaneError('log path must be $importproof token')
    no_private(proof,'import proof'); return ps,fs,ls

def sel()->dict[str,bool]: return {'SelectableInPane':True,'CanCreateTypedAuthoringBinding':False,'RequiresExplicitBindingStep':True,'CatalogPromotionAllowed':False,'RuntimePermissionGranted':False,'RepositoryCommitAllowed':False,'RedistributionAllowed':False}
def pentry(mid:str,proof:Mapping[str,Any],x:Mapping[str,Any])->dict[str,Any]:
    pid=str(x.get('ProductAssetId') or x.get('ProductAssetIds',[''])[0]); cp=str(x.get('ProductCachePath') or x.get('ProductCachePaths',[''])[0]); eid=sid(x.get('ProductEvidenceId'),'ProductEvidenceId')
    return {'PaneEntryId':'assetbrowser.entry.'+hashlib.sha256(canon([mid,pid,cp,eid])).hexdigest()[:16],'EntryKind':'o3de-preview-product','DisplayName':x.get('DisplayName') or pid,'PreviewAvailability':'product-imported','O3dePreviewSourceId':x['O3dePreviewSourceId'],'ProductEvidenceId':eid,'ProductAssetIds':[pid],'ProductCachePaths':[cp],'ProductSha256':x.get('ProductSha256',''),'SourceImportProofId':proof.get('ImportProofId',''),'SourceConversionId':proof.get('SourceConversionId',''),'PrimarySourceAssetRecordId':x.get('PrimarySourceAssetRecordId',proof.get('PrimarySourceAssetRecordId','')),'SourceDependencies':x.get('SourceDependencies',[]),'EvidenceRefs':[eid],'IssueSeverity':'none','Issues':[],'SelectionPolicy':sel()}
def fentry(mid:str,proof:Mapping[str,Any],x:Mapping[str,Any])->dict[str,Any]:
    fid=sid(x.get('FailureId'),'FailureId')
    return {'PaneEntryId':'assetbrowser.entry.'+hashlib.sha256(canon([mid,fid,x.get('O3dePreviewSourceId')])).hexdigest()[:16],'EntryKind':'o3de-import-failure','DisplayName':x.get('DisplayName') or fid,'PreviewAvailability':'import-failed','O3dePreviewSourceId':x['O3dePreviewSourceId'],'ProductEvidenceId':x.get('ProductEvidenceId',''),'ProductAssetIds':[],'ProductCachePaths':[],'SourceImportProofId':proof.get('ImportProofId',''),'SourceConversionId':proof.get('SourceConversionId',''),'PrimarySourceAssetRecordId':x.get('PrimarySourceAssetRecordId',proof.get('PrimarySourceAssetRecordId','')),'SourceDependencies':x.get('SourceDependencies',[]),'EvidenceRefs':[fid],'IssueSeverity':'error','Issues':[{'Code':x.get('Code','o3de-import-failed'),'Message':x.get('Message','O3DE import failed for this preview source.')}],'SelectionPolicy':sel()}

def build_model(workspace:Path,import_proof:Path,*,output_root:Path|None=None,captured_at:str|None=None,replace:bool=False):
    p=profile(workspace); proof=read_json(import_proof); ps,fs,ls=validate_proof(proof,p)
    if not inside(import_proof.resolve(False),p['ExtractedDataPath']): raise AssetBrowserPaneError('import proof outside ExtractedDataPath')
    cap=utc(captured_at or datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),'CapturedAt'); mid='assetbrowser.model.'+p['ProfileId']+'.'+hashlib.sha256(canon([proof.get('ImportProofId'),ps,fs,cap])).hexdigest()[:16]
    root=(output_root.resolve(False) if output_root else (p['ExtractedDataPath']/'PreviewArtifacts'/'AssetBrowser'/mid).resolve(False))
    if not inside(root,p['ExtractedDataPath']): raise AssetBrowserPaneError('browser output outside ExtractedDataPath')
    if root.exists() and any(root.iterdir()):
        if replace: shutil.rmtree(root)
        else: raise AssetBrowserPaneError('browser output exists')
    root.mkdir(parents=True,exist_ok=True); entries=sorted([pentry(mid,proof,x) for x in ps]+[fentry(mid,proof,x) for x in fs],key=lambda e:e['PaneEntryId'])
    doc={'SchemaVersion':1,'DocumentKind':DOCUMENT_KIND,'AssetBrowserModelId':mid,'ProfileId':p['ProfileId'],'GameVersion':p['GameVersion'],'Branch':p['Branch'],'RuntimeTarget':p['RuntimeTarget'],'ToolId':TOOL_ID,'ToolVersion':TOOL_VERSION,'CapturedAt':cap,'PreviewIntent':'editor-preview-only','SourceImportProofId':proof.get('ImportProofId',''),'SourceConversionId':proof.get('SourceConversionId',''),'SourceHandoffId':proof.get('SourceHandoffId',''),'SourceIndexId':proof.get('SourceIndexId',''),'PrimarySourceAssetRecordId':proof.get('PrimarySourceAssetRecordId',''),'SourceDependencies':proof.get('SourceDependencies',[]),'InputContract':{'ImportProofEvidenceConsumed':True,'RawConversionFileConsumed':False,'RawO3dePreviewSourceConsumed':False},'PreviewStageStatus':{'ImportProofEvidenceConsumed':True,'AssetBrowserPaneModelEmitted':True,'AssetBrowserPaneEntriesEmitted':bool(entries),'O3deAssetBrowserEntryCreated':False,'TypedAuthoringBindingCreated':False,'FunctionCompleteAllowed':False,'NextRequiredStages':['asset-browser-pane-ui-rendering','3d-preview-viewport','item-recipe-visual-selectors']},'PaneModelRoot':'$assetbrowser','PaneEntries':entries,'ImportLogRefs':[{'LogEvidenceId':l.get('LogEvidenceId',''),'Path':l.get('Path',''),'Sha256':l.get('Sha256','')} for l in ls],'Issues':[],'OperationalAuthority':{k:False for k in FALSE_KEYS}}
    no_private(doc); out=root/DEFAULT_NAME; out.write_bytes(pretty(doc)); return doc,out

def verify_model(path:Path,*,workspace_path:Path|None=None,import_proof_path:Path|None=None)->dict[str,Any]:
    d=read_json(path)
    if d.get('SchemaVersion')!=1 or d.get('DocumentKind')!=DOCUMENT_KIND: raise AssetBrowserPaneError('not asset browser pane model')
    sid(d.get('AssetBrowserModelId'),'AssetBrowserModelId'); utc(d.get('CapturedAt'),'CapturedAt')
    c=d.get('InputContract'); st=d.get('PreviewStageStatus'); auth=d.get('OperationalAuthority')
    if not isinstance(c,dict) or c.get('ImportProofEvidenceConsumed') is not True or c.get('RawConversionFileConsumed') is not False or c.get('RawO3dePreviewSourceConsumed') is not False: raise AssetBrowserPaneError('model must consume import proof only')
    if not isinstance(st,dict) or st.get('FunctionCompleteAllowed') is not False or st.get('O3deAssetBrowserEntryCreated') is not False or st.get('TypedAuthoringBindingCreated') is not False: raise AssetBrowserPaneError('stage escalation')
    if not isinstance(auth,dict): raise AssetBrowserPaneError('authority missing')
    for k in FALSE_KEYS:
        if auth.get(k) is not False: raise AssetBrowserPaneError(f'authority escalation: {k}')
    seen=set(); entries=d.get('PaneEntries')
    if not isinstance(entries,list) or not entries: raise AssetBrowserPaneError('PaneEntries required')
    for e in entries:
        eid=sid(e.get('PaneEntryId'),'PaneEntryId')
        if eid in seen: raise AssetBrowserPaneError('duplicate PaneEntryId')
        seen.add(eid); pol=e.get('SelectionPolicy')
        if not isinstance(pol,dict): raise AssetBrowserPaneError('SelectionPolicy required')
        for k in ('CanCreateTypedAuthoringBinding','CatalogPromotionAllowed','RuntimePermissionGranted','RepositoryCommitAllowed','RedistributionAllowed'):
            if pol.get(k) is not False: raise AssetBrowserPaneError(f'pane entry authority escalation: {k}')
        if pol.get('RequiresExplicitBindingStep') is not True: raise AssetBrowserPaneError('explicit binding step required')
        if e.get('EntryKind')=='o3de-preview-product' and not all(str(x).startswith('$assetcache/') for x in e.get('ProductCachePaths',[])): raise AssetBrowserPaneError('bad ProductCachePath token')
    if workspace_path:
        p=profile(workspace_path); bind(d,p,'model')
        if not inside(path.resolve(False),p['ExtractedDataPath']): raise AssetBrowserPaneError('model outside ExtractedDataPath')
    if import_proof_path and d.get('SourceImportProofId')!=read_json(import_proof_path).get('ImportProofId'): raise AssetBrowserPaneError('SourceImportProofId mismatch')
    no_private(d); return d

def write_workspace(root:Path)->Path:
    (root/'game'/'FoA').mkdir(parents=True,exist_ok=True); (root/'workspace'/'Extracted').mkdir(parents=True,exist_ok=True)
    ws={'SchemaVersion':1,'WorkspaceId':'fixture.workspace','DisplayName':'Fixture','RootPath':'./workspace','OutputPath':'./workspace/Build','StagingPath':'./workspace/Staging','DeploymentPath':'./workspace/Deploy','ActiveGameProfileId':'foa.mono.fixture','GameProfiles':[{'ProfileId':'foa.mono.fixture','DisplayName':'Fixture','InstallPath':'./game/FoA','GameVersion':'1.23.401','Branch':'mono','RuntimeTarget':'Mono','UnityVersion':'6000.0.64f1','BepInExVersion':'5.4.23.3','ManagedAssembliesPath':'','PluginPath':'','DiagnosticsPath':'./workspace/Diagnostics','ExtractedDataPath':'./workspace/Extracted','DlcScopes':['base-game']}]}
    path=root/'workspace.tgworkspace.json'; path.write_bytes(pretty(ws)); return path

def write_proof(root:Path)->Path:
    pr=root/'workspace'/'Extracted'/'PreviewArtifacts'/'O3DE'/'o3de.preview.foa.mono.fixture.synthetic'/'ImportProofs'/'proof.fixture'; (pr/'logs').mkdir(parents=True,exist_ok=True); log=b'asset processor log\n'; (pr/'logs'/'asset_processor.log').write_bytes(log)
    proof={'SchemaVersion':1,'DocumentKind':PROOF_KIND,'ImportProofId':'o3de.importproof.foa.mono.fixture.synthetic','ProfileId':'foa.mono.fixture','GameVersion':'1.23.401','Branch':'mono','RuntimeTarget':'Mono','CapturedAt':'2026-07-28T00:00:00Z','SourceConversionId':'o3de.preview.foa.mono.fixture.synthetic','SourceHandoffId':'preview.handoff.foa.mono.fixture.synthetic','SourceIndexId':'visual.index.foa.mono.fixture.synthetic','PrimarySourceAssetRecordId':'visual.asset.foa.mono.fixture.synthetic','SourceDependencies':[{'SourceAssetRecordId':'visual.asset.foa.mono.fixture.synthetic','NativeAssetRef':'$install/Tainted Grail_Data/LooseIcons/iron.png','DependencyRole':'primary'}],'PreviewStageStatus':{'O3deAssetProcessorInvocationObserved':True,'GeneratedO3dePreviewProduct':True,'AssetBrowserEntryCreated':False,'TypedAuthoringBindingCreated':False,'FunctionCompleteAllowed':False},'ImportedProducts':[{'ProductEvidenceId':'o3de.product-evidence.fixture','O3dePreviewSourceId':'o3de.source.fixture','PrimarySourceAssetRecordId':'visual.asset.foa.mono.fixture.synthetic','ProductAssetId':'o3de.product.fixture.texture','ProductCachePath':'$assetcache/pc/textures/iron.dds.streamingimage','ProductSha256':sha(b'product'),'DisplayName':'iron.png'}],'ImportFailures':[{'FailureId':'o3de.import-failure.fixture','O3dePreviewSourceId':'o3de.source.failed','PrimarySourceAssetRecordId':'visual.asset.foa.mono.fixture.failed','Code':'ap-build-failed','Message':'Synthetic import failure.'}],'ImportLogs':[{'LogEvidenceId':'o3de.import-log.fixture','Path':'$importproof/logs/asset_processor.log','Sha256':sha(log),'ByteSize':len(log)}],'OperationalAuthority':{'RuntimeInvocationAllowed':False,'GameMutationAllowed':False,'SaveAccessAllowed':False,'CatalogPromotionAllowed':False,'RuntimePermissionGranted':False,'UnityInvoked':False,'AssetBrowserEntryCreated':False,'TypedAuthoringBindingCreated':False,'DeploymentAllowed':False,'RepositoryCommitAllowed':False,'RedistributionAllowed':False,'FunctionCompleteAllowed':False,'AssetProcessorInvocationPerformedByThisTool':False}}
    path=pr/'foa-o3de-asset-processor-import-proof.json'; path.write_bytes(pretty(proof)); return path

def generate_fixture(out:Path,*,replace:bool=False)->dict[str,Any]:
    if out.exists():
        if replace: shutil.rmtree(out) if out.is_dir() else out.unlink()
        else: raise AssetBrowserPaneError('fixture output exists')
    ws=write_workspace(out); proof=write_proof(out); model,path=build_model(ws,proof,captured_at='2026-07-28T00:00:00Z'); verify_model(path,workspace_path=ws,import_proof_path=proof); return {'AssetBrowserModelId':model['AssetBrowserModelId'],'PaneEntryCount':len(model['PaneEntries']),'ModelPath':str(path)}

def main(argv:Sequence[str]|None=None)->int:
    ap=argparse.ArgumentParser(); sub=ap.add_subparsers(dest='cmd',required=True)
    b=sub.add_parser('build'); b.add_argument('--workspace',type=Path,required=True); b.add_argument('--import-proof',type=Path,required=True); b.add_argument('--output-root',type=Path); b.add_argument('--captured-at'); b.add_argument('--replace',action='store_true')
    v=sub.add_parser('verify'); v.add_argument('--input',type=Path,required=True); v.add_argument('--workspace',type=Path); v.add_argument('--import-proof',type=Path)
    f=sub.add_parser('fixture'); f.add_argument('--output',type=Path,required=True); f.add_argument('--replace',action='store_true')
    a=ap.parse_args(argv)
    try:
        if a.cmd=='fixture': r=generate_fixture(a.output,replace=a.replace); print(f"FoA Asset Browser pane fixture wrote {r['PaneEntryCount']} entries to {r['ModelPath']}.")
        elif a.cmd=='build': m,p=build_model(a.workspace,a.import_proof,output_root=a.output_root,captured_at=a.captured_at,replace=a.replace); print(f"FoA Asset Browser pane model wrote {len(m['PaneEntries'])} entries to {p}.")
        else: m=verify_model(a.input,workspace_path=a.workspace,import_proof_path=a.import_proof); print(f"FoA Asset Browser pane model verified: {m['AssetBrowserModelId']} with {len(m['PaneEntries'])} entries.")
    except AssetBrowserPaneError as e: print(f'FoA Asset Browser pane model failed: {e}'); return 1
    return 0
if __name__=='__main__': raise SystemExit(main())
