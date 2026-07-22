# Runtime Routes

FOA-SDK treats Mono and IL2CPP as separate runtime routes with separate loaders, frameworks, binaries, evidence, packaging and compatibility. A result from one route is never inherited by the other.

## Start here

Read [Verified Runtime and Loader Profiles](VERIFIED_PROFILES.md) for the exact currently pinned game, Unity, runtime, loader, framework and evidence observations.

The first optional route package is the [FOA Mono Runtime Adapter](../../tainted-grail-sdk/MONO_RUNTIME_ADAPTER.md). It contains project-owned BepInEx 5 source, deterministic non-executable planning, an external-executor review gate and typed runtime-result evidence. It does not include a process executor or deployment authority.

## Route record requirements

Every runtime profile must bind:

- exact game version and branch;
- runtime kind;
- loader name and exact version;
- framework/adapter name and exact version;
- target framework and compiler assumptions;
- required dependencies;
- source/build/package identities;
- installation and rollback scope;
- loader discovery and startup evidence;
- supported capabilities and explicit prohibitions;
- staleness and supersession state.

## Current pinned route state

The system-port track currently records:

- Mono: game `1.23.401`, Unity `6000.0.64f1`, BepInEx `5.4.23.3`, Tainted Framework `0.1.33`, evidence state `HostLiveLoadValidated`;
- IL2CPP: game `1.23.401`, Unity `6000.0.64f1`, BepInEx `6.0.0-be.735`, Tainted Framework `0.1.36`, evidence state `PackageInstallValidated`.

Those observations qualify only their exact profiles. Current active installation selection, deployment, game launch, runtime mutation and save access remain separately governed.

## Implementation state

Completed for Mono:

- project-owned route-specific source package;
- exact manifest, compatibility, dependencies and expected binary declarations;
- deterministic build plan with execution disabled;
- external-executor review gate with execution authorization disabled;
- typed external runtime-result evidence and adversarial validation.

Still required for Mono:

- external source build against an exact lawful local profile;
- separately reviewed executor and controlled deployment/removal tooling;
- exact live-load evidence for the project-owned adapter binary;
- live hook-target verification, collision and load-order reports;
- profile migration and staleness tooling.

IL2CPP remains a separate later package with its own source, interop inputs, BepInEx 6 dependencies, binaries, evidence schema, tests and execution gate.

## Safety rule

No page may describe a contract-only or inert route as executable. Tutorials, commands and package layouts are published only after exact-profile validation and must state the target and rollback boundary.
