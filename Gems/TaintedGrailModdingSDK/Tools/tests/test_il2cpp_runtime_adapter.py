# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import importlib.util, sys
from pathlib import Path
TEST_MODULE=Path(__file__).resolve().parents[4]/'Plugins/RuntimeAdapters/IL2CPP/Tests/test_il2cpp_runtime_adapter.py'
SPEC=importlib.util.spec_from_file_location('foa_il2cpp_runtime_adapter_tests',TEST_MODULE)
if SPEC is None or SPEC.loader is None: raise RuntimeError(f'Unable to load IL2CPP tests: {TEST_MODULE}')
MODULE=importlib.util.module_from_spec(SPEC); sys.modules[SPEC.name]=MODULE; SPEC.loader.exec_module(MODULE)
Il2CppRuntimeAdapterTests=MODULE.Il2CppRuntimeAdapterTests
