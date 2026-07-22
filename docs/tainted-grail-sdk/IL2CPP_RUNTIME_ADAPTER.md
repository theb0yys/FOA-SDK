# FOA IL2CPP Runtime Adapter

The optional IL2CPP package lives at `Plugins/RuntimeAdapters/IL2CPP` and is independent from the Mono package.

## Exact route

- FoA `1.23.401`;
- Unity `6000.0.64f1`;
- runtime `IL2CPP`;
- BepInEx `6.0.0-be.735`;
- Tainted Framework `0.1.36`;
- current route evidence `PackageInstallValidated`.

The evidence state is deliberately weaker than live behavior validation. The package must not claim live load merely because a package installed or the upstream host source exists.

## Generated interop contract

A build plan requires a canonical generated-interop manifest containing exact SHA-256 and byte counts for:

- `BepInEx/interop/Assembly-CSharp.dll`;
- `BepInEx/interop/TG.Main.dll`.

The manifest also binds the exact profile, generator identity/version, and generation-input fingerprint. Generated interop is local and non-redistributable. Folder presence, file names, or Mono managed assemblies are not accepted as substitutes.

## Build and execution boundary

The package defines a `net6.0` C# source project with exact local BepInEx 6, Il2CppInterop, Unity and generated-interop references. It emits a deterministic non-executable build plan. No generated DLL or PDB is committed.

The external-executor review gate requires three disjoint evidence classes: adapter identity, install parity, and interop provenance. It also binds the exact build plan, interop manifest, built DLL fingerprint, reviewed prebuilt-SDK installation-manifest fingerprint, requester, and UTC window. The gate records `execution_authorized=false`.

## Runtime-result evidence

Successful external evidence must observe the BepInEx IL2CPP loader, Tainted Framework, Il2CppInterop runtime, the exact generated interop set, and `foa-sdk.il2cpp-adapter.ready`. Results remain candidate evidence and are never promoted automatically.

## Separation

The IL2CPP package does not consume Mono DLLs, BepInEx 5 APIs, `netstandard2.1` output, Mono target signatures, or Mono evidence. Mono and IL2CPP deployments, rollback, live validation and release decisions remain separately reviewed.
