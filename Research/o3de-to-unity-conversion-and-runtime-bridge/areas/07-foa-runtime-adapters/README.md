# FoA Runtime Adapters

## Separation from authoring interchange

A FoA runtime adapter is not a Unity Editor interchange provider. It has its own stable adapter identity, runtime-target compatibility, capabilities, permissions, build inputs, package, deployment work order, execution result, cleanup, rollback, and evidence-return contracts.

The Unity Editor package does not contain or build a runtime adapter. A successful Unity import, editor compile, process exit, file copy, or plug-in load does not establish gameplay compatibility or grant another capability.

## Current route observations

Two separate route descriptors exist at the current repository baseline.

### Mono

- adapter ID `adapter.foa.mono`;
- contract version `1.0.0`;
- evidence state `HostLiveLoadValidated`;
- framework `0.1.33`;
- FoA `1.23.401`;
- branch/runtime `mono` / `Mono`;
- Unity `6000.0.64f1`;
- BepInEx `5.4.23.3`;
- licence `NOASSERTION`.

### IL2CPP

- adapter ID `adapter.foa.il2cpp`;
- contract version `1.0.0`;
- evidence state `PackageInstallValidated`;
- framework `0.1.36`;
- FoA `1.23.401`;
- branch/runtime `il2cpp` / `IL2CPP`;
- Unity `6000.0.64f1`;
- BepInEx `6.0.0-be.735`;
- licence `NOASSERTION`.

Source:

https://github.com/theb0yys/FOA-SDK/blob/8fb3f0a729e4be4e513ba896ba52708a73d03eae/Gems/TaintedGrailModdingSDK/Code/Source/FoARuntimeAdapterRoutes.cpp#L99-L183

These are pinned upstream observations, not executable adapter packages and not selection of an operator's active target.

## Current blocked capabilities

Both routes block:

- adapter build;
- deployment;
- execution;
- game API access;
- runtime mutation;
- save access.

The IL2CPP route additionally blocks feature-tested gameplay.

Unknown licensing remains a blocker for dependency redistribution.

## Runtime re-entry sequence

Runtime work may be considered only after the authoring gates and a separate FoA mapping/runtime-payload review:

```text
exact active target-profile evidence
→ one target-specific adapter design
→ synthetic or mock-host build proof
→ reviewed build manifest and package
→ deployment preview
→ explicit confirmation and exact work order
→ separately approved executor with backup and rollback
→ no-op load evidence
→ independent verification
→ candidate evidence intake
→ separately reviewed bounded capability
```

Save or persistence mutation remains an independent later gate.

## Ownership constraints

- Mono and IL2CPP remain separate packages, dependencies, binaries, tests, claims, and evidence.
- `ExternalToolchain` may supervise a generic approved builder later; it does not own game API behavior.
- Runtime adapters own BepInEx, Harmony, Unity, FoA calls, and capability-specific cleanup.
- Manual deployment is not an authoring acceptance criterion.
- Route evidence does not automatically select a target or promote catalog facts.

No research in this area authorizes build, BepInEx/Harmony execution, deployment, game launch, runtime mutation, or save access.
