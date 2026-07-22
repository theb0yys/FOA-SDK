# Runtime Routes

FOA-SDK treats Mono and IL2CPP as separate runtime routes with separate loaders, frameworks, binaries, evidence, packaging and compatibility. A result from one route is never inherited by the other.

## Start here

Read [Verified Runtime and Loader Profiles](VERIFIED_PROFILES.md) for the exact currently pinned game, Unity, runtime, loader, framework and evidence observations.

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

Those observations qualify only their exact profiles. Current active installation selection, executable SDK adapter packages, deployment, game launch, runtime mutation and save access remain separately governed.

## Documentation state

Completed:

- exact pinned profile table;
- evidence-state vocabulary;
- Mono/IL2CPP separation rules;
- active-install verification requirements;
- local-reference and package-boundary rules;
- profile-scoped first-mod process.

Still required:

- project-owned route-specific starter source;
- executable adapter qualification;
- controlled deployment/removal tooling;
- live hook-target verification batches;
- collision and load-order reports;
- profile migration and staleness tooling.

## Safety rule

No page may describe a contract-only or inert route as executable. Tutorials, commands and package layouts are published only after exact-profile validation and must state the target and rollback boundary.