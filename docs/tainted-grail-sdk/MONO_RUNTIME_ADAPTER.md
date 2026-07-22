# FOA Mono Runtime Adapter Package

## Status

The first runtime-adapter package is the optional, project-owned Mono source package under:

```text
Plugins/RuntimeAdapters/Mono/
```

It targets only:

```text
profile: foa-1.23.401-mono-bepinex5-tf-0.1.33
game: 1.23.401
branch: mono
runtime: Mono
Unity: 6000.0.64f1
BepInEx: 5.4.23.3
Tainted Framework: 0.1.33
```

The route's pinned evidence state is `HostLiveLoadValidated`. That evidence is an upstream route observation, not proof that an arbitrary newly built adapter binary loads on an operator's installation.

## Package identity

```text
FOA-SDK package: extension.foa-mono-runtime-adapter
runtime route: adapter.foa.mono
package version: 0.1.0
route contract: 1.0.0
assembly: FOA.SDK.RuntimeAdapter.Mono
BepInEx plug-in GUID: foa.sdk.runtime-adapter.mono
startup marker: foa-sdk.mono-adapter.ready
target framework: netstandard2.1
```

The source package depends on exact local BepInEx 5 and Unity managed references and on Tainted Framework `0.1.33` at runtime. Those dependencies are not committed or redistributed by this package.

## Source and binary boundary

The project-owned C# source is intentionally minimal:

- BepInEx 5 plug-in declaration;
- exact Tainted Framework dependency declaration;
- one deterministic report-only startup marker;
- no Harmony patch;
- no game API call;
- no filesystem or network operation;
- no save access;
- no gameplay or runtime mutation.

The package declares its expected DLL and optional PDB but contains neither generated binary. A later external builder must use the deterministic build plan and publish build-result evidence before package assembly or deployment can be considered.

## Build plan

`mono_runtime_adapter.py plan-build` verifies the package source fingerprints and emits canonical JSON containing:

- exact profile and package identities;
- project source fingerprints;
- exact non-redistributable local dependencies;
- the reviewed `dotnet build` argument vector;
- expected binary paths;
- `build_plan_sha256`;
- all operational authority set to false.

The tool does not run `dotnet`. No executor is included.

## Execution gate

The execution gate is a review-readiness record for a later external executor. It requires:

1. exact Mono profile equality;
2. positive binary size and SHA-256;
3. separate adapter-identity and exact-install-parity evidence sets;
4. a verified Suite Wizard confirmation-receipt fingerprint;
5. named requester;
6. a bounded whole-second UTC window;
7. exact build-plan binding.

A successfully constructed gate records only `ready_for_external_executor_review=true`; `execution_authorized` and every authority field remain false. Invalid or incomplete requests produce no gate. Process launch, deployment, game launch, and runtime execution must be granted only by a later separately reviewed executor contract that independently reverifies the evidence, receipt, profile, and binary.

## Runtime-result evidence

The external result schema supports:

- `not_attempted`;
- `succeeded`;
- `failed`.

A successful result requires exact chronology and positive observation of:

- the BepInEx loader;
- Tainted Framework;
- `foa-sdk.mono-adapter.ready`.

The result carries no raw log content or private path. It accepts only a safe relative logical log reference and bounded failure text. Candidate evidence remains unpromoted.

## Mono/IL2CPP separation

This package must never contain:

- IL2CPP BepInEx 6 dependencies;
- generated interop assemblies;
- IL2CPP loader APIs;
- IL2CPP type/reflection claims;
- IL2CPP binaries or evidence;
- a combined runtime selector.

The IL2CPP adapter is a later package with its own manifest, source, dependencies, binaries, evidence state, result schema, tests, and execution gate.

## Validation

Repository gates check:

- exact profile and route bindings;
- project-owned source fingerprints;
- deterministic build-plan bytes;
- no generated binaries;
- no subprocess or network executor;
- no Harmony/game/save/file authority;
- gate refusal for IL2CPP or profile drift;
- result chronology and startup observations;
- authority escalation refusal;
- mandatory test discovery;
- documentation and runtime-route separation.

Hosted static validation does not replace a source build, BepInEx load test, O3DE compiled test, deployment test, or Windows runtime evidence.
