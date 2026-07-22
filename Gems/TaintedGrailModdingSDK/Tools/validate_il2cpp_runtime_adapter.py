#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import importlib.util, json, sys
from pathlib import Path
REPO_ROOT=Path(__file__).resolve().parents[3]
PACKAGE_REL=Path('Plugins/RuntimeAdapters/IL2CPP'); PACKAGE=REPO_ROOT/PACKAGE_REL
BRIDGE=Path('Gems/TaintedGrailModdingSDK/Tools/tests/test_il2cpp_runtime_adapter.py')
VALIDATOR_TEST=Path('Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_il2cpp_runtime_adapter.py')
ROUTES=Path('Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp')
DOC=Path('docs/tainted-grail-sdk/IL2CPP_RUNTIME_ADAPTER.md'); RUNTIME_DOC=Path('docs/tainted-grail-modding/runtime/README.md'); INDEX=Path('Plugins/RuntimeAdapters/README.md')
REQUIRED={'README.md','Schemas/execution-gate.schema.json','Schemas/interop-manifest.schema.json','Schemas/runtime-result.schema.json','Source/FOA.SDK.RuntimeAdapter.IL2CPP.csproj','Source/Il2CppRuntimeAdapterPlugin.cs','Tests/Fixtures/build-plan.json','Tests/Fixtures/execution-gate.json','Tests/Fixtures/interop-manifest.json','Tests/Fixtures/not-attempted-result.json','Tests/test_il2cpp_runtime_adapter.py','Tools/_il2cpp_contract.py','Tools/_il2cpp_gate.py','Tools/_il2cpp_result.py','Tools/il2cpp_runtime_adapter.py','il2cpp-adapter.json','plugin.json'}
FORBIDDEN_SUFFIXES={'.dll','.exe','.pdb','.so','.dylib','.zip','.7z','.nupkg'}
class Il2CppValidationError(RuntimeError): pass

def text(path: Path)->str:
    try: return (REPO_ROOT/path).read_text(encoding='utf-8',errors='strict')
    except (OSError,UnicodeDecodeError) as exc: raise Il2CppValidationError(f'Unable to read {path.as_posix()}.') from exc

def load_module():
    tools=PACKAGE/'Tools'
    for name in ('_il2cpp_contract','_il2cpp_gate','_il2cpp_result','foa_il2cpp_validation_target'): sys.modules.pop(name,None)
    sys.path.insert(0,str(tools)); spec=importlib.util.spec_from_file_location('foa_il2cpp_validation_target',tools/'_il2cpp_contract.py')
    if spec is None or spec.loader is None: raise Il2CppValidationError('Unable to load IL2CPP contract.')
    module=importlib.util.module_from_spec(spec); sys.modules[spec.name]=module; spec.loader.exec_module(module); return module

def validate(root: Path|None=None)->None:
    global REPO_ROOT,PACKAGE
    if root is not None: REPO_ROOT=Path(root); PACKAGE=REPO_ROOT/PACKAGE_REL
    actual={p.relative_to(PACKAGE).as_posix() for p in PACKAGE.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc'}
    if actual!=REQUIRED: raise Il2CppValidationError(f'IL2CPP package file set drifted; missing={sorted(REQUIRED-actual)}; extra={sorted(actual-REQUIRED)}')
    for p in PACKAGE.rglob('*'):
        if p.is_symlink(): raise Il2CppValidationError('IL2CPP package contains a symlink.')
        if p.is_file() and p.suffix.lower() in FORBIDDEN_SUFFIXES: raise Il2CppValidationError('Generated IL2CPP binary entered the repository.')
    plugin=json.loads((PACKAGE/'plugin.json').read_text())
    if plugin.get('id')!='extension.foa-il2cpp-runtime-adapter' or plugin.get('compatibility')!={'branches':['il2cpp'],'game_versions':['1.23.401'],'runtime_targets':['il2cpp']}: raise Il2CppValidationError('IL2CPP plugin compatibility drifted or became combined.')
    try:
        module=load_module(); module.validate_package(PACKAGE); interop=module.load_json(PACKAGE/'Tests/Fixtures/interop-manifest.json'); module.validate_interop_manifest(interop)
        if module.build_plan(interop,PACKAGE)!=module.load_json(PACKAGE/'Tests/Fixtures/build-plan.json'): raise Il2CppValidationError('IL2CPP golden build plan drifted.')
    except Il2CppValidationError: raise
    except Exception as exc: raise Il2CppValidationError(f'IL2CPP package contract validation failed: {exc}') from exc
    project=(PACKAGE/'Source/FOA.SDK.RuntimeAdapter.IL2CPP.csproj').read_text(); source=(PACKAGE/'Source/Il2CppRuntimeAdapterPlugin.cs').read_text(); tools='\n'.join((PACKAGE/'Tools'/n).read_text() for n in ['_il2cpp_contract.py','_il2cpp_gate.py','_il2cpp_result.py','il2cpp_runtime_adapter.py'])
    required_project=('net6.0','BepInEx.Core.dll','BepInEx.Unity.IL2CPP.dll','Il2CppInterop.Runtime.dll','$(FoAInteropDir)\\Assembly-CSharp.dll','$(FoAInteropDir)\\TG.Main.dll')
    if any(token not in project for token in required_project): raise Il2CppValidationError('IL2CPP project lost an exact dependency or generated interop input.')
    forbidden=('netstandard2.1','BaseUnityPlugin','BepInEx.dll','Fall of Avalon_Data\\Managed','adapter.foa.mono','runtime_targets":["mono"]')
    if any(token in project+source+tools for token in forbidden): raise Il2CppValidationError('Mono or BepInEx 5 material contaminated the IL2CPP package.')
    if 'BasePlugin' not in source or 'foa-sdk.il2cpp-adapter.ready' not in source or '0.1.36' not in source: raise Il2CppValidationError('IL2CPP source identity or framework dependency drifted.')
    for token in ('subprocess','import socket','import requests','import urllib.request','os.system','Popen(','Process.Start'):
        if token in tools: raise Il2CppValidationError('IL2CPP package gained process or network execution.')
    routes=text(ROUTES)
    for token in ('"adapter.foa.mono"','"adapter.foa.il2cpp"','m_branch = "mono"','m_branch = "il2cpp"','m_runtimeTarget = "Mono"','m_runtimeTarget = "IL2CPP"','m_frameworkVersion = "0.1.36"','m_bepInExVersion = "6.0.0-be.735"','EvidenceState::PackageInstallValidated'):
        if token not in routes: raise Il2CppValidationError('Canonical Mono/IL2CPP route separation or IL2CPP identity drifted.')
    for path,token in ((BRIDGE,'Il2CppRuntimeAdapterTests=MODULE.Il2CppRuntimeAdapterTests'),(VALIDATOR_TEST,'Il2CppRuntimeAdapterValidationTests'),(DOC,'generated interop'),(RUNTIME_DOC,'FOA IL2CPP Runtime Adapter'),(INDEX,'[`IL2CPP`](IL2CPP/README.md)')):
        if token not in text(path): raise Il2CppValidationError(f'Required IL2CPP test or documentation integration missing: {path.as_posix()}')

def main()->int:
    try: validate()
    except Exception as exc: print(f'IL2CPP runtime-adapter validation failed: {exc}',file=sys.stderr); return 1
    print('IL2CPP runtime-adapter validation passed.'); return 0
if __name__=='__main__': raise SystemExit(main())
