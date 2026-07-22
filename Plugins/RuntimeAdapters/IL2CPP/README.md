# FOA IL2CPP Runtime Adapter

This optional package is the independent IL2CPP route for FoA `1.23.401`.
It targets Unity `6000.0.64f1`, BepInEx `6.0.0-be.735`, Tainted Framework `0.1.36`, and the canonical `adapter.foa.il2cpp` route whose current evidence state is `PackageInstallValidated`.

## Package boundary

The package contains project-owned `net6.0` source, deterministic manifests and schemas, and no generated binary. It requires local BepInEx 6 IL2CPP assemblies, Unity `unity-libs`, and exact generated interop inputs for `Assembly-CSharp.dll` and `TG.Main.dll`. Those local inputs are fingerprinted and are never redistributed.

`plan-build` verifies the package and a caller-supplied canonical interop manifest, then returns an inert build plan. `evaluate-gate` creates only an external-executor review record. It never authorizes execution. `verify-result` validates separately supplied runtime-result evidence.

## Commands

```powershell
python Plugins/RuntimeAdapters/IL2CPP/Tools/il2cpp_runtime_adapter.py verify-package
python Plugins/RuntimeAdapters/IL2CPP/Tools/il2cpp_runtime_adapter.py plan-build --interop-manifest <interop.json>
```

The package contains no process launcher, dependency downloader, interop generator, deployment engine, game launcher, patch, save access, signer, publisher, catalog mutation, or evidence promotion.

Mono remains a separate BepInEx 5 package. Its binaries, manifests, evidence and compatibility cannot satisfy this route.
