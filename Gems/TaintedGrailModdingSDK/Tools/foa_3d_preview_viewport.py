#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

DOCUMENT_KIND='foa-3d-preview-viewport-render'; UI_KIND='foa-asset-browser-pane-ui-render'; MODEL_KIND='foa-asset-browser-pane-model'
FALSE_KEYS=('RuntimeInvocationAllowed','GameMutationAllowed','SaveAccessAllowed','CatalogPromotionAllowed','RuntimePermissionGranted','UnityInvoked','O3deAssetProcessorInvoked','O3deEditorViewportMutated','O3deEditorPaneMutated','O3deAssetBrowserMutated','AssetBrowserEntryCreated','TypedAuthoringBindingCreated','TypedSelectorCreated','ItemRecipeBindingCreated','DeploymentAllowed','RepositoryCommitAllowed','RedistributionAllowed','FunctionCompleteAllowed')
ID=re.compile(r'^[a-z0-9][a-z0-9._-]{1,191}$'); UTC=re.compile(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$'); ASSET=re.compile(r'^\$assetcache(/[^\\\r\n]*)?$'); VIEW=re.compile(r'^\$viewport(/[^\\\r\n]*)?$')
class PreviewViewportError(RuntimeError): pass

def pretty(v:Any)->bytes: return (json.dumps(v,ensure_ascii=False,indent=2)+'\n').encode()
def canon(v:Any)->bytes: return (json.dumps(v,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
def sha(b:bytes)->str: return 'sha256:'+hashlib.sha256(b).hexdigest()
def read_json(p:Path)->dict[str,Any]:
    v=json.loads(p.read_text(encoding='utf-8',errors='strict'))
    if not isinstance(v,dict): raise PreviewViewportError('JSON document must be an object')
    return v
def sid(v:Any,n:str)->str:
    if not isinstance(v,str) or not ID.match(v): raise PreviewViewportError(f'{n} must be stable lowercase id')
    return v
def utc(v:Any,n:str)->str:
    if not isinstance(v,str) or not UTC.match(v): raise PreviewViewportError(f'{n} must be whole-second UTC')
    datetime.strptime(v,'%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc); return v
def res(raw:str,base:Path)->Path:
    p=Path(raw).expanduser(); return (base/p if not p.is_absolute() else p).resolve(False)
def inside(p:Path,r:Path)->bool:
    try: p.resolve(False).relative_to(r.resolve(False)); return True
    except ValueError: return False

def profile(ws:Path)->dict[str,Any]:
    w=read_json(ws); root=res(str(w.get('RootPath','')),ws.parent); active=sid(w.get('ActiveGameProfileId'),'ActiveGameProfileId')
    hit=[p for p in w.get('GameProfiles',[]) if isinstance(p,dict) and p.get('ProfileId')==active]
    if len(hit)!=1: raise PreviewViewportError('active profile must resolve once')
    p=hit[0]; install=res(str(p.get('InstallPath','')),ws.parent); extracted=res(str(p.get('ExtractedDataPath','')),ws.parent)
    if p.get('RuntimeTarget') not in {'Mono','IL2CPP'} or not install.is_dir() or not inside(extracted,root): raise PreviewViewportError('bad workspace profile')
    return {'ProfileId':sid(p.get('ProfileId'),'ProfileId'),'GameVersion':str(p.get('GameVersion','')),'Branch':str(p.get('Branch','')),'RuntimeTarget':p['RuntimeTarget'],'ExtractedDataPath':extracted}
def bind(d:Mapping[str,Any],p:Mapping[str,Any],name:str)->None:
    for k in ('ProfileId','GameVersion','Branch','RuntimeTarget'):
        if d.get(k)!=p[k]: raise PreviewViewportError(f'{name} profile mismatch')
def auth_false(d:Mapping[str,Any],name:str)->None:
    a=d.get('OperationalAuthority')
    if not isinstance(a,dict): raise PreviewViewportError(f'{name} authority missing')
    for k,v in a.items():
        if v is not False: raise PreviewViewportError(f'{name} authority escalation: {k}')
def pol(e:Mapping[str,Any])->None:
    p=e.get('SelectionPolicy')
    if isinstance(p,dict):
        if p.get('CanCreateTypedAuthoringBinding') is not False or p.get('RequiresExplicitBindingStep') is not True: raise PreviewViewportError('selection binding escalation')

def build_viewport(workspace:Path, ui_render:Path, pane_model:Path, *, output_root:Path|None=None, captured_at:str|None=None, replace:bool=False):
    prof=profile(workspace); ui=read_json(ui_render); model=read_json(pane_model)
    if ui.get('DocumentKind')!=UI_KIND or model.get('DocumentKind')!=MODEL_KIND: raise PreviewViewportError('must consume UI render and pane model only')
    bind(ui,prof,'ui'); bind(model,prof,'model'); auth_false(ui,'ui'); auth_false(model,'model')
    if ui.get('SourceAssetBrowserModelId')!=model.get('AssetBrowserModelId'): raise PreviewViewportError('UI render/model mismatch')
    if not inside(ui_render.resolve(False),prof['ExtractedDataPath']) or not inside(pane_model.resolve(False),prof['ExtractedDataPath']): raise PreviewViewportError('inputs must stay inside ExtractedDataPath')
    st=ui.get('PreviewStageStatus',{})
    if st.get('BoundedPaneUiRendered') is not True: raise PreviewViewportError('UI render stage is incomplete')
    for k in ('FunctionCompleteAllowed','TypedAuthoringBindingCreated','TypedSelectorCreated','O3deEditorPaneMutated','O3deAssetBrowserMutated','AssetBrowserEntryCreated'):
        if st.get(k) is not False: raise PreviewViewportError(f'UI stage escalation: {k}')
    ui_render_id=sid(ui.get('AssetBrowserUiRenderId',ui.get('RenderId')),'AssetBrowserUiRenderId')
    rows=ui.get('UiEntries') if isinstance(ui.get('UiEntries'),list) else ui.get('RenderedEntries') if isinstance(ui.get('RenderedEntries'),list) else []
    rowmap={sid(r.get('SourcePaneEntryId'),'SourcePaneEntryId'):r for r in rows if isinstance(r,dict)}
    entries=[]; captured=utc(captured_at or datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),'CapturedAt')
    rid='viewport.render.'+prof['ProfileId']+'.'+hashlib.sha256(canon([ui_render_id,model.get('AssetBrowserModelId'),captured])).hexdigest()[:16]
    for e in model.get('PaneEntries',[]):
        if not isinstance(e,dict): continue
        eid=sid(e.get('PaneEntryId'),'PaneEntryId'); pol(e)
        r=rowmap.get(eid)
        if r is None: raise PreviewViewportError('every pane entry needs UI evidence')
        if r.get('CanCreateTypedAuthoringBinding') is not False or r.get('RequiresExplicitBindingStep') is not True: raise PreviewViewportError('UI row binding escalation')
        paths=list(e.get('ProductCachePaths',r.get('ProductCachePaths',[])) or [])
        for t in paths:
            if not isinstance(t,str) or not ASSET.match(t): raise PreviewViewportError('bad product cache token')
        state='product-reference-available' if e.get('EntryKind')=='o3de-preview-product' and paths else 'no-product'
        entries.append({'ViewportEntryId':'viewport.entry.'+hashlib.sha256(canon([rid,eid,state])).hexdigest()[:16],'SourcePaneEntryId':eid,'DisplayName':str(e.get('DisplayName',r.get('DisplayName','Unnamed'))),'ViewportState':state,'ProductAssetIds':list(e.get('ProductAssetIds',r.get('ProductAssetIds',[])) or []),'ProductCachePaths':paths,'ProductEvidenceIds':list(e.get('EvidenceRefs',r.get('EvidenceRefs',[])) or []),'PreviewRenderVerified':False,'O3deViewportMutated':False,'CanCreateTypedAuthoringBinding':False,'RequiresExplicitBindingStep':True,'CatalogPromotionAllowed':False,'RuntimePermissionGranted':False,'Issues':list(e.get('Issues',r.get('Issues',[])) or [])})
    if not entries: raise PreviewViewportError('no viewport entries')
    root=(output_root.resolve(False) if output_root else (prof['ExtractedDataPath']/'PreviewArtifacts'/'Viewport3D'/rid).resolve(False))
    if not inside(root,prof['ExtractedDataPath']): raise PreviewViewportError('viewport output outside ExtractedDataPath')
    if root.exists() and any(root.iterdir()):
        if replace: shutil.rmtree(root)
        else: raise PreviewViewportError('viewport output exists')
    root.mkdir(parents=True,exist_ok=True); data=root/'viewport'/'viewport-data.json'; page=root/'viewport'/'viewport.html'; data.parent.mkdir(parents=True,exist_ok=True)
    data_bytes=pretty({'SchemaVersion':1,'DocumentKind':'foa-3d-preview-viewport-data','ViewportRenderId':rid,'Entries':entries}); html_bytes=('<html><body><h1>FOA 3D Preview Viewport</h1><p>static local render only</p></body></html>\n').encode()
    data.write_bytes(data_bytes); page.write_bytes(html_bytes)
    doc={'SchemaVersion':1,'DocumentKind':DOCUMENT_KIND,'ViewportRenderId':rid,'ProfileId':prof['ProfileId'],'GameVersion':prof['GameVersion'],'Branch':prof['Branch'],'RuntimeTarget':prof['RuntimeTarget'],'ToolId':'foa.3d-preview-viewport','ToolVersion':'0.1.0','CapturedAt':captured,'PreviewIntent':'editor-preview-only','SourceAssetBrowserUiRenderId':ui_render_id,'SourceAssetBrowserModelId':model.get('AssetBrowserModelId',''),'InputContract':{'AssetBrowserPaneUiRenderConsumed':True,'AssetBrowserPaneModelConsumed':True,'ProductEvidenceRefsConsumed':True,'RawImportProofConsumed':False,'RawConversionFileConsumed':False,'RawO3dePreviewSourceConsumed':False},'PreviewStageStatus':{'AssetBrowserPaneUiRenderConsumed':True,'ViewportModelEmitted':True,'ViewportStaticRenderEmitted':True,'LiveO3deViewportCreated':False,'O3deEditorViewportMutated':False,'TypedAuthoringBindingCreated':False,'TypedSelectorCreated':False,'FunctionCompleteAllowed':False,'NextRequiredStages':['3d-preview-viewport-live-proof','item-recipe-visual-selectors']},'ViewportArtifacts':[{'ArtifactId':'viewport.artifact.data','Role':'viewport-data','Path':'$viewport/viewport/viewport-data.json','Sha256':sha(data_bytes),'ByteSize':len(data_bytes)},{'ArtifactId':'viewport.artifact.html','Role':'viewport-html','Path':'$viewport/viewport/viewport.html','Sha256':sha(html_bytes),'ByteSize':len(html_bytes)}],'ViewportEntries':entries,'Issues':[],'OperationalAuthority':{k:False for k in FALSE_KEYS}}
    out=root/'foa-3d-preview-viewport-render.json'; out.write_bytes(pretty(doc)); return doc,out

def token_path(t:str,root:Path)->Path:
    if not isinstance(t,str) or not VIEW.match(t): raise PreviewViewportError('artifact path must use $viewport')
    p=(root/t[len('$viewport'):].lstrip('/')).resolve(False)
    if not inside(p,root): raise PreviewViewportError('artifact escaped root')
    return p
def verify_viewport(path:Path, *, workspace_path:Path|None=None, ui_render_path:Path|None=None, pane_model_path:Path|None=None)->dict[str,Any]:
    d=read_json(path)
    if d.get('DocumentKind')!=DOCUMENT_KIND: raise PreviewViewportError('not viewport render')
    sid(d.get('ViewportRenderId'),'ViewportRenderId'); utc(d.get('CapturedAt'),'CapturedAt'); auth_false(d,'viewport')
    c=d.get('InputContract',{}); st=d.get('PreviewStageStatus',{})
    if c.get('AssetBrowserPaneUiRenderConsumed') is not True or c.get('AssetBrowserPaneModelConsumed') is not True or c.get('ProductEvidenceRefsConsumed') is not True: raise PreviewViewportError('input contract incomplete')
    for k in ('RawImportProofConsumed','RawConversionFileConsumed','RawO3dePreviewSourceConsumed'):
        if c.get(k) is not False: raise PreviewViewportError('raw input consumed')
    for k in ('LiveO3deViewportCreated','O3deEditorViewportMutated','TypedAuthoringBindingCreated','TypedSelectorCreated','FunctionCompleteAllowed'):
        if st.get(k) is not False: raise PreviewViewportError(f'stage escalation: {k}')
    root=path.parent.resolve(False)
    for a in d.get('ViewportArtifacts',[]):
        p=token_path(a.get('Path',''),root); b=p.read_bytes()
        if a.get('ByteSize')!=len(b) or a.get('Sha256')!=sha(b): raise PreviewViewportError('artifact payload mismatch')
    for e in d.get('ViewportEntries',[]):
        sid(e.get('ViewportEntryId'),'ViewportEntryId')
        if e.get('CanCreateTypedAuthoringBinding') is not False or e.get('RequiresExplicitBindingStep') is not True or e.get('RuntimePermissionGranted') is not False: raise PreviewViewportError('entry authority escalation')
        for t in e.get('ProductCachePaths',[]) or []:
            if not isinstance(t,str) or not ASSET.match(t): raise PreviewViewportError('bad product cache token')
    if workspace_path: bind(d,profile(workspace_path),'viewport')
    if ui_render_path:
        ui=read_json(ui_render_path)
        if d.get('SourceAssetBrowserUiRenderId')!=ui.get('AssetBrowserUiRenderId',ui.get('RenderId')): raise PreviewViewportError('UI render mismatch')
    if pane_model_path and d.get('SourceAssetBrowserModelId')!=read_json(pane_model_path).get('AssetBrowserModelId'): raise PreviewViewportError('pane model mismatch')
    return d

def write_workspace(root:Path)->Path:
    (root/'game'/'FoA').mkdir(parents=True,exist_ok=True); (root/'workspace'/'Extracted').mkdir(parents=True,exist_ok=True)
    ws={'SchemaVersion':1,'RootPath':'./workspace','ActiveGameProfileId':'foa.mono.fixture','GameProfiles':[{'ProfileId':'foa.mono.fixture','InstallPath':'./game/FoA','GameVersion':'1.23.401','Branch':'mono','RuntimeTarget':'Mono','ExtractedDataPath':'./workspace/Extracted'}]}
    p=root/'workspace.tgworkspace.json'; p.write_bytes(pretty(ws)); return p
def fixture_inputs(root:Path)->tuple[Path,Path]:
    base=root/'workspace'/'Extracted'/'PreviewArtifacts'; mroot=base/'AssetBrowser'/'assetbrowser.model.foa.mono.fixture.synthetic'; uroot=base/'AssetBrowserUI'/'assetbrowser.render.foa.mono.fixture.synthetic'; mroot.mkdir(parents=True); uroot.mkdir(parents=True)
    pol={'CanCreateTypedAuthoringBinding':False,'RequiresExplicitBindingStep':True,'CatalogPromotionAllowed':False,'RuntimePermissionGranted':False,'RepositoryCommitAllowed':False,'RedistributionAllowed':False}
    prod={'PaneEntryId':'assetbrowser.entry.fixture.product','EntryKind':'o3de-preview-product','DisplayName':'iron.png','PreviewAvailability':'product-imported','ProductAssetIds':['o3de.product.fixture.texture'],'ProductCachePaths':['$assetcache/pc/textures/iron.dds.streamingimage'],'EvidenceRefs':['o3de.product-evidence.fixture'],'SelectionPolicy':pol}
    fail={'PaneEntryId':'assetbrowser.entry.fixture.failed','EntryKind':'o3de-import-failure','DisplayName':'failed.dds','PreviewAvailability':'import-failed','ProductAssetIds':[],'ProductCachePaths':[],'EvidenceRefs':['o3de.import-failure.fixture'],'Issues':[{'Code':'ap-build-failed','Message':'Synthetic import failure.'}],'SelectionPolicy':pol}
    auth={k:False for k in FALSE_KEYS if k!='O3deEditorViewportMutated' and k!='ItemRecipeBindingCreated'}
    model={'SchemaVersion':1,'DocumentKind':MODEL_KIND,'AssetBrowserModelId':'assetbrowser.model.foa.mono.fixture.synthetic','ProfileId':'foa.mono.fixture','GameVersion':'1.23.401','Branch':'mono','RuntimeTarget':'Mono','InputContract':{'ImportProofEvidenceConsumed':True,'RawConversionFileConsumed':False,'RawO3dePreviewSourceConsumed':False},'PreviewStageStatus':{'AssetBrowserPaneModelEmitted':True,'TypedAuthoringBindingCreated':False,'FunctionCompleteAllowed':False},'PaneEntries':[prod,fail],'OperationalAuthority':auth}
    mp=mroot/'foa-asset-browser-pane-model.json'; mp.write_bytes(pretty(model))
    rows=[{'UiEntryId':'assetbrowser.ui-entry.fixture.product','SourcePaneEntryId':prod['PaneEntryId'],'ProductAssetIds':prod['ProductAssetIds'],'ProductCachePaths':prod['ProductCachePaths'],'EvidenceRefs':prod['EvidenceRefs'],'CanCreateTypedAuthoringBinding':False,'RequiresExplicitBindingStep':True},{'UiEntryId':'assetbrowser.ui-entry.fixture.failed','SourcePaneEntryId':fail['PaneEntryId'],'ProductAssetIds':[],'ProductCachePaths':[],'EvidenceRefs':fail['EvidenceRefs'],'CanCreateTypedAuthoringBinding':False,'RequiresExplicitBindingStep':True,'Issues':fail['Issues']}]
    ui={'SchemaVersion':1,'DocumentKind':UI_KIND,'AssetBrowserUiRenderId':'assetbrowser.render.foa.mono.fixture.synthetic','ProfileId':'foa.mono.fixture','GameVersion':'1.23.401','Branch':'mono','RuntimeTarget':'Mono','SourceAssetBrowserModelId':model['AssetBrowserModelId'],'InputContract':{'AssetBrowserPaneModelConsumed':True,'ImportProofConsumedDirectly':False,'RawConversionFileConsumed':False,'RawO3dePreviewSourceConsumed':False},'PreviewStageStatus':{'AssetBrowserPaneModelConsumed':True,'BoundedPaneUiRendered':True,'O3deEditorPaneMutated':False,'O3deAssetBrowserMutated':False,'AssetBrowserEntryCreated':False,'TypedAuthoringBindingCreated':False,'TypedSelectorCreated':False,'FunctionCompleteAllowed':False},'UiEntries':rows,'OperationalAuthority':auth}
    up=uroot/'foa-asset-browser-pane-ui-render.json'; up.write_bytes(pretty(ui)); return up,mp
def generate_fixture(output:Path, *, replace:bool=False)->dict[str,Any]:
    if output.exists():
        if replace: shutil.rmtree(output) if output.is_dir() else output.unlink()
        else: raise PreviewViewportError('fixture output exists')
    ws=write_workspace(output); ui,model=fixture_inputs(output); doc,path=build_viewport(ws,ui,model,captured_at='2026-07-28T00:00:00Z'); verify_viewport(path,workspace_path=ws,ui_render_path=ui,pane_model_path=model); return {'ViewportRenderId':doc['ViewportRenderId'],'ViewportEntryCount':len(doc['ViewportEntries']),'RenderPath':str(path)}
def main(argv:Sequence[str]|None=None)->int:
    ap=argparse.ArgumentParser(); sub=ap.add_subparsers(dest='cmd',required=True)
    r=sub.add_parser('render'); r.add_argument('--workspace',type=Path,required=True); r.add_argument('--ui-render',type=Path,required=True); r.add_argument('--pane-model',type=Path,required=True); r.add_argument('--output-root',type=Path); r.add_argument('--captured-at'); r.add_argument('--replace',action='store_true')
    v=sub.add_parser('verify'); v.add_argument('--input',type=Path,required=True); v.add_argument('--workspace',type=Path); v.add_argument('--ui-render',type=Path); v.add_argument('--pane-model',type=Path)
    f=sub.add_parser('fixture'); f.add_argument('--output',type=Path,required=True); f.add_argument('--replace',action='store_true')
    a=ap.parse_args(argv)
    try:
        if a.cmd=='fixture': res=generate_fixture(a.output,replace=a.replace); print(f"FoA 3D preview viewport fixture wrote {res['ViewportEntryCount']} entries to {res['RenderPath']}.")
        elif a.cmd=='render': doc,path=build_viewport(a.workspace,a.ui_render,a.pane_model,output_root=a.output_root,captured_at=a.captured_at,replace=a.replace); print(f"FoA 3D preview viewport render wrote {len(doc['ViewportEntries'])} entries to {path}.")
        else: doc=verify_viewport(a.input,workspace_path=a.workspace,ui_render_path=a.ui_render,pane_model_path=a.pane_model); print(f"FoA 3D preview viewport render verified: {doc['ViewportRenderId']} with {len(doc['ViewportEntries'])} entries.")
    except PreviewViewportError as e:
        print(f'FoA 3D preview viewport render failed: {e}'); return 1
    return 0
if __name__=='__main__': raise SystemExit(main())
