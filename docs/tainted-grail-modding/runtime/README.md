# Runtime Routes

FOA-SDK treats Mono and IL2CPP as separate runtime routes with separate loaders, frameworks, binaries, evidence, packaging and compatibility. A result from one route is never inherited by the other.

## Start here

Read [Verified Runtime and Loader Profiles](VERIFIED_PROFILES.md) for the exact currently pinned game, Unity, runtime, loader, framework and evidence observations.

The optional route packages are the [FOA Mono Runtime Adapter](../../tainted-grail-sdk/MONO_RUNTIME_ADAPTER.md) and the [FOA IL2CPP Runtime Adapter](../../tainted-grail-sdk/IL2CPP_RUNTIME_ADAPTER.md). Both contain project-owned source and non-executable planning/review contracts; neither includes a process executor or deployment authority.

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

Completed independently for both routes:

- project-owned route-specific source packages;
- exact manifests, compatibility, dependencies and expected binary declarations;
- deterministic build plans with execution disabled;
- external-executor review gates with execution authorization disabled;
- typed external runtime-result evidence and adversarial validation.

IL2CPP additionally requires exact generated-interop manifests for `Assembly-CSharp.dll` and `TG.Main.dll`; those inputs are never projected onto Mono.

Within every Mono-specific plan, IL2CPP remains a separate later package: its independently implemented source, dependencies, generated interop, binaries, evidence, deployment and release decisions cannot satisfy the Mono route.

Still required separately for each route:

- external source build against an exact lawful local profile;
- separately reviewed executor and controlled deployment/removal tooling;
- exact live-load evidence for each project-owned adapter binary;
- live hook-target verification, collision and load-order reports;
- profile migration and staleness tooling.

## Safety rule

No page may describe a contract-only or inert route as executable. Tutorials, commands and package layouts are published only after exact-profile validation and must state the target and rollback boundary.
