# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import importlib.util, shutil, sys, tempfile, unittest
from pathlib import Path
REPO=Path(__file__).resolve().parents[4]; VALIDATOR=REPO/'Gems/TaintedGrailModdingSDK/Tools/validate_il2cpp_runtime_adapter.py'
SPEC=importlib.util.spec_from_file_location('foa_validate_il2cpp_runtime_adapter',VALIDATOR)
if SPEC is None or SPEC.loader is None: raise RuntimeError('Unable to load IL2CPP validator.')
MODULE=importlib.util.module_from_spec(SPEC); sys.modules[SPEC.name]=MODULE; SPEC.loader.exec_module(MODULE)
class Il2CppRuntimeAdapterValidationTests(unittest.TestCase):
    def fixture(self):
        temp=tempfile.TemporaryDirectory(); root=Path(temp.name)
        paths=['Plugins/RuntimeAdapters/IL2CPP','Plugins/RuntimeAdapters/README.md','docs/tainted-grail-sdk/IL2CPP_RUNTIME_ADAPTER.md','docs/tainted-grail-modding/runtime/README.md','Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp','Gems/TaintedGrailModdingSDK/Tools/tests/test_il2cpp_runtime_adapter.py','Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_il2cpp_runtime_adapter.py']
        for rel in paths:
            src=REPO/rel; dst=root/rel; dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copytree(src,dst) if src.is_dir() else shutil.copy2(src,dst)
        return temp,root
    def mutate(self,rel,old,new):
        temp,root=self.fixture(); path=root/rel; data=path.read_text(); self.assertIn(old,data); path.write_text(data.replace(old,new)); return temp,root
    def assert_bad(self,temp,root):
        try:
            with self.assertRaises(MODULE.Il2CppValidationError): MODULE.validate(root)
        finally: temp.cleanup()
    def test_reviewed_package_passes(self): MODULE.validate(REPO)
    def test_source_tamper_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/Source/Il2CppRuntimeAdapterPlugin.cs','reportOnly=true','reportOnly=false'))
    def test_binary_insertion_fails(self):
        temp,root=self.fixture(); (root/'Plugins/RuntimeAdapters/IL2CPP/FOA.SDK.RuntimeAdapter.IL2CPP.dll').write_bytes(b'x'); self.assert_bad(temp,root)
    def test_mono_compatibility_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/plugin.json','"il2cpp"','"mono"'))
    def test_bepinex5_api_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/Source/Il2CppRuntimeAdapterPlugin.cs','BasePlugin','BaseUnityPlugin'))
    def test_target_framework_drift_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/Source/FOA.SDK.RuntimeAdapter.IL2CPP.csproj','net6.0','netstandard2.1'))
    def test_missing_interop_reference_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/Source/FOA.SDK.RuntimeAdapter.IL2CPP.csproj','$(FoAInteropDir)\\TG.Main.dll','$(FoAInteropDir)\\Missing.dll'))
    def test_executor_import_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/Tools/il2cpp_runtime_adapter.py','import argparse, sys','import argparse, sys, subprocess'))
    def test_route_conflation_fails(self): self.assert_bad(*self.mutate('Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp','"adapter.foa.il2cpp"','"adapter.foa.mono"'))
    def test_golden_plan_tamper_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/IL2CPP/Tests/Fixtures/build-plan.json','foa-il2cpp-adapter-build-plan','foa-combined-adapter-build-plan'))
    def test_bridge_removal_fails(self): self.assert_bad(*self.mutate('Gems/TaintedGrailModdingSDK/Tools/tests/test_il2cpp_runtime_adapter.py','Il2CppRuntimeAdapterTests','RemovedTests'))
    def test_documented_separation_removal_fails(self): self.assert_bad(*self.mutate('Plugins/RuntimeAdapters/README.md','[`IL2CPP`](IL2CPP/README.md)','IL2CPP'))
if __name__=='__main__': unittest.main()
